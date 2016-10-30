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
#define RESEND_PACKET_LIMIT 10
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
  if (!breakloop){
    breakloop=1;
    printf("SIGINT catched. Please wait to let the server close gracefully.\nTo close hard press Ctrl^C again.\n");
    // check printf error
  } else {
    printf ("SIGINT occurred, exiting hard... please wait\n");
    exit(-1);
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

int check_lost_packet(fd_set *set, int fd, struct timeval *t, struct sockaddr_in *addr,
                      socklen_t *addr_len, struct Datamsg *aud, unsigned int ack) {
  printf("Inside check_lost_packet for ack no: %u", ack);
  int err = wait_for_response(set, fd, t);
  error_handling(err, "Error while monitoring file descriptors");
  if (err == 0) {
    // packet has been lost since client not returning anything.
    return -1;
  }  else if (FD_ISSET(fd, set)) {
    // receiving the acknowledgement from the client.
    err = recvfrom(fd, &ack, (size_t) sizeof(ack), 0,
                   (struct sockaddr*) addr, addr_len);
    error_handling(err, "Error while receiving acknowledgement");
    if (ack != aud->msg_counter){
      // The client has returned the wrong acknowledgement.
      return -1;
    }
  }
  return 0;
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
  struct Firstmsg msg;  // To store the first received msg contining the file and library paths.
  struct Audioconf conf;  // To store the configuaration of the audio file to be sent.
  struct Datamsg audio_chunk;  // To store the chuks of the audio stream.

  err = recvfrom(client_fd, &msg, (size_t) sizeof(msg), 0,
                 (struct sockaddr*) addr, addr_len);
  error_handling(err, "Error while receiving datagram");
  printf("This is the filename: %s\n this the lib: %s\n", msg.filename, msg.libfile); // to remove
  datafile = msg.filename;
  libfile = NULL;
  // open input
  data_fd = aud_readinit(datafile, &sample_rate, &sample_size, &channels); // to uncomment
  
  if (data_fd < 0){
    err = printf("failed to open datafile %s, skipping request\n",datafile);
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
  }
  else{
    pfunc = NULL;
    printf("not using a filter\n");
  }
  // Configuring the Audioconf struct, needed in order to play the file.
  configure_audio(&conf, channels, sample_size, sample_rate);
  // Send Audioconf packet back to client in order to inform on how to play the streamed data.
  err = sendto(client_fd, &conf, sizeof(struct Audioconf), 0, (struct sockaddr*) addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending audio configuration datagram");
  // start streaming
      {
        int bytesread, bytesmod, flag = 1, timeoutcounter;
        unsigned int counter;
        audio_chunk.msg_counter = 0;
        while ((bytesread = read(data_fd, audio_chunk.buffer, sizeof(audio_chunk.buffer))) > 0) {
          audio_chunk.msg_counter+=1;
          //initialise or reset timeout

          //set the length of the audiobuffer and send datamessage
          audio_chunk.length = bytesread;
          err = sendto(client_fd, &audio_chunk, 
                       sizeof(struct Datamsg), 0, 
                       (struct sockaddr*) addr, sizeof(struct sockaddr_in));


          //wait for ack and resend if timeout occurs
          err = wait_for_response(&read_set, client_fd, &timeout);
          error_handling(err, "Error while monitoring file descriptors");
          // check err == 0
          if(FD_ISSET(client_fd, &read_set)) {
            err = recvfrom(client_fd, &counter, sizeof(flag), 0,
                           (struct sockaddr*) addr, addr_len);
            printf("This is the acknowledgement no: %u\n", counter);
            // listen for acknowledgement

            // check for resendflag in acknowledgement
            if (flag == 0) {
              printf("Clientside requests resend of last message");
              //server reverts back to while loop and resends
            }
          }
        }
      }
      // Check errors for this two dio can
      // do not close client_fd since is the same
      /* if (client_fd >= 0)     */
      /*   close(client_fd); */
      if (data_fd >= 0)
        close(data_fd);
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
    // TO IMPLEMENT:
    // wait for connections
    // when a client connects, start streaming data (see the stream_data(...) prototype above)
    err = stream_data(fd, &addr, &addr_len);
    if (err < 0) {
      printf("Some error occurred\nServer still up and running for new requests");
      /* Error handling here, the server can continue running in case of error with a signle client */
    }
    sleep(1);
  }
  err = close(fd);
  error_handling(err, "Error while closing the socket");
  return 0;
}

