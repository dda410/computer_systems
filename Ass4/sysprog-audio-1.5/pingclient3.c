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
#define error_handling(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)
#define printf_error_handling(err) ( (err) < 0 ? fprintf(stderr, "Error while printing to standard output.\n"), exit(EXIT_FAILURE) : 0)
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

void check_message(unsigned int *m1, unsigned int *m2,
                   struct timeval *s, struct timeval *e, struct timeval *d) {
  int err;
  if (*m1 != *m2) {
    err = printf("Packet %u: wrong counter! Received %u instead of %u.\n", *m1, *m2, *m1);
    printf_error_handling(err);
  } else {
    timersub(e, s, d);
    err = printf("Packet %u: %ld.%06ld seconds.\n", *m1, d->tv_sec, d->tv_usec);
    printf_error_handling(err);
  }
}

int main(int argc, char *argv[]) {
  parse_arguments(argc);
  char *hostname = argv[1];
  int fd, err, sleep_time;
  /* Using unsigned integers to avoid an integer overflow since unsigned wrap around on overflows */
  unsigned int received_msg, msg = 0;
  fd_set read_set;
  struct timeval start_time, end_time, timeout, diff_time;
  struct sockaddr_in dest;
  struct in_addr *addr;
  socklen_t dest_len = sizeof(struct sockaddr_in);
  addr = get_IP(hostname);
  /* Creating the socket */
  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  error_handling(fd, "Error while creating the socket");
  /* Initializing the destination server values: port, address and IP protocol version */
  set_dest(&dest, addr);
  while (1) {
    err = gettimeofday(&start_time, NULL);
    error_handling(err, "Error while getting time of the day");
    err = sendto(fd, &msg, (size_t) sizeof(msg), 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
    error_handling(err, "Error while sending datagram to host");
    /* wait_for_response function uses the select function in order not
     * to block the program on waiting for receiving the message back*/
    err = wait_for_response(&read_set, fd, &timeout);
    error_handling(err, "Error while monitoring file descriptors");
    if (err == 0) {
      err = printf("Packet %u: lost.\n", msg);
      printf_error_handling(err);
      sleep_time = 0;
    } else if (FD_ISSET(fd, &read_set)) {
      err = recvfrom(fd, &received_msg, (size_t) sizeof(msg), 0, (struct sockaddr*) &dest, &dest_len);
      error_handling(err, "Error while receiving datagram from host");
      err = gettimeofday(&end_time, NULL);
      error_handling(err, "Error while getting time of the day");
      /* Checks that the received message is the correct one */
      check_message(&msg, &received_msg, &start_time, &end_time, &diff_time);
      sleep_time = 1;
    }
    msg++;
    sleep(sleep_time); /* To send one message per second */
  }
  /* No need to close the socket since it will be closed by the OS once the interrupt signal is catched */
  return 0;
}
