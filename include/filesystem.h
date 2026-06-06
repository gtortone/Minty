
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

typedef struct {
   unsigned int id;
   char isDir;
   char filename[64];            // limit filename to 64 chars
} SCREEN_ENTRY;                  // 69 bytes per entry, 512 entries max = 35Kb

typedef struct {
   char section;
   char key[16];
   char value[64];
} INFO_ENTRY;                   // 81 bytes per entry

int entry_compare(const void *p1, const void *p2);
char *get_filename_ext(char *filename);
int is_rom_file(char *filename);
int is_valid_file(char *filename);
int read_directory(char *path, SCREEN_ENTRY *dst);
int load_file(char *filename);
int collect_info(char *filename, INFO_ENTRY *info_entries);
int collect_info_by_id(unsigned int id, char *path, INFO_ENTRY *info_entries);
int get_file_from_id(unsigned int id, char *path);
int load_file_by_id(unsigned int id, char *path);
void filelist(SCREEN_ENTRY *en, int from, int to, int num);

#endif
