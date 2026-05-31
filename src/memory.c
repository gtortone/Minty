
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "memory.h"

/*
 * each element of slots array is a bucket for MSB of Intellivision
 * bus address
 *
 * e.g.  $5000    ->    bucket 0x50 (80)
 */

struct mapEntry slots[NSLOTS];
struct mapHole holes[NSLOTS];
struct memHack hacks[MAX_HACKS_NUM];

static uint8_t numHacks = 0;

void printSlot(uint8_t idx, uint8_t page) {

   // check for hole...
   //
   printf("slot #0x%X: type: %d, from: 0x%lX, to: 0x%lX, size: 0x%X, target: 0x%X, page: 0x%X, [%s]\n",
         idx, 
         slots[idx].type, 
         slots[idx].from[page], 
         slots[idx].from[page]+slots[idx].size[page],
         slots[idx].size[page], 
         slots[idx].target, 
         page, 
         (slots[idx].usedmask) & (1<<page)?"F":"E");
}

void printFilledSlots(void) {

   for(int i=0; i<NSLOTS; i++) 
      if (slots[i].usedmask) 
         for(int page=0; page<NPAGES; page++) 
            if(slots[i].usedmask & (1<<page)) 
               printSlot(i, page);
}

void cleanSlots(void) {

   for(int i=0; i<NSLOTS; i++) {
      for(int j=0;j<NPAGES;j++) {
         slots[i].from[j] = 0;
         slots[i].size[j] = 0;
      }
      slots[i].target = 0;
      slots[i].type = 0;
      slots[i].usedmask = 0;
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

   if ( (type == ROM_SLOT) || (type == ROM_PAGE_SLOT) )
      baseslot = (target >> 8);
   else
      baseslot = (from >> 8);

   // detect collision  (only for ROM type for not paged addresses)
   if ( (type == ROM_SLOT) && ((slots[baseslot].usedmask) & (1<<page)) && (page == 0)) {

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
     
      if (from_in < from)
         from = from_in;

      if (to_in > to)
         to = to_in;

      if (slots[baseslot].target < target)
         target = slots[baseslot].target;
   }

   if(hole) {
      printf("--- hole 0x%X --- from: 0x%lX, size: %d\n",
         baseslot, holes[baseslot].from, holes[baseslot].size);
   }

   if ( (type == ROM_SLOT) || (type == ROM_PAGE_SLOT) )
      nslots = (( (target + (to - from)) ) >> 8) - (target >> 8);
   else
      nslots = (to >> 8) - (from >> 8);

   for(int i=baseslot; i<=(baseslot + nslots); i++) {

      slots[i].from[page] = from;
      slots[i].size[page] = (to - from) + 1;

      if ( (type == ROM_SLOT) || (type == ROM_PAGE_SLOT) )
         slots[i].target = target;
      else
         slots[i].target = 0;
      slots[i].type = type;
      slots[i].usedmask |= (1 << page);

      if (hole) {
         // extend hole on near segments
         holes[i].from = holes[baseslot].from;
         holes[i].size = holes[baseslot].size;
         holes[i].filled = true;
      }

      //printf("## FILL ## ");
      //printSlot(i, page);
   }
}

// return slot index if address is mapped and value compatible with slot size
bool mapSlot(uint16_t addr, uint8_t page, uint8_t *slot) {

   uint8_t idx = (addr >> 8);

   if ( (slots[idx].type == ROM_SLOT) || (slots[idx].type == ROM_PAGE_SLOT) ) {

      if ( (slots[idx].usedmask & (1<<page)) && ((addr - slots[idx].target) <= (slots[idx].size[page] + holes[idx].size)) ) {
         *slot = idx;
         return true;
      }

   } else {    // RAM8 or RAM16
   
      if ( (slots[idx].usedmask & (1<<page)) && ((addr - slots[idx].from[0]) <= (slots[idx].size[0] - 1)) ) {
         *slot = idx;
         return true;
      }
   }

   return false;
}

// return ROM/cartridge address for Intellivision address
bool mapAddress(uint16_t addr, uint8_t page, uint32_t *romaddr, mapType *type) {

   uint8_t slot;

   if ( mapSlot(addr, page, &slot) ) {

      *type = slots[slot].type;

      if ( (*type == ROM_SLOT) || (*type == ROM_PAGE_SLOT) ) {

         if ( (addr - slots[slot].target) <= (slots[slot].size[page] + holes[slot].size) ) {    

            *romaddr = slots[slot].from[page] + (addr - slots[slot].target);
         
            if (holes[slot].filled) {
               // check if address is inside hole...
               if ( (addr >= holes[slot].from) && (addr <= (holes[slot].from + holes[slot].size)) )
                     return false;

               if ( addr > holes[slot].from )
                  *romaddr -= (holes[slot].size + 1);
            }

            return true;
         }

      } else {    // RAM8_SLOT or ROM16_SLOT

         *romaddr = (addr - slots[slot].from[0]);

         return true;
      }
   }

   return false;
}

void getRAMRange(uint16_t *ramfrom, uint16_t *ramto, uint8_t *ramwidth) {

   uint16_t from = 0;
   uint16_t to = 0;

   for(int slot=0;slot<NSLOTS;slot++) {

      if ( (slots[slot].type == RAM8_SLOT) || (slots[slot].type == RAM16_SLOT) ) {

         uint16_t to_in = slots[slot].from[0] + slots[slot].size[0];
      
         if ( (from == 0) && (to == 0) ) {
            from = slots[slot].from[0];
            to = to_in;
            if(slots[slot].type == RAM8_SLOT)
               *ramwidth = 8;
            else
               *ramwidth = 16;
            continue;
         }

         if (slots[slot].from[0] < from)
            from = slots[slot].from[0];

         if (to_in > to)
            to = to_in;
      }
   }

   *ramfrom = from;
   *ramto = to;
}

// ---

void config_memory(int cfg) {

   cleanSlots();
   cleanHoles();

   switch (cfg) {

      case 0:
         addSlot(0x0000, 0x1FFF, 0x5000, 0, ROM_SLOT);
         addSlot(0x2000, 0x2FFF, 0xD000, 0, ROM_SLOT);
         addSlot(0x3000, 0x3FFF, 0xF000, 0, ROM_SLOT);
         break;

      case 1:
         addSlot(0x0000, 0x1FFF, 0x5000, 0, ROM_SLOT);
         addSlot(0x2000, 0x4FFF, 0xD000, 0, ROM_SLOT);
         break;

      case 2:
         addSlot(0x0000, 0x1FFF, 0x5000, 0, ROM_SLOT);
         addSlot(0x2000, 0x4FFF, 0x9000, 0, ROM_SLOT);
         addSlot(0x5000, 0x5FFF, 0xD000, 0, ROM_SLOT);
         break;

      case 3:
         addSlot(0x0000, 0x1FFF, 0x5000, 0, ROM_SLOT);
         addSlot(0x2000, 0x3FFF, 0x9000, 0, ROM_SLOT);
         addSlot(0x4000, 0x4FFF, 0xD000, 0, ROM_SLOT);
         addSlot(0x5000, 0x5FFF, 0xF000, 0, ROM_SLOT);
         break;

      case 4:
         addSlot(0x0000, 0x1FFF, 0x5000, 0, ROM_SLOT);
         addSlot(0xD000, 0xD3FF, 0, 0, RAM8_SLOT);
         break;

      case 5:
         addSlot(0x0000, 0x2FFF, 0x5000, 0, ROM_SLOT);
         addSlot(0x3000, 0x5FFF, 0x9000, 0, ROM_SLOT);
         break;

      case 6:
         addSlot(0x0000, 0x1FFF, 0x6000, 0, ROM_SLOT);
         break;

      case 7:
         addSlot(0x0000, 0x1FFF, 0x4800, 0, ROM_SLOT);
         break;

      case 8:
         addSlot(0x0000, 0x0FFF, 0x5000, 0, ROM_SLOT);
         addSlot(0x1000, 0x1FFF, 0x7000, 0, ROM_SLOT);
         break;

      case 9:
         addSlot(0x0000, 0x1FFF, 0x5000, 0, ROM_SLOT);
         addSlot(0x2000, 0x3FFF, 0x9000, 0, ROM_SLOT);
         addSlot(0x4000, 0x4FFF, 0xD000, 0, ROM_SLOT);
         addSlot(0x5000, 0x5FFF, 0xF000, 0, ROM_SLOT);
         addSlot(0x8800, 0x8FFF, 0, 0, RAM8_SLOT);
         break;

      default:
         break;
   }
}

void cleanHacks(void) {
   for (int i=0; i<MAX_HACKS_NUM; i++) {
      hacks[i].address = 0;
      hacks[i].value = 0;
   }
   numHacks = 0;
}

void addHack(uint16_t addr, uint8_t value) {
   hacks[numHacks].address = addr;
   hacks[numHacks].value = value;
   numHacks++;
}

uint8_t getHacksNum(void) {
   return numHacks;
}
