
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ff.h"
#include "memory.h"
#include "cartridge.h"
#include "fingerprints.h"
#include "intellicart.h"
#include "utils.h"

typedef enum {
   NONE,
   MAPPING,
   MEMATTR,
   VARS,
   MACRO
} cfgSection;

Cartridge cart;     // main data structure for cart emulation

extern struct mapEntry slots[NSLOTS];

// RAM base address for launcher 
const unsigned int base = 0x17F;

void init_cart(void) {

   memset((uint16_t *) cart.ROM, 0, sizeof(cart.ROM));
   memset((uint16_t *) cart.RAM, 0, sizeof(cart.RAM));

   cart.ramfrom = 0;
   cart.ramto = 0;
   cart.ramwidth = 0;

   cart.pagingSupport = false;

   cart.JLPSupport = false;
   cart.JLPFlash = false;
   cart.JLPFlashSize = 0;
   cart.JLPAccel = 0;
   cart.flashfile[0] = '\0';
}

void load_cfg(char *filename) {

   char line[80];
   char cfgfile[512] = {0};
   FIL fil;
   cfgSection cfgsec; 
   int ret;

   char *dot = strrchr(filename, '.');
   strncpy(cfgfile, filename, (dot - filename));
   strcat(cfgfile, ".cfg");

   // config file not available, try to config memory using fingerprint
   if (f_open(&fil, cfgfile, FA_READ) != FR_OK) {
      int fp = 0;

      for (int i = 0; i < 128; i++)
         fp += ((cart.ROM[i] & 0xFF00) >> 8) + (cart.ROM[i] & 0x00FF);

      printf("filename: %s, fp: %d\n", filename, fp);

      for (int i=0; i<sizeof(fingerprints)/sizeof(int); i += 2) {
         if (fp == fingerprints[i]) {
            if (fp == 11349) {
               // Baseball or MTE Test Cart?
               if (cart.len > 8192)
                  config_memory(8);
               else
                  config_memory(0);

               getRAMRange(&cart.ramfrom, &cart.ramto, &cart.ramwidth);

               return;
            }

            config_memory(fingerprints[i+1]);
            getRAMRange(&cart.ramfrom, &cart.ramto, &cart.ramwidth);
            printf("cart.ramfrom: 0x%X, cart.ramto: 0x%X, cart.ramwidth: %d\n",
                  cart.ramfrom, cart.ramto, cart.ramwidth);

            return;
         }
      }

      // if this line is reached no config file and no fingerprint so use default config
      config_memory(0);
      return;
   }

   printf("load_cfg: use %s config file\n", cfgfile);
  
   cleanSlots();
   cleanHoles();
   cleanHacks();

   cfgsec = NONE;

   while (!(f_eof(&fil))) {

      f_gets(line, sizeof(line), &fil);
      strcpy(line, trim(line));

      //printf("line: %s, len: %d\n", line, strlen(line));

      // skip comments
      if ( (line[0] == ';') || !(stralpha(line)) ) {
         cfgsec = NONE;
         continue;
      }

      if (strstr(line, "[mapping]") != NULL) {
         cfgsec = MAPPING;
         printf("[mapping] section\n");
         continue;
      } else if (strstr(line, "[memattr]") != NULL) {
         cfgsec = MEMATTR;
         printf("[memattr] section\n");
         continue;
      } else if (strstr(line, "[vars]") != NULL) {
         cfgsec = VARS;
         printf("[vars] section\n");
         continue;
      } else if (strstr(line, "[macro]") != NULL) {
         cfgsec = MACRO;
         printf("[macro] section\n");
         continue;
      }

      if (cfgsec == MAPPING) {
         // example:
         // $2200 - $30FF = $7100

         uint32_t a, b, c, p;

         if(strstr(line, "PAGE") != NULL) {

            ret = sscanf(line, "$%lx - $%lx = $%lx%*[^P]PAGE %lx", &a, &b, &c, &p);
            if (ret != 4) {
               printf("E: parsing error in line: \n\t %s\n", line);
               return;
            }
            cart.pagingSupport = true;
            addSlot(a, b, c, p, ROM_PAGE_SLOT);

         } else {

            ret = sscanf(line, "$%lx - $%lx = $%lx", &a, &b, &c); 
            if (ret != 3) {
               printf("E: parsing error in line: \n\t %s\n", line);
               return;
            }
            addSlot(a, b, c, 0, ROM_SLOT);
         }

      } else if (cfgsec == MEMATTR) {
         // example:
         // $8800 - $8FFF = RAM 8
         
         uint32_t a, b;
         uint8_t w;

         ret = sscanf(line, "$%lx - $%lx =%*[^R]RAM %hhd", &a, &b, &w);
         if (ret != 3) {
            printf("E: parsing error in line: \n\t %s\n", line);
            return;
         }
         if (w == 8)
            addSlot(a, b, 0, 0, RAM8_SLOT);
         else
            addSlot(a, b, 0, 0, RAM16_SLOT);

      } else if (cfgsec == VARS) {

         uint8_t value;

         if ( sscanf(line, "jlp = %hhd", &value) == 1  ||
               sscanf(line, "jlp_accel = %hhd", &value) == 1  || 
               sscanf(line, "jlpaccel = %hhd", &value) == 1 ) {

            if (value != 0) {
               // JLP required
               cart.JLPSupport = true;
               printf("JLP support ON\n");

               if (JLP_FEATURE_ACCEL(value)) {
                  cart.JLPAccel = true;
                  printf("JLP accelerators ON\n");
               }
            }
         }

         if ( sscanf(line, "jlp_flash = %hhd", &cart.JLPFlashSize) == 1  ||
               sscanf(line, "jlpflash = %hhd", &cart.JLPFlashSize) == 1 ) {

            if ( JLP_FEATURE_FLASH(value) && (cart.JLPFlashSize > 0) ) {
               cart.JLPFlash = true;
               printf("JLP flash ON\n");
               printf("JLP flash size: %d\n", cart.JLPFlashSize);
            }
         }


      } else if (cfgsec == MACRO) {
         // example: 
         // p 66fe 34

         uint32_t a, b;

         ret = sscanf(line, "p %lx %lx", &a, &b);
         if (ret != 2) {
            printf("E: parsing error in line: \n\t %s\n", line);
            return;
         }

         addHack(a, b);
      }
   }

   f_close(&fil);

   if (cart.JLPFlash) {

      strncpy(cart.flashfile, filename, (dot - filename));
      strcat(cart.flashfile, ".save");

      FILINFO fno;
      FRESULT res;

      // check if JLP flash file exists
      res = f_stat(cart.flashfile, &fno);

      if(res == FR_NO_FILE) {

         uint8_t buffer[JLP_FLASH_SECTOR_BYTES];
         uint32_t size = cart.JLPFlashSize * JLP_FLASH_SECTOR_BYTES;
         
         // create JLP flash file
         res = f_open(&fil, cart.flashfile, FA_CREATE_ALWAYS | FA_WRITE);
         if (res != FR_OK) {
            printf("E: file create error\n");
            f_close(&fil);
            return;
         }

         res = f_expand(&fil, size, 1);
         if (res != FR_OK) {
            printf("E: expand error\n");
            f_close(&fil);
            return;
         }

         printf("creating JLP flash file: %s, size: %ld\n", cart.flashfile, size);

         memset(buffer, 0xFF, sizeof(buffer));

         uint32_t remaining = size;
         unsigned int written = 0;

         f_lseek(&fil, 0);

         while (remaining) {
            uint32_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;

            res = f_write(&fil, buffer, chunk, &written);

            if (res != FR_OK || written != chunk) {
               printf("E: write error\n");
               f_close(&fil);
               break;
            }

            remaining -= written;
         }

         f_close(&fil);
      }

      printf("I: use available JLP flash file: %s\n", cart.flashfile);

   }  // end if(JLPFlash)

   getRAMRange(&cart.ramfrom, &cart.ramto, &cart.ramwidth);

   printf("load_cfg done\n");
}


