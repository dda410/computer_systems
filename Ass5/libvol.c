#include <stdio.h>

#include "library.h"

#define DOWN_VOL 0.2
#define UP_VOL 1.2
#define UP_OPTION 'i'
#define DOWN_OPTION 'd'
/* library's initialization function */
/* void _init() { */
/*   printf("Initializing library\n"); */
/* } */

int encode(char* buffer, int bufferlen, char option) {
  int i;
  printf("This is encode and this is the option: %c\n", option);
  if (option == DOWN_OPTION) {
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] * DOWN_VOL;
    }
    printf("The volume of the buffer has been decreased\n");
  } else if (option == UP_OPTION) {
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] * UP_VOL;
    }
    printf("The volume of the buffer has been increased\n");
  }
  return bufferlen;
}

int decode(char* buffer, int bufferlen, char option) {
  return bufferlen;
}


/* library's cleanup function */
/* void _fini() { */
/*   printf("Cleaning out library\n"); */
/* } */

