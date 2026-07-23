/*
 * intv_memmap_flat.c — Memory map for an Intellivision multicart on MCU
 *                      (flat per-page-plane variant, see the header)
 */

#include <string.h>
#include "memory.h"

mm_map_t m;

/* ------------------------------------------------------------------ */
/*  Map construction                                                   */
/* ------------------------------------------------------------------ */
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
    m->entry[i].src_start = rom_start;
    m->entry[i].src_end   = rom_end;
    m->entry[i].cpu_start = cpu_start;
    m->entry[i].page      = page;
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
    m->entry[i].src_start = m->ram_words;
    m->entry[i].src_end   = m->ram_words + len;
    m->entry[i].cpu_start = cpu_start;
    m->entry[i].page      = MM_NO_PAGE;
    m->delta[i]    = (int32_t)m->ram_words - (int32_t)cpu_start;
    m->kind[i]     = (width == 8) ? MM_RAM8 : MM_RAM16;
    m->ram_words  += len + 1;
    return i;
}

/* Slow resolver (build time only), per page plane: an entry is visible
   on a plane if it is fixed/RAM or belongs to that page. With
   overlaps, the last added entry wins, matching jzintv semantics. */
static uint8_t mm_resolve_slow(const mm_map_t *m, uint8_t plane,
                               uint16_t addr)
{
    uint8_t id = (uint8_t)m->count;             /* ghost "unmapped"       */
    for (int i = 0; i < m->count; i++) {
        const mm_entry_t *e = &m->entry[i];
        if (e->page != MM_NO_PAGE && e->page != (int8_t)plane) continue;
        if (addr < e->cpu_start) continue;
        if ((uint32_t)addr - e->cpu_start > e->src_end - e->src_start)
            continue;
        id = (uint8_t)i;
    }
    return id;
}

/* Finds an identical split table, or appends a new one.
   Returns the split index, or MM_ERR_SPLITS_FULL. */
static int mm_intern_split(mm_map_t *m, int *n_splits, const mm_split_t *s)
{
    for (int k = 0; k < *n_splits; k++)
        if (memcmp(&m->split[k], s, sizeof(*s)) == 0)
            return k;
    if (*n_splits >= MM_MAX_SPLITS) return MM_ERR_SPLITS_FULL;
    m->split[*n_splits] = *s;
    return (*n_splits)++;
}

/* Builds the 16 page planes, the deduplicated split tables and the
   per-block liveness bitmap. Call once, after all the entries have
   been added. Returns 0, or an MM_ERR_* code. */
int mm_finalize(mm_map_t *m)
{
    uint8_t ghost = (uint8_t)m->count;
    m->delta[ghost] = 0;                        /* ghost "unmapped" entry */
    m->kind[ghost]  = MM_NONE;
    m->none_id      = ghost;

    int n_splits = 0;
    int n_blocks = 65536 >> MM_BLOCK_SHIFT;

    memset(m->blk_any, 0, sizeof(m->blk_any));

    for (int plane = 0; plane < 16; plane++) {
        for (int blk = 0; blk < n_blocks; blk++) {
            uint32_t base = (uint32_t)blk << MM_BLOCK_SHIFT;
            uint32_t top  = base + (1u << MM_BLOCK_SHIFT) - 1;

            /* fast path: if no visible entry has a boundary strictly
               inside this block, the block is uniform and a single
               resolve is enough (this covers the vast majority of the
               16 x 256 blocks and makes the build ~100x faster) */
            bool has_boundary = false;
            for (int i = 0; i < m->count && !has_boundary; i++) {
                const mm_entry_t *e = &m->entry[i];
                if (e->page != MM_NO_PAGE && e->page != (int8_t)plane)
                    continue;
                uint32_t s = e->cpu_start;
                uint32_t en = s + (e->src_end - e->src_start);
                if ((s > base && s <= top) || (en >= base && en < top))
                    has_boundary = true;
            }
            if (!has_boundary) {
                m->map[plane][blk] =
                    mm_resolve_slow(m, (uint8_t)plane, (uint16_t)base);
                continue;
            }

            /* compress the block into uniform contiguous sub-ranges */
            uint16_t bound[MM_SPLIT_WAYS];
            uint8_t  id[MM_SPLIT_WAYS];
            int      runs = 0;

            uint8_t cur = mm_resolve_slow(m, (uint8_t)plane,
                                          (uint16_t)base);
            for (uint32_t a = base + 1;
                 a < base + (1u << MM_BLOCK_SHIFT); a++) {
                uint8_t v = mm_resolve_slow(m, (uint8_t)plane,
                                            (uint16_t)a);
                if (v != cur) {
                    if (runs >= MM_SPLIT_WAYS - 1) return MM_ERR_FRAGMENTED;
                    bound[runs] = (uint16_t)(a - 1);
                    id[runs++]  = cur;
                    cur = v;
                }
            }
            bound[runs] = (uint16_t)(base + (1u << MM_BLOCK_SHIFT) - 1);
            id[runs++]  = cur;

            if (runs == 1) {
                m->map[plane][blk] = id[0];     /* uniform block          */
            } else {
                mm_split_t s;
                for (int k = 0; k < MM_SPLIT_WAYS; k++) {
                    int j = (k < runs) ? k : runs - 1;
                    s.id[k] = id[j];
                    if (k < MM_SPLIT_WAYS - 1)
                        s.bound[k] = (k < runs - 1) ? bound[k] : 0xFFFF;
                }
                int si = mm_intern_split(m, &n_splits, &s);
                if (si < 0) return si;
                m->map[plane][blk] = (uint8_t)(MM_SPLIT + si);
            }
        }
    }

    /* per-block liveness bitmap: bit set if any page maps anything in
       the block (a split cell always maps at least one word) */
    for (int blk = 0; blk < n_blocks; blk++) {
        for (int plane = 0; plane < 16; plane++) {
            if (m->map[plane][blk] != ghost) {
                m->blk_any[blk >> 5] |= 1u << (blk & 31);
                break;
            }
        }
    }
    return 0;
}

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
