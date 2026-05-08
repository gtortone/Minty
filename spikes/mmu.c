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

struct mapEntry {
   uint32_t from[NPAGES];
   uint32_t to[NPAGES];
   uint16_t target;
   bool filled;
};

/*
 * each element of slots array is a bucket for MSB of Intellivision
 * bus address
 *
 * e.g.  $5000    ->    bucket 0x50 (80)
 */

struct mapEntry slots[NSLOTS];

void printSlot(uint8_t idx, uint8_t page) {

   printf("slot #0x%X: from: 0x%X, to: 0x%X, target: 0x%X, page: 0x%X, [%s]\n", 
         idx, slots[idx].from[page], slots[idx].to[page], slots[idx].target, 
         page, slots[idx].filled?"FILLED":"EMPTY");
}

void printFilledSlots(void) {

   for(int i=0; i<NSLOTS; i++) {
      if (slots[i].filled) {
         for(int page=0; page<NPAGES; page++) {
            if (slots[i].from[page]) {
               printf("%X) $%X - $%X = $%X PAGE %d\n", 
                     i, slots[i].from[page], slots[i].to[page], slots[i].target, page);
            }
         }
      }
   }
}

void cleanSlots(void) {

   for(int i=0; i<NSLOTS; i++) {
      memset(slots[i].from, 0, NPAGES);
      memset(slots[i].to, 0, NPAGES);
      slots[i].target = 0;
      slots[i].filled = false;
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

void addSlot(uint32_t from, uint32_t to, uint16_t target, uint8_t page) {
   
   uint8_t nslots = (( (target + (to - from)) ) >> 8) - (target >> 8);
   uint8_t baseslot = (target >> 8);

   if (slots[baseslot].filled) {
      // handle collision
      if (from < slots[baseslot].from[page])
         slots[baseslot].from[page] = from;
      else
         from = slots[baseslot].from[page];

      if (to > slots[baseslot].to[page])
         slots[baseslot].to[page] = to;
      else
         to = slots[baseslot].to[page];

      if (target < slots[baseslot].target)
         slots[baseslot].target = target;
      else
         target = slots[baseslot].target;
   }

   for(int i=baseslot; i<=(baseslot + nslots); i++) {

      slots[i].from[page] = from;
      slots[i].to[page] = to;
      slots[i].target = target;

      slots[i].filled = true;

      //printf("## FILL ## ");
      //printSlot(i, page);
   }
}

// return slot index if address is mapped and value compatible with slot size

int mapAddress(uint16_t addr, uint8_t page) {

   uint8_t idx = (addr >> 8);

   if ( (slots[idx].filled) && ((addr - slots[idx].target) <= (slots[idx].to[page] - slots[idx].from[page])) ) {
      return idx;
   } else {
      return -1;
   }
}

void test_cfg0(void) {

   /*
    * $0000 - $1FFF = $5000   ;  8K to $5000 - $6FFF
    * $2000 - $2FFF = $D000   ;  4K to $D000 - $DFFF 
    * $3000 - $3FFF = $F000   ;  4K to $F000 - $FFFF
    */

   cleanSlots();

   addSlot(0x0000, 0x1FFF, 0x5000, 0);
   addSlot(0x2000, 0x2FFF, 0xD000, 0);
   addSlot(0x3000, 0x3FFF, 0xF000, 0);

   uint16_t addr[] = {0x5890, 0xD010, 0xD200, 0xD852, 0xFFFF};
   int ret;
   uint8_t page = 0;

   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X\n", i, addr[i]);
      ret = mapAddress(addr[i], page);
      if (ret >= 0)
         printSlot(ret, page);
      else printf("E: slot not found\n");
      printf("\n");
   }
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

   cleanSlots();
   
   addSlot(0x0000, 0x0FFF, 0xE000, 0);
   addSlot(0x1000, 0x1FFF, 0xF000, 0);
   addSlot(0x2000, 0x2FFF, 0xE000, 2);
   addSlot(0x3000, 0x3FFF, 0xF000, 2);
   addSlot(0x4000, 0x4FFF, 0xE000, 3);
   addSlot(0x5000, 0x5FFF, 0xF000, 3);
   addSlot(0x6000, 0x6FFF, 0xE000, 4);
   addSlot(0x7000, 0x7FFF, 0xF000, 4);
   addSlot(0x8000, 0x8FFF, 0xE000, 5);
   addSlot(0x9000, 0x9FFF, 0xF000, 5);
   addSlot(0xA000, 0xAFFF, 0xE000, 6);
   addSlot(0xB000, 0xBFFF, 0xF000, 6);
   addSlot(0xC000, 0xC00D, 0x4800, 0);
   addSlot(0xC00E, 0xD00D, 0x5000, 0);
   addSlot(0xD00E, 0xDD8A, 0x6000, 0);
   addSlot(0xDD8B, 0xED8A, 0xA000, 0);
   addSlot(0xED8B, 0xFB04, 0xB000, 0);
   addSlot(0xFB05, 0x10AC4, 0xC040, 0);
   addSlot(0x10AC5, 0x11847, 0xD000, 0);

   uint16_t addr[] = {0xE055};
   uint8_t page;
   int ret;

   page = 0;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page);
      if (ret >= 0)
         printSlot(ret, page);
      else printf("E: slot not found\n");
      printf("\n");
   }
   page = 1;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page);
      if (ret >= 0)
         printSlot(ret, page);
      else printf("E: slot not found\n");
      printf("\n");
   }
   page = 2;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page);
      if (ret >= 0)
         printSlot(ret, page);
      else printf("E: slot not found\n");
      printf("\n");
   }
   page = 3;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page);
      if (ret >= 0)
         printSlot(ret, page);
      else printf("E: slot not found\n");
      printf("\n");
   }
}

void test_cfg_collision(void) {

   /*
    * $8000 - $800D = $4800
    * $800E - $844D = $4810
    */

   cleanSlots();
   
   addSlot(0x8000, 0x800D, 0x4800, 0);
   addSlot(0x800E, 0x844D, 0x4810, 0);

   uint16_t addr[] = {0x1234, 0x2345};
   uint8_t page;
   int ret;
   
   page = 0;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page);
      if (ret >= 0)
         printSlot(ret, page);
      else printf("E: slot not found\n");
      printf("\n");
   }
}

int main(void) {

   printf("sizeof slots struct: %ld bytes\n\n", sizeof(slots));

   test_cfg0();

   test_cfg_custom0();

   test_cfg_collision();
   printFilledSlots();

   return 0;
}
