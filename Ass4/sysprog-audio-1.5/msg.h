#ifndef _MSG_H_
#define _MSG_H_

#define BUFSIZE 1024
#define MAX_FILENAME_SIZE 256  // max size of a filename on most linux filesystems

struct Firstmsg {
  char filename[MAX_FILENAME_SIZE];
  char libfile[MAX_FILENAME_SIZE];
};

struct Audioconf {
  int channels, audio_size, audio_rate, errorcode;
};

struct Datamsg {
  char buffer[BUFSIZE];
  int length, endflag;
  unsigned int msg_counter;
};

struct Acknowledgemnt {
  int ack;
};

#endif
