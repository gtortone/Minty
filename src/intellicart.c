
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
#include "rommeta_parser.h"

typedef enum {
   NONE,
   MAPPING,
   MEMATTR,
   VARS,
   MACRO,
   MAIN
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

inline void config_jlp(int jlp_value, int jlpflash_value, char *filename) {

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
      uint32_t chunk = 0;
      uint32_t remaining = 0;
      uint32_t written = 0;

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

         remaining = size;

         while (remaining) {

            chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;

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

int collect_info(char *filename, INFO_ENTRY *info_entries) {

   char line[256];
   char cfgfile[512] = {0};
   vfs_file_t *f;
   cfgSection cfgsec = NONE;
   cfgSection new_cfgsec = NONE;
   int cur_line = 0;
   int cur_page = 1;
   vfs_stat_t st;

   // Get file name and size for MAIN section
   info_entries->section = MAIN;
   // clear new page
   for (int i=0; i<10 ; i++)
      for (int j=0; j<20; j++)
         info_entries->line[i][j] = 0;

   char *basename = strrchr(filename, '/') + 1;
   snprintf(info_entries->line[cur_line++], 20, "%-19s", "Filename");
   snprintf(info_entries->line[cur_line++], 20, "%19s", basename);

   if (vfs_stat(filename, &st) == 0) {
      printf("file size: %d bytes\n", st.size);
      snprintf(info_entries->line[cur_line++], 20, "%-19s", "File size");
      snprintf(info_entries->line[cur_line++], 20, "%12d KWords", st.size/1024/2);
   }   

   if (is_rom_file(filename)) {
      // ROM file, info collection parsing complete file to get mapping and memattr info as well as JLP attributes if available
      snprintf(info_entries->line[cur_line++], 20, "%-19s", "File Type");
      snprintf(info_entries->line[cur_line++], 20, "%19s", "ROM");
      
      f = vfs_open(filename, "r");
      if (f != NULL) {
         uint32_t from, prev_from;
         uint16_t prev_size, target;
         char inputBuffer[3];

         vfs_read(f, inputBuffer, sizeof(inputBuffer));
         
         // read number of segments
         int slots = inputBuffer[1];

         // fill current page so that new page is started for MAPPING section
         cur_line = 10;

         for(int i=0; i<slots; i++) {
            vfs_read(f, inputBuffer, 2);

            int lo = inputBuffer[0] << 8;
            int hi = (inputBuffer[1] << 8) + 0x100;

            target = lo;
            if (i == 0)
               from = 0x0000;
            else
               from = prev_from + prev_size + 1;

            prev_size = (hi - lo) - 1;
            prev_from = from;

            // store mapping info in info_entries
            // if page is full start a new one
            if (cur_line == 10) {
               cur_line = 0;
               cur_page++;
               info_entries++;
               info_entries->section = MAPPING;
               // clear new page
               for (int i=0; i<10 ; i++)
                  for (int j=0; j<20; j++)
                     info_entries->line[i][j] = 0;
            }
            sprintf(line, "$%04X - $%04X", lo, hi-1);
            snprintf(info_entries->line[cur_line++], 20, "%-19s", line);
            sprintf(line, "$%04X", target);
            snprintf(info_entries->line[cur_line++], 20, "%19s", line);

            // skip actual ROM data
            for (int j = lo; j < hi; j++) {
               vfs_read(f, inputBuffer, 2);
            }
            // skip CRC (2 bytes)
            vfs_read(f, inputBuffer, 2);
         }

         // fill current page so that new page is started for MEMATTR section
         cur_line = 10;
         
         // read memory block (2Kb) attributes
         char memattr[50];
         vfs_read(f, memattr, sizeof(memattr));
         for (int i = 0; i < 32; i++) {
            int attr = 0xF & (memattr[(i >> 1)] >> ((i & 1) * 4));
            int lohi = memattr[16 + ((i >> 1) | ((i & 1) << 4))];
            int lo   = (lohi >> 4) & 0x7;
            int hi   = (lohi & 0x7) + 1;

            // check if memory block has write attribute
            if(attr & 0x02) {              
               // store memattr info in info_entries, if page is full start a new one
               if (cur_line == 10) {
                  cur_line = 0;
                  cur_page++;
                  info_entries++;
                  info_entries->section = MEMATTR;
                  // clear new page
                  for (int i=0; i<10 ; i++)
                     for (int j=0; j<20; j++)
                        info_entries->line[i][j] = 0;
               }
               sprintf(line, "$%04X - $%04X", i * 0x800, (i * 0x800) + ((hi - lo) * 0x100) - 1);
               snprintf(info_entries->line[cur_line++], 20, "%-19s", line);
               if(attr & 0x04)
                  snprintf(info_entries->line[cur_line++], 20, "%19s", "RAM 8");
               else
                  snprintf(info_entries->line[cur_line++], 20, "%19s", "RAM 16");
            }
         }
         // fill current page so that new page is started for VARS section
         cur_line = 10;

         // check for metadata section 
         if (!vfs_eof(f)) {
            rommeta_status_t st;
            rommeta_parser_t parser;
            char key[ROMMETA_STR_SIZE];
            char value[ROMMETA_STR_SIZE];

            rommeta_parser_init(&parser);

            for (;;)
            {
               st = rommeta_read_next(&parser, f, key, value);

               if (st == ROMMETA_END)
                  break;

               if (st == ROMMETA_ERR)
               {
                  printf("Error parsing ROM metadata\n");
                  break;
               }

               // need to store VARS attributes in info_entries for display in UI
               // store memattr info in info_entries, if page is full start a new one
               if (cur_line == 10) {
                  cur_line = 0;
                  cur_page++;
                  info_entries++;
                  info_entries->section = VARS;
                  // clear new page
                  for (int i=0; i<10 ; i++)
                     for (int j=0; j<20; j++)
                        info_entries->line[i][j] = 0;
               }
               snprintf(info_entries->line[cur_line++], 20, "%-19s", key);
               snprintf(info_entries->line[cur_line++], 20, "%19s", value);      
            }
         }
         vfs_close(f);
      }
   }
   else {
      // not a ROM file, try to collect info from corresponding config file
      char *dot = strrchr(filename, '.');
      strncpy(cfgfile, filename, (dot - filename));
      strcat(cfgfile, ".cfg");  
     
      f = vfs_open(cfgfile, "r");
      if (f == NULL) {
         printf("collect_info: could not open file %s\n", cfgfile);
         snprintf(info_entries->line[cur_line++], 20, "%-19s", "file Type");
         snprintf(info_entries->line[cur_line++], 20, "%19s", "BIN (no cfg)");
      }
      else {
         snprintf(info_entries->line[cur_line++], 20, "%-19s", "file Type");
         snprintf(info_entries->line[cur_line++], 20, "%19s", "BIN+CFG");
         // now parse the file to collect info entries
         while (!(vfs_eof(f))) {
            vfs_gets(f, line, sizeof(line));
            strcpy(line, trim(line));

            // skip comments
            if ( (line[0] == ';') || !(stralpha(line)) ) {
               continue;
            }

            if (strstr(line, "[mapping]") != NULL) {
               new_cfgsec = MAPPING;
               printf("[mapping] section\n");
            } else if (strstr(line, "[memattr]") != NULL) {
               new_cfgsec = MEMATTR;
               printf("[memattr] section\n");
            } else if (strstr(line, "[vars]") != NULL) {
               new_cfgsec = VARS;
               printf("[vars] section\n");
            } else if (strstr(line, "[macro]") != NULL) {
               new_cfgsec = MACRO;
               printf("[macro] section\n");
            }

            if (new_cfgsec != cfgsec) {
               // new config section detected => fill current page so that new page gets started for new section
               cfgsec = new_cfgsec;
               cur_line = 10;
               continue;
            }

            if (cfgsec != NONE) {
               printf("collecting info from line: %s\n", line);
               char *equal_sign = strchr(line, '=');
               if (equal_sign) {
                  // split the line into key and value
                  *equal_sign = 0;
                  char *key = trim(line);
                  char *value = trim(equal_sign + 1);
                  // remove trailing comment
                  char *comment_sign = strchr(value, ';');
                  if (comment_sign) *comment_sign = 0;
                  value = trim(value);
                  // if page is full start a new one
                  if (cur_line == 10) {
                     cur_line = 0;
                     cur_page++;
                     info_entries++;
                     info_entries->section = cfgsec;
                     // clear new page
                     for (int i=0; i<10 ; i++)
                        for (int j=0; j<20; j++)
                           info_entries->line[i][j] = 0;
                  }
                  printf("key: %s, value: %s\n", key, value);
                  snprintf(info_entries->line[cur_line++], 20, "%-19s", key);
                  snprintf(info_entries->line[cur_line++], 20, "%19s", value);
               } else {
                  printf("E: invalid line:\t %s\n", line);
               }
            }
         }
         vfs_close(f);
      }
   }
   return cur_page;
}
