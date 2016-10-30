#include <stdio.h>

#include "library.h"

#define DOWN_VOL 0.2
#define UP_VOL 1.4
#define UP_OPTION 'i'
#define DOWN_OPTION 'd'

/* Increseases or decreases the volume accordingly to the option parameter */
int encode(char* buffer, int bufferlen, char option) {
  int i;
  if (option == DOWN_OPTION) {
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] * DOWN_VOL;
    }
  } else if (option == UP_OPTION) {
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] * UP_VOL;
    }
  }
  return bufferlen;
}

int decode(char* buffer, int bufferlen, char option) {
  return bufferlen;
}
