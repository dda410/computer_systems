#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define CHUNK 1024

/* This function prints the buffer received as argument reversely*/
void reverse_print_buffer(char *pointer, int characters_to_print) {
  int i = characters_to_print -1;
  for (; i >= 0; i--) {
    printf("%c", pointer[i]);
  }
}

void read_buffer(int file_descriptor, size_t nbytes) {
  char *buf = (char *)malloc(CHUNK);
  if (buf == NULL) {
    fprintf(stderr, "Error while allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  ssize_t bytes_read = read(file_descriptor, buf, nbytes);
  if ( ( (int) bytes_read) < 0) {
    fprintf(stderr, "Error while reading file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  /* The exit condition of the recursion consist in the file being completely read.
     This happens when the bytes read are less then the buffer size */
  if (bytes_read < CHUNK) {
    reverse_print_buffer(buf, (int) bytes_read);
    free(buf);
    return;
  }
  read_buffer(file_descriptor, nbytes);
  reverse_print_buffer(buf, (int) bytes_read);
  free(buf);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: reverse <input_file>\n\n");
    exit(EXIT_FAILURE);
  }
  size_t nbytes;
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Error while opening file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  nbytes = CHUNK;
  read_buffer(fd, nbytes);
  int closing_status = close(fd);
  if (closing_status < 0) {
    fprintf(stderr, "Error while closing file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  return 0;
}

