
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

struct SlotEntry slots[NSLOTS];

void printSlot(uint8_t idx, uint8_t page) {

   printf("slot #0x%02X, page: %d, from: 0x%02X, to: 0x%02X, Mapped to: 0x%08lX - from: 0x%02X, to: 0x%02X, Mapped to: 0x%08lX\n",
         idx,
         page,
         slots[idx].from[0][page], 
         slots[idx].to[0][page],
         (uint32_t)((slots[idx].RomAddr_H[0][page] << 16) + slots[idx].RomAddr_L[0][page]),
         slots[idx].from[1][page], 
         slots[idx].to[1][page],
         (uint32_t)((slots[idx].RomAddr_H[1][page] << 16) + slots[idx].RomAddr_L[1][page])
         );
}

void printFilledSlots(void) {

   for(int i=0; i<NSLOTS; i++)
      for(int page=0; page<NPAGES; page++) 
         if(slots[i].to[0][page]) 
            printSlot(i, page);
}

void cleanSlots(void) {
   printf("Clean slots\n");
   for(int i=0; i<NSLOTS; i++) {
      for (int k=0; k<NSECTIONS; k++) {
         for(int j=0;j<NPAGES;j++) {
            slots[i].from[k][j] = 0xFF;
            slots[i].to[k][j] = 0x00;
            slots[i].RomAddr_H[k][j] = UNUSED_SLOT;
            slots[i].RomAddr_L[k][j] = 0;
         }
      }
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

void addSlot(uint32_t from, uint32_t to, uint16_t target, uint8_t page, uint8_t type) {
   
   printf("addSlot 0x%08lX - 0x%08lX : 0x%04X page %d type %d\n", from,to,target,page,type);

   if ((type == RAM8_SLOT) || (type == RAM16_SLOT)) {
      // RAM slots, no need to consider holes

      uint8_t first_slot = from >> 8;
      uint8_t last_slot  = to >> 8;
      
      // RAM slots aren't paged, and have no hole, only first section is populated
      for (int idx=first_slot; idx<last_slot; idx++) {
         slots[idx].from[0][0]      = 0x00;
         slots[idx].to[0][0]        = 0xFF;
         slots[idx].RomAddr_H[0][0] = type;
      }
      // last slot can be inclomplete
      slots[last_slot].from[0][0]      = 0x00;
      slots[last_slot].to[0][0]        = to - (last_slot << 8);
      slots[last_slot].RomAddr_H[0][0] = type;

      //printf("Allocated slots 0x%X to 0x%X as RAM (0x%02X)\n", first_slot, last_slot, type);
   }
   else {
      // ROM slots
      uint8_t first_slot = target >> 8;
      uint8_t last_slot  = (target + (to - from)) >> 8;
      uint8_t slot_section = 0;
      uint32_t slot_RomAddr = from;

      // check if first section already used, if yes use second one (will create a hole)
      slot_section = (slots[first_slot].RomAddr_H[0][page] == UNUSED_SLOT)?0:1;
      slots[first_slot].from[slot_section][page] = target - (first_slot << 8);
      slots[first_slot].to[slot_section][page] = 0xFF; // if first slot is also last slot this will be overwritten later
      slots[first_slot].RomAddr_H[slot_section][page] = (slot_RomAddr >> 16) & 0x0000000F; // 4 high bits of 20 bits address in ROM file
      slots[first_slot].RomAddr_L[slot_section][page] = (slot_RomAddr & 0x0000FFFF); // 16 low bits of 20 bits address in ROM file
      slot_RomAddr += (slots[first_slot].to[slot_section][page] - slots[first_slot].from[slot_section][page] + 1);
      
      // next slots are complete, so can't have holes!
      for (int idx=first_slot+1; idx<last_slot; idx++) {
         slots[idx].from[0][page]      = 0x00;
         slots[idx].to[0][page]        = 0xFF;
         slots[idx].RomAddr_H[0][page] = (slot_RomAddr >> 16) & 0x0000000F; // 4 high bits of 20 bits address in ROM file
         slots[idx].RomAddr_L[0][page] = (slot_RomAddr & 0x0000FFFF); // 16 low bits of 20 bits address in ROM file
         slot_RomAddr += (0xFF + 1);
      }

      // last slot might be truncated again
      if (last_slot > first_slot) {
         slot_section = (slots[last_slot].RomAddr_H[0][page] == UNUSED_SLOT)?0:1;
         slots[last_slot].from[slot_section][page] = 0x00;
         slots[last_slot].RomAddr_H[slot_section][page] = (slot_RomAddr >> 16) & 0x0000000F; // 8 high bits of 20 bits address in ROM file
         slots[last_slot].RomAddr_L[slot_section][page] = (slot_RomAddr & 0x0000FFFF); // 16 low bits of 20 bits address in ROM file
      }
      slots[last_slot].to[slot_section][page] = (target + (to - from)) & 0xFF;

      //printf("Allocated slots 0x%02X to 0x%02X from page %d as ROM (0x%02X)\n", first_slot, last_slot, page, slots[last_slot].RomAddr_H[slot_section][page]);
   }
}


// return ROM/cartridge address for Intellivision address
bool mapAddress(uint16_t addr, uint8_t page, uint32_t *romaddr) {

   uint8_t idx = (addr >> 8);

   if ((slots[idx].RomAddr_H[0][0] == RAM8_SLOT) || (slots[idx].RomAddr_H[0][0] == RAM16_SLOT)) {
      // This is RAM return first RAM address directly
      *romaddr = slots[idx].RomAddr_L[0][0];
      return false;
   }
   else {
      // check for section
      uint8_t short_Address = addr & 0xFF;

      if ((short_Address >= slots[idx].from[0][page]) && (short_Address <= slots[idx].to[0][page]))
         *romaddr = (uint32_t)((slots[idx].RomAddr_H[0][page] << 16) + slots[idx].RomAddr_L[0][page]) + (uint32_t)(short_Address - slots[idx].from[0][page]);
      else if ((short_Address >= slots[idx].from[1][page]) && (short_Address <= slots[idx].to[1][page]))
         *romaddr = (uint32_t)((slots[idx].RomAddr_H[1][page] << 16) + slots[idx].RomAddr_L[1][page]) + (uint32_t)(short_Address - slots[idx].from[1][page]);
      else
         return false;
   }

   // printf("mapAddress : 0x%04X => 0x%08lX\n",addr,*romaddr);

   return true;
}


void getRAMRange(uint16_t *ramfrom, uint16_t *ramto, uint8_t *ramwidth) {

   uint16_t from = 0;
   uint16_t to = 0;

   for(int slot=0;slot<NSLOTS;slot++) {

      if ( (slots[slot].RomAddr_H[0][0] == RAM8_SLOT) || (slots[slot].RomAddr_H[0][0] == RAM16_SLOT) ) {

         uint16_t to_in = (slot << 8) + slots[slot].to[0][0];
      
         if ( (from == 0) && (to == 0) ) {
            from = (slot << 8) + slots[slot].from[0][0];
            to = to_in;
            if(slots[slot].RomAddr_H[0][0] == RAM8_SLOT)
               *ramwidth = 8;
            else
               *ramwidth = 16;
            continue;
         }

         if (to_in > to)
            to = to_in;
      }
   }

   *ramfrom = from;
   *ramto = to;

   printf("%d bits RAM ranges from 0x%04X to 0x%04X\n",*ramwidth,*ramfrom,*ramto);
}

// ---

void config_memory(int cfg) {

   // printf("Config Memory %d\n",cfg);

   cleanSlots();

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

/*
void main() {
   uint32_t romaddr;
   uint16_t ramfrom;
   uint16_t ramto;
   uint8_t ramwidth;

   printf("test mem slots\n");

   cleanSlots();
   addSlot(0x0000,0x007F, 0x4800, 0, ROM_SLOT);
   addSlot(0x0080,0x00E0, 0x488F, 0, ROM_SLOT);
   addSlot(0x8000,0x9FFF, 0, 0, RAM8_SLOT);

   printFilledSlots();

   getRAMRange(&ramfrom, &ramto, &ramwidth);

   return;
}
*/
