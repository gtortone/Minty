/*
//   Minty - MultiCart for Mattel Intellivision by Gennaro Tortone 2025
//
//   based on PiRTO II Flash MultiCART by Andrea Ottaviani 2024
//   parts of code are directly from the A8PicoCart project by Robin Edwards 2023
*/

#ifndef __FLASH_FS_H__
#define __FLASH_FS_H__

#include <stdbool.h>

#define SECTOR_SIZE  512
#define HW_FLASH_STORAGE_BASE  (1 * 1024 * 1024)
#define NUM_FAT_SECTORS ((PICO_FLASH_SIZE_BYTES - HW_FLASH_STORAGE_BASE) / 512) - 4
#define NUM_FLASH_SECTORS ((PICO_FLASH_SIZE_BYTES - HW_FLASH_STORAGE_BASE) / 4096)

int flash_fs_mount();
void flash_fs_create();
void flash_fs_sync();
void flash_fs_read_FAT_sector(uint16_t fat_sector, void *buffer);
void flash_fs_write_FAT_sector(uint16_t fat_sector, const void *buffer);
bool flash_fs_verify_FAT_sector(uint16_t fat_sector, const void *buffer);

#endif
