
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CONFIG_VERSION        1
#define CONFIG_MAGIC_NUMBER   (uint16_t) 0xCAFE
#define CONFIG_FILENAME       ".Minty.cfg"

struct boardConfig {
   uint8_t version;
   uint16_t magicNumber;
   char lastPath[512];
}; 

#endif
