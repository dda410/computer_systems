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

#define DEST_PORT 1234
#define WAITING_TIME 1
#define WRONG_PACKETS_LIMIT 5
#define NO_RESPONSE_LIMIT 5
#define NO_AUDIO_FILE -1
#define error_handling(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)
#define printf_error_handling(err) ( (err) < 0 ? fprintf(stderr, "Error while printing to standard output.\n"), exit(EXIT_FAILURE) : 0)

static int breakloop = 0;	///< use this variable to stop your wait-loop. Occasionally check its value, !1 signals that the program should close

void parse_arguments(int argc, char *prog) {
  if (argc < 3 || argc > 4) {
    fprintf(stderr, "error : called with incorrect number of parameters\nusage : %s <server_name/IP> <filename> [<filter> [filter_options]]]\n", prog);
    exit(EXIT_FAILURE);
  }
}

// unimportant: the signal handler. This function gets called when Ctrl^C is pressed
void sigint_handler(int sigint) {
  int err;
  if (!breakloop) {
    breakloop = 1;
    err = printf("SIGINT catched. Please wait to let the client close gracefully.\nTo close hard press Ctrl^C again.\n");
    printf_error_handling(err);
  } else {
    err = printf("SIGINT occurred, exiting hard... please wait\n");
    printf_error_handling(err);
    exit(EXIT_FAILURE);
  }
}

void close_after_interrupt(int sock, int device) {
  int err;
  if (sock >= 0) {
    err = close(sock);
    error_handling(err, "Error while closing the socket");
  }
  if (device >= 0) {
    err = close(device);
    error_handling(err, "Error while closing the audio file");
  }
  err = printf("The program has been closed gracefully.\n");
  printf_error_handling(err);
  exit(EXIT_SUCCESS);
}

void initialize_firstmsg(struct Firstmsg *m, int argc, char **argv) {
  strncpy(m->filename, argv[2], NAME_MAX);
  if (argc == 4) {
    strncpy(m->libfile, argv[3], NAME_MAX);
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

void check_conf_error(struct Audioconf *c) {
  if (c->error == AUDIO_FILE_NOT_FOUND) {
    fprintf(stderr, "The input file was not found on server\n");
    exit(EXIT_FAILURE);
  } else if (c->error == LIB_NOT_FOUND) {
    fprintf(stderr, "The input library was not found\n");
    exit(EXIT_FAILURE);
  } else if (c->error != SUCCESS) {
    fprintf(stderr, "The server is already serving another client. Aborting.\n");
    exit(EXIT_FAILURE);
  }
}

int wait_for_server(fd_set *set, int fd) {
  /* Waits indefinetely for a server to answer */
  int err;
  FD_ZERO(set);
  FD_SET(fd, set);
  err = select(fd+1, set, NULL, NULL, NULL);
  if (err < 0 && breakloop != 0) {
    close_after_interrupt(fd, NO_AUDIO_FILE);
  }
  return err;
}

int wait_for_response(fd_set *set, int fd, struct timeval *t, int device) {
  /* Initializing the read set used by the select function to monitor the sockets */
  int err;
  FD_ZERO(set);
  FD_SET(fd, set);
  t->tv_sec = WAITING_TIME;
  t->tv_usec = 0;
  err = select(fd+1, set, NULL, NULL, t);
  /* In the case the user ctrl-c while waiting for a response */
  if (err < 0 && breakloop != 0) {
    close_after_interrupt(fd, device);
  }
  return err;
}

int main(int argc, char **argv) {
  parse_arguments(argc, argv[0]);
  int server_fd, audio_fd, err;
  client_filterfunc pfunc;
  struct timeval timeout;
  fd_set read_set;
  struct Firstmsg msg;  // To store the first received msg contining the file and library paths.
  struct Audioconf conf;  // To store the configuaration of the audio file to be sent.
  struct Datamsg audio_chunk;  // To store the chuks of the audio stream.
  struct sockaddr_in dest;
  struct in_addr *addr;
  socklen_t dest_len = sizeof(struct sockaddr_in);
  err = printf("SysProg2006 network client\nhanded in by Dimitri Diomaiuta\n");
  printf_error_handling(err);
  signal(SIGINT, sigint_handler);  // trap Ctrl^C signals
  addr = get_IP(argv[1]);
  initialize_firstmsg(&msg, argc, argv);
  server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  error_handling(server_fd, "Error while creating the socket");
  printf("The socket was created\n");  // to remove
  /* Destination server values initialization. */
  set_dest(&dest, addr);
  err = sendto(server_fd, &msg, sizeof(struct Firstmsg), 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending first datagram to host");
  printf("First packet with path sent.\n");  // to remove
  printf("This the msg.filename: %s.", msg.filename);  // to remove
  err = wait_for_server(&read_set, server_fd);
  error_handling(err, "Error while monitoring file descriptors");
  err = recvfrom(server_fd, &conf, (size_t) sizeof(conf), 0, (struct sockaddr*) &dest, &dest_len);
  error_handling(err, "Error while receiving audio configuration datagram");
  check_conf_error(&conf);  // in the case file or lib were not found.
  // open output
  audio_fd = aud_writeinit(conf.audio_rate, conf.audio_size, conf.channels);
  error_handling(audio_fd, "Error while opening/finding audio output device.");
  // open the library on the clientside if one is requested
  if (argv[3] && strcmp(argv[3], "")) {
    // try to open the library, if one is requested
    pfunc = NULL;
    if (!pfunc){
      fprintf(stderr, "failed to open the requested library. breaking hard\n");
      return -1;
    }
    err = printf("opened libraryfile %s\n", argv[3]);
    printf_error_handling(err);
  } else {
    pfunc = NULL;
    err = printf("not using a filter\n");
    printf_error_handling(err);
  }
  printf("Before while loop for reading data\n");  // to remove
  // start receiving
  {
    int bytesread, len = BUFSIZE;
    unsigned int expected_chunk_no = 1, wrong_packets = 0, no_response = 0;
    while (len >= BUFSIZE) {
      if (breakloop != 0) {
        close_after_interrupt(server_fd, audio_fd);
      }
      err = wait_for_response(&read_set, server_fd, &timeout, audio_fd);
      error_handling(err, "Error while monitoring file descriptors");
      if (err == 0) {
        no_response++;
        if (no_response > NO_RESPONSE_LIMIT) {
          fprintf(stderr, "The server is not responding any more. Aborting.\n");
          return -1;
        }
      } else if (FD_ISSET(server_fd, &read_set)) {
        // receive datamessage from server
        bytesread = recvfrom(server_fd, &audio_chunk,
                             sizeof(struct Datamsg), 0, (struct sockaddr*) &dest, &dest_len);
        error_handling(bytesread, "Error while receiving audio chunk");
        //send acknowledgement packet
        err = sendto(server_fd, &audio_chunk.msg_counter, sizeof(audio_chunk.msg_counter),
                     0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));   
        error_handling(err, "Error while sending acknowledgement");
        len = audio_chunk.length;
        if(audio_chunk.msg_counter == expected_chunk_no) {
          err = write(audio_fd, audio_chunk.buffer, audio_chunk.length);
          error_handling(err, "Error while writing to audio device");
          expected_chunk_no++;
          wrong_packets = 0;
        } else if (wrong_packets < WRONG_PACKETS_LIMIT) {
          wrong_packets++;
          printf("Wrong packet increased\nnow is: %d\n", wrong_packets);  // to remove
        } else {
          fprintf(stderr, "The server is not sending packets in sequence, Aborting.\n");
          return -1;
        }
        printf("Dio cane questa e' la lunghezza dei cazzo di chunk di merda: %d\n", audio_chunk.length);  // to remove
        printf("E questa il fottuto counter del chunk dio madonna puttana: %d\n", audio_chunk.msg_counter);  // to remove
      }
    }
    err = printf("The file has been played. Exiting.\n");
    printf_error_handling(err);
    err = close(audio_fd);
    error_handling(err, "Error while closing the audio file");
    err = close(server_fd);
    error_handling(err, "Error while closing the socket");
    return 0;
  }
}

