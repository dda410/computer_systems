#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "msg.h"

#include "library.h"

#define KEY_FILE "keyfile"
#define printf_error_handling(err) ( (err) < 0 ? fprintf(stderr, "Error while printing to standard output.\n"), exit(EXIT_FAILURE) : 0)

static int otp_key[BUFSIZE];
static int generate_key = 0;
static int read_key = 0;

/* Randomly creates the key of the requested algorithm and stores it in a file */
void init() {
  int err = printf("The buffers will be encrypted with the one time pad algorithm\n");
  printf_error_handling(err);
  int i;
  FILE *f = fopen(KEY_FILE, "w+");
  if (f == NULL) {
    fprintf(stderr, "Error while opening file.\n");
    exit(EXIT_FAILURE);
  }
    for (i = 0; i < BUFSIZE; i++) {
      otp_key[i] = (rand() % 256);
      fprintf(f, "%d,", otp_key[i]);
    }
  err = fclose(f);
  if (err < 0) {
    fprintf(stderr, "Error while closing file.\n");
  }
}

/* Encrypts the data stream accordingly to the requested algorithm */
void encode(char* buffer, int bufferlen, char option) {
  if (generate_key == 0) {
    generate_key++;
    init(option);
  }
  int i;
    for (i = 0; i < BUFSIZE; i++) {
      buffer[i] = buffer[i] + otp_key[i];
    }
}

/* Reads the key created by the server */
void read_table() {
  int err = printf("The buffers will be decrypted with the one time pad algorithm\n");
  printf_error_handling(err);
  int i;
  FILE *f = fopen(KEY_FILE, "r");
  if (f == NULL) {
    fprintf(stderr, "Error while opening file.\n");
    exit(EXIT_FAILURE);
  }
    for (i = 0; i < BUFSIZE; i++) {
      fscanf(f, "%d,", &otp_key[i]);
    }
  err = fclose(f);
  if (err < 0) {
    fprintf(stderr, "Error while closing file.\n");
  }
}

/* Decrypts the received cryted stream */
void decode(char* buffer, int bufferlen, char option) {
  if (read_key == 0) {
    read_key++;
    read_table(option);
  }
  int i;
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] - otp_key[i];
    }
}
