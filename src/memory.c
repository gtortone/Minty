/*
 * intv_memmap_flat.c — Memory map for an Intellivision multicart on MCU
 *                      (flat per-page-plane variant)
 *
 * See intv_memmap_flat.h for the architecture overview and the API.
 *
 * File layout:
 *   1. Build API ......... mm_init / mm_add / mm_add_ram
 *   2. Build helpers ..... entry visibility, resolver, run compression,
 *                          split-table interning, block encoding
 *   3. Finalization ...... mm_finalize (planes + splits + liveness)
 *   4. Convenience ....... mm_load
 */

#include <string.h>
#include "memory.h"

mm_map_t m;

/* ================================================================== */
/*  1. Build API                                                       */
/* ================================================================== */

void mm_init(mm_map_t *m)
{
    memset(m, 0, sizeof(*m));
}

/* Adds a ROM entry (equivalent to one [mapping] line).
   Partial pages, multiple fragments on the same (segment, page) and
   entries straddling segments are all accepted.
   Returns the entry index (>= 0) or an MM_ERR_* code. */
int mm_add(mm_map_t *m, uint32_t rom_start, uint32_t rom_end,
           uint16_t cpu_start, int8_t page)
{
    if (m->count >= MM_MAX_ENTRIES)                    return MM_ERR_FULL;
    if (rom_end < rom_start)                           return MM_ERR_RANGE;
    uint32_t len = rom_end - rom_start;                /* len = words-1   */
    if ((uint32_t)cpu_start + len > 0xFFFF)            return MM_ERR_RANGE;
    if (page != MM_NO_PAGE && (page < 0 || page > 15)) return MM_ERR_PAGED_ALIGN;

    int i = m->count++;
    m->entry[i] = (mm_entry_t){ rom_start, rom_end, cpu_start, page };
    m->delta[i] = (int32_t)rom_start - (int32_t)cpu_start;
    m->kind[i]  = MM_ROM;
    return i;
}

/* Adds a RAM area (equivalent to one "RAM 8" / "RAM 16" line in
   [memattr]). cpu_start/cpu_end are Intellivision bus addresses
   (inclusive); width is the data width in bits (8 or 16).
   Areas are appended to the RAM space: the first starts at offset 0,
   the following ones continue contiguously. RAM is visible on every
   page plane. Returns the entry index (>= 0) or an MM_ERR_* code. */
int mm_add_ram(mm_map_t *m, uint16_t cpu_start, uint16_t cpu_end,
               uint8_t width)
{
    if (m->count >= MM_MAX_ENTRIES)                    return MM_ERR_FULL;
    if (cpu_end < cpu_start)                           return MM_ERR_RANGE;
    if (width != 8 && width != 16)                     return MM_ERR_RAM_WIDTH;
    uint32_t len = (uint32_t)cpu_end - cpu_start;      /* words-1         */

    int i = m->count++;
    m->entry[i] = (mm_entry_t){ m->ram_words, m->ram_words + len,
                                cpu_start, MM_NO_PAGE };
    m->delta[i]    = (int32_t)m->ram_words - (int32_t)cpu_start;
    m->kind[i]     = (width == 8) ? MM_RAM8 : MM_RAM16;
    m->ram_words  += len + 1;
    return i;
}

/* ================================================================== */
/*  2. Build helpers (build time only)                                 */
/* ================================================================== */

/* An entry is visible on a plane if it is fixed/RAM or belongs to
   that page. This is THE visibility rule: every build step below goes
   through it. */
static bool entry_visible(const mm_entry_t *e, uint8_t plane)
{
    return e->page == MM_NO_PAGE || e->page == (int8_t)plane;
}

/* Last Intellivision bus address covered by the entry (inclusive). */
static uint16_t entry_cpu_end(const mm_entry_t *e)
{
    return (uint16_t)(e->cpu_start + (e->src_end - e->src_start));
}

static bool entry_covers(const mm_entry_t *e, uint16_t addr)
{
    return addr >= e->cpu_start && addr <= entry_cpu_end(e);
}

/* Resolves one address on one plane. With overlaps, the last added
   entry wins, matching jzintv .cfg semantics.
   Returns an entry index, or m->count (the ghost "unmapped" entry). */
static uint8_t resolve_addr(const mm_map_t *m, uint8_t plane,
                            uint16_t addr)
{
    uint8_t id = (uint8_t)m->count;
    for (int i = 0; i < m->count; i++) {
        const mm_entry_t *e = &m->entry[i];
        if (entry_visible(e, plane) && entry_covers(e, addr))
            id = (uint8_t)i;
    }
    return id;
}

/* True if no visible entry has a boundary strictly inside the block:
   the block is then uniform and one resolve_addr() is enough. This
   fast path covers the vast majority of the 16 x 256 blocks and makes
   the build ~100x faster than resolving every word. */
static bool block_is_uniform(const mm_map_t *m, uint8_t plane,
                             uint32_t base, uint32_t top)
{
    for (int i = 0; i < m->count; i++) {
        const mm_entry_t *e = &m->entry[i];
        if (!entry_visible(e, plane)) continue;
        uint32_t s = e->cpu_start;
        uint32_t en = entry_cpu_end(e);
        if ((s > base && s <= top) || (en >= base && en < top))
            return false;
    }
    return true;
}

/* Compresses one block into uniform contiguous sub-ranges.
   Returns the number of runs (1..MM_SPLIT_WAYS), filling bound[]/id[],
   or MM_ERR_FRAGMENTED if the block has too many zones. */
static int compress_block(const mm_map_t *m, uint8_t plane, uint32_t base,
                          uint16_t bound[MM_SPLIT_WAYS],
                          uint8_t  id[MM_SPLIT_WAYS])
{
    int runs = 0;
    uint8_t cur = resolve_addr(m, plane, (uint16_t)base);

    for (uint32_t a = base + 1; a < base + MM_BLOCK_WORDS; a++) {
        uint8_t v = resolve_addr(m, plane, (uint16_t)a);
        if (v == cur) continue;
        if (runs >= MM_SPLIT_WAYS - 1) return MM_ERR_FRAGMENTED;
        bound[runs] = (uint16_t)(a - 1);
        id[runs++]  = cur;
        cur = v;
    }
    bound[runs] = (uint16_t)(base + MM_BLOCK_WORDS - 1);
    id[runs++]  = cur;
    return runs;
}

/* Finds an identical split table, or appends a new one.
   Returns the split index, or MM_ERR_SPLITS_FULL. */
static int intern_split(mm_map_t *m, int *n_splits, const mm_split_t *s)
{
    for (int k = 0; k < *n_splits; k++)
        if (memcmp(&m->split[k], s, sizeof(*s)) == 0)
            return k;
    if (*n_splits >= MM_MAX_SPLITS) return MM_ERR_SPLITS_FULL;
    m->split[*n_splits] = *s;
    return (*n_splits)++;
}

/* Normalizes runs into a split table: unused slots repeat the last
   run, unused bounds are 0xFFFF, so the unrolled lookup compares work
   for every address. */
static void runs_to_split(const uint16_t bound[], const uint8_t id[],
                          int runs, mm_split_t *s)
{
    for (int k = 0; k < MM_SPLIT_WAYS; k++) {
        int j = (k < runs) ? k : runs - 1;
        s->id[k] = id[j];
        if (k < MM_SPLIT_WAYS - 1)
            s->bound[k] = (k < runs - 1) ? bound[k] : 0xFFFF;
    }
}

/* Encodes one (plane, block) cell: uniform id, or split reference.
   Returns 0, or an MM_ERR_* code. */
static int encode_block(mm_map_t *m, int *n_splits, uint8_t plane, int blk)
{
    uint32_t base = (uint32_t)blk << MM_BLOCK_SHIFT;
    uint32_t top  = base + MM_BLOCK_WORDS - 1;

    if (block_is_uniform(m, plane, base, top)) {
        m->map[plane][blk] = resolve_addr(m, plane, (uint16_t)base);
        return 0;
    }

    uint16_t bound[MM_SPLIT_WAYS];
    uint8_t  id[MM_SPLIT_WAYS];
    int runs = compress_block(m, plane, base, bound, id);
    if (runs < 0) return runs;

    if (runs == 1) {
        m->map[plane][blk] = id[0];
        return 0;
    }

    mm_split_t s;
    runs_to_split(bound, id, runs, &s);
    int si = intern_split(m, n_splits, &s);
    if (si < 0) return si;
    m->map[plane][blk] = (uint8_t)(MM_SPLIT + si);
    return 0;
}

/* Per-block liveness bitmap: bit set if any page maps anything in the
   block (a split cell always maps at least one word). Feeds
   mm_block_dead(). */
static void build_liveness(mm_map_t *m)
{
    memset(m->blk_any, 0, sizeof(m->blk_any));
    for (int blk = 0; blk < MM_NUM_BLOCKS; blk++)
        for (int plane = 0; plane < MM_NUM_PLANES; plane++)
            if (m->map[plane][blk] != m->none_id) {
                m->blk_any[blk >> 5] |= 1u << (blk & 31);
                break;
            }
}

/* ================================================================== */
/*  3. Finalization                                                    */
/* ================================================================== */

/* Builds the 16 page planes, the deduplicated split tables and the
   liveness bitmap. Call once, after all the entries have been added.
   Returns 0, or an MM_ERR_* code. */
int mm_finalize(mm_map_t *m)
{
    m->none_id      = (uint8_t)m->count;        /* ghost "unmapped" entry */
    m->delta[m->none_id] = 0;
    m->kind[m->none_id]  = MM_NONE;

    int n_splits = 0;
    for (int plane = 0; plane < MM_NUM_PLANES; plane++)
        for (int blk = 0; blk < MM_NUM_BLOCKS; blk++) {
            int r = encode_block(m, &n_splits, (uint8_t)plane, blk);
            if (r < 0) return r;
        }

    build_liveness(m);
    return 0;
}

/* ================================================================== */
/*  4. Convenience                                                     */
/* ================================================================== */

/* One-call loader: builds a whole map from definition tables. */
int mm_load(mm_map_t *m,
            const mm_rom_def_t *rom_defs, unsigned n_rom,
            const mm_ram_def_t *ram_defs, unsigned n_ram)
{
    mm_init(m);
    for (unsigned i = 0; i < n_rom; i++) {
        int r = mm_add(m, rom_defs[i].rom_start, rom_defs[i].rom_end,
                          rom_defs[i].cpu_start, rom_defs[i].page);
        if (r < 0) return r;
    }
    for (unsigned j = 0; j < n_ram; j++) {
        int r = mm_add_ram(m, ram_defs[j].cpu_start, ram_defs[j].cpu_end,
                              ram_defs[j].width);
        if (r < 0) return r;
    }
    return mm_finalize(m);
}


void config_memory(int cfg) {

   mm_init(&m);

   switch (cfg) {

      case 0:
         mm_add(&m, 0x0000, 0x1FFF, 0x5000, MM_NO_PAGE);
         mm_add(&m, 0x2000, 0x2FFF, 0xD000, MM_NO_PAGE);
         mm_add(&m, 0x3000, 0x3FFF, 0xF000, MM_NO_PAGE);
         mm_finalize(&m);
         break;

      case 1:
         mm_add(&m, 0x0000, 0x1FFF, 0x5000, MM_NO_PAGE);
         mm_add(&m, 0x2000, 0x4FFF, 0xD000, MM_NO_PAGE);
         mm_finalize(&m);
         break;

      case 2:
         mm_add(&m, 0x0000, 0x1FFF, 0x5000, MM_NO_PAGE); 
         mm_add(&m, 0x2000, 0x4FFF, 0x9000, MM_NO_PAGE);
         mm_add(&m, 0x5000, 0x5FFF, 0xD000, MM_NO_PAGE);
         mm_finalize(&m);
         break;

      case 3:
         mm_add(&m, 0x0000, 0x1FFF, 0x5000, MM_NO_PAGE); 
         mm_add(&m, 0x2000, 0x3FFF, 0x9000, MM_NO_PAGE);
         mm_add(&m, 0x4000, 0x4FFF, 0xD000, MM_NO_PAGE);
         mm_add(&m, 0x5000, 0x5FFF, 0xF000, MM_NO_PAGE);
         mm_finalize(&m);
         break;

      case 4: 
         mm_add(&m, 0x0000, 0x1FFF, 0x5000, MM_NO_PAGE);
         mm_add_ram(&m, 0xD000, 0xD3FF, 8);
         mm_finalize(&m);
         break;

      case 5:
         mm_add(&m, 0x0000, 0x2FFF, 0x5000, MM_NO_PAGE);
         mm_add(&m, 0x3000, 0x5FFF, 0x9000, MM_NO_PAGE);
         mm_finalize(&m);
         break;

      case 6:
         mm_add(&m, 0x0000, 0x1FFF, 0x6000, MM_NO_PAGE);
         mm_finalize(&m);
         break;

      case 7:
         mm_add(&m, 0x0000, 0x1FFF, 0x4800, MM_NO_PAGE);
         mm_finalize(&m);
         break;

      case 8:
         mm_add(&m, 0x0000, 0x0FFF, 0x5000, MM_NO_PAGE);
         mm_add(&m, 0x1000, 0x1FFF, 0x7000, MM_NO_PAGE);
         mm_finalize(&m);
         break;

      case 9:
         mm_add(&m, 0x0000, 0x1FFF, 0x5000, MM_NO_PAGE);
         mm_add(&m, 0x2000, 0x3FFF, 0x9000, MM_NO_PAGE);
         mm_add(&m, 0x4000, 0x4FFF, 0xD000, MM_NO_PAGE);
         mm_add(&m, 0x5000, 0x5FFF, 0xF000, MM_NO_PAGE);
         mm_add_ram(&m, 0x8800, 0x8FFF, 8);
         mm_finalize(&m);
         break;

      default:
         break;
   }
}
