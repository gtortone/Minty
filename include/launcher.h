#ifndef LAUNCHER_H_
#define LAUNCHER_H_

// Inty RAM addresses shared with cart firmware
#define TV_MODE_ADDR    0x0100      // 0: PAL, 1: NTSC
#define ECS_PRES_ADDR   0x0101      // 0: ECS absent, 1: ECS present
#define STATUS_ADDR     0x0119      // 0: Pi is ready, 1: Pi is Buzzy
#define DEV_ADDR        0x0120      // 0: flash, 1: SD
#define HAS_SD_ADDR     0x0121      // 0: no SD support, 1: SD support
#define BOARD_ID_ADDR   0x0122      // (see firmware)
#define SDPRES_ADDR     0x0123      // SD card presence (0: no SD Card present, 1: SD Card present)
#define ENTRY_LIST_ADDR 0x017F      // start of entry list array (10 entries of 64 characters each) going to 0x03FF
#define INFO_NUM_ADDR   0x0400      // address to store the number of info pages
#define INFO_DISP_ADDR  0x0401      // address to store the displayed info page
#define INFO_ADDR       0x0402      // start of info page 10 lines of 19 chars each, going to 0x04C0
#define CMD_ADDR        0x0889      // address used by INTY launcher to send command to pi
#define ERROR_ADDR      0x088A      // Used to send error code to Pi
#define SELECTION_ADDR  0x0899      // Used to send actual selected entry to Pi
#define ENTRY_TYPE_ADDR 0x1000      // start of entry type array (directory = 1 or file = 0)
#define FFROM_HI_ADDR   0x1028      // high byte of first displayed entries from current directory
#define FFROM_LO_ADDR   0x1029      // low byte of first displayed entries from current directory
#define FTO_HI_ADDR     0x1030      // high byte of last displayed entries from current directory
#define FTO_LO_ADDR     0x1031      // low byte of last displayed entries from current directory
#define FTOT_HI_ADDR    0x1032      // high byte of total number of entries from current directory
#define FTOT_LO_ADDR    0x1033      // low byte of total number of entries from current directory
#define PATH_ADDR       0x1100      // current path
#define SECTION_ADDR    0x1300      // current section

#define READ_PAGE       1
#define NEXT_PAGE       2
#define PREV_PAGE       3
#define UP_DIR          4

void RunLauncher();

#endif