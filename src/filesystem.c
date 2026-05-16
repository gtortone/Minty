
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "filesystem.h"
#include "intellicart.h"
#include "memory.h"
#include "ff.h"
#include "f_util.h"

extern Cartridge cart;

extern struct mapEntry slots[NSLOTS];

extern const unsigned int base;

int entry_compare(const void *p1, const void *p2) {
   DIR_ENTRY *e1 = (DIR_ENTRY *) p1;
   DIR_ENTRY *e2 = (DIR_ENTRY *) p2;

   if (e1->isDir && !e2->isDir)
      return -1;
   else if (!e1->isDir && e2->isDir)
      return 1;
   else
      return strcasecmp(e1->long_filename, e2->long_filename);
}

char *get_filename_ext(char *filename) {
   char *dot = strrchr(filename, '.');

   if (!dot || dot == filename)
      return "";
   return dot + 1;
}

int is_rom_file(char *filename) {
   FIL fil;
   FRESULT fr;
   UINT br;
   char inputBuffer[3];

   if ( (fr = f_open(&fil, filename, FA_READ)) != FR_OK ) {
      printf("load_file %s error (%s)!\n", filename, FRESULT_str(fr));
   }

   f_read(&fil, inputBuffer, sizeof(inputBuffer), &br);

   f_close(&fil);

   return !((inputBuffer[0] != 0xA8 && (inputBuffer[0] & ~0x20) != 0x41) ||
         (inputBuffer[1] ^ inputBuffer[2]) != 0xFF);
}

int is_valid_file(char *filename) {
   char *ext = get_filename_ext(filename);

   // .BIN, .INT, .ITV files are raw image ROMs
   // .ROM files are Intellicart ROMs
   return (strcasecmp(ext, "BIN") == 0 || strcasecmp(ext, "INT") == 0 || 
         strcasecmp(ext, "ITV") == 0 || strcasecmp(ext, "ROM") == 0);
}

int read_directory(char *path, unsigned char *list) {
   UINT id = 0;
   FILINFO fno;

   int n = 0;
   DIR_ENTRY *dst = (DIR_ENTRY *) & list[0];

   DIR dir;
   FRESULT fr;

   if ((fr = f_opendir(&dir, path)) == FR_OK) {
      while (1) {
         if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
            break;
         if (fno.fattrib & (AM_HID | AM_SYS))
            continue;
         dst->isDir = fno.fattrib & AM_DIR ? 1 : 0;
         if (!dst->isDir)
            if (!is_valid_file(fno.fname))
               continue;
         dst->id = id++;
         strncpy(dst->long_filename, fno.fname, 20);
         dst->long_filename[20] = 0;
         //printf("%d) entry: %s\n", dst->id, dst->long_filename);
         dst++;
         n++;
      }
      f_closedir(&dir);
   } 

   qsort((DIR_ENTRY *) & list[0], n, sizeof(DIR_ENTRY), entry_compare);
   
   return n;
}

void load_file(char *filename) {
   UINT br, size = 0;
   unsigned char byteread[2];
   int bytes_to_read = 2;
   FIL fil;
   FRESULT fr;

   if ( (fr = f_open(&fil, filename, FA_READ)) != FR_OK ) {
      printf("load_file %s error (%s)!\n", filename, FRESULT_str(fr));
   }
  
   // init cartridge
   init_cart();

   // handle Intellicart rom file
   if(is_rom_file(filename)) {

      uint32_t from, prev_from;
      uint16_t prev_size, target;
      char inputBuffer[3];

      f_read(&fil, inputBuffer, sizeof(inputBuffer), &br);
      
      // read number of segments
      int slots = inputBuffer[1];

      for(int i=0; i<slots; i++) {

         f_read(&fil, byteread, bytes_to_read, &br);
         int lo = byteread[0] << 8;
         int hi = (byteread[1] << 8) + 0x100;

         target = lo;
         if (i == 0)
            from = 0x0000;
         else
            from = prev_from + prev_size + 1;

         prev_size = (hi - lo) - 1;
         prev_from = from;

         addSlot(from, from + (hi - lo) - 1, target, 0, ROM_SLOT);

         for (int j = lo; j < hi; j++) {

            f_read(&fil, byteread, bytes_to_read, &br);
            cart.ROM[size] = byteread[1] | (byteread[0] << 8);
            size++;
         }

         // skip CRC (2 bytes)
         f_read(&fil, byteread, bytes_to_read, &br);
      }

      // read memory block (2Kb) attributes
      char memattr[50];

      f_read(&fil, memattr, sizeof(memattr), &br);

      for (int i = 0; i < 32; i++) {
         
         int attr = 0xF & (memattr[(i >> 1)] >> ((i & 1) * 4));
         int lohi = memattr[16 + ((i >> 1) | ((i & 1) << 4))];
         int lo   = (lohi >> 4) & 0x7;
         int hi   = (lohi & 0x7) + 1;

         // check if memory block has write attribute
         if(attr & 0x02) { 

            mapType type;

            if(attr & 0x04)
               type = RAM8_SLOT;   // narrow flag (8-bit)
            else
               type = RAM16_SLOT;

            addSlot(i * 0x800, (i * 0x800) + ((hi - lo) * 0x100) - 1, 0, 0, type);
         }
      }

      getRAMRange(&cart.ramfrom, &cart.ramto, &cart.ramwidth);

   } else { 

      // handle raw rom file

      // read the file to SRAM
      while (!(f_eof(&fil))) {
         f_read(&fil, byteread, bytes_to_read, &br);
         cart.ROM[size] = byteread[1] | (byteread[0] << 8);
         size++;
      }
   }

   cart.len = size;
   cart.RAM[base + 202] = cart.len;
   f_close(&fil);

   printf("load_file: size: %ld\n", cart.len);
}

void load_file_by_id(UINT id, char *path, char *fullpath) {
   DIR dir;
   UINT i = 0;
   FILINFO fno;

   if (f_opendir(&dir, path) == FR_OK) {
      while (1) {
         if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
            break;
         if (fno.fattrib & (AM_HID | AM_SYS))
            continue;
         if (!(fno.fattrib & AM_DIR))
            if (!is_valid_file(fno.fname))
               continue;
         if (i++ == id) {
            f_closedir(&dir);
            strcat(fullpath, path);
            strcat(fullpath, "/");
            strcat(fullpath, fno.fname);
            printf("load_file_by_id: id %d, opening %s\n", id, fullpath);
            load_file(fullpath); 
            return;
         }
      }
      f_closedir(&dir);
   } 
}

void filelist(DIR_ENTRY *en, int from, int to, int num) {
   int base = 0x17f;

   for (int i = 0; i < 20 * 20; i++)
      cart.RAM[base + i * 2] = 0;
   for (int n = 0; n < (to - from); n++) {
      if (en[n + from].isDir)
         cart.RAM[0x1000 + n] = 1;
      else
         cart.RAM[0x1000 + n] = 0;
      
      for (int i = 0; i < 20; i++) {
         int pos = base + i * 2 + (n * 40);
         cart.RAM[pos] = en[n + from].long_filename[i];
         if (cart.RAM[pos] <= 20)
            cart.RAM[pos] = 32;
      }
   }
   cart.RAM[0x1028] = (from & 0xFF00) >> 8;   // MSB
   cart.RAM[0x1029] = (from & 0x00FF);        // LSB
   cart.RAM[0x1030] = (to & 0xFF00) >> 8;    // MSB
   cart.RAM[0x1031] = (to & 0x00FF);         // LSB
   cart.RAM[0x1032] = (num & 0xFF00) >> 8;  // MSB
   cart.RAM[0x1033] = (num & 0x00FF);       // LSB
}

