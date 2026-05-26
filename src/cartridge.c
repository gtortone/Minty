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

#include "board.h"

#ifdef PINTYCARD
   #include "pintyrom.h"
#else
   #include "mintyrom.h"
#endif

#include "memory.h"

#include "fatfs_disk.h"
#include "interface.h"
#include "filesystem.h"
#include "intellicart.h"
#include "vfs.h"
#include "fatfs_backend.h"
#include "littlefs_backend.h"

#if CONFIG_JLP
   #include "jlpflash.h"
#endif

#if CONFIG_SD_STORAGE
   #include "config.h"
#endif

#if CONFIG_USB_DEVICE
   #include "usb_tasks.h"
#endif

extern Cartridge cart;     // main data structure for cart emulation

unsigned char busLookup[8];

char curPath[512] = "";

#if CONFIG_FLASH_FAT_STORAGE
int volumeId = 0;    // default flash storage
#else
int volumeId = 1;    // default SD storage
#endif

unsigned char files[512 * 24];

int filefrom = 0, fileto = 0;
volatile char cmd = 0;

int num_dir_entries = 0;         // how many entries in the current directory
char fullpath[512];              // full path of current file

volatile uint16_t addrInCopy;

extern struct mapEntry slots[NSLOTS];
extern struct mapHole holes[NSLOTS];
extern struct memHack hacks[MAX_HACKS_NUM];

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

__attribute__((optimize("O3")))
void __time_critical_func(core1_main()) {
   volatile unsigned int lastBusState, busState;
   volatile uint16_t addrIn;
   volatile uint16_t dataOut;
   volatile uint32_t dataWrite = 0;
   volatile unsigned char busBit;
   volatile bool deviceAddress = false;
   volatile uint8_t curPageArr[16];        
   volatile uint8_t seg = 0;
   volatile uint32_t romaddr;
   volatile uint8_t idx;

#if CONFIG_JLP
   volatile uint16_t crc = 0;
#endif

   multicore_lockout_victim_init();

   sleep_ms(480);

   busState = BUS_NACT;
   lastBusState = BUS_NACT;

   dataOut = 0;

   gpio_set_dir_in_masked(ALWAYS_IN_MASK);
   gpio_set_dir_out_masked(ALWAYS_OUT_MASK);

   // Initial conditions
   SET_DATA_MODE_IN;
   memset((uint8_t *) curPageArr, 0, sizeof(curPageArr));

   while (1) {
      // Wait for the bus state to change

      do {
      } while (!((sio_hw->gpio_in ^ lastBusState) & BUS_STATE_MASK));
      
      // We detected a change, but reread the bus state to make sure that all three pins have settled
      lastBusState = sio_hw->gpio_in;

      busState = (bool)(lastBusState & BC1_MASK) << 2 |
                  (bool)(lastBusState & BC2_MASK) << 1 |
                  (bool)(lastBusState & BDIR_MASK);

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
            while (sio_hw->gpio_in & BC1_MASK) ;  // wait BC1 go down
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
                  while (sio_hw->gpio_in & BC1_MASK) ;  // wait BC1 go down 
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

            while (sio_hw->gpio_in & BDIR_MASK) ; 

            addrIn = sio_hw->gpio_in & 0xFFFF;
            addrInCopy = addrIn;

            deviceAddress = false;

#if CONFIG_JLP
            // check for JLP support and accelerators/RAM enabled 
            if ( cart.JLPSupport ) {

                  if ( (cart.JLPAccel || cart.JLPFlash) && ((addrIn >= 0x8000) && (addrIn <= 0x9FFF)) ) {


                     if ((cart.JLPFlash) && (addrIn == 0x8023)) {
                        dataOut = 0;
                        deviceAddress = true;
                     } else if ((cart.JLPFlash) && addrIn == 0x8024) {
                        dataOut = (cart.JLPFlashSize * JLP_FLASH_ROWS_PER_SECTOR) - 1;
                        deviceAddress = true;
                     } else {
                        dataOut = cart.RAM[addrIn - 0x8000];
                        deviceAddress = true;
                     }

                     continue;
                  }
            } 
#endif

            idx = (addrIn >> 8);

            if (slots[idx].usedmask) {

               if (slots[idx].type == ROM_SLOT) {

                  if ( (addrIn - slots[idx].target) <= slots[idx].size[0] ) { 

                     romaddr = slots[idx].from[0] + (addrIn - slots[idx].target);

                     if (holes[idx].filled) {
                        if(addrIn > holes[idx].from)
                           romaddr -= holes[idx].size;
                     }

                     dataOut = cart.ROM[romaddr];
                     deviceAddress = true;

                  } 

               } else if (slots[idx].type == ROM_PAGE_SLOT) {

                  seg = addrIn >> 12;
                  uint8_t page = curPageArr[seg];

                  if (slots[idx].usedmask & (1<<page)) {    // page is filled

                     if ( (addrIn - slots[idx].target) <= slots[idx].size[page] ) { 

                        romaddr = slots[idx].from[page] + (addrIn - slots[idx].target);
                        dataOut = cart.ROM[romaddr];
                        deviceAddress = true;
                     } 

                  } //else printf("A:0x%X, S:%d, P:%d\n", addrIn, seg, page);

               } else { // RAM8_SLOT or RAM16_SLOT
               
                  if ( (addrIn - slots[idx].from[0]) <= slots[idx].size[0] ) {

                     romaddr = (addrIn - slots[idx].from[0]);
                     dataOut = cart.RAM[romaddr];
                     deviceAddress = true;
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
               
               dataWrite = sio_hw->gpio_in & 0xFFFF;

               if (cart.pagingSupport) {
                  if ((addrIn & 0xFFF) == 0xFFF) {
                     if ( (dataWrite & 0x0A50) == 0x0A50 ) {
                        // read segment
                        seg = (addrIn >> 12) & 0xF;
                        // set page
                        curPageArr[seg] = dataWrite & 0xF;
                     }
                  }
               }              

               if (deviceAddress) {

#if CONFIG_JLP
                  // check for JLP support and accelerators/RAM enabled 
                  if ( cart.JLPSupport ) { 

                     if ( (cart.JLPAccel || cart.JLPFlash) && ((addrIn >= 0x8000) && (addrIn <= 0x9FFF)) ) {

                        // JLP CRC-16 accelerator function
                        if (addrIn == 0x9FFC) { 
                           crc = cart.RAM[0x1FFD];
                           crc ^= dataWrite;
                           for (int i=0; i<16; i++)
                              crc = (crc >> 1) ^ (crc & 1 ? 0xAD52 : 0);
                           cart.RAM[0x1FFD] = crc;
                           continue;
                        }

                        // JLP RAM addressing
                        cart.RAM[addrIn - 0x8000] = dataWrite;
                     } 

                  } else {
#endif

                     if ( (addrIn >= cart.ramfrom) && (addrIn <= cart.ramto) ) {
                        if(cart.ramwidth == 8)
                           cart.RAM[addrIn - cart.ramfrom] = dataWrite & 0xFF;
                        else  // cart.ramwidth == 16
                           cart.RAM[addrIn - cart.ramfrom] = dataWrite;
                     }
#if CONFIG_JLP
                  }
#endif

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

void IntyMenu(int type) {       // 1=start, 2=next page, 3=prev page, 4=dir up

   int maxfile = 0;
   int path_char = 0;
   
   switch (type) {
      case 1:
         num_dir_entries = read_directory(curPath, files);
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

   filelist((SCREEN_ENTRY *) & files[0], filefrom, fileto, num_dir_entries);

   // make path available to launcher
   while(curPath[path_char]) {
      int pos = PATH_ADDR + path_char;
      cart.RAM[pos] = curPath[path_char];
      if (cart.RAM[pos] <= 32)
         cart.RAM[pos] = 0;
      else
         cart.RAM[pos] -= 32;
      path_char++;
   }
   for (int pos = PATH_ADDR + path_char; pos < PATH_ADDR + 20; pos++) {
      cart.RAM[pos] = 0;
   }
}

void DirUp() {
   int len = strlen(curPath);

   if ( (strcmp(curPath, "/sd") == 0) || (strcmp(curPath, "/fl") == 0) )
      return;

   if (len > 0) {
      while (len && curPath[--len] != '/') ;
      curPath[len] = 0;
   }
}

void LoadGame(void) {

   int numfile = 0;

   numfile = cart.RAM[0x899] + filefrom - 1;

   SCREEN_ENTRY *entry = (SCREEN_ENTRY *) & files[0];

   if (entry[numfile].isDir) {  // directory

      strcat(curPath, "/");
      strcat(curPath, entry[numfile].filename);
      IntyMenu(1);

   } else { 

      load_file_by_id(entry[numfile].id, curPath, fullpath);

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

      resetCart();              // start game !

#if CONFIG_JLP
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
#endif

      while (1) {

#if CONFIG_JLP
         if (cart.JLPSupport) {

            pbc = addrInCopy;

            switch(pbc) {

               // switch off JLP RAM and accelerators
               case 0x8034: {
                  if (cart.RAM[0x34] == 0x6A7A)
                     cart.JLPAccel = false;
               }
               break;

               // switch on JLP RAM and accelerators
               case 0x8033: {
                  if (cart.RAM[0x33] == 0x4A5A)
                     cart.JLPAccel = true;
               break;
               }
            }

            // handle JLP accelerator feature
            if ( (cart.JLPAccel) && (pbc >= 0x9F80) && (pbc <= 0x9FFF) ) {

               switch(pbc) {
      
                  // MPYSS: signed 16bit by signed 16bit multiply into 32bit result
                  case 0x9F80:
                  case 0x9F81: {
                     s16_op1 = cart.RAM[0x1F80];
                     s16_op2 = cart.RAM[0x1F81];
                     if ( (s16_op1 != prev_s16_op1) && (s16_op2 != prev_s16_op2) ) {
                        int32_t res = s16_op1 * s16_op2;
                        cart.RAM[0x1F8F] = (res) >> 16;
                        cart.RAM[0x1F8E] = (res & 0xffff);
                        s16_op1 = s16_op2 = 0;
                        prev_s16_op1 = prev_s16_op2 = 0;
                     }
                  }
                  break;
                  
                  // MPYSU: signed 16bit by unsigned 16bit multiply into 32bit result
                  case 0x9F82:
                  case 0x9F83: {
                     s16_op1 = cart.RAM[0x1F82];
                     u16_op2 = cart.RAM[0x1F83];
                     if ( (s16_op1 != prev_s16_op1) && (u16_op2 != prev_u16_op2) ) {
                        int32_t res = s16_op1 * u16_op2;
                        cart.RAM[0x1F8F] = (res) >> 16;
                        cart.RAM[0x1F8E] = (res & 0xffff);
                        s16_op1 = u16_op2 = 0;
                        prev_s16_op1 = prev_u16_op2 = 0;
                     }
                  }
                  break;

                  // MPYUS: unsigned 16bit by signed 16bit multiply into 32bit result
                  case 0x9F84:
                  case 0x9F85: {
                     u16_op1 = cart.RAM[0x1F84];
                     s16_op2 = cart.RAM[0x1F85];
                     if ( (u16_op1 != prev_u16_op1) && (s16_op2 != prev_s16_op2) ) {
                        int32_t res = u16_op1 * s16_op2;
                        cart.RAM[0x1F8F] = (res) >> 16;
                        cart.RAM[0x1F8E] = (res & 0xffff);
                        u16_op1 = s16_op2 = 0;
                        prev_u16_op1 = prev_s16_op2 = 0;
                     }
                  }
                  break;
                  
                  // MPYUU: unsigned 16bit by unsigned 16bit multiply into 32bit result
                  case 0x9F86:
                  case 0x9F87: {
                     u16_op1 = cart.RAM[0x1F86];
                     u16_op2 = cart.RAM[0x1F87];
                     if ( (u16_op1 != prev_u16_op1) && (u16_op2 != prev_u16_op2) ) {
                        int32_t res = u16_op1 * u16_op2;
                        cart.RAM[0x1F8F] = (res) >> 16;
                        cart.RAM[0x1F8E] = (res & 0xffff);
                        u16_op1 = u16_op2 = 0;
                        prev_u16_op1 = prev_u16_op2 = 0;
                     }
                  }
                  break;
                  
                  // DIVSS: signed 16bit by signed 16bit divide with remainder
                  case 0x9F88:
                  case 0x9F89: {
                     s16_op1 = cart.RAM[0x1F88];
                     s16_op2 = cart.RAM[0x1F89];
                     if ( (s16_op1 != prev_s16_op1) && (s16_op2 != prev_s16_op2) ) {
                        int16_t res = s16_op1 % s16_op2;
                        cart.RAM[0x1F8F] = res;
                        res = s16_op1 / s16_op2;
                        cart.RAM[0x1F8E] = res;
                        s16_op1 = s16_op2 = 0;
                        prev_s16_op1 = prev_s16_op2 = 0;
                     }
                  }
                  break;
                  
                  // DIVUU: unsigned 16bit by unsigned 16bit divide with remainder
                  case 0x9F8A:
                  case 0x9F8B: {
                     u16_op1 = cart.RAM[0x1F8A];
                     u16_op2 = cart.RAM[0x1F8B];
                     if ( (u16_op1 != prev_u16_op1) && (u16_op2 != prev_u16_op2) ) {
                        int16_t res = u16_op1 % u16_op2;
                        cart.RAM[0x1F8F] = res;
                        res = u16_op1 / u16_op2;
                        cart.RAM[0x1F8E] = res;
                        u16_op1 = u16_op2 = 0;
                        prev_u16_op1 = prev_u16_op2 = 0;
                     }
                  }
                  break;

                  // nondeterministic hardware random number generator
                  case 0x9FFE: {
                     cart.RAM[0x1FFE] = get_rand_32() & 0xFFFF;
                  }
                  break;

                  default:
                     break;
               }
            }
            
            // handle JLP flash feature
            if ( (cart.JLPFlash) && (pbc >= 0x802D) && (pbc <= 0x802F) ) {

               switch (pbc) {
 
                  // JF.wrcmd: copy JLP RAM to flash. Must write the value $C0DE.
                  case 0x802D: {
                     if (cart.RAM[0x2D] == 0xC0DE) {
                        cart.RAM[0x2D] = 0xFFFF;
                        writeFlash(JLP_ROW_NUMBER, JLP_RAM_ADDRESS);
                        cart.RAM[0x2D] = 0x0000;
                     }
                  }
                  break;

                  // JF.rdcmd: copy flash to JLP RAM. Must write the value $DEC0.
                  case 0x802E: {
                     if (cart.RAM[0x2E] == 0xDEC0) {
                        cart.RAM[0x2E] = 0xFFFF;
                        readFlash(JLP_ROW_NUMBER, JLP_RAM_ADDRESS);
                        cart.RAM[0x2E] = 0x0000;
                     }
                  }
                  break;

                  // JF.ercmd: erase flash sector. Must write the value $BEEF.
                  case 0x802F: {
                     if (cart.RAM[0x2F] == 0xBEEF) {
                        cart.RAM[0x2F] = 0xFFFF;
                        eraseFlash(JLP_ROW_NUMBER);
                        cart.RAM[0x2F] = 0x0000;
                     }
                  break;
                  }

                  default:
                     break;
               }
            }

         } else { 
#endif
            
            // JLP off
            gpio_put(LED, true);
            sleep_ms(2000);
            gpio_put(LED, false);
            sleep_ms(2000);

#if CONFIG_JLP
         }
#endif

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
   gpio_init_mask(DATA_MASK);
   gpio_init_mask(BUS_STATE_MASK);
   gpio_set_dir_in_masked(ALWAYS_IN_MASK);
   gpio_set_dir_out_masked(ALWAYS_OUT_MASK);
   gpio_init(LED);
   gpio_put(LED, true);
   gpio_init(RESET);

   gpio_set_dir(MSYNC, GPIO_IN);
   gpio_pull_down(MSYNC);

   sleep_ms(800);

   printf("Inty Pow-ON\n");

   gpio_put(LED, true);

   // init filesystems
   vfs_init();

#if CONFIG_SD_STORAGE
   printf("mount SD storage...");
   if (vfs_add_mount(&fatfs_driver, "/sd", 1, NULL) != -1)
      printf(" OK\n");
   else
      printf(" FAIL\n");
#endif

#if CONFIG_FLASH_FAT_STORAGE
   mount_fatfs_disk();
   printf("mount flash FAT storage...");
   if (vfs_add_mount(&fatfs_driver, "/fl", 0, NULL) != -1)
      printf(" OK\n");
   else
      printf(" FAIL\n");

#elif CONFIG_FLASH_LFS_STORAGE
   printf("mount flash LittleFs storage...");
   if (vfs_add_mount(&littlefs_driver, "/fl", 0, NULL) != -1)
      printf(" OK\n");
   else
      printf(" FAIL\n");
#endif

   // init cartridge
   init_cart();

   for (int i = 0; i < (sizeof(mintyfw) / 2); i++) 
      cart.ROM[i] = mintyfw[(i * 2) + 1] | (mintyfw[i * 2] << 8);

   cart.RAM[BOARD_ID_ADDR] = BOARD_ID;

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

   sleep_ms(200);
   resetCart();
#if defined(PIRTO)
   sleep_ms(200);
   resetCart();
#endif
   sleep_ms(1200);

   cart.RAM[STATUS_ADDR] = 1;      // block cart access until initialisation is done
   cart.RAM[CMD_ADDR] = 0;

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

   IntyMenu(1);

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
