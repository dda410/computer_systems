#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "msg.h"

#include "library.h"

#define ONE_TIME_PAD 'o'
#define VIGENERE 'v'
#define KEY_FILE "keyfile"
#define VIG_KEY_LEN 20

static int otp_key[BUFSIZE];
static int vig_key[VIG_KEY_LEN];
static int generate_key = 0;
static int read_key = 0;

/* Randomly creates the key of the requested algorithm and stores it in a file */
void init(char c) {
  int i;
  FILE *f = fopen(KEY_FILE, "w");
  if (f == NULL) {
    fprintf(stderr, "Error opening file.\n");
    exit(EXIT_FAILURE);
  }
  if (c == ONE_TIME_PAD) {
    for (i = 0; i < BUFSIZE; i++) {
      otp_key[i] = (rand() % 256);
      fprintf(f, "%d,", otp_key[i]);
    }
  } else if (c == VIGENERE) {
    for (i = 0; i < VIG_KEY_LEN; i++) {
      vig_key[i] = (rand() % 256);
      fprintf(f, "%d,", vig_key[i]);
    }
  }
  if (fclose(f) < 0) {
    fprintf(stderr, "Error while closing the file.\n");
    exit(EXIT_FAILURE);
  }
}

/* Encodes the data stream accordingly to the requested algorithm */
int encode(char* buffer, int bufferlen, char option) {
  if (generate_key == 0) {
    generate_key++;
    init(option);
  }
  printf("This is generate_key: %d\n", generate_key);
  int i;
  if (option == ONE_TIME_PAD) {
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] + otp_key[i];
    }
    printf("The buffer has been encoded with one time pad algorithm\n");
  } else  if (option == VIGENERE) {
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] + vig_key[i % VIG_KEY_LEN];
    }
    printf("The buffer has been encoded with vigenere algorithm\n");
  }
  return bufferlen;
}

/* Reads the key created by the server */
void read_table(char c) {
  int i;
  FILE *f = fopen(KEY_FILE, "r");
  if (f == NULL) {
    fprintf(stderr, "Error opening file.\n");
    exit(EXIT_FAILURE);
  }
  if (c == ONE_TIME_PAD) {
    for (i = 0; i < BUFSIZE; i++) {
      fscanf(f, "%d,", &otp_key[i]);
    }
  } else if (c == VIGENERE) {
    for (i = 0; i < VIG_KEY_LEN; i++) {
      fscanf(f, "%d,", &vig_key[i]);
    }
  }
  if (fclose(f) < 0) {
    fprintf(stderr, "Error while closing the file.\n");
    exit(EXIT_FAILURE);
  }
}

/* Decodes the received cryted stream */
int decode(char* buffer, int bufferlen, char option) {
  if (read_key == 0) {
    read_key++;
    read_table(option);
  }
  int i;
  if (option == ONE_TIME_PAD) {
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] - otp_key[i];
    }
    printf("The buffer has been decoded with one time pad algorithm\n");
  } else  if (option == VIGENERE) {
    for (i = 0; i < bufferlen; i++) {
      buffer[i] = buffer[i] - vig_key[i % VIG_KEY_LEN];
    }
    printf("The buffer has been decoded with vigenere algorithm\n");
  }
  return bufferlen;
}
