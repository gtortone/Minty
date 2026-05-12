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
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/rand.h"

#include "rom.h"
#include "memory.h"

#include "ff.h"
#include "f_util.h"
#include "fatfs_disk.h"
#include "fingerprints.h"
#include "interface.h"
#include "usb_tasks.h"
#include "utils.h"

unsigned char busLookup[8];

#if PICO_RP2040
   #define BINLENGTH 1024*59     // 120 kb
   #define RAMSIZE   0x2000
#elif PICO_RP2350
   #define BINLENGTH 1024*205    // ~420 kb
   #define RAMSIZE   0x4000
#endif

uint16_t ROM[BINLENGTH];
volatile uint16_t RAM[RAMSIZE];

#define MAX_HACKS_NUM   32

struct memHack {
   uint16_t address;
   uint8_t value;
};

struct memHack hacks[MAX_HACKS_NUM];
uint8_t numhacks = 0;

char curPath[256] = "";
char path[512];

int volumeId = 0;    // flash
unsigned char files[512 * 24] = {0};

int filefrom = 0, fileto = 0;
volatile char cmd = 0;

typedef struct {
   UINT id;
   char isDir;
   char long_filename[21];       // limit filename to 20 chars for Inty display
} DIR_ENTRY;                     // 24 bytes = 256 entries in ~6kb

int num_dir_entries = 0;         // how many entries in the current directory
char fullpath[512];              // full path of current file

volatile uint16_t addrInCopy;

typedef enum {
   NONE,
   MAPPING,
   MEMATTR,
   VARS,
   MACRO
} cfgSection;

uint32_t romLen;
extern uint16_t ramfrom;
extern uint16_t ramto;
extern uint8_t ramwidth;

bool JLPSupport = false;
bool JLPFlash = false;
uint8_t JLPFlashSize = 0;  // number of 1.5KB JLP flash sectors
bool JLPAccel = false;

#define JLP_FEATURE_ACCEL(status)   (value & (1U << 0))
#define JLP_FEATURE_FLASH(status)   (value & (1U << 1))

bool pagingOn = false;

volatile uint32_t romaddr;
volatile uint8_t idx;

extern struct mapEntry slots[NSLOTS];

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

__attribute__((optimize("O3")))
void __time_critical_func(core1_main()) {
   volatile unsigned int lastBusState, busState;
   volatile uint16_t addrIn;
   volatile uint16_t dataOut;
   volatile uint32_t dataWrite = 0;
   volatile unsigned char busBit;
   volatile bool deviceAddress = false;
   uint8_t curPageArr[16];        
   volatile uint8_t seg = 0;
   volatile uint16_t crc = 0;

   sleep_ms(480);

   busState = BUS_NACT;
   lastBusState = BUS_NACT;

   dataOut = 0;

   gpio_set_dir_in_masked(ALWAYS_IN_MASK);
   gpio_set_dir_out_masked(ALWAYS_OUT_MASK);

   // Initial conditions
   SET_DATA_MODE_IN;
   memset(curPageArr, 0, sizeof(curPageArr));

   while (1) {
      // Wait for the bus state to change

      do {
      } while (!((sio_hw->gpio_in ^ lastBusState) & BUS_STATE_MASK));
      
      // We detected a change, but reread the bus state to make sure that all three pins have settled
      lastBusState = sio_hw->gpio_in;

      busState = (bool)(lastBusState & BC1_PIN_MASK) << 2 |
                  (bool)(lastBusState & BC2_PIN_MASK) << 1 |
                  (bool)(lastBusState & BDIR_PIN_MASK);

      busBit = busLookup[busState];

      // Avoiding switch statements here because timing is critical and needs to be deterministic
      if (!busBit) {
         
         // -----------------------
         // DTB
         // -----------------------

         // DTB needs to come first since its timing is critical.  The CP-1600 expects data to be
         // placed on the bus early in the bus cycle (i.e. we need to get data on the bus quickly!)
         if (deviceAddress) {
            // The data was prefetched during BAR/ADAR. Output data here.  
            DATA_OUT(dataOut);
            SET_DATA_MODE_OUT;
            // wait 20ns (@200Mhz)
            asm inline("nop;nop;nop;nop;");
            while (sio_hw->gpio_in & BC1_PIN_MASK) ;  // wait BC1 go down
            SET_DATA_MODE_IN;
         }
      } else {
         busBit >>= 1;
         if (!busBit) {
            
            // -----------------------
            // BAR, ADAR
            // -----------------------

            if (busState == BUS_ADAR) {
               if (deviceAddress) {
                  // The data was prefetched during BAR/ADAR. Output data here.  
                  DATA_OUT(dataOut);
                  SET_DATA_MODE_OUT;
                  // wait 20ns (@200Mhz)
                  asm inline("nop;nop;nop;nop;");
                  while (sio_hw->gpio_in & BC1_PIN_MASK) ;  // wait BC1 go down 
                  SET_DATA_MODE_IN;
               }
            }

            /// ELSE is BAR   
            // Prefetch data here because there won't be enough time to get it during DTB.
            // However, we can't take forever because of all the time we had to wait for
            // the address to appear on the bus.
            //
            // We have to wait until the address is stable on the bus
            // waiting bus is stable 66 nop at 200mhz is ok/85 at 240

            // wait DIR go low for finish BAR cycle 

            SET_DATA_MODE_IN;

            while (sio_hw->gpio_in & BDIR_PIN_MASK) ; 

            addrIn = sio_hw->gpio_in & 0xFFFF;
            addrInCopy = addrIn;

            deviceAddress = false;

            // check for JLP support and accelerators/RAM enabled 
            if ( JLPSupport && JLPAccel && ((addrIn >= 0x8040) && (addrIn <= 0x9FFF)) ) {

               deviceAddress = true;
               dataOut = RAM[addrIn - 0x8000];

            } else {

               idx = (addrIn >> 8);

               if (slots[idx].filled) {

                  deviceAddress = true;

                  if (slots[idx].type == ROM_SLOT) {

                     if ( (addrIn - slots[idx].target) <= slots[idx].size[0] ) { 

                        romaddr = slots[idx].from[0] + (addrIn - slots[idx].target);
                        dataOut = ROM[romaddr];
                     }

                  } else if (slots[idx].type == ROM_PAGE_SLOT) {

                     if ( (addrIn - slots[idx].target) <= slots[idx].size[curPageArr[seg]] ) { 

                        seg = (addrIn >> 12) & 0xF;
                        uint8_t page = curPageArr[seg];

                        if (slots[idx].size[page] != 0) {    // page is filled
                           romaddr = slots[idx].from[page] + (addrIn - slots[idx].target);
                           dataOut = ROM[romaddr];
                        } else {
                           dataOut = 0xFFFF;
                        }
                     }

                  } else { // RAM8_SLOT or RAM16_SLOT
                  
                     if ( (addrIn - slots[idx].from[0]) <= slots[idx].size[0] ) {

                        romaddr = (addrIn - slots[idx].from[0]);
                        dataOut = RAM[romaddr];
                     }
                  }
               }
            }

         } else {
            busBit >>= 1;
            if (!busBit) {

               // -----------------------
               // DWS WRITE
               // -----------------------
               
               SET_DATA_MODE_IN;

               if (pagingOn) {
                  if ((addrIn & 0xFFF) == 0xFFF) {
                     dataWrite = sio_hw->gpio_in;
                     if ( (dataWrite & 0x0A50) == 0x0A50 ) {
                        // read segment
                        seg = (addrIn >> 12) & 0xF;
                        // set page
                        curPageArr[seg] = dataWrite & 0xF;
                     }
                  }
               }              

               if (deviceAddress) {

                  dataWrite = sio_hw->gpio_in & 0xFFFF;

                  // check for JLP support and accelerators/RAM enabled 
                  if ( JLPSupport && JLPAccel && ((addrIn >= 0x8040) && (addrIn <= 0x9FFF)) ) {

                     // JLP CRC-16 accelerator function
                     if (addrIn == 0x9FFC) { 
                        crc = RAM[0x1FFD];
                        crc ^= dataWrite;
                        for (int i=0; i<16; i++)
                           crc = (crc >> 1) ^ (crc & 1 ? 0xAD52 : 0);
                        RAM[0x1FFD] = crc;

                     } else {

                        RAM[addrIn - 0x8000] = dataWrite;
                     }

                  } else {

                     if ( (addrIn >= ramfrom) && (addrIn <= ramto) ) {
                        if(ramwidth == 8)
                           RAM[addrIn - ramfrom] = dataWrite & 0xFF;
                        else  // ramwidth == 16
                           RAM[addrIn - ramfrom] = dataWrite;
                     }
                  }

               } else {
                  
                  // -----------------------
                  // NACT, IAB, DW, INTAK
                  // -----------------------

                  // reconnect to bus
                  SET_DATA_MODE_IN;
               }

            }
         }
      }
   } // end while
}

void error(int numblink) {
   while (1) {
      gpio_set_dir(LED_PIN, GPIO_OUT);

      for (int i = 0; i < numblink; i++) {
         gpio_put(25, true);
         sleep_ms(600);
         gpio_put(25, false);
         sleep_ms(500);
      }
      sleep_ms(2000);
   }
}

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
      error(2);
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

int read_directory(char *path) {
   UINT id = 0;
   FILINFO fno;

   num_dir_entries = 0;
   DIR_ENTRY *dst = (DIR_ENTRY *) & files[0];

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
         num_dir_entries++;
      }
      f_closedir(&dir);
   } 

   qsort((DIR_ENTRY *) & files[0], num_dir_entries, sizeof(DIR_ENTRY), entry_compare);
   
   return 1;
}

void load_file(char *filename) {
   UINT br, size = 0;
   unsigned char byteread[2];
   int bytes_to_read = 2;
   FIL fil;
   FRESULT fr;

   if ( (fr = f_open(&fil, filename, FA_READ)) != FR_OK ) {
      printf("load_file %s error (%s)!\n", filename, FRESULT_str(fr));
      error(2);
   }
  
   // clean ROM space
   memset(ROM, 0, sizeof(ROM));

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
            ROM[size] = byteread[1] | (byteread[0] << 8);
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

      getRAMRange(&ramfrom, &ramto, &ramwidth);

   } else { 

      // handle raw rom file

      // read the file to SRAM
      while (!(f_eof(&fil))) {
         f_read(&fil, byteread, bytes_to_read, &br);
         ROM[size] = byteread[1] | (byteread[0] << 8);
         size++;
      }
   }

   romLen = size;
   RAM[base + 202] = romLen;
   f_close(&fil);

   printf("load_file: size: %ld\n", romLen);
}

void load_file_by_id(UINT id) {
   DIR dir;
   FRESULT fr;
   UINT i = 0;
   FILINFO fno;

   if ((fr = f_opendir(&dir, curPath)) == FR_OK) {
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
            memset(fullpath, 0, sizeof(fullpath));
            strcat(fullpath, curPath);
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

               getRAMRange(&ramfrom, &ramto, &ramwidth);

               return;
            }

            config_memory(fingerprints[i+1]);
            getRAMRange(&ramfrom, &ramto, &ramwidth);
            printf("ramfrom: 0x%X, ramto: 0x%X, ramwidth: %d\n",
                  ramfrom, ramto, ramwidth);

            return;
         }
      }

      // if this line is reached no config file and no fingerprint so use default config
      config_memory(0);
      return;
   }

   printf("load_cfg: use %s config file\n", cfgfile);
   // read config file to SRAM

   numhacks = 0;
   pagingOn = false;
   ramfrom = 0;
   ramto = 0;

   JLPSupport = false;
   JLPFlash = false;
   JLPFlashSize = 0; 
   JLPAccel = false;

   cleanSlots();
   cleanHoles();

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
            pagingOn = true;
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
               JLPSupport = true;
               printf("JLP support ON\n");

               if (JLP_FEATURE_ACCEL(value)) {
                  JLPAccel = true;
                  printf("JLP accelerators ON\n");
               }
            }
         }

         if ( sscanf(line, "jlp_flash = %hhd", &JLPFlashSize) == 1  ||
               sscanf(line, "jlpflash = %hhd", &JLPFlashSize) == 1 ) {

            if ( JLP_FEATURE_FLASH(value) && (JLPFlashSize > 0) ) {
               JLPFlash = true;
               printf("JLP flash ON\n");
               printf("JLP flash size: %d\n", JLPFlashSize);
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

         hacks[numhacks].address = a;
         hacks[numhacks].value = b;
         numhacks++;
      }
   }

   f_close(&fil);

   getRAMRange(&ramfrom, &ramto, &ramwidth);

   printf("load_cfg done\n");

   return;
}

void filelist(DIR_ENTRY *en, int da, int a) {
   int base = 0x17f;

   for (int i = 0; i < 20 * 20; i++)
      RAM[base + i * 2] = 0;
   for (int n = 0; n < (a - da); n++) {
      if (en[n + da].isDir)
         RAM[0x1000 + n] = 1;
      else
         RAM[0x1000 + n] = 0;
      
      for (int i = 0; i < 20; i++) {
         int pos = base + i * 2 + (n * 40);
         RAM[pos] = en[n + da].long_filename[i];
         if (RAM[pos] <= 20)
            RAM[pos] = 32;
      }
   }
   RAM[0x1028] = (da & 0xFF00) >> 8;   // MSB
   RAM[0x1029] = (da & 0x00FF);        // LSB
   RAM[0x1030] = (a & 0xFF00) >> 8;    // MSB
   RAM[0x1031] = (a & 0x00FF);         // LSB
   RAM[0x1032] = (num_dir_entries & 0xFF00) >> 8;  // MSB
   RAM[0x1033] = (num_dir_entries & 0x00FF);       // LSB
}

void IntyMenu(int type) {       // 1=start, 2=next page, 3=prev page, 4=dir up
   int maxfile = 0;
   
   if (volumeId == 0)
      mount_fatfs_disk();

   printf("Mounting %s...\n", curPath);

   if (f_mount(&FatFs, curPath, 1) != FR_OK)
      printf("E: mount %s failed\n", curPath);
   else
      printf("I: mount %s ok\n", curPath);

   switch (type) {
      case 1:
         read_directory(curPath);
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

// disable optimization here to avoid issues with core "sleep"
#pragma GCC push_options
#pragma GCC optimize ("O0")
void LoadGame(void) {

   int numfile = 0;

   numfile = RAM[0x899] + filefrom - 1;

   DIR_ENTRY *entry = (DIR_ENTRY *) & files[0];

   if (entry[numfile].isDir) {  // directory
      strcat(curPath, "/");
      strcat(curPath, entry[numfile].long_filename);
      IntyMenu(1);
   } else { 
      memset(path, 0, sizeof(path));
      strcat(path, curPath);
      strcat(path, "/");
      strcat(path, entry[numfile].long_filename);

      load_file_by_id(entry[numfile].id);
      // ROM file has internal cfg info
      if(!is_rom_file(fullpath))
         load_cfg(fullpath);

      for (int i=0; i<numhacks; i++) {
         uint32_t romaddr;
         mapAddress(hacks[i].address, 0, &romaddr, ROM_SLOT);
         ROM[romaddr] = hacks[i].value;
      }

      gpio_put(LED_PIN, false);

      sleep_ms(200);
      memset((uint16_t *) RAM, 0, sizeof(RAM));

      resetCart();              // start game !

      volatile uint16_t pbc; 

      // initialize random seed with first random number request 
      get_rand_32();

      int16_t s16_op1, s16_op2;
      int16_t prev_s16_op1, prev_s16_op2;

      uint16_t u16_op1, u16_op2;
      uint16_t prev_u16_op1, prev_u16_op2;

      s16_op1 = s16_op2 = 0;
      prev_s16_op1 = prev_s16_op2 = 0;

      u16_op1 = u16_op2 = 0;
      prev_u16_op1 = prev_u16_op2 = 0;

      while (1) {

         if (JLPSupport) {

            pbc = addrInCopy;

            switch(pbc) {

               // switch off JLP RAM and accelerators
               case 0x8034: {
                  if (RAM[0x34] == 0x6A7A)
                     JLPAccel = false;
               }
               break;

               // switch on JLP RAM and accelerators
               case 0x8033: {
                  if (RAM[0x33] == 0x4A5A)
                     JLPAccel = true;
               break;
               }
            }

            // handle JLP accelerator features
            if ( (JLPAccel) && (pbc >= 0x9F80) && (pbc <= 0x9FFF) ) {

               switch(pbc) {
      
                  // MPYSS: signed 16bit by signed 16bit multiply into 32bit result
                  case 0x9F80:
                  case 0x9F81: {
                     s16_op1 = RAM[0x1F80];
                     s16_op2 = RAM[0x1F81];
                     if ( (s16_op1 != prev_s16_op1) && (s16_op2 != prev_s16_op2) ) {
                        int32_t res = s16_op1 * s16_op2;
                        RAM[0x1F8F] = (res) >> 16;
                        RAM[0x1F8E] = (res & 0xffff);
                        s16_op1 = s16_op2 = 0;
                        prev_s16_op1 = prev_s16_op2 = 0;
                     }
                  }
                  break;
                  
                  // MPYSU: signed 16bit by unsigned 16bit multiply into 32bit result
                  case 0x9F82:
                  case 0x9F83: {
                     s16_op1 = RAM[0x1F82];
                     u16_op2 = RAM[0x1F83];
                     if ( (s16_op1 != prev_s16_op1) && (u16_op2 != prev_u16_op2) ) {
                        int32_t res = s16_op1 * u16_op2;
                        RAM[0x1F8F] = (res) >> 16;
                        RAM[0x1F8E] = (res & 0xffff);
                        s16_op1 = u16_op2 = 0;
                        prev_s16_op1 = prev_u16_op2 = 0;
                     }
                  }
                  break;

                  // MPYUS: unsigned 16bit by signed 16bit multiply into 32bit result
                  case 0x9F84:
                  case 0x9F85: {
                     u16_op1 = RAM[0x1F84];
                     s16_op2 = RAM[0x1F85];
                     if ( (u16_op1 != prev_u16_op1) && (s16_op2 != prev_s16_op2) ) {
                        int32_t res = u16_op1 * s16_op2;
                        RAM[0x1F8F] = (res) >> 16;
                        RAM[0x1F8E] = (res & 0xffff);
                        u16_op1 = s16_op2 = 0;
                        prev_u16_op1 = prev_s16_op2 = 0;
                     }
                  }
                  break;
                  
                  // MPYUU: unsigned 16bit by unsigned 16bit multiply into 32bit result
                  case 0x9F86:
                  case 0x9F87: {
                     u16_op1 = RAM[0x1F86];
                     u16_op2 = RAM[0x1F87];
                     if ( (u16_op1 != prev_u16_op1) && (u16_op2 != prev_u16_op2) ) {
                        int32_t res = u16_op1 * u16_op2;
                        RAM[0x1F8F] = (res) >> 16;
                        RAM[0x1F8E] = (res & 0xffff);
                        u16_op1 = u16_op2 = 0;
                        prev_u16_op1 = prev_u16_op2 = 0;
                     }
                  }
                  break;
                  
                  // DIVSS: signed 16bit by signed 16bit divide with remainder
                  case 0x9F88:
                  case 0x9F89: {
                     s16_op1 = RAM[0x1F88];
                     s16_op2 = RAM[0x1F89];
                     if ( (s16_op1 != prev_s16_op1) && (s16_op2 != prev_s16_op2) ) {
                        int16_t res = s16_op1 % s16_op2;
                        RAM[0x1F8F] = res;
                        res = s16_op1 / s16_op2;
                        RAM[0x1F8E] = res;
                        s16_op1 = s16_op2 = 0;
                        prev_s16_op1 = prev_s16_op2 = 0;
                     }
                  }
                  break;
                  
                  // DIVUU: unsigned 16bit by unsigned 16bit divide with remainder
                  case 0x9F8A:
                  case 0x9F8B: {
                     u16_op1 = RAM[0x1F8A];
                     u16_op2 = RAM[0x1F8B];
                     if ( (u16_op1 != prev_u16_op1) && (u16_op2 != prev_u16_op2) ) {
                        int16_t res = u16_op1 % u16_op2;
                        RAM[0x1F8F] = res;
                        res = u16_op1 / u16_op2;
                        RAM[0x1F8E] = res;
                        u16_op1 = u16_op2 = 0;
                        prev_u16_op1 = prev_u16_op2 = 0;
                     }
                  }
                  break;

                  // non­deterministic hardware random number generator
                  case 0x9FFE: {
                     RAM[0x1FFE] = get_rand_32() & 0xFFFF;
                  }
                  break;

                  default:
                     break;
               }
            }

         } else { 
            
            // JLP off
            gpio_put(LED_PIN, true);
            sleep_ms(2000);
            gpio_put(LED_PIN, false);
            sleep_ms(2000);
         }

      }  // end while
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
   memset(ROM, 0, sizeof(ROM));

   for (int i = 0; i < (sizeof(mintyfw) / 2); i++) 
      ROM[i] = mintyfw[(i * 2) + 1] | (mintyfw[i * 2] << 8);

   memset((uint16_t *) RAM, 0, sizeof(RAM));

   for (int i=0; i<MAX_HACKS_NUM; i++) {
      hacks[i].address = 0;
      hacks[i].value = 0;
   }

   /*
    * [mapping]
    * $0000 - $0FFF = $5000
    * $1000 - $114E = $6000
    * [memattr]
    * $8000 - $9FFF = RAM 8
    */

   cleanSlots();
   cleanHoles();

   addSlot(0x0000, 0x0FFF, 0x5000, 0, ROM_SLOT);
   addSlot(0x1000, 0x1FFF, 0x6000, 0, ROM_SLOT);
   addSlot(0x8000, 0x9FFF, 0, 0, RAM8_SLOT);
   getRAMRange(&ramfrom, &ramto, &ramwidth);

   sleep_ms(200);
   resetCart();
   sleep_ms(1200);

   RAM[CMD_ADDR] = 0;
   IntyMenu(1);
   sleep_ms(800);
   
   // initial conditions 
#ifdef PIRTO_II_SD
   RAM[HAS_SD_ADDR] = 1;
#else
   RAM[HAS_SD_ADDR] = 0;
#endif

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
            case 2:            // run file
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
#ifdef PIRTO_II_SD
            case 6:            // change storage device
               volumeId = RAM[DEV_ADDR];
               sprintf(curPath, "%d:/", volumeId);
               IntyMenu(1);
               break;
#endif
         }
         cmd_executing = false;
         RAM[DONE_ADDR] = 1;
      }

#ifdef DEBUG
      tud_task();
      cdc_task();
#endif
   }
}

#pragma GCC pop_options
