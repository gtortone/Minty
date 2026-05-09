#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define NSLOTS    256
#define NPAGES     16

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
    ROM,
    RAM8,
    RAM16
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

/*
 * each element of slots array is a bucket for MSB of Intellivision
 * bus address
 *
 * e.g.  $5000    ->    bucket 0x50 (80)
 */

struct mapEntry slots[NSLOTS];
struct mapHole holes[NSLOTS];

void printSlot(uint8_t idx, uint8_t page) {

   printf("slot #0x%X: type: %d, from: 0x%X, to: 0x%X, target: 0x%X, page: 0x%X, [%s]\n", 
         idx, slots[idx].type, slots[idx].from[page], slots[idx].from[page]+slots[idx].size[page], 
         slots[idx].target, page, slots[idx].filled?"FILLED":"EMPTY");
}

void printFilledSlots(void) {

   for(int i=0; i<NSLOTS; i++) {
      if (slots[i].filled) {
         for(int page=0; page<NPAGES; page++) {
            if (slots[i].from[page]) {
               printf("%X) $%X - $%X = $%X PAGE %d\n", 
                     i, slots[i].from[page], slots[i].from[page]+slots[i].size[page], 
                     slots[i].target, page);
            }
         }
      }
   }
}

void cleanSlots(void) {

   for(int i=0; i<NSLOTS; i++) {
      memset(slots[i].from, 0, NPAGES);
      memset(slots[i].size, 0, NPAGES);
      slots[i].target = 0;
      slots[i].type = 0;
      slots[i].filled = false;
   }
}

void cleanHoles(void) {

   for(int i=0; i<NSLOTS; i++) {
      holes[i].from = 0;
      holes[i].size = 0;
      holes[i].filled = false;
   }
}

/*
   * addSlot function fills a slot using nearby slots to fill
   * whole available range
   *
   * e.g.    $8000 - $844D = $4800
   *
   * bus address $4800 is mapped from $8000 up to $844D
   * so this range goes from $4800 to $4800 + ($844D - $8000) = $4C4D
   *
   * relevant slots are:   48, 49, 4A, 4B, 4C
   *
   * 48) $8000 - $844D = $4800
   * 49) $8000 - $844D = $4800
   * 4A) $8000 - $844D = $4800
   * 4B) $8000 - $844D = $4800
   * 4C) $8000 - $844D = $4800
   *
   */

void addSlot(uint32_t from, uint32_t to, uint16_t target, uint8_t page, mapType type) {
   
   uint8_t nslots = 0;
   bool hole = false;
   uint8_t baseslot = 0;

   if (type == ROM)
      baseslot = (target >> 8);
   else
      baseslot = (from >> 8);

   // detect collision  (only for ROM type for not paged addresses)
   if ( (type == ROM) && slots[baseslot].filled && (page == 0)) {

      uint32_t from_in = slots[baseslot].from[page];
      uint32_t to_in = slots[baseslot].from[page] + slots[baseslot].size[page];

      uint32_t target_from = target;
      uint32_t target_to = target + (to - from);

      uint32_t target_from_in = slots[baseslot].target;
      uint32_t target_to_in = slots[baseslot].target + slots[baseslot].size[page];

      // handle map hole
      if (target_from > target_from_in) {
         if ( (target_from - target_to_in) > 1 ) {
            // hole detected
            hole = true;
            holes[baseslot].from = target_to_in;
            holes[baseslot].size = (target_from - target_to_in) - 1;
            holes[baseslot].filled = true;
         }

      } else {
         if ( (target_from_in - target_to) > 1 ) {
            // hole detected
            hole = true;
            holes[baseslot].from = target_to;
            holes[baseslot].size = (target_from_in - target_to) - 1; 
            holes[baseslot].filled = true;
         }
      }
      
      if (slots[baseslot].from[page]) {
         if (from_in < from)
            from = from_in;
      }

      if (slots[baseslot].size[page]) {
         if (to_in > to)
            to = to_in;
      }
      if (slots[baseslot].target < target)
         target = slots[baseslot].target;
   }

   //if(hole) {
   //   printf("--- hole --- from: 0x%X, size: %d\n",
   //      holes[baseslot].from, holes[baseslot].size);
   //}

   if (type == ROM)
      nslots = (( (target + (to - from)) ) >> 8) - (target >> 8);
   else
      nslots = (to >> 8) - (from >> 8);

   for(int i=baseslot; i<=(baseslot + nslots); i++) {

      slots[i].from[page] = from;
      slots[i].size[page] = to - from;
      if (type == ROM)
         slots[i].target = target;
      else
         slots[i].target = 0;
      slots[i].type = type;
      slots[i].filled = true;

      //printf("## FILL ## ");
      //printSlot(i, page);
   }
}

// return slot index if address is mapped and value compatible with slot size
bool mapSlot(uint16_t addr, uint8_t page, uint8_t *slot) {

   uint8_t idx = (addr >> 8);
   uint16_t offset = 0;

   if (slots[idx].type == ROM) {

      if ( (slots[idx].filled) && ((addr - slots[idx].target) <= slots[idx].size[page]) ) {
         *slot = idx;
         return true;
      }

   } else {    // RAM8 or RAM16
   
      if ( (slots[idx].filled) && ((addr - slots[idx].from[0]) <= slots[idx].size[0]) ) {
         *slot = idx;
         return true;
      }
   }

   return false;
}

// return ROM/cartridge address for Intellivision address
bool mapAddress(uint16_t addr, uint8_t page, uint32_t *romaddr) {

   uint8_t slot;

   if ( mapSlot(addr, page, &slot) ) {

      *romaddr = slots[slot].from[page] + (addr - slots[slot].target);
      
      if (holes[slot].filled) {
         if (addr > holes[slot].from)
            *romaddr -= holes[slot].size;
      }

      return true;
   }

   return false;
}

void test_cfg0(void) {

   /*
    * $0000 - $1FFF = $5000   ;  8K to $5000 - $6FFF
    * $2000 - $2FFF = $D000   ;  4K to $D000 - $DFFF 
    * $3000 - $3FFF = $F000   ;  4K to $F000 - $FFFF
    */

   printf("--- start test_cfg0 ---\n");

   cleanSlots();
   cleanHoles();

   addSlot(0x0000, 0x1FFF, 0x5000, 0, ROM);
   addSlot(0x2000, 0x2FFF, 0xD000, 0, ROM);
   addSlot(0x3000, 0x3FFF, 0xF000, 0, ROM);
   addSlot(0x8000, 0x8FFF, 0, 0, RAM8);
   addSlot(0x9000, 0x9FFF, 0, 0, RAM8);

   uint16_t addr[] = {0x5890, 0xD852, 0xFFFF, 0x8050};
   int ret;
   uint8_t page = 0;
   uint32_t romaddr = 0;

   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X\n", i, addr[i]);
      ret = mapAddress(addr[i], page, &romaddr);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }

   printf("--- end test_cfg0 ---\n");
}

void test_cfg_custom0(void) {
   
   /* 
    * $0000 - $0FFF = $E000 PAGE 0
    * $1000 - $1FFF = $F000 PAGE 0
    * $2000 - $2FFF = $E000 PAGE 2
    * $3000 - $3FFF = $F000 PAGE 2
    * $4000 - $4FFF = $E000 PAGE 3
    * $5000 - $5FFF = $F000 PAGE 3
    * $6000 - $6FFF = $E000 PAGE 4
    * $7000 - $7FFF = $F000 PAGE 4
    * $8000 - $8FFF = $E000 PAGE 5
    * $9000 - $9FFF = $F000 PAGE 5
    * $A000 - $AFFF = $E000 PAGE 6
    * $B000 - $BFFF = $F000 PAGE 6
    * $C000 - $C00D = $4800 
    * $C00E - $D00D = $5000 
    * $D00E - $DD8A = $6000 
    * $DD8B - $ED8A = $A000 
    * $ED8B - $FB04 = $B000 
    * $FB05 - $10AC4 = $C040 
    * $10AC5 - $11847 = $D000
    */

   printf("--- start test_cfg_custom0 ---\n");

   cleanSlots();
   cleanHoles();
   
   addSlot(0x0000, 0x0FFF, 0xE000, 0, ROM);
   addSlot(0x1000, 0x1FFF, 0xF000, 0, ROM);
   addSlot(0x2000, 0x2FFF, 0xE000, 2, ROM);
   addSlot(0x3000, 0x3FFF, 0xF000, 2, ROM);
   addSlot(0x4000, 0x4FFF, 0xE000, 3, ROM);
   addSlot(0x5000, 0x5FFF, 0xF000, 3, ROM);
   addSlot(0x6000, 0x6FFF, 0xE000, 4, ROM);
   addSlot(0x7000, 0x7FFF, 0xF000, 4, ROM);
   addSlot(0x8000, 0x8FFF, 0xE000, 5, ROM);
   addSlot(0x9000, 0x9FFF, 0xF000, 5, ROM);
   addSlot(0xA000, 0xAFFF, 0xE000, 6, ROM);
   addSlot(0xB000, 0xBFFF, 0xF000, 6, ROM);
   addSlot(0xC000, 0xC00D, 0x4800, 0, ROM);
   addSlot(0xC00E, 0xD00D, 0x5000, 0, ROM);
   addSlot(0xD00E, 0xDD8A, 0x6000, 0, ROM);
   addSlot(0xDD8B, 0xED8A, 0xA000, 0, ROM);
   addSlot(0xED8B, 0xFB04, 0xB000, 0, ROM);
   addSlot(0xFB05, 0x10AC4, 0xC040, 0, ROM);
   addSlot(0x10AC5, 0x11847, 0xD000, 0, ROM);

   uint16_t addr[] = {0xE055, 0xF055};
   uint8_t page;
   int ret;
   uint32_t romaddr;

   page = 0;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }
   page = 1;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }
   page = 2;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }
   page = 3;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }

   printf("--- end test_cfg_custom0 ---\n");
}

void test_cfg_collision(void) {

   /*
    * $8000 - $800D = $4800
    * $800E - $844D = $480E
    */

   printf("--- start test_cfg_collision---\n");

   cleanSlots();
   cleanHoles();
   
   // without hole
   //addSlot(0x8000, 0x800D, 0x4800, 0);
   //addSlot(0x800E, 0x844D, 0x480E, 0);
   
   // with hole
   addSlot(0x8000, 0x800D, 0x4800, 0, ROM);
   addSlot(0x800E, 0x844D, 0x4810, 0, ROM);

   uint16_t addr[] = {0x4801, 0x4810, 0x4811, 0x4812, 0x4C4B};
   uint8_t page;
   int ret;
   uint32_t romaddr;
   
   page = 0;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }

   printf("--- end test_cfg_collision---\n");
}

int main(void) {

   printf("sizeof slots struct: %ld bytes\n\n", sizeof(slots));
   printf("sizeof holes struct: %ld bytes\n\n", sizeof(holes));

   //test_cfg0();

   test_cfg_custom0();

   //test_cfg_collision();
   //printFilledSlots();

   return 0;
}
