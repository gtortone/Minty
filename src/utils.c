
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

char* trim(char *s) {
   while (*s == ' ') s++;

   char *end = s + strlen(s) - 1;
   while (end > s && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
      *end = '\0';
      end--;
   }

   return s;
}

bool stralpha(char *s) {

   if (*s == '\0') 
      return false;

   while (*s) {
      if (isalnum((unsigned char)*s))
         return true;
      s++;
   }

   return false;
}

void to_lower(char *str) {
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

void hexdump(const void *data, size_t size) {

    const unsigned char *p = data;

    for (size_t i = 0; i < size; i += 16) {

        printf("%08zx  ", i);

        for (size_t j = 0; j < 16; j++) {
            if (i + j < size)
                printf("%02x ", p[i + j]);
            else
                printf("   ");

            if (j == 7)
                printf(" ");
        }

        printf(" |");

        for (size_t j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = p[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }

        printf("|\n");
    }
}
