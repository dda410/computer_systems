#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "library.h"
#include "audio.h"
#include "msg.h"

#define PORT 1234
#define WAITING_TIME 1
#define RESEND_PACKET_LIMIT 3
#define CLIENT_NOT_RESPONDING -1
#define error_handling(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)

static int breakloop = 0;	///< use this variable to stop your wait-loop. Occasionally check its value, !1 signals that the program should close

void parse_arguments(int argc) {
  if (argc != 1) {
    fprintf(stderr, "Usage: server\n\n");
    exit(EXIT_FAILURE);
  }
}

/// unimportant: the signal handler. This function gets called when Ctrl^C is pressed
void sigint_handler(int sigint) {
  if (!breakloop) {
    breakloop = 1;
    printf("SIGINT catched. Please wait to let the server close gracefully.\nTo close hard press Ctrl^C again.\n");
    // check printf error
  } else {
    printf("SIGINT occurred, exiting hard... please wait\n");
    exit(EXIT_FAILURE);
  }
}

void set_socket(struct sockaddr_in *addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(PORT);
  addr->sin_addr.s_addr = htonl(INADDR_ANY);
}

int wait_for_client(fd_set *set, int fd) {
  /* Waits indefinetely for a client to connect */
  FD_ZERO(set);
  FD_SET(fd, set);
  return select(fd+1, set, NULL, NULL, NULL);
}

int wait_for_response(fd_set *set, int fd, struct timeval *t) {
  /* Initializing the read set used by the select function to monitor the sockets */
  FD_ZERO(set);
  FD_SET(fd, set);
  t->tv_sec = WAITING_TIME;
  t->tv_usec = 0;
  return select(fd+1, set, NULL, NULL, t);
}

void send_error_to_client(int fd, int err, struct Audioconf *c,
                          struct sockaddr_in *addr) {
  c->error = err;
  err = sendto(fd, c, sizeof(struct Audioconf), 0,
               (struct sockaddr*) addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending audio configuration datagram");
  // Send error message back to client in the case the wav file was not found
}

void configure_audio(struct Audioconf *c, int ch, int ss, int sr) {
  c->channels = ch;
  c->audio_size = ss;
  c->audio_rate = sr;
  c->error = SUCCESS;
}

int resend_lost_packet(fd_set *set, int fd, struct timeval *t) {
  int i, err;
  for (i = 0; i < RESEND_PACKET_LIMIT; i++) {
    printf("Inside for loop %d\n", i);
    int err = wait_for_response(set, fd, t);
    error_handling(err, "Error while monitoring file descriptors");
    if (err != 0) {
      break;
    }
  }
  return (i >= RESEND_PACKET_LIMIT) ? -1 : 0;
}

/// @param fd an opened file descriptor for reading and writing
/// @return returns 0 on success or a negative errorcode on failure
int stream_data(int client_fd, struct sockaddr_in *addr, socklen_t *addr_len) {
  int data_fd, err, nb;
  int channels, sample_size, sample_rate;
  server_filterfunc pfunc;
  char *datafile, *libfile, *em;
  fd_set read_set;
  struct timeval timeout;
  struct Firstmsg msg;  // To store filepath and libpath.
  struct Audioconf conf;  // To store the audio configuration.
  struct Datamsg audio_chunk;  // To store the chuks of the audio file.

  err = recvfrom(client_fd, &msg, (size_t) sizeof(msg), 0,
                 (struct sockaddr*) addr, addr_len);
  error_handling(err, "Error while receiving datagram");
  printf("This is the filename: %s\n this the lib: %s\n", msg.filename, msg.libfile); // to remove
  datafile = msg.filename;
  libfile = NULL;
  // open input
  data_fd = aud_readinit(datafile, &sample_rate, &sample_size, &channels);
  if (data_fd < 0) {
    err = printf("failed to open datafile %s, skipping request\n", datafile);
    // check printf error code
    send_error_to_client(client_fd, AUDIO_FILE_NOT_FOUND, &conf, addr);
    return -1;
  }
  printf("opened datafile %s\n", datafile);
  // check printf error code
  printf("This the sample_rate: %d\nThis the sample_size: %d\nThis the channels: %d\n", sample_rate, sample_size, channels);  //  to remove
  // optionally open a library
  if (libfile) {
    // try to open the library, if one is requested
    pfunc = NULL;  // This will be implemented in the next assignment
    if (!pfunc) {
      err = printf("failed to open the requested library. breaking hard\n");
      // check error code
      send_error_to_client(client_fd, LIB_NOT_FOUND, &conf, addr);
      // Send message back to client in the case the library was not found
      return -1;
    }
    printf("opened libraryfile %s\n", libfile);
    // check error code
  } else {
    pfunc = NULL;
    printf("not using a filter\n");
  }
  // Configuring the Audioconf struct, needed in order to play the file.
  configure_audio(&conf, channels, sample_size, sample_rate);
  err = sendto(client_fd, &conf, sizeof(struct Audioconf), 0,
               (struct sockaddr*) addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending audio configuration datagram");
  // start streaming
      {
        int bytesread, bytesmod, flag = 1, timeoutcounter;
        unsigned int counter;
        audio_chunk.msg_counter = 0;
        while ((bytesread = read(data_fd, audio_chunk.buffer, sizeof(audio_chunk.buffer))) > 0) {
          audio_chunk.msg_counter+=1;
          audio_chunk.length = bytesread;
          err = sendto(client_fd, &audio_chunk, sizeof(struct Datamsg), 0,
                       (struct sockaddr*) addr, sizeof(struct sockaddr_in));
          err = resend_lost_packet(&read_set, client_fd, &timeout);
          printf("This is the error of resend lost dio porco: %d\n", err);
          if (err < 0) {
            printf("The client stopped responding. Closing connection...\n");
            // check for error
            close(data_fd);
            // check for errors
            return -1;
          }
          if (FD_ISSET(client_fd, &read_set)) {
            err = recvfrom(client_fd, &counter, sizeof(flag), 0,
                           (struct sockaddr*) addr, addr_len);
            printf("This is the acknowledgement no: %u\n", counter);
            if (flag == 0) {
              printf("Clientside requests resend of last message");
              // server reverts back to while loop and resends
            }
          }
        }
      }
      // Check errors for this two dio can
      /* do not close client_fd since is the same file descriptor
       * used for other connections as well */
      /* if (client_fd >= 0)     */
      /*   close(client_fd); */
      if (data_fd >= 0) {
        close(data_fd);
      }
      return 0;
}

/// the main loop, continuously waiting for clients
int main(int argc, char **argv) {
  parse_arguments(argc);
  int fd, err;
  printf("SysProg network server\n");
  printf("handed in by VOORBEELDSTUDENT\n");
  signal(SIGINT, sigint_handler);  // trap Ctrl^C signals
  struct sockaddr_in addr;
  fd_set read_set;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  /* Creating the socket */
  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  error_handling(fd, "Error while creating the socket");
  /* Initializing the socket with the values: port to listen to and addresses to accept from  */
  set_socket(&addr);
  /* Binding the socket in order to listen to a port that can receive from different address */
  err = bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while binding the socket");
  while (!breakloop) {
    err = wait_for_client(&read_set, fd);
    error_handling(err, "Error while monitoring file descriptors");
    /* Streaming the requested file when a client connects */
    err = stream_data(fd, &addr, &addr_len);
    if (err < 0) {
      printf("Some error occurred\nServer still up and running for new requests\n");
      /* Error handling here, the server can continue running
         in case of error with a single client */
    }
    sleep(1);
  }
  err = close(fd);
  error_handling(err, "Error while closing the socket");
  return 0;
}

