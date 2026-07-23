#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/rand.h"

#include "board.h"

#include "memory.h"

#include "interface.h"
#include "filesystem.h"
#include "intellicart.h"
#include "launcher.h"
#include "version.h"

#ifdef PINTYCARD
   #include "pintyrom.h"
#else
   #include "mintyrom.h"
#endif

#if CONFIG_SD_STORAGE
   #include "config.h"
#endif

#if CONFIG_USB_DEVICE
   #include "usb_tasks.h"
#endif

#if CONFIG_SD_STORAGE
// config file
vfs_file_t *cfgfile;
vfs_stat_t st;

struct boardConfig cfg = {
   .version = CONFIG_VERSION,
   .magicNumber = CONFIG_MAGIC_NUMBER 
}; 
#endif

extern Cartridge cart;     // main data structure for cart emulation

extern mm_map_t m;

// concole configuration
uint8_t tv_mode;      // 0: PAL, 1: NTSC
uint8_t ecs_present;  // 0: ECS absent, 1: ECS present
uint8_t ecs_volume = 0xFF;   

#if CONFIG_FLASH_FAT_STORAGE
int volumeId = 0;    // default flash storage
#else
int volumeId = 1;    // default SD storage
#endif

char curPath[512] = "";

// recycle cartridge ROM to allocate temporary buffers (min size is 2x50x1024 i.e. going till 0x18FFF)
// start from 0x4000 to allow launcher ROM size up to 16Kb
SCREEN_ENTRY *screen_entries = (SCREEN_ENTRY *) (cart.ROM + 0x2000);
// maximum 512 entries allowed, 69 bytes per entry, 512 entries max = 34Kb (40kB allocated)
// 0x2000 + (512 * 69)/2 = 0x6500
INFO_ENTRY *info_entries = (INFO_ENTRY *) (cart.ROM + 0x7000);
// buffer for information data to pass to INTY launcher
// 204 bytes per page (20400 bytes allocated allowing for 100 pages)
// 0x7000 + (204 * 100)/2 = 0x97D8

int filefrom = 0, fileto = 0;

int num_dir_entries = 0;         // how many entries in the current directory
int num_info_pages  = 0;         // Total number of infopage
int cur_info_page   = 0;         // Currently displayed infopage

void file_list(SCREEN_ENTRY *en, int from, int to, int num) {
   for (int n = 0; n < (to - from); n++) {
      cart.RAM[ENTRY_TYPE_ADDR + n] = en[n + from].isDir;
      
      for (int i = 0; i < 64; i++) {
         int pos = ENTRY_LIST_ADDR + i + (n * 64);
         cart.RAM[pos] = en[n + from].filename[i];
         if (cart.RAM[pos] < 32)
            // 255 to indicate end of string if shorter than 64
            cart.RAM[pos] = 255;
		   else
            // convert to INTY numbering here (much faster than on INTY side)
            // only ascii chars from 32 to 127 are displayed and are mapped to 0 - 95 in lookup table
			   cart.RAM[pos] = (cart.RAM[pos] & 0x7F) - 32;
      }
   }
   cart.RAM[FFROM_HI_ADDR] = (from & 0xFF00) >> 8;  // MSB
   cart.RAM[FFROM_LO_ADDR] = (from & 0x00FF);       // LSB
   cart.RAM[FTO_HI_ADDR  ] = (to   & 0xFF00) >> 8;  // MSB
   cart.RAM[FTO_LO_ADDR  ] = (to   & 0x00FF);       // LSB
   cart.RAM[FTOT_HI_ADDR ] = (num  & 0xFF00) >> 8;  // MSB
   cart.RAM[FTOT_LO_ADDR ] = (num  & 0x00FF);       // LSB
}

void ChangeDirectory(int entry_num) {
   strcat(curPath, "/");
   strcat(curPath, screen_entries[entry_num].filename);
}

int LoadGame(int entry_num) {

   // get intellivision details for ECS emulation
   tv_mode = cart.RAM[TV_MODE_ADDR];
   ecs_present = cart.RAM[ECS_PRES_ADDR];
   ecs_volume = cart.RAM[ECS_VOL_ADDR];

   int result = load_file_by_id(screen_entries[entry_num].id, curPath);
   if (result != 0) {
      printf("E: failed to load file\n");
      return result;
   }

#if CONFIG_SD_STORAGE
   // save config file only on SD 
   if (volumeId == 1) {

      // save last loaded game to config file
      strcpy(cfg.lastPath, curPath);

      // save ECS audio volume level
#if CONFIG_ECS_AUDIO
      cfg.ecs_volume = 255 - ecs_volume;
#else
      cfg.ecs_volume = 255;
#endif
      printf("save cfg file: %s\n", CONFIG_FILENAME);

      if ( (cfgfile = vfs_open(CONFIG_FILENAME, "w")) != NULL) {
         vfs_write(cfgfile, &cfg, sizeof(struct boardConfig));
         vfs_close(cfgfile);
      } else {
         printf("E: config file %s not written\n", CONFIG_FILENAME);
      }
   }
#endif

   // ROM file has internal cfg info
   if(!is_rom_file(curPath)) {
      if (load_cfg(curPath) > 0) {
         // pokes found in cfg file, apply them now
         apply_pokes(curPath);
      }
   }

   // test
   /*
   mm_init(&m);

   // Cat Attack
   mm_add(&m, 0x10000, 0x1000C, 0x4800, MM_NO_PAGE);
   mm_add(&m, 0x1000D, 0x103AB, 0x4810, MM_NO_PAGE);
   mm_add(&m, 0x103AC, 0x113AB, 0x5000, MM_NO_PAGE);
   mm_add(&m, 0x113AC, 0x123A7, 0x6000, MM_NO_PAGE);
   mm_add(&m, 0x123A8, 0x12A57, 0xA000, MM_NO_PAGE);

   uint16_t addr = 0x4800;
   uint32_t romaddr;

   while (addr <= 0xAFFF) {
      if (mm_lookup(&m, addr, 0, &romaddr))
         printf("R:0x%lX  A:0x%X\n", romaddr, addr);
      else
         printf("R: (n/a)  A:0x%X\n", addr);
      addr++;
   }
   */
   // test

   gpio_put(LED, false);

   sleep_ms(200);
   memset((uint16_t *) cart.RAM, 0, sizeof(cart.RAM));

   // returns 0 to indicate success so that launcher execution stops and actual game is launched
   return 0;
}

void info_list(INFO_ENTRY *en, int page) {
   int pos;
   int sec_pos = 0;
   char section_str[20];
   
   // write section
   switch(en[page].section) {
      case 0: // NONE
         sprintf(section_str, "[NONE]");
         break;
      case 1: // MAPPING
         sprintf(section_str, "[MAPPING]");
         break;
      case 2: // MEMATTR
         sprintf(section_str, "[MEMATTR]");
         break;
      case 3: // VARS
         sprintf(section_str, "[VARS]");
         break;
      case 4: // MACRO
         sprintf(section_str, "[MACRO]");
         break;
      case 5: // MAIN
         sprintf(section_str, "[MAIN]");
         break;
      default:
         sprintf(section_str, "[UNDEFINED]");
         break;
   }

   while(section_str[sec_pos]) {
      pos = SECTION_ADDR + sec_pos;
      cart.RAM[pos] = section_str[sec_pos];
      if (cart.RAM[pos] < 32) {
         cart.RAM[pos] = 0;
      }
      else {
         // convert to INTY numbering here (much faster than on INTY side)
         // only ascii chars from 32 to 127 are displayed and are mapped to 0 - 95 in lookup table
         cart.RAM[pos] = (cart.RAM[pos] & 0x7F) - 32;
      }
      sec_pos++;
   }
   for (pos = SECTION_ADDR + sec_pos; pos < SECTION_ADDR + 21; pos++) {
      cart.RAM[pos] = 0;
   }

   for (int n = 0; n < 10; n++) { 
      pos = INFO_ADDR + n * 19;
      printf("info_list: page %d, line %d: %s\n", page, n, en[page].line[n]);
      for (int i = 0; i < 19; i++) {
         cart.RAM[pos] = en[page].line[n][i];

         if (cart.RAM[pos] < 32)
            // 255 to indicate end of string if shorter than 19
            cart.RAM[pos] = 255;
		   else
            // convert to INTY numbering here (much faster than on INTY side)
            // only ascii chars from 32 to 127 are displayed and are mapped to 0 - 95 in lookup table
			   cart.RAM[pos] = (cart.RAM[pos] & 0x7F) - 32;
         pos++;
      }
      
   }
}

void IntyMenu(int type) {       // 1=start, 2=next page, 3=prev page, 4=dir up

   int path_char = 0;
   
   switch (type) {
      case UP_DIR:
         // go up one level if not in root directory     
         if (!( (strcmp(curPath, "/sd") == 0) || (strcmp(curPath, "/fl") == 0) )) {
            char* curDir = strrchr(curPath,'/');
            if (curDir) {
               int n;
               *curDir = 0; // curPath ends at last slash
               curDir++;    // curDir contains last visited directory

               printf("UpDir : Leaving %s new dir %s\n", curDir, curPath);

               // read new directory
               num_dir_entries = read_directory(curPath, screen_entries);
               if (num_dir_entries < 0) {
                     // could not read directory, show empty list and send info to INTY launcher
                     num_dir_entries = 0;
                     cart.RAM[SDPRES_ADDR] = 0;
               }
               // search for previously visited directory
               for (n = num_dir_entries; n > 0; n--) {
                  if (strcmp(curDir, screen_entries[n-1].filename) == 0) break;
               }
               filefrom = (int)((n-1) / 10) * 10;
               fileto = filefrom + 10;
               if (fileto > num_dir_entries)
                  fileto = num_dir_entries;
               cart.RAM[SELECTION_ADDR] = n - filefrom - 1;
            }
         }
         break;
      case INIT_PAGE:
         num_dir_entries = read_directory(curPath, screen_entries);
         if (num_dir_entries < 0) {
            // could not read directory, show empty list and send info to INTY launcher
            num_dir_entries = 0;
            cart.RAM[SDPRES_ADDR] = 0;
         }
         filefrom = 0;
         fileto = (((10)<(num_dir_entries))?(10):(num_dir_entries)) + filefrom;
         break;
      case NEXT_PAGE:
         if (fileto < num_dir_entries) {
            filefrom = fileto;
            fileto += 10;
            if (fileto > num_dir_entries)
               fileto = num_dir_entries;
         }
         break;
      case PREV_PAGE:
         if (filefrom >= 10) {
            filefrom = filefrom - 10;
            fileto = filefrom + 10;
         }
         break;
      case READ_PAGE:
         // Nothing to be done 
         break;
   }

   // update screen entries with new parameters
   file_list(screen_entries, filefrom, fileto, num_dir_entries);

   // make path available to launcher
   while(curPath[path_char]) {
      int pos = PATH_ADDR + path_char;
      cart.RAM[pos] = curPath[path_char];
      if (cart.RAM[pos] < 32) {
         cart.RAM[pos] = 0;
      }
      else {
         // convert to INTY numbering here (much faster than on INTY side)
         // only ascii chars from 32 to 127 are displayed and are mapped to 0 - 95 in lookup table
         cart.RAM[pos] = (cart.RAM[pos] & 0x7F) - 32;
      }
      path_char++;
   }
   for (int pos = PATH_ADDR + path_char; pos < PATH_ADDR + 21; pos++) {
      cart.RAM[pos] = 0;
   }
}

void RunLauncher() {
   
   // Load launcher into memory
   for (int i = 0; i < (sizeof(mintyfw) / 2); i++) 
      cart.ROM[i] = mintyfw[(i * 2) + 1] | (mintyfw[i * 2] << 8);
   
   printf("load launcher memory map...");

   mm_init(&m);

   mm_add_ram(&m, 0x8000, 0x9FFF, 8);

	if ((sizeof(mintyfw) / 2) < 0x1000) {
      mm_add(&m, 0x0000, (sizeof(mintyfw) / 2)-1, 0x5000, MM_NO_PAGE);
	} else {
      mm_add(&m, 0x0000, 0x0FFF, 0x5000, MM_NO_PAGE);
      mm_add(&m, 0x1000, (sizeof(mintyfw) / 2)-1, 0x6000, MM_NO_PAGE);
   }

   printf(" DONE\n");

   printf("memory map data structure size: %d bytes\n", sizeof(m));

   // initialise exchange RAM data
   cart.RAM[VERSION_MAJOR_ADDR] = VERSION_MAJOR;
   cart.RAM[VERSION_MINOR_ADDR] = VERSION_MINOR;
   cart.RAM[BOARD_ID_ADDR] = BOARD_ID;
   cart.RAM[STATUS_ADDR] = 1;      // block cart access until initialisation is done
   cart.RAM[CMD_ADDR] = 0;
   cart.RAM[MSIZE_HI_ADDR] = ((MAX_ROM_SIZE*2/1024)   & 0xFF00) >> 8;  // MSB
   cart.RAM[MSIZE_LO_ADDR] = ((MAX_ROM_SIZE*2/1024)   & 0x00FF);       // LSB
   cart.RAM[ECS_VOL_ADDR] = ecs_volume;
   cart.RAM[JLP_EMU_ADDR] = CONFIG_JLP;       // JLP emulation available
   cart.RAM[ECS_EMU_ADDR] = CONFIG_ECS_AUDIO; // ECS EMU available

   // Start Launcher on INTY side
   sleep_ms(200);
   resetCart();
#if defined(PIRTO)
   sleep_ms(200);
   resetCart();
#endif
   sleep_ms(1200);

   // set default storage device
#if CONFIG_SD_STORAGE
   strcpy(curPath, "/sd");
#elif (CONFIG_FLASH_FAT_STORAGE || CONFIG_FLASH_LFS_STORAGE)
   strcpy(curPath, "/fl");
#endif

   cart.RAM[DEV_ADDR] = volumeId;
   cart.RAM[SDPRES_ADDR] = 1;

#if CONFIG_SD_STORAGE
   if (volumeId == 1) {

      // try to read configuration file    
      if ( (cfgfile = vfs_open(CONFIG_FILENAME, "r")) != NULL) {

         vfs_read(cfgfile, &cfg, sizeof(struct boardConfig));

         printf("cfg.lastpath: %s\n", cfg.lastPath);

         if (cfg.magicNumber == CONFIG_MAGIC_NUMBER) {

            vfs_stat(cfg.lastPath, &st);
            if (st.type & VFS_TYPE_DIR) {
               strcpy(curPath, cfg.lastPath);
            } else {
               char* curFile = strrchr(cfg.lastPath+1,'/');
               if (curFile) {
                  int n;
                  *curFile = 0; // curPath ends at last slash
                  curFile++;    // curFile contains last launched file
                  strcpy(curPath, cfg.lastPath);

                  // read  directory
                  num_dir_entries = read_directory(curPath, screen_entries);
                  if (num_dir_entries < 0) {
                        // could not read directory, show empty list and send info to INTY launcher
                        num_dir_entries = 0;
                        cart.RAM[SDPRES_ADDR] = 0;
                  }
                  // search for previously launched file
                  for (n = num_dir_entries; n > 0; n--) {
                     if (strcmp(curFile, screen_entries[n-1].filename) == 0) break;
                  }
                  filefrom = (int)((n-1) / 10) * 10;
                  fileto = filefrom + 10;
                  if (fileto > num_dir_entries)
                     fileto = num_dir_entries;
                  cart.RAM[SELECTION_ADDR] = n - filefrom - 1;
               }
            }

            ecs_volume = 255 - cfg.ecs_volume; 
            cart.RAM[ECS_VOL_ADDR] = ecs_volume;
         }

         vfs_close(cfgfile);
      }
   }
   cart.RAM[HAS_SD_ADDR] = 1;
#else
   cart.RAM[HAS_SD_ADDR] = 0;
#endif

   // Initialise Display list
   if (filefrom)
      IntyMenu(READ_PAGE);
   else
      IntyMenu(INIT_PAGE);

   printf("max size of ROM file: %d bytes\n", MAX_ROM_SIZE*2);
  
   cart.RAM[STATUS_ADDR] = 0;      // release welcome screen
   gpio_put(LED, true);

   while (1) {
      char cmd = cart.RAM[CMD_ADDR];

      if (cmd > 0) {

         cart.RAM[STATUS_ADDR] = 1;
         cart.RAM[CMD_ADDR] = 0;
         cart.RAM[ERROR_ADDR] = 0;

         printf("Received command %d\n", cmd);

         switch (cmd) {
            case 1:            // initialise file list with current path
               IntyMenu(INIT_PAGE);
               break;
            case 2:            // select entry
               {
                  int entry_num = cart.RAM[SELECTION_ADDR] + filefrom - 1;

                  if (screen_entries[entry_num].isDir) {  // directory
                     ChangeDirectory(entry_num);
                     IntyMenu(INIT_PAGE);
                  } else {
                     int result = LoadGame(entry_num);
                     if (result == 0) {
                        // game loaded return from launcher to actually run it
                        return;
                     } else {
                        // loading game failed => tell inty launcher to show error message
                        cart.RAM[ERROR_ADDR] = -result;   // error codes are negative, convert to positive for launcher
                     }
                  }
               }
               break;
            case 3:            // next page
               IntyMenu(NEXT_PAGE);
               break;
            case 4:            // prev page
               IntyMenu(PREV_PAGE);
               break;
            case 5:            // up dir
               IntyMenu(UP_DIR);
               break;
#if CONFIG_SD_STORAGE && (CONFIG_FLASH_FAT_STORAGE || CONFIG_FLASH_LFS_STORAGE)
            case 6:            // change storage device
               volumeId = cart.RAM[DEV_ADDR];
               if (volumeId == 0)
                  strcpy(curPath, "/fl");
               else if(volumeId == 1)
                  strcpy(curPath, "/sd");
               IntyMenu(1);
               break;
#endif
            case 7:       // show info for selected entry
               {
                  int entry_num = cart.RAM[SELECTION_ADDR] + filefrom - 1;

                  num_info_pages = 0; // reset number of info entries
                  cur_info_page  = 0;

                  if (!(screen_entries[entry_num].isDir)) {
                     num_info_pages = collect_info_by_id(screen_entries[entry_num].id, curPath, info_entries);
                     if (num_info_pages > 0) {
                        // successfully parsed info entries for the selected game, now make them available to launcher
                        info_list(info_entries, 0);
                     }
                     if (num_info_pages < 0) {
                        num_info_pages = 0;   // in case of error set number of info pages to 0 to avoid showing wrong info
                     }
                  }
                  else {
                     printf("E: Information requested for directory entry (%d)\n",entry_num);
                  }

                  cart.RAM[INFO_DISP_ADDR] = cur_info_page;   // default to show first info entry in the list
                  cart.RAM[INFO_NUM_ADDR] = num_info_pages;   // now launcher can display the vars entries
               }
               break;
            case 8:       // show next info page
               if (cur_info_page < num_info_pages-1) {
                  cart.RAM[INFO_DISP_ADDR] = ++cur_info_page;
                  info_list(info_entries, cur_info_page);
               }
               break;
            case 9:       // show prev info page
               if (cur_info_page > 0) {
                  cart.RAM[INFO_DISP_ADDR] = --cur_info_page;
                  info_list(info_entries, cur_info_page);
               }
               break;
            default:
               printf("E: unknown cmd\n");
               break;
         }
         cart.RAM[STATUS_ADDR] = 0;
      }

#if CONFIG_USB_DEVICE
      tud_task();
      cdc_task();
#endif
   }
}
