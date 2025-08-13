/*
//   Minty - MultiCart for Mattel Intellivision by Gennaro Tortone 2025
//
//   based on PiRTO II Flash MultiCART by Andrea Ottaviani 2024
//   parts of code are directly from the A8PicoCart project by Robin Edwards 2023
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/divider.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "rom.h"
#include "memory.h"

#include "tusb.h"
#include "ff.h"
#include "fatfs_disk.h"
#include "fingerprints.h"
#include "board.h"

unsigned char busLookup[8];

#define BINLENGTH 1024*64
#define RAMSIZE   0x2000

uint16_t ROM[BINLENGTH];
uint16_t RAM[RAMSIZE];

#define maxHacks 32
uint16_t HACK[maxHacks];
uint16_t HACK_CODE[maxHacks];

char curPath[256] = "";
char path[512];

int volumeId = 0;    // flash
unsigned char files[256 * 64] = {0};

int filefrom = 0, fileto = 0;
volatile char cmd = 0;

unsigned int parallelBus2;

extern unsigned int romLen;
extern unsigned int ramfrom;
extern unsigned int ramto;
extern unsigned int mapfrom[80];
extern unsigned int mapto[80];
extern unsigned int maprom[80];
extern int mapdelta[80];
extern unsigned int mapsize[80];
extern unsigned int addrto[80];
extern unsigned int RAMused;
extern unsigned int type[80];          // 0-rom / 1-page / 2-ram
extern unsigned int page[80];          // page number

extern int slot;
extern int hacks;

int base = 0x17f;

FATFS FatFs;

void resetCart() {
   gpio_init(MSYNC_PIN);
   gpio_set_dir(MSYNC_PIN, false);
   gpio_set_pulls(MSYNC_PIN, false, true);
   gpio_put(LED_PIN, false);
   resetHigh();
   sleep_ms(30);                // was 20 for Model II; 30 works for both
   resetLow();
   //sleep_ms(3);  // was 2 for Model II; 3 works for both

   //while ((gpio_get(MSYNC_PIN)==1)) ;

   sleep_ms(1);                 // was 1 for Model II; 
   resetHigh();
   gpio_put(LED_PIN, true);
}

/*
 Theory of Operation
 -------------------
 Inty sends command to mcu on cart by writing to 50000 (CMD), 50001 (parameter) and menu (50002-50641) 
 Inty must be running from RAM when it sends a command, since the mcu on the cart will
 go away at that point. Inty polls 50001 until it reads $1.
*/

void __not_in_flash_func(core1_main()) {
   unsigned int lastBusState, busState1;
   unsigned int parallelBus;
   unsigned int dataOut;
   unsigned int dataWrite = 0;
   unsigned char busBit;
   bool deviceAddress = false;
   unsigned int curPage = 0;
   unsigned int checpage = 0;

   multicore_lockout_victim_init();

   sleep_ms(480);

   busState1 = BUS_NACT;
   lastBusState = BUS_NACT;

   dataOut = 0;

   gpio_set_dir_in_masked(ALWAYS_IN_MASK);
   gpio_set_dir_out_masked(ALWAYS_OUT_MASK);

   // Initial conditions
   SET_DATA_MODE_IN;

   while (1) {
      // Wait for the bus state to change

      do {
      } while (!((gpio_get_all() ^ lastBusState) & BUS_STATE_MASK));
      // We detected a change, but reread the bus state to make sure that all three pins have settled
      lastBusState = gpio_get_all();

      busState1 = ((lastBusState & BUS_STATE_MASK) >> BDIR_PIN);        //if gpio9    

      busBit = busLookup[busState1];
      // Avoiding switch statements here because timing is critical and needs to be deterministic
      if (!busBit) {
         // -----------------------
         // DTB
         // -----------------------
         // DTB needs to come first since its timing is critical.  The CP-1600 expects data to be
         // placed on the bus early in the bus cycle (i.e. we need to get data on the bus quickly!)
         if (deviceAddress) {
            // The data was prefetched during BAR/ADAR.  There isn't nearly enough time to fetch it here.
            // We can just output it.
            SET_DATA_MODE_OUT;
            gpio_put_masked(DATA_PIN_MASK, dataOut);
            asm inline("nop;nop;nop;nop;");

            // while ((gpio_get_all() & BC1_PIN_MASK)); // wait while bc1 & bc2 are high... it's enough test BC1
            while (((gpio_get_all() & BC1e2_PIN_MASK) >> BC2_PIN) == 3) ;
            //asm inline (delWR); //150ns


            SET_DATA_MODE_IN;
         }
      } else {
         busBit >>= 1;
         if (!busBit) {
            // -----------------------
            // BAR, ADAR
            // -----------------------
            if (busState1 == BUS_ADAR) {
               if (deviceAddress) {
                  // The data was prefetched during BAR/ADAR.  There isn't nearly enough time to fetch it here.
                  // We can just output it.
                  SET_DATA_MODE_OUT;
                  gpio_put_masked(DATA_PIN_MASK, dataOut);

                  while ((gpio_get_all() & BC1_PIN_MASK) >> BC1_PIN) ;  //wait BC1 go down 
                  //asm inline (delWR); //150ns

                  SET_DATA_MODE_IN;

               }
            }
            /// ELSE is BAR   
            // Prefetch data here because there won't be enough time to get it during DTB.
            // However, we can't take forever because of all the time we had to wait for
            // the address to appear on the bus.
            SET_DATA_MODE_IN;
            // We have to wait until the address is stable on the bus
            // waiting bus is stable 66 nop at 200mhz is ok/85 at 240

            while (((parallelBus = gpio_get_all()) & BDIR_PIN_MASK)) ;  // wait DIR go low for finish BAR cycle 
            //asm inline (delRD); //150ns

            parallelBus = gpio_get_all() & 0xFFFF;

            deviceAddress = false;

            // Load data for DTB here to save time
            for (int8_t i = 0; i <= slot; i++) {
               if ((parallelBus - maprom[i]) <= mapsize[i]) {
                  if (type[i] == 0) {
                     dataOut = ROM[(parallelBus - mapdelta[i])];
                     deviceAddress = true;
                     break;
                  }
                  if (type[i] == 1) {
                     if (page[i] == curPage) {
                        dataOut = ROM[(parallelBus - mapdelta[i])];
                        deviceAddress = true;
                        break;
                     }
                     if ((parallelBus & 0xfff) == 0xfff) {
                        checpage = 1;
                        deviceAddress = true;
                        break;
                     }
                  }
                  if (type[i] == 2) {
                     dataOut = RAM[parallelBus - ramfrom];
                     deviceAddress = true;
                     break;
                  }
               }
            }

            if (hacks > 0) {
               for (int i = 0; i < maxHacks; i++) {
                  if (parallelBus == HACK[i]) {
                     dataOut = HACK_CODE[i];
                     deviceAddress = true;
                  }
                  break;
               }
            }
         } else {
            busBit >>= 1;
            if (!busBit) {
               // -----------------------
               // DWS
               // -----------------------

               if (deviceAddress) {

                  SET_DATA_MODE_IN;

                  dataWrite = gpio_get_all() & 0xFFFF;

                  if (RAMused == 1) {
                     RAM[parallelBus - ramfrom] = dataWrite & 0xFF;
                  }
                  if ((checpage == 1) && (((dataWrite >> 4) & 0xff) == 0xA5)) {
                     curPage = dataWrite & 0xf;
                     checpage = 0;
                  }

               } else {
                  // -----------------------
                  // NACT, IAB, DW, INTAK
                  // -----------------------
                  // reconnect to bus
                  parallelBus2 = parallelBus;
                  SET_DATA_MODE_IN;
               }

            }
         }
      }
   }
}

void error(int numblink) {
   while (1) {
      gpio_set_dir(25, GPIO_OUT);

      for (int i = 0; i < numblink; i++) {
         gpio_put(25, true);
         sleep_ms(600);
         gpio_put(25, false);
         sleep_ms(500);
      }
      sleep_ms(2000);
   }
}

typedef struct {
   char isDir;
   char filename[13];
   char long_filename[255];
} DIR_ENTRY;                    // 269 bytes = 256 entries in ~68k

int num_dir_entries = 0;        // how many entries in the current directory

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

int is_valid_file(char *filename) {
   char *ext = get_filename_ext(filename);

   if (strcasecmp(ext, "BIN") == 0 || strcasecmp(ext, "INT") == 0)
      return 1;
   return 0;
}

FILINFO fno;

int read_directory(char *path) {
   int ret = 0;

   num_dir_entries = 0;
   DIR_ENTRY *dst = (DIR_ENTRY *) & files[0];

   DIR dir;
   FRESULT fr;

   if ((fr = f_opendir(&dir, path)) == FR_OK) {
      while (num_dir_entries < 64) {
         if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
            break;
         if (fno.fattrib & (AM_HID | AM_SYS))
            continue;
         dst->isDir = fno.fattrib & AM_DIR ? 1 : 0;
         if (!dst->isDir)
            if (!is_valid_file(fno.fname))
               continue;
         // copy file record to first ram block
         // long file name
         strcpy(dst->long_filename, fno.fname);
         dst->long_filename[strlen(fno.fname)] = 0;
         // 8.3 name
         if (fno.altname[0])
            strcpy(dst->filename, fno.altname);
         else {              // no altname when lfn is 8.3
            strncpy(dst->filename, fno.fname, 12);
            dst->filename[12] = 0;
         }
         dst++;
         num_dir_entries++;
      }
      f_closedir(&dir);
   } 

   qsort((DIR_ENTRY *) & files[0], num_dir_entries, sizeof(DIR_ENTRY), entry_compare);
   ret = 1;
   
   return ret;
}

int load_file(char *filename) {
   UINT br, size = 0;
   unsigned char byteread[2];
   FIL fil;

   if (f_open(&fil, filename, FA_READ) != FR_OK) {
      printf("load_file %s error !\n", filename);
      error(2);
      return 0;
   }

   int bytes_to_read = 2;

   // clean ROM space
   memset(ROM, 0, BINLENGTH);

   // read the file to SRAM
   size = 0;
   while (!(f_eof(&fil))) {
      f_read(&fil, byteread, bytes_to_read, &br);
      ROM[size] = byteread[1] | (byteread[0] << 8);
      size++;
   }

 closefile:
   romLen = size;
   RAM[base + 202] = romLen;
   f_close(&fil);

   printf("load_file: size: %d\n", romLen);

   return br;
}

void load_cfg(char *filename) {
   char riga[80];
   char tmp[80] = {0};
   char cfgfile[80] = {0};
   int linepos;
   FIL fil;

   char *dot = strchr(filename, '.');
   strncpy(cfgfile, filename, (dot - filename));
   strcat(cfgfile, ".cfg");

   // config file not available, try to config memory using fingerprint
   if (f_open(&fil, cfgfile, FA_READ) != FR_OK) {
      int fp = 0;

      for (int i = 0; i < 128; i++)
         fp += ((ROM[i] & 0xFF00) >> 8) + (ROM[i] & 0x00FF);

      printf("filename: %s, fp: %d\n", filename, fp);

      for (int i=0; i<sizeof(fingerprints)/sizeof(int); i += 2) {
         if (fp == fingerprints[i]) {
            if (fp == 11349) {
               // Baseball or MTE Test Cart?
               if (romLen > 8192)
                  config_memory(8);
               else
                  config_memory(0);
               return;
            }

            config_memory(fingerprints[i+1]);
            return;
         }
      }
   }

   // read config file to SRAM
   slot = 0;

   hacks = 0;
   RAMused = 0;

   while (!(f_eof(&fil))) {
      memset(riga, 0, sizeof(riga));
      f_gets(riga, 79, &fil);

      if (riga[0] >= 32) {
         memset(tmp, 0, sizeof(tmp));
         memcpy(tmp, riga, 9);
         if (slot == 0) {
            if (!(strcmp(tmp, "[mapping]"))) {
               memset(riga, 0, sizeof(riga));
               f_gets(riga, 79, &fil);
            } else {
               error(4);        // 3 error [mapping] section not found
            }
         }
         if (!(strcmp(tmp, "[memattr]"))) {
            memset(riga, 0, sizeof(riga));
            f_gets(riga, 79, &fil);
            memset(tmp, 0, sizeof(tmp));
            memcpy(tmp, riga + 1, 5);
            ramfrom = strtoul(tmp, NULL, 16);
            mapfrom[slot] = ramfrom;
            memset(tmp, 0, sizeof(tmp));
            memcpy(tmp, riga + 9, 5);
            ramto = strtoul(tmp, NULL, 16);
            mapto[slot] = ramto;
            maprom[slot] = ramfrom;
            addrto[slot] = maprom[slot] + (mapto[slot] - mapfrom[slot]);
            type[slot] = 2;     // RAM
            RAMused = 1;
            mapdelta[slot] = maprom[slot] - mapfrom[slot];
            mapsize[slot] = mapto[slot] - mapfrom[slot];
            slot++;
         } else {
            memset(tmp, 0, sizeof(tmp));
            memcpy(tmp, riga, 1);
            if (!strcmp(tmp, "p")) {
               // [MACRO]
               memset(tmp, 0, sizeof(tmp));
               memcpy(tmp, riga + 2, 4);
               HACK[hacks] = strtoul(tmp, NULL, 16);
               memset(tmp, 0, sizeof(tmp));
               memcpy(tmp, riga + 7, 4);
               HACK_CODE[hacks] = strtoul(tmp, NULL, 16);
               hacks++;
            } else {
               //mapping
               linepos = strcspn(riga, "-");
               if ((linepos >= 0) && (riga[linepos] == '-')) {
                  memset(tmp, 0, sizeof(tmp));
                  memcpy(tmp, riga + 1, 4);
                  mapfrom[slot] = strtoul(tmp, NULL, 16);
                  if (linepos == 6) {
                     memset(tmp, 0, sizeof(tmp));
                     memcpy(tmp, riga + (linepos + 3), 4);
                     mapto[slot] = strtoul(tmp, NULL, 16);
                     memset(tmp, 0, sizeof(tmp));
                     memcpy(tmp, riga + (linepos + 11), 4);
                     maprom[slot] = strtoul(tmp, NULL, 16);
                  } else {
                     memset(tmp, 0, sizeof(tmp));
                     memcpy(tmp, riga + (linepos + 3), 5);
                     mapto[slot] = strtoul(tmp, NULL, 16);
                     memset(tmp, 0, sizeof(tmp));
                     memcpy(tmp, riga + (linepos + 12), 5);
                     maprom[slot] = strtoul(tmp, NULL, 16);
                  }
                  addrto[slot] = maprom[slot] + (mapto[slot] - mapfrom[slot]);
                  linepos = strcspn(riga, "P");
                  if ((linepos > 0) && (riga[linepos] != 0)) {
                     type[slot] = 1;
                     memset(tmp, 0, sizeof(tmp));
                     memcpy(tmp, riga + (linepos + 5), 2);
                     page[slot] = strtoul(tmp, NULL, 16);

                  } else {
                     type[slot] = 0;
                  }
                  slot++;
               }
            }
         }
         mapdelta[slot - 1] = maprom[slot - 1] - mapfrom[slot - 1];
         mapsize[slot - 1] = mapto[slot - 1] - mapfrom[slot - 1];
      }
   }
   slot = slot - 1;

   f_close(&fil);

   return;
}

void filelist(DIR_ENTRY *en, int da, int a) {
   char longfilename[255];
   char tmp[32];

   int base = 0x17f;

   for (int i = 0; i < 20 * 20; i++)
      RAM[base + i * 2] = 0;
   for (int n = 0; n < (a - da); n++) {
      memset(longfilename, 0, 32);
      memset(tmp, 0, 32);
      if (en[n + da].isDir) {
         RAM[0x1000 + n] = 1;
         strcat(longfilename, en[n + da].long_filename);
      } else {
         RAM[0x1000 + n] = 0;
         strcpy(longfilename, en[n + da].long_filename);
         char *dot = strchr(longfilename, '.');
         if((dot != NULL) && (dot-longfilename <= 27)) {
            strncpy(tmp, longfilename, (dot - longfilename));
            memset(longfilename, 0, 32);
            strcpy(longfilename, tmp);
         } else {
            strncpy(longfilename, en[n + da].long_filename, 32);
            longfilename[31] = 0;
         }
      }

      for (int i = 0; i < 20; i++) {
         RAM[base + i * 2 + (n * 40)] = longfilename[i];
         if ((RAM[base + i * 2 + (n * 40)]) <= 20)
            RAM[base + i * 2 + (n * 40)] = 32;
      }
   }
   RAM[0x1030] = da;
   RAM[0x1031] = a;
   RAM[0x1032] = num_dir_entries;
}

void IntyMenu(int type) {       // 1=start, 2=next page, 3=prev page, 4=dir up
   int maxfile = 0;
   int ret = 0;
   
   if (volumeId == 0)
      mount_fatfs_disk();

   printf("Mounting %s...\n", curPath);

   if (f_mount(&FatFs, curPath, 1) != FR_OK) {
      printf("E: mount %s failed\n", curPath);
   } else printf("I: mount %s ok\n", curPath);

   switch (type) {
      case 1:
         ret = read_directory(curPath);
         if (!(ret))
            error(1);
         maxfile = 10;
         filefrom = 0;
         if (maxfile > num_dir_entries)
            maxfile = num_dir_entries;
         fileto = filefrom + maxfile;
         break;
      case 2:
         if (fileto < num_dir_entries) {
            maxfile = 10;
            if ((fileto + maxfile) > num_dir_entries)
               maxfile = num_dir_entries - fileto;
            filefrom = fileto;
            fileto = filefrom + maxfile;
         }
         break;
      case 3:
         if (filefrom >= 10) {
            filefrom = filefrom - 10;
            fileto = filefrom + 10;
         }
         break;
   }

   filelist((DIR_ENTRY *) & files[0], filefrom, fileto);
}

void DirUp() {
   int len = strlen(curPath);

   if(len == 3)      // e.g. "0:/"
      return;

   if (len > 0) {
      while (len && curPath[--len] != '/') ;
      curPath[len] = 0;
   }
}

#pragma GCC push_options
#pragma GCC optimize ("O0")
void LoadGame() {
   int numfile = 0;
   char longfilename[255];

   numfile = RAM[0x899] + filefrom - 1;

   DIR_ENTRY *entry = (DIR_ENTRY *) & files[0];

   strcpy(longfilename, entry[numfile].long_filename);

   if (entry[numfile].isDir) {  // directory
      strcat(curPath, "/");
      strcat(curPath, entry[numfile].filename);
      IntyMenu(1);
   } else {
      memset(path, 0, sizeof(path));
      strcat(path, curPath);
      strcat(path, "/");
      strcat(path, longfilename);

      load_file(path);      // load rom in files[]
      load_cfg(path);

      gpio_put(LED_PIN, false);

      sleep_ms(200);
      resetCart();              // start game !
      memset(RAM, 0, sizeof(RAM));
      while (1) {
         gpio_put(LED_PIN, true);
         sleep_ms(2000);
         gpio_put(LED_PIN, false);
         sleep_ms(2000);
      }
   }
}

void Inty_cart_main() {
   printf("Inty_cart_main\n");

   // Initialize the bus state variables
   busLookup[BUS_NACT] = 4;     // 100
   busLookup[BUS_BAR] = 1;      // 001
   busLookup[BUS_IAB] = 4;      // 100
   busLookup[BUS_DWS] = 2;      // 010   // test without dws handling
   busLookup[BUS_ADAR] = 1;     // 001
   busLookup[BUS_DW] = 4;       // 100
   busLookup[BUS_DTB] = 0;      // 000
   busLookup[BUS_INTAK] = 4;    // 100

   multicore_launch_core1(core1_main);

   gpio_init_mask(ALWAYS_OUT_MASK);
   gpio_init_mask(DATA_PIN_MASK);
   gpio_init_mask(BUS_STATE_MASK);
   gpio_set_dir_in_masked(ALWAYS_IN_MASK);
   gpio_set_dir_out_masked(ALWAYS_OUT_MASK);
   gpio_init(LED_PIN);
   gpio_put(LED_PIN, true);
   gpio_init(RST_PIN);

   gpio_set_dir(MSYNC_PIN, GPIO_IN);
   gpio_pull_down(MSYNC_PIN);

   sleep_ms(800);

   resetHigh();
   sleep_ms(30);
   resetLow();
   printf("Inty Pow-ON\n");

   gpio_put(LED_PIN, true);
   memset(ROM, 0, BINLENGTH);

   for (int i = 0; i < (sizeof(mintyfw) / 2); i++) {
      ROM[i] = mintyfw[(i * 2) + 1] | (mintyfw[i * 2] << 8);
   }
   memset(RAM, 0, sizeof(RAM));

   for (int i = 0; i < maxHacks; i++) {
      HACK[i] = 0;
      HACK_CODE[i] = 0;
   }

   //  [mapping]
   //$0000 - $0FFF = $5000
   mapfrom[0] = 0x0;
   mapto[0] = 0xfff;
   maprom[0] = 0x5000;
   type[0] = 0;
   page[0] = 0;
   addrto[0] = 0x5fff;
   mapdelta[0] = maprom[0] - mapfrom[0];
   mapsize[0] = mapto[0] - mapfrom[0];

   //$1000 - $10B8 = $6000
   mapfrom[1] = 0x1000;
   mapto[1] = 0x1fff;
   maprom[1] = 0x6000;
   type[1] = 0;
   page[1] = 0;
   addrto[1] = 0x6fff;
   mapdelta[1] = maprom[1] - mapfrom[1];
   mapsize[1] = mapto[1] - mapfrom[1];

   //[memattr]
   //$8000 - $9FFF = RAM 16
   RAMused = 1;
   ramfrom = 0x8000;
   mapfrom[2] = 0x8000;
   mapto[2] = 0x9fff;
   maprom[2] = 0x8000;
   type[2] = 2;
   page[2] = 0;
   addrto[2] = 0x9fff;
   mapdelta[2] = maprom[2] - mapfrom[2];
   mapsize[2] = mapto[2] - mapfrom[2];

   slot = 2;

   sleep_ms(200);
   resetCart();
   sleep_ms(1200);

   RAM[CMD_ADDR] = 0;
   IntyMenu(1);
   sleep_ms(800);
   
   // initial conditions 
   sprintf(curPath, "%d:/", volumeId);
   RAM[DEV_ADDR] = volumeId;

   RAM[DONE_ADDR] = 123;
   gpio_put(LED_PIN, true);

   IntyMenu(1);
   
   bool cmd_executing = false;

   while (1) {
      cmd = RAM[CMD_ADDR];

      if ((cmd > 0) && !(cmd_executing)) {
         cmd_executing = true;
         RAM[DONE_ADDR] = 0;
         RAM[CMD_ADDR] = 0;
         printf("cmd: %d\n", cmd);
         switch (cmd) {
            case 1:            // read file list
               IntyMenu(1);
               break;
            case 2:            // run file list
               LoadGame();
               break;
            case 3:            // next page
               IntyMenu(2);
               break;
            case 4:            // prev page
               IntyMenu(3);
               break;
            case 5:            // up dir
               DirUp();
               IntyMenu(1);
               break;
            case 6:            // change storage device
               volumeId = RAM[DEV_ADDR];
               sprintf(curPath, "%d:/", volumeId);
               IntyMenu(1);
               break;
         }
         cmd_executing = false;
         RAM[DONE_ADDR] = 1;
      }
   }
}
#pragma GCC pop_options
