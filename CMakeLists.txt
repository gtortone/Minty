cmake_minimum_required(VERSION 3.13)

set(PROJECT Minty)

# set boards directory
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

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

target_compile_definitions(${PROJECT} PRIVATE "-DPICO_FLASH_SIZE_BYTES=${PICO_FLASH_SIZE_BYTES}")
target_compile_options(${PROJECT} PRIVATE "-DPICO_FLASH_SPI_CLKDIV=${PICO_FLASH_SPI_CLKDIV}")
#pico_set_linker_script(${PROJECT} ${CMAKE_CURRENT_SOURCE_DIR}/support/memmap_custom_16MB.ld)

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
   #${CMAKE_CURRENT_LIST_DIR}/src/diskio.c
   ${CMAKE_CURRENT_LIST_DIR}/src/msc_disk.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs_disk.c
   ${CMAKE_CURRENT_LIST_DIR}/src/flash_fs.c
   ${CMAKE_CURRENT_LIST_DIR}/src/hw_config.c
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
