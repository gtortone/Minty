#ifndef PIRTO_II_DEFAULT_H
#define PIRTO_II_DEFAULT_H

// PirtoII by aotta (https://github.com/aotta/PiRTOII)

pico_board_cmake_set(PICO_PLATFORM, rp2040)

// for board detection
#define PIRTO_II_DEFAULT 1

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
 
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif
 
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (16 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

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

#define BDIR_PIN  16
#define BC2_PIN   17
#define BC1_PIN   18
#define MSYNC_PIN 19

// UART
#define UART_ID   uart1
#define UART_TX   21
#define UART_RX   24
#define UART_BAUDRATE   115200

#define BDIR_PIN_MASK   ((uint32_t)1 << BDIR_PIN)
#define BC1_PIN_MASK    ((uint32_t)1 << BC1_PIN)
#define BC2_PIN_MASK    ((uint32_t)1 << BC2_PIN)
#define LED_PIN_MASK    ((uint32_t)1 << LED_PIN)
#define DATA_PIN_MASK   0x0000FFFFL
#define BUS_STATE_MASK  0x00070000L
#define ALWAYS_IN_MASK  (BUS_STATE_MASK)
#define ALWAYS_OUT_MASK (LED_PIN_MASK)

#define SET_DATA_MODE_OUT     sio_hw->gpio_oe_set = DATA_PIN_MASK; 
#define SET_DATA_MODE_IN      sio_hw->gpio_oe_clr = DATA_PIN_MASK;

#define DATA_OUT(v) sio_hw->gpio_togl = (sio_hw->gpio_out ^ v) & 0xFFFF;

#endif
