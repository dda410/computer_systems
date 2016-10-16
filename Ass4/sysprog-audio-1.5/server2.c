/* server.c
 *
 * part of the Systems Programming assignment
 * (c) Vrije Universiteit Amsterdam, 2005-2015. BSD License applies
 * author  : wdb -_at-_ few.vu.nl
 * contact : arno@cs.vu.nl
 * */

#include <dlfcn.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "msg.h"
#include "library.h"
#include "audio.h"
#include "audio.c"  // to remove

/// a define used for the copy buffer in stream_data(...)
/* #define BUFSIZE 1024 */
#define PORT 1234
#define error_handling(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)

static int breakloop = 0;	///< use this variable to stop your wait-loop. Occasionally check its value, !1 signals that the program should close

void parse_arguments(int argc) {
  if (argc != 1) {
    fprintf(stderr, "Usage: server\n\n");
    exit(EXIT_FAILURE);
  }
}

void set_socket(struct sockaddr_in *addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(PORT);
  addr->sin_addr.s_addr = htonl(INADDR_ANY);
}

int wait_for_response(fd_set *set, int fd) {
  /* Initializing the read set used by the select function to monitor the sockets */
  FD_ZERO(set);
  FD_SET(fd, set);
  return select(fd+1, set, NULL, NULL, NULL);
}

/// stream data to a client. 
///
/// This is an example function; you do *not* have to use this and can choose a different flow of control
///
/// @param fd an opened file descriptor for reading and writing
/// @return returns 0 on success or a negative errorcode on failure
int stream_data(int client_fd, struct sockaddr_in *addr, socklen_t *addr_len) {
  int data_fd, err;
  int channels, sample_size, sample_rate;
  server_filterfunc pfunc;
  char *datafile, *libfile;
  char buffer[BUFSIZE];
  struct Firstmsg msg;  // To store the first received msg contining the file and library paths.
  struct Audioconf conf;  // To store the configuaration of the audio file to be sent.
  struct Datamsg audio_chunk;  // To store the chuks of the audio stream.


        
  // TO IMPLEMENT
  // receive a control packet from the client 
  // containing at the least the name of the file to stream and the library to use
  err = recvfrom(client_fd, &msg, (size_t) sizeof(msg), 0,
                 (struct sockaddr*) addr, addr_len);
  error_handling(err, "Error while receiving datagram");
  printf("This is the filename: %s\n this the lib: %s\n", msg.filename, msg.libfile); // to remove
  /* { */
  /*   datafile = strdup("example.wav"); */
  /*   libfile = NULL; */
  /* } */
  datafile = msg.filename;
  libfile = NULL;
	
  // open input
  data_fd = aud_readinit(datafile, &sample_rate, &sample_size, &channels); // to uncomment
  if (data_fd < 0){
    printf("failed to open datafile %s, skipping request\n",datafile);
    // check printf error code or use fprintf to print to standard error
    conf.error = AUDIO_FILE_NOT_FOUND;
    // Send error message back to client in the case the wav file was not found
    return -1;
  }
  printf("opened datafile %s\n", datafile);
  // check printf error code
  printf("This the sample_rate: %d\nThis the sample_size: %d\nThis the channels: %d\n", sample_rate, sample_size, channels);  // to remove
  // optionally open a library
  if (libfile) {
    // try to open the library, if one is requested
    pfunc = NULL;
    if (!pfunc) {
      printf("failed to open the requested library. breaking hard\n");
      // check error code
      conf.error = LIB_NOT_FOUND;
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

  conf.channels = channels;
  conf.audio_size = sample_size;
  conf.audio_rate = sample_rate;
  conf.error = SUCCESS;
  err = sendto(client_fd, &conf, sizeof(struct Audioconf), 0, (struct sockaddr*) addr, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending audio configuration datagram");
  // TO IMPLEMENT : optionally return an error code to the client if initialization went wrong

  // Send Audioconf packet back to client in order to inform on how to play the streamed data.

  // start streaming
  {
    int bytesread, bytesmod, i = 0;
		
    /* bytesread = read(data_fd, buffer, BUFSIZE); */
    while ( (bytesread = read(data_fd, audio_chunk.buffer, BUFSIZE)) > 0){
      i++;
      // you might also want to check that the client is still active, whether it wants resends, etc..
      /* audio_chunk.buffer = buffer; */
      audio_chunk.length = bytesread;
      audio_chunk.endflag = (bytesread < BUFSIZE) ? LAST_CHUNK : STILL_READING_FILE;
      audio_chunk.msg_counter = i;
      // edit data in-place. Not necessarily the best option
      if (pfunc) {
        bytesmod = pfunc(buffer,bytesread);
      }
      err = sendto(client_fd, &audio_chunk, sizeof(struct Datamsg), 0, (struct sockaddr*) addr, sizeof(struct sockaddr_in));
      error_handling(err, "Error while sending audio chunk to client");
      write(client_fd, buffer, bytesmod);
      /* bytesread = read(data_fd, buffer, BUFSIZE); */
    }
  }

  // TO IMPLEMENT : optionally close the connection gracefully 	
	
  if (client_fd >= 0)
    close(client_fd);
  if (data_fd >= 0)
    close(data_fd);
  if (datafile)
    free(datafile);
  if (libfile)
    free(libfile);
	
  return 0;
}

/// unimportant: the signal handler. This function gets called when Ctrl^C is pressed
void sigint_handler(int sigint)
{
  if (!breakloop){
    breakloop=1;
    printf("SIGINT catched. Please wait to let the server close gracefully.\nTo close hard press Ctrl^C again.\n");
  }
  else{
    printf ("SIGINT occurred, exiting hard... please wait\n");
    exit(-1);
  }
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
    err = wait_for_response(&read_set, fd);
    error_handling(err, "Error while monitoring file descriptors");
    // TO IMPLEMENT:
    // wait for connections
    // when a client connects, start streaming data (see the stream_data(...) prototype above)
    err = stream_data(fd, &addr, &addr_len);
    if (err < 0) {
      /* Error handling here, the server can continue running in case of error with a signle client */
    }
    sleep(1);
  }

  return 0;
}
