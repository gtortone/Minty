
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CONFIG_VERSION        2
#define CONFIG_MAGIC_NUMBER   (uint16_t) 0xCAFE
#define CONFIG_FILENAME       "/sd/.Minty.cfg"

struct boardConfig {
   uint8_t version;
   uint16_t magicNumber;
   char lastPath[512];
   uint8_t ecs_volume;
}; 

#endif
