/*
//   Minty - MultiCart for Mattel Intellivision by Gennaro Tortone 2025
//
//   based on PiRTO II Flash MultiCART by Andrea Ottaviani 2024
//   parts of code are directly from the A8PicoCart project by Robin Edwards 2023
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/watchdog.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/rand.h"

#include "board.h"

#include "memory.h"

#include "fatfs_disk.h"
#include "interface.h"
#include "filesystem.h"
#include "intellicart.h"
#include "launcher.h"
#include "fatfs_backend.h"
#include "littlefs_backend.h"
#include "emu2149.h"

#if CONFIG_JLP
   #include "jlpflash.h"
#endif

#if CONFIG_ECS_AUDIO
   #define ECS_BUF_SIZE    32
   extern PSG* psg0;
   extern const uint8_t ECS_LUT[16];
   uint8_t ayRead = 0;
   volatile uint8_t ayWrite = 0;
   volatile uint8_t ayRegister[ECS_BUF_SIZE] = {0};
   volatile uint8_t ayValue[ECS_BUF_SIZE] = {0};
#endif

extern Cartridge cart;     // main data structure for cart emulation

volatile uint16_t addrInCopy;

volatile uint8_t spyData = 0xFF;

extern struct SlotEntry slots[NSLOTS];

__attribute__((optimize("O3")))
void __time_critical_func(core1_main()) {
   volatile unsigned int gpio_snapshot, busState;
   volatile uint16_t addrIn = 0;
   volatile uint16_t dataOut = 0;
   volatile uint32_t dataIn = 0;
   volatile bool deviceAddress = false;
   volatile uint8_t curPageArr[16];        
   volatile uint8_t seg = 0;
   volatile uint32_t romaddr;
   volatile uint8_t idx;

#if CONFIG_JLP
   uint16_t crc = 0;
#endif

   multicore_lockout_victim_init();

   sleep_ms(480);

   busState = BUS_NACT;
   gpio_snapshot = 0;

   dataOut = 0;

   gpio_set_dir_in_masked(ALWAYS_IN_MASK);
   gpio_set_dir_out_masked(ALWAYS_OUT_MASK);

   // Initial conditions
   SET_DATA_MODE_IN;

   seg = 0;
   memset((uint8_t *) curPageArr, 0, sizeof(curPageArr));

   while (1) {

      // Wait for the bus state to change
      do {
      } while (!((sio_hw->gpio_in ^ gpio_snapshot) & BUS_STATE_MASK));
      
      // We detected a change, but reread the bus state to make sure that all three pins have settled
      gpio_snapshot = sio_hw->gpio_in;

      busState = (bool)(gpio_snapshot & BC1_MASK) << 2 |
                 (bool)(gpio_snapshot & BC2_MASK) << 1 |
                 (bool)(gpio_snapshot & BDIR_MASK);

      // Avoiding switch statements here because timing is critical and needs to be deterministic
      if (busState == BUS_DTB) {     
         // -----------------------
         // DTB
         // -----------------------

         // DTB needs to come first since its timing is critical.  The CP-1600 expects data to be
         // placed on the bus early in the bus cycle (i.e. we need to get data on the bus quickly!)
         if (deviceAddress) {
            // The data was prefetched during BAR/ADAR. Output data here.  
            DATA_OUT(dataOut);
            SET_DATA_MODE_OUT;
            asm inline("nop;nop;nop;nop;nop;"); // wait 20ns (@250Mhz)
            while (sio_hw->gpio_in & BC1_MASK) ;  // wait BC1 go down
            SET_DATA_MODE_IN;
         }
      } else {
         if ((busState == BUS_ADAR) || (busState == BUS_BAR)) {
            // -----------------------
            // BAR, ADAR
            // -----------------------

            if (busState == BUS_ADAR) {
               if (deviceAddress) {
                  // The data was prefetched during BAR/ADAR. Output data here.  
                  DATA_OUT(dataOut);
                  SET_DATA_MODE_OUT;
                  asm inline("nop;nop;nop;nop;nop;"); // wait 20ns (@250Mhz)
                  while (sio_hw->gpio_in & BC1_MASK) ;  // wait BC1 go down 
                  SET_DATA_MODE_IN;
               }
            }

            // ELSE is BAR   
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
            deviceAddress = false;

#if CONFIG_JLP
            // addrInCopy is to pass address to other core for JLP functions processing
            addrInCopy = addrIn;
            
            // check for JLP support and accelerators/RAM enabled 
            if ( cart.JLPSupport ) {
                  if ( (cart.JLPAccel || cart.JLPFlash) && ((addrIn >= 0x8000) && (addrIn <= 0x9FFF)) ) {
                     if ((cart.JLPFlash) && (addrIn == 0x8023)) {
                        dataOut = 0;
                        deviceAddress = true;
                     } else if ((cart.JLPFlash) && (addrIn == 0x8024)) {
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
            
            if ((slots[idx].RomAddr_H[0][0] == RAM8_SLOT) || (slots[idx].RomAddr_H[0][0] == RAM16_SLOT)) {
               // This is RAM
               //printf("Read 0x%04X => 0x%04X (RAM)\n", addrIn, (addrIn - cart.ramfrom));
               romaddr = (addrIn - cart.ramfrom);
               dataOut = cart.RAM[romaddr];
               deviceAddress = true;
               continue;
            }
            else {
               // This is ROM check for section it falls in.
               uint8_t short_Address = addrIn & 0x00FF;
               // gather current used page for mem segment
               seg = addrIn >> 12;
               uint8_t page = curPageArr[seg];

               if (slots[idx].RomAddr_H[0][page] != UNUSED_SLOT) {
                  if ((short_Address >= slots[idx].from[0][page]) && (short_Address <= slots[idx].to[0][page])) {
                     romaddr = (uint32_t)((slots[idx].RomAddr_H[0][page] << 16) + (uint32_t)slots[idx].RomAddr_L[0][page]) + (uint32_t)(short_Address - slots[idx].from[0][page]);
                     dataOut = cart.ROM[romaddr];
                     deviceAddress = true;
                     //printf("Read 0x%04X => 0x%08lX\n", addrIn, romaddr);
                  }
                  else if ((short_Address >= slots[idx].from[1][page]) && (short_Address <= slots[idx].to[1][page])) {
                     romaddr = (uint32_t)((slots[idx].RomAddr_H[1][page] << 16) + (uint32_t)slots[idx].RomAddr_L[1][page]) + (uint32_t)(short_Address - slots[idx].from[1][page]);
                     dataOut = cart.ROM[romaddr];
                     deviceAddress = true;
                     //printf("Read 0x%04X => 0x%08lX\n", addrIn, romaddr);
                  }
                  continue;
               }
            }

/*
            if (slots[idx].usedmask) {

               if (slots[idx].type == ROM_SLOT) {

                  if (addrIn < slots[idx].target) {
                     dataOut = 0xFFFF;
                     deviceAddress = true;
                     continue;
                  }

                  uint16_t size;
                  
                  // handle holes for not-paged ROM slots
                  if (holes[idx].filled && (holes[idx].page == 0))
                     size = slots[idx].size[0] + holes[idx].size + 1;
                  else
                     size = slots[idx].size[0];

                  if ( (addrIn - slots[idx].target) < size ) { 

                     romaddr = slots[idx].from[0] + (addrIn - slots[idx].target);

                     if (holes[idx].filled && (holes[idx].page == 0)) {

                        if ( (addrIn >= holes[idx].from) && (addrIn <= (holes[idx].from + holes[idx].size)) ) {
                           dataOut = 0xFFFF;
                           deviceAddress = true;
                           continue;
                        } 

                        if ( addrIn > (holes[idx].from) )
                           romaddr -= (holes[idx].size + 1);
                     } 

                     dataOut = cart.ROM[romaddr];
                     deviceAddress = true;
                  } 

               } else if (slots[idx].type == ROM_PAGE_SLOT) {

                  seg = addrIn >> 12;
                  uint8_t page = curPageArr[seg];

                  uint16_t size;

                  // handle holes for paged ROM slots
                  if (holes[idx].filled && (holes[idx].page == page))
                     size = slots[idx].size[page] + holes[idx].size + 1;
                  else
                     size = slots[idx].size[page];

                  if (slots[idx].usedmask & (1<<page)) {    // page is filled

                     if ( (addrIn - slots[idx].target) < size ) { 

                        romaddr = slots[idx].from[page] + (addrIn - slots[idx].target);

                        if (holes[idx].filled && (holes[idx].page == page)) {

                           if ( (addrIn >= holes[idx].from) && (addrIn <= (holes[idx].from + holes[idx].size)) ) {
                              dataOut = 0xFFFF;
                              deviceAddress = true;
                              continue;
                           } 
                           
                           if ( addrIn > (holes[idx].from) )
                              romaddr -= (holes[idx].size + 1);

                        }

                        dataOut = cart.ROM[romaddr];
                        deviceAddress = true;
                     } 

                  } else {
                     deviceAddress = true;
                     dataOut = 0xFFFF;
                  }

               } else { // RAM8_SLOT or RAM16_SLOT
               
                  if ( (addrIn - slots[idx].from[0]) < (slots[idx].size[0]) ) {

                     romaddr = (addrIn - slots[idx].from[0]);
                     dataOut = cart.RAM[romaddr];
                     deviceAddress = true;
                  }
               } 
            }
*/
         } else {
            if (busState == BUS_DWS) {

               // -----------------------
               // DWS WRITE
               // -----------------------
               
               SET_DATA_MODE_IN;
               
               dataIn = sio_hw->gpio_in & 0xFFFF;          

#if CONFIG_ECS_AUDIO
               if (cart.ECSSupport) {
                  if ( (addrIn & 0xFFF0) == 0x00F0 ) {
                     ayRegister[ayWrite] = ECS_LUT[addrIn & 0x000F];
                     ayValue[ayWrite] = dataIn;
                     ayWrite = (ayWrite + 1) % ECS_BUF_SIZE;
                     continue;
                  }
               }
#endif

               if (cart.pagingSupport) {
                  if ((addrIn & 0xFFF) == 0xFFF) {
                     if ( ((dataIn >> 4) & 0x00FF) == 0xA5 ) {
                        // read segment
                        seg = (addrIn >> 12) & 0xF;
                        // set page
                        curPageArr[seg] = dataIn & 0xF;
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
                           crc ^= dataIn;
                           for (int i=0; i<16; i++)
                              crc = (crc >> 1) ^ (crc & 1 ? 0xAD52 : 0);
                           cart.RAM[0x1FFD] = crc;
                           continue;
                        }

                        // JLP RAM addressing
                        cart.RAM[addrIn - 0x8000] = dataIn;
                     } 

                  } else {
#endif

                     if ( (addrIn >= cart.ramfrom) && (addrIn <= cart.ramto) ) {
                        if(cart.ramwidth == 8)
                           cart.RAM[addrIn - cart.ramfrom] = dataIn & 0xFF;
                        else  // cart.ramwidth == 16
                           cart.RAM[addrIn - cart.ramfrom] = dataIn;
                     }
#if CONFIG_JLP
                  }
#endif
               }
            }  
            else {
               // -----------------------
               // NACT, IAB, DW, INTAK
               // -----------------------

               // reconnect to bus
               SET_DATA_MODE_IN; 

               if ((busState == BUS_NACT) && ((addrIn == 0x01FF) || (addrIn == 0x01FE))) {

                  do {
                     spyData = sio_hw->gpio_in & 0xFF;
                  } while (spyData != (sio_hw->gpio_in & 0xFF));

                  while (!(sio_hw->gpio_in & BUS_STATE_MASK)) ;  // wait NACT state end
               }
            }
         }
      }
   } // end while
}

#if CONFIG_JLP 
static void generate_random(uint16_t *arr, int n) {

   uint64_t rand = get_rand_64();
   int k = 0;

   for(int i=0; i<n; i++) {
      arr[i] = (rand >> 16*k++) & 0xFFFF;
      if ( (k % 4) == 0 ) {
         rand = get_rand_64();
         k = 0;
      }
   }
}
#endif

void RunGame() {

#if CONFIG_JLP  
   // initialize random seed and preallocate an array of random numbers
   uint16_t randarr[4];
   uint8_t randidx = 0;
   bool randrefill = false;

   generate_random(randarr, sizeof(randarr)/sizeof(uint16_t));
#endif
   uint64_t resetTimeout = 0;

   resetCart();              // start game !

#if CONFIG_JLP
   volatile uint16_t pbc; 

   int16_t s16_op1, s16_op2;
   uint16_t u16_op1, u16_op2;

   s16_op1 = s16_op2 = 0;
   u16_op1 = u16_op2 = 0;
#endif

   while (1) {

      if (spyData == 0x5a) {
         resetTimeout = make_timeout_time_ms(2000);
         //printf("Card Reset request 1/2\n");
      }
      if ((spyData == 0x55) && (get_absolute_time() < resetTimeout)) {
            //printf("Card Reset request 2/2\n");
            watchdog_enable(1, 1);
            while(1);
      }

#if CONFIG_ECS_AUDIO

      if(ayRead != ayWrite) {
         PSG_writeReg(psg0, ayRegister[ayRead], ayValue[ayRead]);
         ayRead = (ayRead + 1) % ECS_BUF_SIZE;
      }

#endif

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
                  int32_t res = s16_op1 * s16_op2;
                  cart.RAM[0x1F8F] = (res) >> 16;
                  cart.RAM[0x1F8E] = (res & 0xffff);
               }
               break;
               
               // MPYSU: signed 16bit by unsigned 16bit multiply into 32bit result
               case 0x9F82:
               case 0x9F83: {
                  s16_op1 = cart.RAM[0x1F82];
                  u16_op2 = cart.RAM[0x1F83];
                  int32_t res = s16_op1 * u16_op2;
                  cart.RAM[0x1F8F] = (res) >> 16;
                  cart.RAM[0x1F8E] = (res & 0xffff);
               }
               break;

               // MPYUS: unsigned 16bit by signed 16bit multiply into 32bit result
               case 0x9F84:
               case 0x9F85: {
                  u16_op1 = cart.RAM[0x1F84];
                  s16_op2 = cart.RAM[0x1F85];
                  int32_t res = u16_op1 * s16_op2;
                  cart.RAM[0x1F8F] = (res) >> 16;
                  cart.RAM[0x1F8E] = (res & 0xffff);
               }
               break;
               
               // MPYUU: unsigned 16bit by unsigned 16bit multiply into 32bit result
               case 0x9F86:
               case 0x9F87: {
                  u16_op1 = cart.RAM[0x1F86];
                  u16_op2 = cart.RAM[0x1F87];
                  uint32_t res = u16_op1 * u16_op2;
                  cart.RAM[0x1F8F] = (res) >> 16;
                  cart.RAM[0x1F8E] = (res & 0xffff);
               }
               break;
               
               // DIVSS: signed 16bit by signed 16bit divide with remainder
               case 0x9F88:
               case 0x9F89: {
                  s16_op1 = cart.RAM[0x1F88];
                  s16_op2 = cart.RAM[0x1F89];
                  int16_t res = s16_op1 % s16_op2;
                  cart.RAM[0x1F8F] = res;
                  res = s16_op1 / s16_op2;
                  cart.RAM[0x1F8E] = res;
               }
               break;
               
               // DIVUU: unsigned 16bit by unsigned 16bit divide with remainder
               case 0x9F8A:
               case 0x9F8B: {
                  u16_op1 = cart.RAM[0x1F8A];
                  u16_op2 = cart.RAM[0x1F8B];
                  int16_t res = u16_op1 % u16_op2;
                  cart.RAM[0x1F8F] = res;
                  res = u16_op1 / u16_op2;
                  cart.RAM[0x1F8E] = res;
               }
               break;

               // nondeterministic hardware random number generator
               case 0x9FFE: {
                  cart.RAM[0x1FFE] = randarr[randidx++];
                  if (randidx == 4) {
                     randidx = 0;
                     randrefill = true;
                  }
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

         // refill random numbers
         if (randrefill) {
            generate_random(randarr, sizeof(randarr)/sizeof(uint16_t));
            randrefill = false;
         }
      }
#endif
   }  // end while
}


void Inty_cart_main() {
   printf("Inty_cart_main\n");

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

   //  Run Minty game selection interface
   RunLauncher();

   // Game selected and loaded => Run emulation
   RunGame();
}
