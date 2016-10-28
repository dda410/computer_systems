#include <stdio.h>

#include "library.h"

#define LOW_VOL 0.2
/* library's initialization function */
/* void _init() { */
/*   printf("Initializing library\n"); */
/* } */

int encode(char* buffer, int bufferlen) {
  int i;
  for (i = 0; i < bufferlen; i++) {
    buffer[i] = buffer[i] * LOW_VOL;
  }
  printf("The volume of the buffer has been decreased\n");
  return bufferlen;
}

int decode(char* buffer, int bufferlen) {
  return bufferlen;
}


/* library's cleanup function */
/* void _fini() { */
/*   printf("Cleaning out library\n"); */
/* } */

