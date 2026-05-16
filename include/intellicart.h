
#ifndef INTELLICART_H
#define INTELLICART_H

#include <stdint.h>
#include <stdbool.h>

#include "ff.h"

#if PICO_RP2040
   #define BINLENGTH 1024*55     // 110 kb
   #define RAMSIZE   0x2000
#elif PICO_RP2350
   #define BINLENGTH 1024*205    // ~420 kb
   #define RAMSIZE   0x4000
#endif

typedef struct {
   uint16_t ROM[BINLENGTH];
   volatile uint16_t RAM[RAMSIZE];
   uint32_t len;

   uint16_t ramfrom;
   uint16_t ramto;
   uint8_t ramwidth;

   bool pagingSupport;

   bool JLPSupport;
   bool JLPFlash;
   uint8_t JLPFlashSize;   // number of 1.5KB JLP flash sectors
   bool JLPAccel;
   char flashfile[512];
   FIL filesave;
} Cartridge;

#define JLP_FEATURE_ACCEL(status)   (value & (1U << 0))
#define JLP_FEATURE_FLASH(status)   (value & (1U << 1))

#define JLP_FLASH_ROWS_PER_SECTOR    8
#define JLP_FLASH_ROW_BYTES         96 * 2     // 96 * uint16_t
#define JLP_FLASH_SECTOR_BYTES      JLP_FLASH_ROWS_PER_SECTOR * JLP_FLASH_ROW_BYTES
#define JLP_RAM_ADDRESS             cart.RAM[0x25]
#define JLP_ROW_NUMBER              cart.RAM[0x26]

void init_cart(void);
void load_cfg(char *filename);

#endif
