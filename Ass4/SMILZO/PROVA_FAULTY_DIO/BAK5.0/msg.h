#include <limits.h>

#ifndef _MSG_H_
#define _MSG_H_

#ifndef NAME_MAX  // The max filename under the current filesystem
#define NAME_MAX 255
#endif

#define BUFSIZE 1024
#define SUCCESS 0
#define AUDIO_FILE_NOT_FOUND -1
#define LIB_NOT_FOUND -2
#define LAST_CHUNK 0
#define STILL_READING_FILE 1

struct Firstmsg {
  char filename[NAME_MAX+1];  // plus one since NAME_MAX doesn't contain '\0'
  char libfile[NAME_MAX+1];
};

struct Audioconf {
  int channels, audio_size, audio_rate, error;
};

struct Datamsg {
  char buffer[BUFSIZE];
  int length, endflag;  // before eliminate endflag no padding should be added
  unsigned int msg_counter;
};

struct Acknowledgemnt {
  int ack;
};

#endif
