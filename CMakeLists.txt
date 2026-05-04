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

#
# fetch no-OS-FatFS-SD-SDIO-SPI-RPi-Pico library
#
include(FetchContent)

set(PICO_FATFS_DIR ${CMAKE_SOURCE_DIR}/lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)

FetchContent_Declare(
    pico_fatfs
    GIT_REPOSITORY https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    GIT_TAG main
    SOURCE_DIR ${PICO_FATFS_DIR}
)

FetchContent_MakeAvailable(pico_fatfs)
###

add_subdirectory(
    lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src
)

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
   #${CMAKE_CURRENT_LIST_DIR}/src/fatfs/diskio.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/msc_disk.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/fatfs_disk.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/fl_driver/flash_fs.c
)

target_include_directories(${PROJECT} PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/include
)

if(PIRTOII_VARIANT STREQUAL SD_BOARD)
   target_sources(${PROJECT} PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/hw_config.c
   ) 
endif()

#
# overwrite ffconf.h and use custom flash/SD driver
#
target_include_directories(no-OS-FatFS-SD-SDIO-SPI-RPi-Pico BEFORE INTERFACE
   ${CMAKE_CURRENT_LIST_DIR}/include
)

add_custom_target(glue.c
   COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/src/fatfs/diskio.c
            ${CMAKE_CURRENT_LIST_DIR}/lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src/src/glue.c
)

add_custom_target(ffconf.h
   COMMAND ${CMAKE_COMMAND} -E rm -f
      ${CMAKE_CURRENT_LIST_DIR}/lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src/include/ffconf.h
)

add_dependencies(${PROJECT} ffconf.h glue.c)
###

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 1)

target_link_libraries(${PROJECT} PUBLIC 
   pico_stdlib
   hardware_flash 
   pico_stdio_uart
   tinyusb_device
   pico_multicore
   no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT})
