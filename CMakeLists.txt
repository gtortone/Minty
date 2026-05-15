cmake_minimum_required(VERSION 3.13)

set(PROJECT Minty)

# set boards directory
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

# import board build options
include(${CMAKE_SOURCE_DIR}/boards/${PICO_BOARD}.cmake)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${PROJECT} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

include(FetchContent)

#
# fetch no-OS-FatFS-SD-SDIO-SPI-RPi-Pico library
#
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
   ${PICO_FATFS_DIR}/src
)

add_executable(${PROJECT})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   target_link_libraries(${PROJECT} pico_stdio_uart)
   pico_enable_stdio_uart(${PROJECT} 1)
endif()

if(CONFIG_USB_DEVICE)
   target_link_libraries(${PROJECT} tinyusb_device)
   target_compile_options(${PROJECT} PRIVATE "-DCFG_TUSB_RHPORT0_MODE")
   target_sources(${PROJECT} PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/src/msc_disk.c
      ${CMAKE_CURRENT_LIST_DIR}/src/usb_tasks.c
      ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
   )
endif()

target_compile_definitions(${PROJECT} PRIVATE "PICO_FLASH_SIZE_BYTES=${PICO_FLASH_SIZE_BYTES}")
target_compile_definitions(${PROJECT} PRIVATE "PICO_FLASH_SPI_CLKDIV=${PICO_FLASH_SPI_CLKDIV}")
target_compile_definitions(${PROJECT} PRIVATE "CONFIG_USB_DEVICE=${CONFIG_USB_DEVICE}")
target_compile_definitions(${PROJECT} PRIVATE "CONFIG_SD_STORAGE=${CONFIG_SD_STORAGE}")
target_compile_definitions(${PROJECT} PRIVATE "CONFIG_FLASH_STORAGE=${CONFIG_FLASH_STORAGE}")

target_compile_options(${PROJECT} PRIVATE "-O2")
target_compile_options(${PROJECT} PRIVATE "-Wall")

set_property(TARGET ${PROJECT} APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

target_sources(${PROJECT} PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/src/main.c
   ${CMAKE_CURRENT_LIST_DIR}/src/inty_cart.c
   ${CMAKE_CURRENT_LIST_DIR}/src/memory.c
   #${CMAKE_CURRENT_LIST_DIR}/src/diskio.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs_disk.c
   ${CMAKE_CURRENT_LIST_DIR}/src/flash_fs.c
   ${CMAKE_CURRENT_LIST_DIR}/src/hw_config.c
   ${CMAKE_CURRENT_LIST_DIR}/src/utils.c
)

target_include_directories(${PROJECT} PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/include
)

#
# overwrite ffconf.h and use custom flash/SD driver
#
add_custom_target(glue.c
   COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/src/diskio.c
            ${PICO_FATFS_DIR}/src/src/glue.c
)

add_custom_target(ffconf.h
   COMMAND ${CMAKE_COMMAND} -E rm -f
   ${PICO_FATFS_DIR}/src/include/ffconf.h
)

add_dependencies(${PROJECT} ffconf.h glue.c)

target_link_libraries(${PROJECT}
   pico_stdlib
   hardware_flash 
   pico_multicore
   pico_rand
   no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT})
