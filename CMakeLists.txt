cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PIRTOII_VARIANT)
   set(PIRTOII_VARIANT "DEFAULT_BOARD")
   message(STATUS "NOTE: PirtoII variant not defined. Using DEFAULT_BOARD.")
endif()

if(PIRTOII_VARIANT STREQUAL DEFAULT_BOARD)
   set(PROJECT Minty.default_board)
elseif(PIRTOII_VARIANT STREQUAL SD_BOARD)
   set(PROJECT Minty.sd_board)
endif()

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${PROJECT} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT})

# DEFAULT_BOARD is PirtoII by aotta (https://github.com/aotta/PiRTOII)
# SD_BOARD is PirtoII with microSD slot by sukkopera (https://github.com/SukkoPera/PiRTOII)

if(PIRTOII_VARIANT STREQUAL DEFAULT_BOARD)
   # 16MB of flash on purple Pico clones
   target_compile_definitions(${PROJECT} PRIVATE "-DPICO_FLASH_SIZE_BYTES=16777216") 
   target_compile_options(${PROJECT} PRIVATE "-DDEFAULT_BOARD")
   target_compile_options(${PROJECT} PRIVATE "-DPICO_FLASH_SPI_CLKDIV=2")
   pico_set_linker_script(${PROJECT} ${CMAKE_CURRENT_SOURCE_DIR}/support/memmap_custom_16MB.ld)
elseif(PIRTOII_VARIANT STREQUAL SD_BOARD)
   target_compile_definitions(${PROJECT} PRIVATE "-DPICO_FLASH_SIZE_BYTES=2097152") 
   target_compile_options(${PROJECT} PRIVATE "-DSD_BOARD")
   target_compile_options(${PROJECT} PRIVATE "-DPICO_FLASH_SPI_CLKDIV=4")
endif()

if(DEFINED DEBUG)
   target_compile_definitions(${PROJECT} PRIVATE "-DDEBUG")
endif()

target_compile_options(${PROJECT} PRIVATE "-O2")
target_compile_options(${PROJECT} PRIVATE "-Wunused-variable")
target_compile_options(${PROJECT} PRIVATE "-DCFG_TUSB_RHPORT0_MODE")

set_property(TARGET ${PROJECT} APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

target_sources(${PROJECT} PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/src/main.c
   ${CMAKE_CURRENT_LIST_DIR}/src/inty_cart.c
   ${CMAKE_CURRENT_LIST_DIR}/src/memory.c
   ${CMAKE_CURRENT_LIST_DIR}/src/usb_tasks.c
   ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/diskio.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ff.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ffunicode.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ffsystem.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/f_util.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/msc_disk.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/fatfs_disk.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/flash_fs.c
)

target_include_directories(${PROJECT} PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/include
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver
)

if(PIRTOII_VARIANT STREQUAL SD_BOARD)
   target_sources(${PROJECT} PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/hw_config.c
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
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ff_stdio.c
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/file_stream.c
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/my_debug.c
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/my_rtc.c
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/util.c
   ) 

   target_include_directories(${PROJECT} PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/sd_driver/SDIO
   )
endif()

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 0)

target_link_libraries(${PROJECT} PUBLIC 
   pico_stdlib
   hardware_flash 
   hardware_uart
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
