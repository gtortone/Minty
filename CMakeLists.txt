cmake_minimum_required(VERSION 3.13)

set(PROJECT Minty)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# set boards directory
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

# import board build options
include(${CMAKE_SOURCE_DIR}/boards/${PICO_BOARD}.cmake)

if(DEFINED ENV{PICO_SDK_PATH})
   include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
else()
   include(pico_sdk_import.cmake)
endif()

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
    GIT_TAG v3.6.2
    SOURCE_DIR ${PICO_FATFS_DIR}
)

FetchContent_MakeAvailable(pico_fatfs)

#
# overwrite sd_card_spi.c to support SD <=2GB
#
file(COPY
    ${CMAKE_SOURCE_DIR}/src/sd_card_spi.c
    DESTINATION ${PICO_FATFS_DIR}/src/sd_driver/SPI
)

# disable warning on f_util.c
set_source_files_properties(
   ${PICO_FATFS_DIR}/src/src/f_util.c
   PROPERTIES COMPILE_OPTIONS "-Wno-format"
)
###

#
# fetch LittleFS library
#
set(LITTLEFS_DIR ${CMAKE_SOURCE_DIR}/lib/littlefs)

FetchContent_Declare(
    littlefs
    GIT_REPOSITORY https://github.com/littlefs-project/littlefs.git
    GIT_TAG master
    SOURCE_DIR ${LITTLEFS_DIR}
)

FetchContent_MakeAvailable(littlefs)

# disable warning on lfs.c
set_source_files_properties(
   ${LITTLEFS_DIR}/lfs.c
   PROPERTIES COMPILE_OPTIONS "-Wno-unused-function"
)
###

add_subdirectory(
   ${PICO_FATFS_DIR}/src
   ${LITTLEFS_DIR}
)

add_executable(${PROJECT})

set_target_properties(Minty PROPERTIES
    OUTPUT_NAME "Minty_${PICO_BOARD}"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   target_link_libraries(${PROJECT} pico_stdio_uart)
   pico_enable_stdio_uart(${PROJECT} 1)
   pico_enable_stdio_usb(${PROJECT} 1)
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

if(CONFIG_FLASH_FAT_STORAGE)
   target_sources(${PROJECT} PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/src/fatfs_disk.c
      ${CMAKE_CURRENT_LIST_DIR}/src/flash_fs.c
      ${CMAKE_CURRENT_LIST_DIR}/src/msc_disk.c
   )
elseif(CONFIG_FLASH_LFS_STORAGE)
   target_sources(${PROJECT} PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/lib/littlefs/lfs.c
      ${CMAKE_CURRENT_LIST_DIR}/lib/littlefs/lfs_util.c
      ${CMAKE_CURRENT_LIST_DIR}/src/pico_lfs.c
   )
endif()

if(PICO_RP2350)
   target_link_libraries(${PROJECT} pico_sha256)

endif()

target_compile_definitions(${PROJECT} PRIVATE "PICO_FLASH_SIZE_BYTES=${PICO_FLASH_SIZE_BYTES}")
target_compile_definitions(${PROJECT} PRIVATE "PICO_FLASH_SPI_CLKDIV=${PICO_FLASH_SPI_CLKDIV}")
target_compile_definitions(${PROJECT} PRIVATE "CONFIG_USB_DEVICE=${CONFIG_USB_DEVICE}")
target_compile_definitions(${PROJECT} PRIVATE "CONFIG_SD_STORAGE=${CONFIG_SD_STORAGE}")
target_compile_definitions(${PROJECT} PRIVATE "CONFIG_FLASH_FAT_STORAGE=${CONFIG_FLASH_FAT_STORAGE}")
target_compile_definitions(${PROJECT} PRIVATE "CONFIG_FLASH_LFS_STORAGE=${CONFIG_FLASH_LFS_STORAGE}")
target_compile_definitions(${PROJECT} PRIVATE "CONFIG_JLP=${CONFIG_JLP}")
target_compile_definitions(${PROJECT} PRIVATE "MAX_ROM_SIZE=${MAX_ROM_SIZE}")

if(CONFIG_FLASH_LFS_STORAGE)
   #target_compile_definitions(${PROJECT} PRIVATE "LFS_THREADSAFE=1")
   target_compile_definitions(${PROJECT} PRIVATE "LFS_NO_DEBUG=1")
   target_compile_definitions(${PROJECT} PRIVATE "LFS_THREADSAFE=1")
endif()

# options to enable FatFS SD debug
#target_compile_definitions(${PROJECT} PRIVATE "USE_PRINTF=1")
#target_compile_definitions(${PROJECT} PRIVATE "USE_DBG_PRINTF=1")

target_compile_options(${PROJECT} PRIVATE "-O2")
target_compile_options(${PROJECT} PRIVATE "-Wall")

set_property(TARGET ${PROJECT} APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

target_sources(${PROJECT} PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/src/main.c
   ${CMAKE_CURRENT_LIST_DIR}/src/cartridge.c
   ${CMAKE_CURRENT_LIST_DIR}/src/interface.c
   ${CMAKE_CURRENT_LIST_DIR}/src/filesystem.c
   ${CMAKE_CURRENT_LIST_DIR}/src/intellicart.c
   ${CMAKE_CURRENT_LIST_DIR}/src/jlpflash.c
   ${CMAKE_CURRENT_LIST_DIR}/src/memory.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs_disk.c
   ${CMAKE_CURRENT_LIST_DIR}/src/flash_fs.c
   ${CMAKE_CURRENT_LIST_DIR}/src/hw_config.c
   ${CMAKE_CURRENT_LIST_DIR}/src/utils.c
   ${CMAKE_CURRENT_LIST_DIR}/src/vfs.c
   ${CMAKE_CURRENT_LIST_DIR}/src/fatfs_backend.c
   ${CMAKE_CURRENT_LIST_DIR}/src/littlefs_backend.c
   ${CMAKE_CURRENT_LIST_DIR}/src/launcher.c
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
