
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "filesystem.h"
#include "intellicart.h"
#include "memory.h"
#include "vfs.h"
#include "launcher.h"

#if PICO_RP2350
   #include "pico/sha256.h"
#endif

extern Cartridge cart;

extern struct mapEntry slots[NSLOTS];

int entry_compare(const void *p1, const void *p2) {
   SCREEN_ENTRY *e1 = (SCREEN_ENTRY *) p1;
   SCREEN_ENTRY *e2 = (SCREEN_ENTRY *) p2;

   if (e1->isDir && !e2->isDir)
      return -1;
   else if (!e1->isDir && e2->isDir)
      return 1;
   else
      return strncasecmp(e1->filename, e2->filename, 64 );
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

int read_directory(char *path, SCREEN_ENTRY *dst) {

   unsigned int id = 0;
   int n = 0;
   SCREEN_ENTRY *orig_dst = dst;
   vfs_dirent_t ent;

   vfs_dir_t *dir = vfs_opendir(path);
   if (!dir) {
      printf("read_directory: vfs_opendir error (%s)\n", path);
      return -1;
   };

   // stop at 512 entries to avoid overflowing the buffer
   while ((vfs_readdir(dir, &ent) > 0) & (n<512)) {

      if ((strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0))
         continue;

      if ( (ent.type & VFS_TYPE_HIDDEN) || (ent.type & VFS_TYPE_SYSTEM) )
         continue;

      dst->isDir = (ent.type & VFS_TYPE_DIR) ? 1 : 0;

      if (!dst->isDir)
         if (!is_valid_file(ent.name))
            continue;

      dst->id = id++;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
      // 64 chars to launcher display width
      strncpy(dst->filename, ent.name, 64);
#pragma GCC diagnostic pop

      dst++;
      n++;
   }
   vfs_closedir(dir);

   qsort(orig_dst, n, sizeof(SCREEN_ENTRY), entry_compare);
   
   return n;
}

int load_file(char *filename) {

   unsigned int size = 0;
   unsigned char buf[2];
   vfs_stat_t st;

   if (vfs_stat(filename, &st) == 0) {
      printf("file size: %d bytes\n", st.size);
      if (st.size > MAX_ROM_SIZE*2) {
         printf("file size exceeds maximum supported size of %d bytes\n", MAX_ROM_SIZE*2);
         return -2;
      }
   }
   
   printf("load_file(): filename %s\n", filename);
   vfs_file_t *f = vfs_open(filename, "r");

   if (f == NULL) {
      printf("load_file %s error\n", filename);
      return -1;
   }

   // init cartridge
   init_cart();

   // handle Intellicart rom file
   if(is_rom_file(filename)) {

      uint32_t from, prev_from;
      uint16_t prev_size, target;
      char inputBuffer[3];

      cleanSlots();
      cleanHoles();

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
         //printf("from: 0x%lX  to: 0x%lX  target: 0x%X\n", from, from + (hi - lo) - 1, target);

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

      // check for metadata section 
      if (!vfs_eof(f)) {

         printf("ROM has metadata section @%d\n", size);
         //printf("current file position: %d\n", vfs_tell(f));

         while (!vfs_eof(f)) {

            vfs_read(f, inputBuffer, 1);

            // find number of bytes in length
            int nb = inputBuffer[0] >> 6;

            // get 6 LSBs of length
            int len = inputBuffer[0] & 0x3F;

            // process additional bytes
            for(int i=0; i<nb; i++) {
               vfs_read(f, inputBuffer, 1);
               len |= inputBuffer[0] << (6 + i*8);
            }

            //printf("metadata len: %d, nb: %d\n", len, nb);

            // read tag code
            vfs_read(f, inputBuffer, 1);

            int tag = inputBuffer[0];
            //printf("\ttag: 0x%X\n", tag);

            if (tag == 0x06) {       // Game Attribute / Compatibility Flags

               vfs_read(f, inputBuffer, 3);  // skip first 3 bytes to search JLP attributes
            
               if(len > 3) {

                  vfs_read(f, inputBuffer, 2);

                  //printf("tag 0x06:  byte[3]: 0x%X, byte[4]: 0x%X\n", inputBuffer[0], inputBuffer[1]);

                  int jlp_value = inputBuffer[0] >> 6;
                  int jlpflash_value = inputBuffer[1];

                  config_jlp(jlp_value, jlpflash_value, filename);
               }

            } else {

               // skip metadata info
               for(int i=0; i<len; i++) 
                  vfs_read(f, inputBuffer, 1);
            }

            // skip metadata crc
            vfs_read(f, inputBuffer, 2);
         }
      }

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
   vfs_close(f);

   printf("load_file: size: %ld bytes\n", cart.len*2);
   return 0;
}

int load_file_by_id(unsigned int id, char *path) {

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

         if ( (ent.type & VFS_TYPE_HIDDEN) || (ent.type & VFS_TYPE_SYSTEM) )
            continue;

         if ( !(ent.type & VFS_TYPE_DIR) )
            if (!is_valid_file(ent.name))
               continue;

         if (i++ == id) {
            strcat(path, "/");
            strcat(path, ent.name);

            vfs_closedir(dir);
            printf("load_file_by_id: id %d, opening %s\n", id, path);
            int result = load_file(path);
            if (result != 0) {
               printf("load_file_by_id: failed to load %s\n", path);
               // need to remove the file from the path since it was not loaded successfully
               char *last_slash = strrchr(path, '/');
               if (last_slash)                  
                  *last_slash = 0; 
            }
            return result;
         }
      }
      vfs_closedir(dir);
   }
   return -1;
}

void filelist(SCREEN_ENTRY *en, int from, int to, int num) {
   for (int n = 0; n < (to - from); n++) {
      cart.RAM[ENTRY_TYPE_ADDR + n] = en[n + from].isDir;
      
      for (int i = 0; i < 64; i++) {
         int pos = ENTRY_LIST_ADDR + i + (n * 64);
         cart.RAM[pos] = en[n + from].filename[i];
         if (cart.RAM[pos] < 32)
            // 255 to indicate end of string if shorter than 64
            cart.RAM[pos] = 255;
		   else
            // convert to INTY numbering here (much faster than on INTY side)
            // only ascii chars from 32 to 127 are displayed and are mapped to 0 - 95 in lookup table
			   cart.RAM[pos] = (cart.RAM[pos] & 0x7F) - 32;
      }
   }
   cart.RAM[FFROM_HI_ADDR] = (from & 0xFF00) >> 8;  // MSB
   cart.RAM[FFROM_LO_ADDR] = (from & 0x00FF);       // LSB
   cart.RAM[FTO_HI_ADDR  ] = (to   & 0xFF00) >> 8;  // MSB
   cart.RAM[FTO_LO_ADDR  ] = (to   & 0x00FF);       // LSB
   cart.RAM[FTOT_HI_ADDR ] = (num  & 0xFF00) >> 8;  // MSB
   cart.RAM[FTOT_LO_ADDR ] = (num  & 0x00FF);       // LSB
}

