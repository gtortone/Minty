
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

typedef struct {
   unsigned int id;
   char isDir;
   char filename[64];            // limit filename to 64 chars
} SCREEN_ENTRY;                  // 69 bytes per entry, 512 entries max = 35Kb

int entry_compare(const void *p1, const void *p2);
char *get_filename_ext(char *filename);
int is_rom_file(char *filename);
int is_valid_file(char *filename);
int read_directory(char *path, SCREEN_ENTRY *dst);
void load_file(char *filename);
void load_file_by_id(unsigned int id, char *path, char *fullpath);
void filelist(SCREEN_ENTRY *en, int da, int a, int num);

#endif
