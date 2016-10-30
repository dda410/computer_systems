#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "msg.h"


/* In the case of error this macro prints to stderr the passed expression followed by the errno description */
#define error_handling(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)
#define DEST_PORT 1234

/* returns the difference between two timeval structs in seconds */
double elapsed_time(struct timeval start, struct timeval end) {
  return (double) ( ( (start.tv_sec - end.tv_sec) * 1000000L + end.tv_usec) - start.tv_usec) /1000000L;
}

void parse_arguments(int argc) {
  if (argc != 2) {
    fprintf(stderr, "Usage: pingclient <hostname>\n\n");
    exit(EXIT_FAILURE);
  }
}

struct in_addr* get_IP(char *hostname) {
  struct hostent *resolve = gethostbyname(hostname);
  if (resolve == NULL) {
    fprintf(stderr, "Error while resolving address: %s not found.\n", hostname);
    exit(EXIT_FAILURE);
  }
  return (struct in_addr*) resolve->h_addr_list[0];
}

void set_dest(struct sockaddr_in *dest, struct in_addr *addr) {
  dest->sin_family = AF_INET;
  dest->sin_port = htons(DEST_PORT);
  dest->sin_addr = *addr;
}

int main(int argc, char *argv[]) {
  parse_arguments(argc);
  char* hostname = argv[1];
  int fd, err;
  struct Firstmsg msg;
  struct timeval start_time;
  struct timeval end_time;
  struct sockaddr_in dest;
  struct in_addr *addr;
  socklen_t dest_len = sizeof(struct sockaddr_in);
  strncpy(msg.filename, "cas3tle_a.wav",30);
  strncpy(msg.libfile, "dio cane aaa libreria",30); 
  addr = get_IP(hostname);
  /* Creating the socket */
  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  error_handling(fd, "Error while creating the socket");
  /* Initializing the destination server values: port, address and IP protocol version */
  set_dest(&dest, addr);
  err = gettimeofday(&start_time, NULL);
  error_handling(err, "Error while getting time of the day");
  err = sendto(fd, &msg, sizeof(struct Firstmsg), 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending datagram to host");
  /* err = recvfrom(fd, msg, (size_t) sizeof(msg), 0, (struct sockaddr*) &dest, &dest_len); */
  error_handling(err, "Error while receiving datagram from host");
  err = gettimeofday(&end_time, NULL);
  error_handling(err, "Error while getting time of the day");
  err = printf("TheRTTwas: %f seconds.\n", elapsed_time(start_time, end_time));
  if (err < 0) {
    fprintf(stderr, "Error while printing to standard output.\n");
    exit(EXIT_FAILURE);
  }
  err = close(fd);
  error_handling(err, "Error while closing the socket");
  return 0;
}
