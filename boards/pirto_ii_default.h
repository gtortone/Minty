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
#define MI_AD0_PIN            0

#define MI_RST_PIN            20
#define MI_LED_PIN            25

#define MI_MSYNC_PIN          19
#define MI_BDIR_PIN           16
#define MI_BC1_PIN            18
#define MI_BC2_PIN            17

#define MI_DBG_UART_ID        1
#define MI_DBG_UART_RX_PIN    21
#define MI_DBG_UART_TX_PIN    24

#endif
