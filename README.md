# Minty
MultiCart for Mattel Intellivision

## Introduction

Multi-cart based on Raspberry Pi Pico hardware and PiRTOII firmware (https://github.com/aotta/PiRTOII)

## Features

- Pico C/C++ code refactoring
- Intybasic code refactoring
- no more config files for "official" ITV titles
- new navigation keys for UI ROM
- new color schema for UI ROM
- support for long filenames (up to 255 chars)
- move from 64 to 512 max number of files for directory
- SD support (with sukkopera board: https://github.com/SukkoPera/PiRTOII)

## Getting started

To simply program Pi Pico:
- connect it to your PC/laptop using an USB-C cable while pressing Pico on-board button (boot MODE)
- drag and drop inside root directory `Minty.default_board.uf2` (for PirtoII default board) or `Minty.sd_board.uf2` (for PirtoII board with microSD slot)

Setup ROMs:
- copy your ITV ROM to flash (or microSD) root directory
- if ROM name is included in [cfg/0game-maps.csv](cfg/0game-maps.csv) you do not need to add config (.cfg) file
- enjoy your PiRTOII cart with Minty firmware !

User can switch flash/SD storage device using controller key.

## User interface

### Controller keys

<div align="center">
   <img src="images/controller.png"/>
</div>

## Build (TL;DR)

### Requirements

- Pico-SDK (https://github.com/raspberrypi/pico-sdk)
- CMake (>3.13)

### Commands

```
mkdir build
cd build
cmake -DPIRTOII_VARIANT=DEFAULT_BOARD ..   # for default PirtoII board
   or
cmake -DPIRTOII_VARIANT=SD_BOARD ..       # for PirtoII board with microSD slot
make
```

After compilation `Minty.bin` and `Minty.uf2` are generated.

To use `Minty.bin` to flash PiRTOII cart `picotool` is required. Run following
command after setting Pi Pico in BOOT mode:

```
picotool load -f Minty.bin; picotool reboot
```

## Credits

Thanks a lot to Andrea Ottaviani for his PiRTOII firmware (https://github.com/aotta/PiRTOII) and PCB files.

Thanks a lot to sukkopera for his PirtoII board with microSD card slot (https://github.com/SukkoPera/PiRTOII)










