#ifndef PINTYCARD_H
#define PINTYCARD_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

// for board detection
#define PINTYCARD 1

// internal flash W25Q16JVWI
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (2 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

// Pico pin usage definitions
#define MI_AD0_PIN            0

#define MI_RST_PIN            20
#define MI_LED_PIN            25

#define MI_MSYNC_PIN          21
#define MI_BDIR_PIN           22
#define MI_BC1_PIN            26
#define MI_BC2_PIN            27

// UART
#define MI_DBG_UART_ID        1
#define MI_DBG_UART_RX_PIN    -1    // TX only
#define MI_DBG_UART_TX_PIN    24
                     
#endif
