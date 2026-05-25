
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "filesystem.h"
#include "intellicart.h"
#include "memory.h"
#include "vfs.h"

#if PICO_RP2350
   #include "pico/sha256.h"
#endif

extern Cartridge cart;

extern struct mapEntry slots[NSLOTS];

extern const unsigned int base;

int entry_compare(const void *p1, const void *p2) {
   SCREEN_ENTRY *e1 = (SCREEN_ENTRY *) p1;
   SCREEN_ENTRY *e2 = (SCREEN_ENTRY *) p2;

   if (e1->isDir && !e2->isDir)
      return -1;
   else if (!e1->isDir && e2->isDir)
      return 1;
   else
      return strcasecmp(e1->filename, e2->filename);
}

char *get_filename_ext(char *filename) {
   char *dot = strrchr(filename, '.');

   if (!dot || dot == filename)
      return "";
   return dot + 1;
}

int is_rom_file(char *filename) {
   char inputBuffer[3];
   vfs_file_t *f;

   if ( (f = vfs_open(filename, "r")) == NULL) {
      printf("load_file %s error\n", filename);
      return -1;
   }

   vfs_read(f, inputBuffer, sizeof(inputBuffer));
   vfs_close(f);

   return !((inputBuffer[0] != 0xA8 && (inputBuffer[0] & ~0x20) != 0x41) ||
         (inputBuffer[1] ^ inputBuffer[2]) != 0xFF);
}

int is_valid_file(char *filename) {
   char *ext = get_filename_ext(filename);

   // .BIN, .INT, .ITV files are raw image ROMs
   // .ROM files are Intellicart ROMs
   return (strcasecmp(ext, "BIN") == 0 || strcasecmp(ext, "INT") == 0 || 
         strcasecmp(ext, "ITV") == 0 || strcasecmp(ext, "ROM") == 0);
}

int read_directory(char *path, unsigned char *list) {

   unsigned int id = 0;
   int n = 0;
   SCREEN_ENTRY *dst = (SCREEN_ENTRY *) & list[0];
   vfs_dirent_t ent;

   vfs_dir_t *dir = vfs_opendir(path);
   if (!dir) {
      printf("read_directory: vfs_opendir error (%s)\n", path);
      return 0;
   };

   while (vfs_readdir(dir, &ent) > 0) {

      if ((strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0))
         continue;

      if ( (ent.type & VFS_TYPE_HIDDEN) || (ent.type & VFS_TYPE_SYSTEM) )
         continue;

      dst->isDir = (ent.type & VFS_TYPE_DIR) ? 1 : 0;

      if (!dst->isDir)
         if (!is_valid_file(ent.name))
            continue;

      dst->id = id++;
      // 20 chars to launcher display width
      strncpy(dst->filename, ent.name, 20);
      dst->filename[20] = 0;

      dst++;
      n++;
   }
   vfs_closedir(dir);

   qsort((SCREEN_ENTRY *) & list[0], n, sizeof(SCREEN_ENTRY), entry_compare);
   
   return n;
}

void load_file(char *filename) {

   unsigned int size = 0;
   unsigned char buf[2];

   printf("load_file(): filename %s\n", filename);
   vfs_file_t *f = vfs_open(filename, "r");

   if (f == NULL) {
      printf("load_file %s error\n", filename);
      return;
   }
  
   // init cartridge
   init_cart();

   // handle Intellicart rom file
   if(is_rom_file(filename)) {

      uint32_t from, prev_from;
      uint16_t prev_size, target;
      char inputBuffer[3];

      vfs_read(f, inputBuffer, sizeof(inputBuffer));
      
      // read number of segments
      int slots = inputBuffer[1];

      for(int i=0; i<slots; i++) {

         vfs_read(f, inputBuffer, 2);

         int lo = inputBuffer[0] << 8;
         int hi = (inputBuffer[1] << 8) + 0x100;

         target = lo;
         if (i == 0)
            from = 0x0000;
         else
            from = prev_from + prev_size + 1;

         prev_size = (hi - lo) - 1;
         prev_from = from;

         addSlot(from, from + (hi - lo) - 1, target, 0, ROM_SLOT);

         for (int j = lo; j < hi; j++) {

            vfs_read(f, inputBuffer, 2);
            cart.ROM[size] = inputBuffer[1] | (inputBuffer[0] << 8);
            size++;
         }

         // skip CRC (2 bytes)
         vfs_read(f, inputBuffer, 2);
      }

      // read memory block (2Kb) attributes
      char memattr[50];

      vfs_read(f, memattr, sizeof(memattr));

      for (int i = 0; i < 32; i++) {
         
         int attr = 0xF & (memattr[(i >> 1)] >> ((i & 1) * 4));
         int lohi = memattr[16 + ((i >> 1) | ((i & 1) << 4))];
         int lo   = (lohi >> 4) & 0x7;
         int hi   = (lohi & 0x7) + 1;

         // check if memory block has write attribute
         if(attr & 0x02) { 

            mapType type;

            if(attr & 0x04)
               type = RAM8_SLOT;   // narrow flag (8-bit)
            else
               type = RAM16_SLOT;

            addSlot(i * 0x800, (i * 0x800) + ((hi - lo) * 0x100) - 1, 0, 0, type);
         }
      }

      getRAMRange(&cart.ramfrom, &cart.ramto, &cart.ramwidth);

   } else { 

      // handle raw rom file

#if PICO_RP2350
      pico_sha256_state_t state;
      sha256_result_t result;
      pico_sha256_start_blocking(&state, SHA256_BIG_ENDIAN, true);
#endif

      // read the file to SRAM
      while (!(vfs_eof(f))) {
         vfs_read(f, buf, sizeof(buf));
         cart.ROM[size] = buf[1] | (buf[0] << 8);
         size++;
      }

#if PICO_RP2350
      pico_sha256_update_blocking(&state, (const uint8_t*)cart.ROM, (size*2) - 1);
      pico_sha256_finish(&state, &result);
      
      printf("SHA256: ");
      for (int i = 0; i < SHA256_RESULT_BYTES; i++)
         printf("%02x", result.bytes[i]);
      printf("\n");
#endif
   }

   cart.len = size;
   cart.RAM[base + 202] = cart.len;
   vfs_close(f);

   printf("load_file: size: %ld bytes\n", cart.len*2);
}

void load_file_by_id(unsigned int id, char *path, char *fullpath) {

   unsigned int i = 0;
   vfs_dir_t *dir;
   vfs_dirent_t ent;

   printf("id: %d, path: %s\n", id, path);

   if ( (dir = vfs_opendir(path)) != NULL) {

      while (1) {

         if (vfs_readdir(dir, &ent) <= 0)
            break;

         // skip '.', '..' and hidden files
         if ((strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0) || ent.name[0] == '.')
            continue;

         if ( !(ent.type & VFS_TYPE_DIR) )
            if (!is_valid_file(ent.name))
               continue;

         sprintf(fullpath, "%s/%s", path, ent.name);

         if (i++ == id) {
            vfs_closedir(dir);
            printf("load_file_by_id: id %d, opening %s\n", id, fullpath);
            load_file(fullpath); 
            return;
         }
      }
      vfs_closedir(dir);
   } 
}

void filelist(SCREEN_ENTRY *en, int from, int to, int num) {
   int base = 0x17f;

   for (int n = 0; n < (to - from); n++) {
      cart.RAM[0x1000 + n] = en[n + from].isDir;
      
      for (int i = 0; i < 20; i++) {
         int pos = base + i + (n * 20);
         cart.RAM[pos] = en[n + from].filename[i];
         if (cart.RAM[pos] <= 32)
            cart.RAM[pos] = 0;
		 else
			cart.RAM[pos] -= 32;
      }
   }
   cart.RAM[0x1028] = (from & 0xFF00) >> 8;   // MSB
   cart.RAM[0x1029] = (from & 0x00FF);        // LSB
   cart.RAM[0x1030] = (to & 0xFF00) >> 8;    // MSB
   cart.RAM[0x1031] = (to & 0x00FF);         // LSB
   cart.RAM[0x1032] = (num & 0xFF00) >> 8;  // MSB
   cart.RAM[0x1033] = (num & 0x00FF);       // LSB
}

