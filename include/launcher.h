#ifndef LAUNCHER_H_
#define LAUNCHER_H_

// Inty RAM addresses shared with cart firmware
#define STATUS_ADDR     0x0119      // 0: Pi is ready, 1: Pi is Buzzy
#define DEV_ADDR        0x0120      // 0: flash, 1: SD
#define HAS_SD_ADDR     0x0121      // 0: no SD support, 1: SD support
#define BOARD_ID_ADDR   0x0122      // (see firmware)
#define SDPRES_ADDR     0x0123      // SD card presence (0: no SD Card present, 1: SD Card present)
#define ENTRY_LIST_ADDR 0x017f      // start of entry list array (10 entries of 64 characters each)
#define CMD_ADDR        0x0889      // address used by INTY launcher to send command to pi
#define SELECTION_ADDR  0x0899      // Used to send actual selected entry to Pi
#define ENTRY_TYPE_ADDR 0x1000      // start of entry type array (directory = 1 or file = 0)
#define FFROM_HI_ADDR   0x1028      // high byte of first displayed entries from current directory
#define FFROM_LO_ADDR   0x1029      // low byte of first displayed entries from current directory
#define FTO_HI_ADDR     0x1030      // high byte of last displayed entries from current directory
#define FTO_LO_ADDR     0x1031      // low byte of last displayed entries from current directory
#define FTOT_HI_ADDR    0x1032      // high byte of total number of entries from current directory
#define FTOT_LO_ADDR    0x1033      // low byte of total number of entries from current directory
#define PATH_ADDR       0x1100      // current path

#define READ_PAGE       1
#define NEXT_PAGE       2
#define PREV_PAGE       3
#define UP_DIR          4

void RunLauncher();

#endif