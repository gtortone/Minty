#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "memory.h"

void test_cfg0(void) {

   /*
    * $0000 - $1FFF = $5000   ;  8K to $5000 - $6FFF
    * $2000 - $2FFF = $D000   ;  4K to $D000 - $DFFF 
    * $3000 - $3FFF = $F000   ;  4K to $F000 - $FFFF
    */

   printf("--- start test_cfg0 ---\n");

   cleanSlots();
   cleanHoles();

   addSlot(0x0000, 0x1FFF, 0x5000, 0, ROM_SLOT);
   addSlot(0x2000, 0x2FFF, 0xD000, 0, ROM_SLOT);
   addSlot(0x3000, 0x3FFF, 0xF000, 0, ROM_SLOT);
   addSlot(0x8000, 0x8FFF, 0, 0, RAM8_SLOT);
   addSlot(0x9000, 0x9FFF, 0, 0, RAM8_SLOT);

   uint16_t addr[] = {0x5890, 0xD852, 0xFFFF, 0x8050};
   int ret;
   uint8_t page = 0;
   uint32_t romaddr = 0;
   mapType slot_type;

   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X\n", i, addr[i]);
      ret = mapAddress(addr[i], page, &romaddr, &slot_type);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }

   printf("--- end test_cfg0 ---\n");
}

void test_cfg_custom0(void) {
   
   /* 
    * $0000 - $0FFF = $A000 PAGE 0
    * $1000 - $1FFF = $C000 PAGE 0
    * $2000 - $2FFF = $E000 PAGE 2
    * $3000 - $3FFF = $7000 PAGE 1
    * $4000 - $4FFF = $A000 PAGE 1
    * $5000 - $5FFF = $C000 PAGE 1
    * $6000 - $6FFF = $7000 PAGE 2
    * $7000 - $7FFF = $A000 PAGE 2
    * $8000 - $8FFF = $C000 PAGE 2
    * $9000 - $9FFF = $E000 PAGE 2
    * $A000 - $AFFF = $7000 PAGE 3
    * $B000 - $BFFF = $A000 PAGE 3
    * $C000 - $CFFF = $C000 PAGE 3
    * $D000 - $DFFF = $E000 PAGE 3
    * $E000 - $EFFF = $7000 PAGE 4
    * $F000 - $FFFF = $A000 PAGE 4
    * $10000 - $10FFF = $C000 PAGE 4
    * $11000 - $11FFF = $E000 PAGE 4
    * $12000 - $12FFF = $7000 PAGE 5
    * $13000 - $13FFF = $A000 PAGE 5
    * $14000 - $14FFF = $C000 PAGE 5
    * $15000 - $15FFF = $E000 PAGE 5
    * $16000 - $16FFF = $7000 PAGE 6
    * $17000 - $17FFF = $A000 PAGE 6
    * $18000 - $18FFF = $C000 PAGE 6
    * $19000 - $19FFF = $7000 PAGE 7
    * $1A000 - $1AFFF = $A000 PAGE 7
    * $1B000 - $1BFFF = $C000 PAGE 7
    * $1C000 - $1CFFF = $7000 PAGE 8
    * $1D000 - $1D00D = $4800
    * $1D00E - $1E00D = $5000
    * $1E00E - $1E5B1 = $6000
    */

   printf("--- start test_cfg_custom0 ---\n");

   cleanSlots();
   cleanHoles();
   
   addSlot(0x0000, 0x0FFF, 0xA000, 0, ROM_SLOT);
   addSlot(0x1000, 0x1FFF, 0xC000, 0, ROM_SLOT);
   addSlot(0x2000, 0x2FFF, 0xE000, 0, ROM_SLOT);
   addSlot(0x3000, 0x3FFF, 0x7000, 1, ROM_SLOT);
   addSlot(0x4000, 0x4FFF, 0xA000, 1, ROM_SLOT);
   addSlot(0x5000, 0x5FFF, 0xC000, 1, ROM_SLOT);
   addSlot(0x6000, 0x6FFF, 0x7000, 2, ROM_SLOT);
   addSlot(0x7000, 0x7FFF, 0xA000, 2, ROM_SLOT);
   addSlot(0x8000, 0x8FFF, 0xC000, 2, ROM_SLOT);
   addSlot(0x9000, 0x9FFF, 0xE000, 2, ROM_SLOT);
   addSlot(0xA000, 0xAFFF, 0x7000, 3, ROM_SLOT);
   addSlot(0xB000, 0xBFFF, 0xA000, 3, ROM_SLOT);
   addSlot(0xC000, 0xCFFF, 0xC000, 3, ROM_SLOT);
   addSlot(0xD000, 0xDFFF, 0xE000, 3, ROM_SLOT);
   addSlot(0xE000, 0xEFFF, 0x7000, 4, ROM_SLOT);
   addSlot(0xF000, 0xFFFF, 0xA000, 4, ROM_SLOT);
   addSlot(0x10000, 0x10FFF, 0xC000, 4, ROM_SLOT);
   addSlot(0x11000, 0x11FFF, 0xE000, 4, ROM_SLOT);
   addSlot(0x12000, 0x12FFF, 0x7000, 5, ROM_SLOT);
   addSlot(0x13000, 0x13FFF, 0xA000, 5, ROM_SLOT);
   addSlot(0x14000, 0x14FFF, 0xC000, 5, ROM_SLOT);
   addSlot(0x15000, 0x15FFF, 0xE000, 5, ROM_SLOT);
   addSlot(0x16000, 0x16FFF, 0x7000, 6, ROM_SLOT);
   addSlot(0x17000, 0x17FFF, 0xA000, 6, ROM_SLOT);
   addSlot(0x18000, 0x18FFF, 0xC000, 6, ROM_SLOT);
   addSlot(0x19000, 0x19FFF, 0x7000, 7, ROM_SLOT);
   addSlot(0x1A000, 0x1AFFF, 0xA000, 7, ROM_SLOT);
   addSlot(0x1B000, 0x1BFFF, 0xC000, 7, ROM_SLOT);
   addSlot(0x1C000, 0x1CFFF, 0x7000, 8, ROM_SLOT);
   addSlot(0x1D000, 0x1D00D, 0x4800, 0, ROM_SLOT);
   addSlot(0x1D00E, 0x1E00D, 0x5000, 0, ROM_SLOT);
   addSlot(0x1E00E, 0x1E5B1, 0x6000, 0, ROM_SLOT);

   uint16_t addr[] = {0xE055, 0xF055};
   uint8_t page;
   int ret;
   uint32_t romaddr;
   mapType slot_type;

   page = 0;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr, &slot_type);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }
   page = 1;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr, &slot_type);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }
   page = 2;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr, &slot_type);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }
   page = 3;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr, &slot_type);
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
   addSlot(0x8000, 0x800D, 0x4800, 0, ROM_SLOT);
   addSlot(0x800E, 0x844D, 0x4810, 0, ROM_SLOT);

   uint16_t addr[] = {0x4801, 0x4810, 0x4811, 0x4812, 0x4C4B};
   uint8_t page;
   int ret;
   uint32_t romaddr;
   mapType slot_type;
   
   page = 0;
   for(int i=0; i<sizeof(addr)/sizeof(uint16_t); i++) {
      printf("%d) requested address: 0x%X, page: %d\n", i, addr[i], page);
      ret = mapAddress(addr[i], page, &romaddr, &slot_type);
      if (ret)
         printf("** ROM address: 0x%X\n", romaddr);
      else printf("E: address not found\n");
      printf("\n");
   }

   printf("--- end test_cfg_collision---\n");
}

int main(void) {

   //printf("sizeof slots struct: %ld bytes\n\n", sizeof(slots));
   //printf("sizeof holes struct: %ld bytes\n\n", sizeof(holes));

   //test_cfg0();

   test_cfg_custom0();
   printFilledSlots();

   //test_cfg_collision();
   //printFilledSlots();

   return 0;
}
