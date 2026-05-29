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
char filename[32];
unsigned int br, bw;

struct boardConfig cfg = {
   .version = CONFIG_VERSION,
   .magicNumber = CONFIG_MAGIC_NUMBER 
}; 
#endif

extern Cartridge cart;     // main data structure for cart emulation

extern struct mapEntry slots[NSLOTS];
extern struct mapHole holes[NSLOTS];
extern struct memHack hacks[MAX_HACKS_NUM];

char curPath[512] = "";
char fullpath[512] = "";

#if CONFIG_FLASH_FAT_STORAGE
int volumeId = 0;    // default flash storage
#else
int volumeId = 1;    // default SD storage
#endif

// recycle cartridge ROM to allocate temporary buffers
// start from 0x4000 to allow launcher ROM size <= 16Kb
SCREEN_ENTRY *screen_entries = (SCREEN_ENTRY *) (cart.ROM + 0x2000);

int filefrom = 0, fileto = 0;
volatile char cmd = 0;

int num_dir_entries = 0;         // how many entries in the current directory

void ChangeDirectory(int entry_num) {
   strcat(curPath, "/");
   strcat(curPath, screen_entries[entry_num].filename);
}

int LoadGame(int entry_num) {

#if CONFIG_SD_STORAGE
   // save config file only on SD 
   if (volumeId == 1) {
      
      // save last path to config file
      strcpy(cfg.lastPath, curPath);

      sprintf(filename, "/sd/%s", CONFIG_FILENAME);
      printf("save cfg file: %s\n", filename);

      if ( (cfgfile = vfs_open(filename, "w")) != NULL) {
         vfs_write(cfgfile, &cfg, sizeof(struct boardConfig));
         vfs_close(cfgfile);
      } else {
         printf("E: config file %s not written\n", filename);
      }
   }
#endif

   load_file_by_id(screen_entries[entry_num].id, curPath, fullpath);

   // ROM file has internal cfg info
   if(!is_rom_file(fullpath))
      load_cfg(fullpath);

   for (int i=0; i<getHacksNum(); i++) {
      uint32_t romaddr;
      mapType type;
      mapAddress(hacks[i].address, 0, &romaddr, &type);
      cart.ROM[romaddr] = hacks[i].value;
   }

   gpio_put(LED, false);

   sleep_ms(200);
   memset((uint16_t *) cart.RAM, 0, sizeof(cart.RAM));

   // returns 1 to indicate success so that launcher execution stops and actual game is launched
   return 1;
}

void IntyMenu(int type) {       // 1=start, 2=next page, 3=prev page, 4=dir up

   int path_char = 0;
   
   switch (type) {
      case UP_DIR:
         // go up one level if not in root directory     
         if (!( (strcmp(curPath, "/sd") == 0) || (strcmp(curPath, "/fl") == 0) )) {
            int len = strlen(curPath);
            if (len > 0) {
               while (len && curPath[--len] != '/') ;
               curPath[len] = 0;
            }
         }
         // and fell into new page read
      case READ_PAGE:
         num_dir_entries = read_directory(curPath, screen_entries);
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
   }

   // update screen entries with new parameters
   filelist(screen_entries, filefrom, fileto, num_dir_entries);

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
   for (int pos = PATH_ADDR + path_char; pos < PATH_ADDR + 20; pos++) {
      cart.RAM[pos] = 0;
   }
}

void RunLauncher() {
   // Load launcher into memory
   for (int i = 0; i < (sizeof(mintyfw) / 2); i++) 
      cart.ROM[i] = mintyfw[(i * 2) + 1] | (mintyfw[i * 2] << 8);
   
   // Configure card
   cleanSlots();
   cleanHoles();
   cleanHacks();

	if ((sizeof(mintyfw) / 2) < 0x1000) {
		addSlot(0x0000, (sizeof(mintyfw) / 2)-1, 0x5000, 0, ROM_SLOT);
	}
	else {
		addSlot(0x0000, 0x0FFF, 0x5000, 0, ROM_SLOT);
		addSlot(0x1000, (sizeof(mintyfw) / 2)-1, 0x6000, 0, ROM_SLOT);
   }
	addSlot(0x8000, 0x8FFF, 0, 0, RAM8_SLOT);
   addSlot(0x8000, 0x9FFF, 0, 0, RAM8_SLOT);
   getRAMRange(&cart.ramfrom, &cart.ramto, &cart.ramwidth);

   // initialise exchange RAM data
   cart.RAM[BOARD_ID_ADDR] = BOARD_ID;
   cart.RAM[STATUS_ADDR] = 1;      // block cart access until initialisation is done
   cart.RAM[CMD_ADDR] = 0;

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

#if CONFIG_SD_STORAGE
   if (volumeId == 1) {
      // try to read configuration file
      sprintf(filename, "/sd/%s", CONFIG_FILENAME);
      
      if ( (cfgfile = vfs_open(filename, "r")) != NULL) {

         vfs_read(cfgfile, &cfg, sizeof(struct boardConfig));

         printf("cfg.lastpath: %s\n", cfg.lastPath);

         if (cfg.magicNumber == CONFIG_MAGIC_NUMBER) {

            vfs_stat(cfg.lastPath, &st);
            if (st.type & VFS_TYPE_DIR) 
               strcpy(curPath, cfg.lastPath);
         }

         vfs_close(cfgfile);
      }
   } 
#endif

   cart.RAM[DEV_ADDR] = volumeId;

   // initial conditions 
#if CONFIG_SD_STORAGE
   cart.RAM[HAS_SD_ADDR] = 1;
#else
   cart.RAM[HAS_SD_ADDR] = 0;
#endif

   // Initialise display list
   IntyMenu(READ_PAGE);

   printf("max size of ROM file: %d bytes\n", BINLENGTH*2);
  
   cart.RAM[STATUS_ADDR] = 0;      // release welcome screen
   gpio_put(LED, true);

   while (1) {
      cmd = cart.RAM[CMD_ADDR];

      if (cmd > 0) {

         cart.RAM[STATUS_ADDR] = 1;
         cart.RAM[CMD_ADDR] = 0;
         printf("cmd: %d\n", cmd);

         switch (cmd) {
            case 1:            // intialise file list with current path
               IntyMenu(READ_PAGE);
               break;
            case 2:            // select entry
               {
                  int entry_num = cart.RAM[SELECTION_ADDR] + filefrom - 1;;

                  if (screen_entries[entry_num].isDir) {  // directory
                     ChangeDirectory(entry_num);
                     IntyMenu(READ_PAGE);
                  } else { 
                     if (LoadGame(entry_num) == 1) {
                        // game loaded return from launcher to actually run it
                        return;
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
         }
         cart.RAM[STATUS_ADDR] = 0;
      }

#if CONFIG_USB_DEVICE
      tud_task();
      cdc_task();
#endif
   }
}