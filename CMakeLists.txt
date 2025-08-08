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

target_compile_options(${PROJECT} PRIVATE "-O3")
target_compile_options(${PROJECT} PRIVATE "-Wunused-variable")
target_compile_options(${PROJECT} PRIVATE "-DCFG_TUSB_RHPORT0_MODE")

target_sources(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/src/inty_cart.c
    ${CMAKE_CURRENT_LIST_DIR}/src/memory.c
    ${CMAKE_CURRENT_LIST_DIR}/src/msc_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/src/flash_fs.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs/diskio.c
) 

target_include_directories(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src/fatfs
)

# Disable usb output, enable uart output
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 1)

# In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
# for TinyUSB device support
target_link_libraries(${PROJECT} PUBLIC pico_stdlib hardware_flash tinyusb_device pico_multicore)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT})


