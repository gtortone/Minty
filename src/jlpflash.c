#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "vfs.h"
#include "intellicart.h"

extern Cartridge cart;

void readFlash(int row, uint16_t addr) {

   unsigned int br = 0;
   int c;
   uint32_t offset = row * JLP_FLASH_ROW_BYTES;
   uint8_t temp[2];

   if ( (cart.filesave = vfs_open(cart.flashfile, "r")) == NULL) {
      printf("E: JLP flash read - open error\n");
      return;
   }

   vfs_lseek(cart.filesave, 0);
   vfs_lseek(cart.filesave, offset);

   //printf("ROW:%d\n", row);
   for(int i=0; i<(JLP_FLASH_ROW_BYTES/2); i++) {
      c = vfs_read(cart.filesave, temp, 2); 
      //printf("R[%d]: %X %X\n", i, temp[0], temp[1]);
      if ( (c == -1) || (c < 2) ) {
         printf("E: JLP flash read - chunk read error %d/2\n", c);
         vfs_close(cart.filesave);
         return;
      }

      cart.RAM[(addr - 0x8000) + i] = (uint16_t) ((temp[0] << 8) | temp[1]);
      br += 2;
   }

   if (br != JLP_FLASH_ROW_BYTES) {
      printf("E: JLP flash read - size mismatch %d/%d\n", br, JLP_FLASH_ROW_BYTES);
      vfs_close(cart.filesave);
      return;
   }

   vfs_close(cart.filesave);
}

// RAM -> flash
void writeFlash(int row, uint16_t addr) {

   int c;
   unsigned int bw = 0;
   uint32_t offset = row * JLP_FLASH_ROW_BYTES;
   uint8_t temp[2];

   if ( (cart.filesave = vfs_open(cart.flashfile, "w")) == NULL) {
      printf("E: JLP flash write - open error\n");
      return;
   }

   vfs_lseek(cart.filesave, 0);
   vfs_lseek(cart.filesave, offset);

   //printf("ROW:%d\n", row);
   for(int i=0; i<(JLP_FLASH_ROW_BYTES/2); i++) {
      temp[0] = (cart.RAM[(addr - 0x8000) + i] >> 8) & 0xFF;
      temp[1] = cart.RAM[(addr - 0x8000) + i] & 0xFF;
      //printf("W[%d]: %X %X\n", i, temp[0], temp[1]);
      c = vfs_write(cart.filesave, temp, 2);
      if ( (c == -1) || (c < 2) ) {
         printf("E: JLP flash write - chunk write error %d/2\n", c);
         vfs_close(cart.filesave);
         return;
      }
      bw += 2;
   }
   if (bw != JLP_FLASH_ROW_BYTES) {
      printf("E: JLP flash write - size mismatch %d/%d\n", bw, JLP_FLASH_ROW_BYTES);
      vfs_close(cart.filesave);
      return;
   }

   vfs_close(cart.filesave);
}

void eraseFlash(int row) {

   int bw;
   uint16_t sector = row % JLP_FLASH_ROWS_PER_SECTOR;
   uint32_t offset = sector * JLP_FLASH_SECTOR_BYTES;
   uint8_t erase_pattern[JLP_FLASH_SECTOR_BYTES];

   if ( (cart.filesave = vfs_open(cart.flashfile, "w")) == NULL) {
      printf("E: JLP flash erase - open error\n");
      return;
   }

   memset(erase_pattern, 0xFF, sizeof(erase_pattern));

   vfs_lseek(cart.filesave, offset);

   if ( (bw = vfs_write(cart.filesave, erase_pattern, sizeof(erase_pattern))) == -1) {
      //printf("E: JLP flash erase - erase sector %d\n", sector);
      vfs_close(cart.filesave);
      return;
   }

   if (bw != JLP_FLASH_SECTOR_BYTES) {
      //printf("E: JLP flash erase - size mismatch %d/%d\n", bw, JLP_FLASH_SECTOR_BYTES);
      vfs_close(cart.filesave);
      return;
   }

   vfs_close(cart.filesave);
}


