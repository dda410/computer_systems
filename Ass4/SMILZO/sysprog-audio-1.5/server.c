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

#include "audio.h"
#include "library.h"
#include "msg.h"

#define PORT 1234
#define WAITING_TIME 1
#define RESEND_PACKET_LIMIT 5
#define NO_AUDIO_FILE -1
#define error_handling(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)
#define printf_error_handling(err) ( (err) < 0 ? fprintf(stderr, "Error while printing to standard output.\n"), exit(EXIT_FAILURE) : 0)

static int breakloop = 0;  // changes when interrupt is catched.

void parse_arguments(int argc) {
  if (argc != 1) {
    fprintf(stderr, "Usage: server\n\n");
    exit(EXIT_FAILURE);
  }
}

/* Catches the ctrl-c interrupt signal */
void sigint_handler(int sigint) {
  int err;
  if (!breakloop) {
    breakloop = 1;
    err = printf("SIGINT catched. Please wait to let the server close gracefully.\nTo close hard press Ctrl^C again.\n");
    printf_error_handling(err);
  } else {
    err = printf("SIGINT occurred, exiting hard... please wait\n");
    printf_error_handling(err);
    exit(EXIT_FAILURE);
  }
}

/* Gracefully close the server in the case an interrupt has been catched */
void close_after_interrupt(int sock, int file) {
  int err;
  if (sock >= 0) {
    err = close(sock);
    error_handling(err, "Error while closing the socket");
  }
  if (file >= 0) {
    err = close(file);
    error_handling(err, "Error while closing the audio file");
  }
  err = printf("The program has been closed gracefully.\n");
  printf_error_handling(err);
  exit(EXIT_SUCCESS);
}

void set_socket(struct sockaddr_in *addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(PORT);
  addr->sin_addr.s_addr = htonl(INADDR_ANY);
}

/* Waits indefinetely for a client to connect. @return returns select return value */
int wait_for_client(fd_set *set, int fd) {
  FD_ZERO(set);
  FD_SET(fd, set);
  return select(fd+1, set, NULL, NULL, NULL);
}

/* Waits for a client response and closes server in case of interrupt.
 * @return returns the select() return value in case of no interrupt */
int wait_for_response(fd_set *set, int fd, struct timeval *t, int file) {
  int err;
  FD_ZERO(set);
  FD_SET(fd, set);
  t->tv_sec = WAITING_TIME;
  t->tv_usec = 0;
  err = select(fd+1, set, NULL, NULL, t);
  if (err < 0 && breakloop != 0) {
    close_after_interrupt(fd, file);
  }
  return err;
}

/* Sends error message back to client if audio file or library was not found */
void send_error_to_client(int fd, int err, struct Audioconf *c,
                          struct sockaddr_in *addr) {
  c->error = err;
  err = sendto(fd, c, sizeof(struct Audioconf), 0,
               (struct sockaddr*) addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending audio configuration datagram");
}

void configure_audio(struct Audioconf *c, int ch, int ss, int sr) {
  c->channels = ch;
  c->audio_size = ss;
  c->audio_rate = sr;
  c->error = SUCCESS;
}

/* Streams readed chunks from the audio file to the client till it has been
 * completely read. @return returns 0 on success or a negative errorcode on failure */
int stream_data(int client_fd, struct sockaddr_in *addr, socklen_t *addr_len) {
  int data_fd, err;
  int channels, sample_size, sample_rate;
  server_filterfunc pfunc;
  char *datafile, *libfile;
  fd_set read_set;
  struct timeval timeout;
  struct Firstmsg msg;  // To store filepath and libpath.
  struct Audioconf conf;  // To store the audio configuration.
  struct Datamsg audio_chunk;  // To store the chunks of the audio file.
  err = recvfrom(client_fd, &msg, (size_t) sizeof(msg), 0,
                 (struct sockaddr*) addr, addr_len);
  error_handling(err, "Error while receiving first datagram");
  datafile = msg.filename;
  if (strcmp(msg.libfile, NO_LIB_REQUESTED) == 0) {
    libfile = NULL;
  } else {
    libfile = msg.libfile;
  }
  /* Opening input audio file */
  data_fd = aud_readinit(datafile, &sample_rate, &sample_size, &channels);
  if (data_fd < 0) {
    err = printf("failed to open datafile %s, skipping request\n", datafile);
    printf_error_handling(err);
    send_error_to_client(client_fd, AUDIO_FILE_NOT_FOUND, &conf, addr);
    return -1;
  }
  err = printf("opened datafile %s\n", datafile);
  printf_error_handling(err);
  /* optionally open a library if one is requested (next assignment) */
  if (libfile) {
    pfunc = NULL;
    if (!pfunc) {
      err = printf("failed to open the requested library. breaking hard\n");
      printf_error_handling(err);
      send_error_to_client(client_fd, LIB_NOT_FOUND, &conf, addr);
      return -1;
    }
    err = printf("opened libraryfile %s\n", libfile);
    printf_error_handling(err);
  } else {
    pfunc = NULL;
    err = printf("not using a filter\n");
    printf_error_handling(err);
  }
  /* Configuring the Audioconf struct, needed in order to play the file. */
  configure_audio(&conf, channels, sample_size, sample_rate);
  err = sendto(client_fd, &conf, sizeof(struct Audioconf), 0,
               (struct sockaddr*) addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending audio configuration datagram");
  /* start streaming content to client  */
  {
    int bytesread;
    unsigned int counter, wrong_ack = 0, lost_packets = 0;
    audio_chunk.msg_counter = 1;
    /* The loop stops when the file has been read completely */
    bytesread = read(data_fd, audio_chunk.buffer, sizeof(audio_chunk.buffer));
    audio_chunk.length = bytesread;
    while (bytesread > 0) {
      err = sendto(client_fd, &audio_chunk, sizeof(struct Datamsg), 0,
                   (struct sockaddr*) addr, sizeof(struct sockaddr_in));
      error_handling(err, "Error while sending the audio chunk");
      err = wait_for_response(&read_set, client_fd, &timeout, data_fd);
      error_handling(err, "Error while monitoring file descriptors");
      lost_packets = (err == 0) ? lost_packets + 1 : 0;
      if (FD_ISSET(client_fd, &read_set)) {
        /* Receive acknowledgement if no packet loss and client still active */
        err = recvfrom(client_fd, &counter, sizeof(counter), 0,
                       (struct sockaddr*) addr, addr_len);
        error_handling(err, "Error while receiving the acknowledgement");
        wrong_ack = (audio_chunk.msg_counter == counter) ? 0 : wrong_ack + 1;
      }
      if (lost_packets > RESEND_PACKET_LIMIT || wrong_ack > RESEND_PACKET_LIMIT) {
        err = printf("Error while comunicating with client. Closing connection...\n");
        printf_error_handling(err);
        err = close(data_fd);
        error_handling(err, "Error closing the audio file");
        return -1;
      } else if (lost_packets == 0 && wrong_ack == 0) {
        /* The next chunks is read only if no packet loss or wrong ack received.
         * Otherwise the already read chunk needs to be resent. */
        bytesread = read(data_fd, audio_chunk.buffer, sizeof(audio_chunk.buffer));
        audio_chunk.msg_counter+=1;
        audio_chunk.length = bytesread;
      } /* else the same packet will be resent to the client */
    }
    error_handling(bytesread, "Error while reading the file");
    /* client_fd is not close since the same file descriptor is used for
     * other connections as well. it will be close once the server exits. */
    if (data_fd >= 0) {
      err = close(data_fd);
      error_handling(err, "Error closing the audio file");
    }
    return 0;
  }
}
/* The main loop, continuously waiting for clients */
int main(int argc, char **argv) {
  parse_arguments(argc);
  int fd, err;
  err = printf("SysProg network server\nhanded in by Dimitri Diomaiuta\n");
  printf_error_handling(err);
  signal(SIGINT, sigint_handler);  // trap Ctrl^C signals
  struct sockaddr_in addr;
  fd_set read_set;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  /* Creating the socket */
  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  error_handling(fd, "Error while creating the socket");
  /* Initializing the socket */
  set_socket(&addr);
  /* Binding the socket in order to listen to a
   * port that can receive from different address */
  err = bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while binding the socket");
  while (!breakloop) {
    err = wait_for_client(&read_set, fd);
    if (err < 0 && breakloop != 0) {
      close_after_interrupt(fd, NO_AUDIO_FILE);
    }
    error_handling(err, "Error while monitoring file descriptors");
    /* open connection when client connects and start streaming */
    err = stream_data(fd, &addr, &addr_len);
    if (err < 0) {
      err = printf("Connection with client closed due to errors. Waiting for new requests\n");
      printf_error_handling(err);
    } else {
      err = printf("The audio was succesfully sent to the client. Waiting for new requests\n");
      printf_error_handling(err);
    }
    sleep(1);
  }
  err = close(fd);
  error_handling(err, "Error while closing the socket");
  return 0;
}

