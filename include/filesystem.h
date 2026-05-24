
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

typedef struct {
   unsigned int id;
   char isDir;
   char filename[21];            // limit filename to 20 chars for Inty display
} SCREEN_ENTRY;                  // 24 bytes = 256 entries in ~6kb

int entry_compare(const void *p1, const void *p2);
char *get_filename_ext(char *filename);
int is_rom_file(char *filename);
int is_valid_file(char *filename);
int read_directory(char *path, unsigned char *list);
void load_file(char *filename);
void load_file_by_id(unsigned int id, char *path, char *fullpath);
void filelist(SCREEN_ENTRY *en, int da, int a, int num);

#endif
