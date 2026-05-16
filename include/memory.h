#ifndef MEMORY_H_
#define MEMORY_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NSLOTS    256
#define NPAGES     16
#define MAX_HACKS_NUM   32

/*
 * mapEntry is a struct to contain Intellicart config "row"
 *
 * e.g.  $0000 - $1FFF = $5000
 *
 * $0000:   ROM cart "from" address    (16 pages available)
 * $1FFF:   ROM cart "to" address      (16 pages available)
 * $5000:   Intellivision bus address
 *
 */

typedef enum {
    ROM_SLOT,
    ROM_PAGE_SLOT,
    RAM8_SLOT,
    RAM16_SLOT
} mapType;

struct mapEntry {
   uint32_t from[NPAGES];
   uint16_t size[NPAGES];
   uint16_t target;
   mapType type;
   bool filled;
};

struct mapHole {
   uint32_t from;
   uint16_t size;
   bool filled;
};

struct memHack {
   uint16_t address;
   uint8_t value;
};

void printSlot(uint8_t idx, uint8_t page);
void printFilledSlots(void);
void cleanSlots(void);
void cleanHoles(void);
void addSlot(uint32_t from, uint32_t to, uint16_t target, uint8_t page, mapType type);
bool mapSlot(uint16_t addr, uint8_t page, uint8_t *slot);
bool mapAddress(uint16_t addr, uint8_t page, uint32_t *romaddr, mapType *type);
void getRAMRange(uint16_t *ramfrom, uint16_t *ramto, uint8_t *ramwidth);
void cleanHacks(void);
void addHack(uint16_t addr, uint8_t value);
uint8_t getHacksNum(void);

void config_memory(int cfg);

#endif
