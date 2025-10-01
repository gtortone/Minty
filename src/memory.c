#include "memory.h"

unsigned int romLen;
unsigned int ramfrom = 0;
unsigned int ramto = 0;
unsigned int mapfrom[80];
unsigned int mapto[80];
unsigned int maprom[80];
int mapdelta[80];
unsigned int mapsize[80];
unsigned int addrto[80];
unsigned int RAMused = 0;
unsigned int RAMwidth = 0;
unsigned int type[80];          // 0-rom / 1-page / 2-ram
unsigned int page[80];          // page number

int slot;
int hacks;

void config_memory(int cfg) {
   slot = 0;
   RAMused = 0;
   hacks = 0;

   switch (cfg) {

      case 0:
         mapfrom[0] = 0;
         mapto[0] = 0x1FFF;
         maprom[0] = 0x5000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         mapfrom[1] = 0x2000;
         mapto[1] = 0x2FFF;
         maprom[1] = 0xD000;
         addrto[1] = maprom[1] + (mapto[1] - mapfrom[1]);
         mapdelta[1] = maprom[1] - mapfrom[1];
         mapsize[1] = mapto[1] - mapfrom[1];
         type[1] = 0;
         page[1] = 0;

         mapfrom[2] = 0x3000;
         mapto[2] = 0x3FFF;
         maprom[2] = 0xF000;
         addrto[2] = maprom[2] + (mapto[2] - mapfrom[2]);
         mapdelta[2] = maprom[2] - mapfrom[2];
         mapsize[2] = mapto[2] - mapfrom[2];
         type[2] = 0;
         page[2] = 0;

         slot = 2;
         break;

      case 1:
         mapfrom[0] = 0;
         mapto[0] = 0x1FFF;
         maprom[0] = 0x5000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         mapfrom[1] = 0x2000;
         mapto[1] = 0x4FFF;
         maprom[1] = 0xD000;
         addrto[1] = maprom[1] + (mapto[1] - mapfrom[1]);
         mapdelta[1] = maprom[1] - mapfrom[1];
         mapsize[1] = mapto[1] - mapfrom[1];
         type[1] = 0;
         page[1] = 0;

         slot = 1;
         break;

      case 2:
         mapfrom[0] = 0;
         mapto[0] = 0x1FFF;
         maprom[0] = 0x5000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         mapfrom[1] = 0x2000;
         mapto[1] = 0x4FFF;
         maprom[1] = 0x9000;
         addrto[1] = maprom[1] + (mapto[1] - mapfrom[1]);
         mapdelta[1] = maprom[1] - mapfrom[1];
         mapsize[1] = mapto[1] - mapfrom[1];
         type[1] = 0;
         page[1] = 0;

         mapfrom[2] = 0x5000;
         mapto[2] = 0x5FFF;
         maprom[2] = 0xD000;
         addrto[2] = maprom[2] + (mapto[2] - mapfrom[2]);
         mapdelta[2] = maprom[2] - mapfrom[2];
         mapsize[2] = mapto[2] - mapfrom[2];
         type[2] = 0;
         page[2] = 0;

         slot = 2;
         break;

      case 3:
         mapfrom[0] = 0;
         mapto[0] = 0x1FFF;
         maprom[0] = 0x5000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         mapfrom[1] = 0x2000;
         mapto[1] = 0x3FFF;
         maprom[1] = 0x9000;
         addrto[1] = maprom[1] + (mapto[1] - mapfrom[1]);
         mapdelta[1] = maprom[1] - mapfrom[1];
         mapsize[1] = mapto[1] - mapfrom[1];
         type[1] = 0;
         page[1] = 0;

         mapfrom[2] = 0x4000;
         mapto[2] = 0x4FFF;
         maprom[2] = 0xD000;
         addrto[2] = maprom[2] + (mapto[2] - mapfrom[2]);
         mapdelta[2] = maprom[2] - mapfrom[2];
         mapsize[2] = mapto[2] - mapfrom[2];
         type[2] = 0;
         page[2] = 0;

         mapfrom[3] = 0x5000;
         mapto[3] = 0x5FFF;
         maprom[3] = 0xF000;
         addrto[3] = maprom[3] + (mapto[3] - mapfrom[3]);
         mapdelta[3] = maprom[3] - mapfrom[3];
         mapsize[3] = mapto[3] - mapfrom[3];
         type[3] = 0;
         page[3] = 0;

         slot = 3;
         break;

      case 4:
         mapfrom[0] = 0;
         mapto[0] = 0x1FFF;
         maprom[0] = 0x5000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         RAMused = 1;
         RAMwidth = 8;
         ramfrom = 0xD000;
         mapfrom[1] = 0xD000;
         mapto[1] = 0xD3FF;
         maprom[1] = 0xD000;
         addrto[1] = 0xD3FF;
         mapdelta[1] = maprom[1] - mapfrom[1];
         mapsize[1] = mapto[1] - mapfrom[1];
         type[1] = 2;
         page[1] = 0;

         slot = 1;
         break;

      case 5:
         mapfrom[0] = 0;
         mapto[0] = 0x2FFF;
         maprom[0] = 0x5000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         mapfrom[1] = 0x3000;
         mapto[1] = 0x5FFF;
         maprom[1] = 0x9000;
         addrto[1] = maprom[1] + (mapto[1] - mapfrom[1]);
         mapdelta[1] = maprom[1] - mapfrom[1];
         mapsize[1] = mapto[1] - mapfrom[1];
         type[1] = 0;
         page[1] = 0;

         slot = 1;
         break;

      case 6:
         mapfrom[0] = 0;
         mapto[0] = 0x1FFF;
         maprom[0] = 0x6000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         slot = 0;
         break;

      case 7:
         mapfrom[0] = 0;
         mapto[0] = 0x1FFF;
         maprom[0] = 0x4800;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         slot = 0;
         break;

      case 8:
         mapfrom[0] = 0;
         mapto[0] = 0x0FFF;
         maprom[0] = 0x5000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         mapfrom[1] = 0x1000;
         mapto[1] = 0x1FFF;
         maprom[1] = 0x7000;
         addrto[1] = maprom[1] + (mapto[1] - mapfrom[1]);
         mapdelta[1] = maprom[1] - mapfrom[1];
         mapsize[1] = mapto[1] - mapfrom[1];
         type[1] = 0;
         page[1] = 0;

         slot = 1;
         break;

      case 9:
         mapfrom[0] = 0;
         mapto[0] = 0x1FFF;
         maprom[0] = 0x5000;
         addrto[0] = maprom[0] + (mapto[0] - mapfrom[0]);
         mapdelta[0] = maprom[0] - mapfrom[0];
         mapsize[0] = mapto[0] - mapfrom[0];
         type[0] = 0;
         page[0] = 0;

         mapfrom[1] = 0x2000;
         mapto[1] = 0x3FFF;
         maprom[1] = 0x9000;
         addrto[1] = maprom[1] + (mapto[1] - mapfrom[1]);
         mapdelta[1] = maprom[1] - mapfrom[1];
         mapsize[1] = mapto[1] - mapfrom[1];
         type[1] = 0;
         page[1] = 0;

         mapfrom[2] = 0x4000;
         mapto[2] = 0x4FFF;
         maprom[2] = 0xD000;
         addrto[2] = maprom[2] + (mapto[2] - mapfrom[2]);
         mapdelta[2] = maprom[2] - mapfrom[2];
         mapsize[2] = mapto[2] - mapfrom[2];
         type[2] = 0;
         page[2] = 0;

         mapfrom[3] = 0x5000;
         mapto[3] = 0x5FFF;
         maprom[3] = 0xF000;
         addrto[3] = maprom[3] + (mapto[3] - mapfrom[3]);
         mapdelta[3] = maprom[3] - mapfrom[3];
         mapsize[3] = mapto[3] - mapfrom[3];
         type[3] = 0;
         page[3] = 0;

         RAMused = 1;
         RAMwidth = 8;
         ramfrom = 0x8800;
         mapfrom[4] = 0x8800;
         mapto[4] = 0x8FFF;
         maprom[4] = 0x8800;
         addrto[4] = 0x8FFF;
         mapdelta[4] = maprom[4] - mapfrom[4];
         mapsize[4] = mapto[4] - mapfrom[4];
         type[4] = 2;
         page[4] = 0;

         slot = 4;
         break;

      default:
         break;
   }
}

