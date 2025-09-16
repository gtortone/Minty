#ifndef BOARD_H_
#define BOARD_H_

// Pico pin usage definitions
#define B0_PIN    0
#define B1_PIN    1
#define B2_PIN    2
#define B3_PIN    3
#define B4_PIN    4
#define B5_PIN    5
#define B6_PIN    6
#define B7_PIN    7
#define F0_PIN    8
#define F1_PIN    9
#define F2_PIN    10
#define F3_PIN    11
#define F4_PIN    12
#define F5_PIN    13
#define F6_PIN    14
#define F7_PIN    15

#define RST_PIN   20
#define LED_PIN   25

#define resetLow()  gpio_set_dir(RST_PIN,true); gpio_put(RST_PIN,true);    // Minty to INTV BUS ; RST Output set to 0
#define resetHigh() gpio_set_dir(RST_PIN,true); gpio_put(RST_PIN,false);   // RST is INPUT; B->A, INTV BUS to Pico

#ifdef DEFAULT_BOARD
   #define BDIR_PIN  16
   #define BC2_PIN   17
   #define BC1_PIN   18
   #define MSYNC_PIN 19
   // UART
   #define UART_TX   21
   #define UART_RX   24

   #define BDIR_PIN_MASK   0x00010000L     // gpio 16
   #define BC2_PIN_MASK    0x00020000L     // gpio 17
   #define BC1_PIN_MASK    0x00040000L     // gpio 18
   #define LED_PIN_MASK    0x02000000L     // gpio 25
   #define BC1e2_PIN_MASK  0x00060000L
   #define DATA_PIN_MASK   0x0000FFFFL
   #define BUS_STATE_MASK  0x00070000L
   #define ALWAYS_IN_MASK  (BUS_STATE_MASK)
   #define ALWAYS_OUT_MASK (LED_PIN_MASK)

   #define SET_DATA_MODE_OUT   gpio_set_dir_out_masked(DATA_PIN_MASK)
   #define SET_DATA_MODE_IN    gpio_set_dir_in_masked(DATA_PIN_MASK)
#else
#ifdef SD_BOARD
   #define BDIR_PIN  22
   #define BC2_PIN   27
   #define BC1_PIN   26
   #define MSYNC_PIN 21
   #define DIR_PIN   28
   // SD
   #define SD_MISO   16
   #define SD_CS     17
   #define SD_SCK    18
   #define SD_MOSI   19

   #define BDIR_PIN_MASK   0x00400000L     // gpio 22
   #define BC2_PIN_MASK    0x08000000L     // gpio 27
   #define BC1_PIN_MASK    0x04000000L     // gpio 26
   #define LED_PIN_MASK    0x02000000L     // gpio 25
   #define DIR_PIN_MASK	   0x10000000L	    // gpio 28
   #define B0_PIN_MASK     0x00000001L     // gpio 0
   #define B1_PIN_MASK     0x00000002L
   #define B2_PIN_MASK     0x00000004L
   #define B3_PIN_MASK     0x00000008L
   #define B4_PIN_MASK     0x00000010L
   #define B5_PIN_MASK     0x00000020L
   #define B6_PIN_MASK     0x00000040L
   #define B7_PIN_MASK     0x00000080L
   #define F0_PIN_MASK     0x00000100L
   #define F1_PIN_MASK     0x00000200L
   #define F2_PIN_MASK     0x00000400L
   #define F3_PIN_MASK     0x00000800L
   #define F4_PIN_MASK     0x00001000L
   #define F5_PIN_MASK     0x00002000L
   #define F6_PIN_MASK     0x00004000L
   #define F7_PIN_MASK     0x00008000L     // gpio 15
   #define BX_PIN_MASK     (B0_PIN_MASK | B1_PIN_MASK | B2_PIN_MASK | B3_PIN_MASK | B4_PIN_MASK | B5_PIN_MASK | B6_PIN_MASK | B7_PIN_MASK)
   #define FX_PIN_MASK     (F0_PIN_MASK | F1_PIN_MASK | F2_PIN_MASK | F3_PIN_MASK | F4_PIN_MASK | F5_PIN_MASK | F6_PIN_MASK | F7_PIN_MASK)
   #define BC1e2_PIN_MASK  (BC1_PIN_MASK | BC2_PIN_MASK)
   #define DATA_PIN_MASK   (BX_PIN_MASK | FX_PIN_MASK)
   #define BUS_STATE_MASK  (BDIR_PIN_MASK | BC1_PIN_MASK | BC2_PIN_MASK)
   #define ALWAYS_IN_MASK  (BUS_STATE_MASK)
   #define ALWAYS_OUT_MASK (LED_PIN_MASK | DIR_PIN_MASK)

   #define SET_DATA_MODE_OUT   do {gpio_clr_mask(DIR_PIN_MASK); gpio_set_dir_out_masked(DATA_PIN_MASK);} while (0)
   #define SET_DATA_MODE_IN    do {gpio_set_dir_in_masked(DATA_PIN_MASK); gpio_set_mask(DIR_PIN_MASK);} while (0)
#endif
#endif

// Inty bus values (BC1+BC2+BDIR) GPIO 18-17-16
#define BUS_NACT  0b000         //0
#define BUS_BAR   0b001         //1
#define BUS_IAB   0b010         //2
#define BUS_DWS   0b011         //3
#define BUS_ADAR  0b100         //4
#define BUS_DW    0b101         //5
#define BUS_DTB   0b110         //6
#define BUS_INTAK 0b111         //7

// Inty RAM addresses shared with cart firmware
#define CMD_ADDR     0x889
#define DONE_ADDR    0x119       // 0: cart wait, 1: cart run
#define DEV_ADDR     0x120       // 0: flash, 1: SD

#endif
