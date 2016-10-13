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

/* In the case of error this macro prints to stderr the passed expression followed by the errno description */
#define error_checker(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)
#define DEST_PORT 1234
#define WAITING_TIME 1

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

int wait_for_response(fd_set *set, int fd, struct timeval *t) {
  /* Initializing the read set used by the select function to monitor the sockets */
  FD_ZERO(set);
  FD_SET(fd, set);
  t->tv_sec = WAITING_TIME;
  t->tv_usec = 0;
  return select(fd+1, set, NULL, NULL, t);
}

/* returns the difference between two timeval structs in seconds */
double elapsed_time(struct timeval start, struct timeval end) {
  return (double) ( ( (start.tv_sec - end.tv_sec) * 1000000L + end.tv_usec) - start.tv_usec) /1000000L;
}

int main(int argc, char *argv[]) {
  parse_arguments(argc);
  char* hostname = argv[1];
  int fd, err;
  fd_set read_set;
  char msg[64] = "message";
  struct timeval start_time, end_time, timeout;
  struct sockaddr_in dest;
  struct in_addr *addr;
  socklen_t dest_len = sizeof(struct sockaddr_in);
  addr = get_IP(hostname);
  /* Creating the socket */
  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  error_checker(fd, "error while creating the socket");
  /* Initializing the destination server values: port, address and IP protocol version */
  set_dest(&dest, addr);
  err = gettimeofday(&start_time, NULL);
  error_checker(err, "Error while getting time of the day");
  err = sendto(fd, msg, 64, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
  error_checker(err, "Error while sending datagram to host");
  /* wait_for_response function uses the select function in order
   * not to block the program on waiting for receiving the message */
  err = wait_for_response(&read_set, fd, &timeout);
  error_checker(err, "Error while monitoring file descriptors");
  if (err == 0) {
    err = printf("The packet was lost.\n");
    if (err < 0) {
      fprintf(stderr, "Error while printing to standard output.\n");
      exit(EXIT_FAILURE);
    }
  } else if (FD_ISSET(fd, &read_set)) {
    err = recvfrom(fd, msg, (size_t) sizeof(msg), 0, (struct sockaddr*) &dest, &dest_len);
    error_checker(err, "Error while receiving datagram from host");
    err = gettimeofday(&end_time, NULL);
    error_checker(err, "Error while getting time of the day");
    err = printf("TheRTTwas: %f seconds.\n", elapsed_time(start_time, end_time));
    if (err < 0) {
      fprintf(stderr, "Error while printing to standard output.\n");
      exit(EXIT_FAILURE);
    }
  }
  err = close(fd);
  error_checker(err, "Error while closing the socket");
  return 0;
}
