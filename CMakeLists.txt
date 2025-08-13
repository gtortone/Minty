cmake_minimum_required(VERSION 3.13)

set(PROJECT Minty)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${PROJECT} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT})
# 16MB of flash on purple Pico clones
target_compile_definitions(${PROJECT} PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216
)

# SD
pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/SDIO/rp2040_sdio.pio)
target_compile_definitions(${PROJECT} PUBLIC PICO_MAX_SHARED_IRQ_HANDLERS=8u)

# low optimization level due to timing issue with cart firmware
target_compile_options(${PROJECT} PRIVATE "-O1")
target_compile_options(${PROJECT} PRIVATE "-Wunused-variable")
target_compile_options(${PROJECT} PRIVATE "-DCFG_TUSB_RHPORT0_MODE")

target_sources(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/src/inty_cart.c
    ${CMAKE_CURRENT_LIST_DIR}/src/memory.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/hw_config.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/diskio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/msc_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/fatfs_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/flash_fs.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/dma_interrupts.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/sd_card.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/sd_timeouts.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/SDIO/rp2040_sdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/SDIO/sd_card_sdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/SPI/my_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/SPI/sd_card_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/SPI/sd_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/crash.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/crc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/f_util.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/file_stream.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/my_debug.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/my_rtc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/util.c
) 

target_include_directories(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/SDIO
)

# Disable usb output, enable uart output
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 1)

# In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
# for TinyUSB device support
target_link_libraries(${PROJECT} PUBLIC 
   pico_stdlib
   hardware_flash 
   hardware_uart
   pico_stdio_uart
   hardware_dma
   hardware_pio
   hardware_spi
   hardware_sync
   cmsis_core  ###
   pico_aon_timer
   tinyusb_device
   pico_multicore
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT})


