
#ifndef INTELLICART_H
#define INTELLICART_H

#include <stdint.h>
#include <stdbool.h>

#include "vfs.h"
#include "filesystem.h"

#define RAMSIZE   0x2800 // biggest contigus memory block useable for RAM is from 0x8000 to 0x9BFF (10kW = 20kB) based on memory map

typedef struct {
   volatile uint16_t ROM[MAX_ROM_SIZE];
   volatile uint16_t RAM[RAMSIZE];
   uint32_t len;

   bool pagingSupport;
#if CONFIG_JLP
   bool JLPSupport;
   bool JLPFlash;
   uint8_t JLPFlashSize;   // number of 1.5KB JLP flash sectors
   bool JLPAccel;
   char flashfile[512];
   vfs_file_t *filesave;
#endif

#if CONFIG_ECS_AUDIO
   bool ECSSupport;
#endif
} Cartridge;

#define JLP_FEATURE_ACCEL(status)   (status > 0) // Turn on Acceleration for any jlp value <> 0 should be (status & (1U << 0))
#define JLP_FEATURE_FLASH(status)   (status > 1) // Turn on Flash for any jlp value > 1 should be (status & (1U << 1))

#define JLP_FLASH_ROWS_PER_SECTOR   8
#define JLP_FLASH_ROW_BYTES         96 * 2     // 96 * uint16_t
#define JLP_FLASH_SECTOR_BYTES      JLP_FLASH_ROWS_PER_SECTOR * JLP_FLASH_ROW_BYTES
#define JLP_RAM_ADDRESS             cart.RAM[0x25]
#define JLP_ROW_NUMBER              cart.RAM[0x26]

void init_cart(void);
int load_cfg(char *filename);
void apply_pokes(char *filename);
void config_jlp(int jlp_value, int jlpflash_value, char *filename);
int collect_info(char *filename, INFO_ENTRY *info_entries);

#endif
