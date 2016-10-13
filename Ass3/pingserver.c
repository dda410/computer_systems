#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* In the case of error this macro prints to stderr the passed expression followed by the errno description */
#define error_handling(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)
#define PORT 1234

void parse_arguments(int argc) {
  if (argc != 1) {
    fprintf(stderr, "Usage: pingserver\n\n");
    exit(EXIT_FAILURE);
  }
}

void set_socket(struct sockaddr_in *addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(PORT);
  addr->sin_addr.s_addr = htonl(INADDR_ANY);
}

int main(int argc, char *argv[]) {
  parse_arguments(argc);
  int fd, err;
  char msg[64];
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  /* Creating the socket */
  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  error_handling(fd, "Error while creating the socket");
  /* Initializing the socket with the values: port to listen to and addresses to accept from  */
  set_socket(&addr);
  /* Binding the socket in order to listen to a port that can receive from different address */
  err = bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while binding the socket");
  while (1) {
    err = recvfrom(fd, msg, (size_t) sizeof(msg), 0, (struct sockaddr*) &addr, &addr_len);
    error_handling(err, "Error while receiving datagram");
    err = sendto(fd, msg, (size_t) sizeof(msg), 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in));
    error_handling(err, "Error while sending datagram back");
  }
  /* No need to close the socket since it will be closed by the OS once the interrupt signal is catched */
  return 0;
}
