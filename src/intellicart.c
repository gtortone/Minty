
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "memory.h"
#include "cartridge.h"
#include "fingerprints.h"
#include "intellicart.h"
#include "utils.h"
#include "vfs.h"

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

void config_jlp(int jlp_value, int jlpflash_value, char *filename) {

   if (jlp_value != 0) {
      // JLP required
      cart.JLPSupport = true;
      printf("JLP support ON\n");

      if (JLP_FEATURE_ACCEL(jlp_value)) {
         cart.JLPAccel = true;
         printf("JLP accelerators ON\n");
      }
   }

   cart.JLPFlashSize = jlpflash_value;

   if ( JLP_FEATURE_FLASH(jlp_value) && (cart.JLPFlashSize > 0) ) {
      cart.JLPFlash = true;
      printf("JLP flash ON\n");
      printf("JLP flash size: %d\n", cart.JLPFlashSize);
   }

   if (cart.JLPFlash) {

      // check if JLP flash file exists
      vfs_file_t *f;
      vfs_stat_t st;
      char *dot = strrchr(filename, '.');

      strncpy(cart.flashfile, filename, (dot - filename));
      strcat(cart.flashfile, ".save");

      if(vfs_stat(cart.flashfile, &st) == -1) {

         uint8_t buffer[JLP_FLASH_SECTOR_BYTES];
         uint32_t size = cart.JLPFlashSize * JLP_FLASH_SECTOR_BYTES;
         
         // create JLP flash file
         f = vfs_open(cart.flashfile, "w");
         if (f == NULL) {
            printf("E: file create error\n");
            return;
         }

         printf("creating JLP flash file: %s, size: %ld\n", cart.flashfile, size);

         memset(buffer, 0xFF, sizeof(buffer));

         uint32_t remaining = size;
         uint32_t written = 0;

         while (remaining) {

            uint32_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;

            written = vfs_write(f, buffer, chunk);

            if (written != chunk) {
               printf("E: write error\n");
               vfs_close(f);
               break;
            }

            remaining -= written;
         }

         vfs_close(f);
      }

      printf("I: use available JLP flash file: %s\n", cart.flashfile);

   }  // end if(JLPFlash)
}

int load_cfg(char *filename) {

   char line[128];
   char cfgfile[512] = {0};
   cfgSection cfgsec; 
   vfs_file_t *f;
   int ret;
   int num_pokes = 0;

#if CONFIG_JLP
   int jlp_value = 0;
   int jlpflash_value = 0;
#endif

   char *dot = strrchr(filename, '.');
   strncpy(cfgfile, filename, (dot - filename));
   strcat(cfgfile, ".cfg");

   // config file not available, try to config memory using fingerprint
   if ((f = vfs_open(cfgfile, "r")) == NULL) {
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

               return num_pokes;
            }

            config_memory(fingerprints[i+1]);
            getRAMRange(&cart.ramfrom, &cart.ramto, &cart.ramwidth);
            //printf("cart.ramfrom: 0x%X, cart.ramto: 0x%X, cart.ramwidth: %d\n",
            //      cart.ramfrom, cart.ramto, cart.ramwidth);

            return num_pokes;
         }
      }

      // if this line is reached no config file and no fingerprint so use default config
      config_memory(0);
      return num_pokes;
   }

   printf("load_cfg: use %s config file\n", cfgfile);
  
   cleanSlots();
   cleanHoles();

   cfgsec = NONE;

   while (!(vfs_eof(f))) {

      if (vfs_gets(f, line, sizeof(line)) == NULL)
         continue;

      strcpy(line, trim(line));

      //printf("line: %s, len: %d\n", line, strlen(line));

      // skip comments
      if ( (line[0] == ';') || !(stralpha(line)) )
         continue;

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
               return num_pokes;
            }
            cart.pagingSupport = true;
            addSlot(a, b, c, p, ROM_PAGE_SLOT);

         } else {

            ret = sscanf(line, "$%lx - $%lx = $%lx", &a, &b, &c); 
            if (ret != 3) {
               printf("E: parsing error in line: \n\t %s\n", line);
               return num_pokes;
            }
            addSlot(a, b, c, 0, ROM_SLOT);
         }

      } else if (cfgsec == MEMATTR) {
         // example:
         // $8800 - $8FFF = RAM 8
         
         uint32_t a, b;
         int w;
         char type[4];

         ret = sscanf(line, "$%lx - $%lx = %3s %d", &a, &b, type, &w);
         if (ret != 4) {
            printf("E: parsing error in line: \n\t %s\n", line);
            return num_pokes;
         }
         if ( (strcmp(type, "ROM") != 0) && (strcmp(type, "RAM") != 0) ) {
            printf("E: parsing error in line: \n\t %s\n", line);
            return num_pokes;
         }
         if (w == 8)
            addSlot(a, b, 0, 0, RAM8_SLOT);
         else
            addSlot(a, b, 0, 0, RAM16_SLOT);

      } else if (cfgsec == VARS) {

#if CONFIG_JLP
         if ( sscanf(line, "jlp = %d", &jlp_value) == 1  ||
               sscanf(line, "jlp_accel = %d", &jlp_value) == 1  || 
               sscanf(line, "jlpaccel = %d", &jlp_value) == 1 ) {
            printf("JLP config found\n");
         }

         if ( sscanf(line, "jlp_flash = %d", &jlpflash_value) == 1  ||
               sscanf(line, "jlpflash = %d", &jlpflash_value) == 1 ) {

            printf("JLP flash config found\n");
         }
#endif

      } else if (cfgsec == MACRO) {
         // example: 
         // p 66fe 34 or poke 66fe 34
         uint32_t poke_address, poke_value;
         char cmd[16];
            
         ret = sscanf(line, "%15s %lx %lx", cmd, &poke_address, &poke_value);

         if (ret == 3 && (strcasecmp(cmd, "p") == 0 || strcasecmp(cmd, "poke") == 0)) {
            num_pokes++;
            printf("Poke %d detected : %lx @ %lx\n",num_pokes,poke_value,poke_address);
         }
         else {
            printf("E: not a MACRO/poke cmd in line: \n\t %s\n", line);
         }
      }
   }

   // rewind cfg file to apply POKES (hacks)

   vfs_close(f);

#if CONFIG_JLP
   config_jlp(jlp_value, jlpflash_value, filename);
#endif

   getRAMRange(&cart.ramfrom, &cart.ramto, &cart.ramwidth);

   //printFilledSlots();

   printf("load_cfg done\n");

   return num_pokes;
}

void apply_pokes(char *filename) {
   char line[256];
   char cfgfile[512] = {0};
   cfgSection cfgsec; 
   vfs_file_t *f;
   int ret;

   char *dot = strrchr(filename, '.');
   strncpy(cfgfile, filename, (dot - filename));
   strcat(cfgfile, ".cfg");

   printf("applying pokes\n");

   f = vfs_open(cfgfile, "r");

   cfgsec = NONE;
   while (!(vfs_eof(f))) {
      vfs_gets(f, line, sizeof(line));
      strcpy(line, trim(line));

      // skip comments
      if ( (line[0] == ';') || !(stralpha(line)) ) {
         continue;
      }

      // detect start of MACRO section
      if (strstr(line, "[macro]") != NULL) {
         cfgsec = MACRO;
         printf("[macro] section\n");
         continue;
      }

      if (cfgsec == MACRO) {
         // detect end of MACRO section
         if (strstr(line, "[") != NULL) {
            cfgsec = NONE;
            printf("End of MACRO section\n");
         }
         else {
            // example: 
            // p 66fe 34 or poke 66fe 34
            uint32_t poke_address, poke_value;
            char cmd[16];
            
            ret = sscanf(line, "%15s %lx %lx", cmd, &poke_address, &poke_value);

            if (ret == 3 && (strcasecmp(cmd, "p") == 0 || strcasecmp(cmd, "poke") == 0)) {
               // Modify actual value @corresponding address in binary
               uint32_t romaddr;
               mapType type = ROM_SLOT;
               
               if (mapAddress(poke_address, 0, &romaddr, &type)) {
                  cart.ROM[romaddr] = poke_value;               /* valid command */
                  printf("Apply poke : value %lx @ address %lx (%lx)\n",poke_value, poke_address, romaddr);
               }
               else {
                  printf("E: failure to map address %lx\n", poke_address);
               }
            }
            else {
               printf("E: not a MACRO/poke cmd in line: \n\t %s\n", line);
            }
         }
      }
   }
}
