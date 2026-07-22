#ifndef MEMORY_H_
#define MEMORY_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NSLOTS    256
#define NPAGES     16
#define NSECTIONS   2

#define ROM_SLOT     0x00
#define RAM8_SLOT    0x10
#define RAM16_SLOT   0x20
#define UNUSED_SLOT  0xF0

/*
   To get address in ROM area :
   INTV address is 0xABCD
   => look at slot[0xAB]
   => Gather current active page for section 0xA and store it in page 
   => check if from[0][page] <= 0xCD <= to[0][page], if yes return 
      ((RomAddr_H[0][page] << 16 + RomAddr_L[0][page]) - (0xCD - from[0][page])) 
   => check if from[1][page] <= 0xCD <= to[1][page], if yes return 
      ((RomAddr_H[1][page] << 16 + RomAddr_L[1][page]) - (0xCD - from[1][page]))
   => else return error (out of allocated ROM, should return 0xFFFF)

   RomAddr_H is also used to encode RAM type
   0xE0 means RAM8
   0xF0 means RAM16
   any other value (4 high bits shall never be used) is for ROM 
*/


struct SlotEntry {
   uint8_t from[NSECTIONS][NPAGES];
   uint8_t to[NSECTIONS][NPAGES];
   uint8_t RomAddr_H[NSECTIONS][NPAGES];
   uint16_t RomAddr_L[NSECTIONS][NPAGES];
};

void printSlot(uint8_t idx, uint8_t page);
void printFilledSlots(void);
void cleanSlots(void);
void addSlot(uint32_t from, uint32_t to, uint16_t target, uint8_t page, uint8_t type);
bool mapAddress(uint16_t addr, uint8_t page, uint32_t *romaddr);
void getRAMRange(uint16_t *ramfrom, uint16_t *ramto, uint8_t *ramwidth);
void config_memory(int cfg);

#endif
