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

#define DEST_PORT 1234
#define WAITING_TIME 1
#define RESEND_PACKET_LIMIT 10
#define error_handling(err, expr) ( (err) < 0 ? fprintf(stderr, expr ": %s\n", strerror(errno)), exit(EXIT_FAILURE): 0)

static int breakloop = 0;	///< use this variable to stop your wait-loop. Occasionally check its value, !1 signals that the program should close

void parse_arguments(int argc, char *prog) {
  if (argc < 3 || argc > 4) {
    fprintf(stderr, "error : called with incorrect number of parameters\nusage : %s <server_name/IP> <filename> [<filter> [filter_options]]]\n", prog);
    exit(EXIT_FAILURE);
  }
}

void initialize_firstmsg(struct Firstmsg *m, int argc, char **argv) {
  strncpy(m->filename, argv[2], NAME_MAX);
  if(argc == 4) {
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
  }
}

int wait_for_response(fd_set *set, int fd, struct timeval *t) {
  /* Initializing the read set used by the select function to monitor the sockets */
  FD_ZERO(set);
  FD_SET(fd, set);
  t->tv_sec = WAITING_TIME;
  t->tv_usec = 0;
  return select(fd+1, set, NULL, NULL, t);
}

/// unimportant: the signal handler. This function gets called when Ctrl^C is pressed
void sigint_handler(int sigint) {
  if (!breakloop) {
    breakloop = 1;
    printf("SIGINT catched. Please wait to let the client close gracefully.\nTo close hard press Ctrl^C again.\n");
  } else {
    printf("SIGINT occurred, exiting hard... please wait\n");
    exit(-1);
  }
}

int main(int argc, char **argv) {
  int server_fd, audio_fd, err, end_of_data = STILL_READING_FILE;
  /* int sample_size, sample_rate, channels; */
  client_filterfunc pfunc;
  char buffer[BUFSIZE];
  struct timeval timeout;
  fd_set read_set;
  struct Firstmsg msg;  // To store the first received msg contining the file and library paths.
  struct Audioconf conf;  // To store the configuaration of the audio file to be sent.
  struct Datamsg audio_chunk;  // To store the chuks of the audio stream.
  struct sockaddr_in dest;
  struct in_addr *addr;
  socklen_t dest_len = sizeof(struct sockaddr_in);

  printf("SysProg2006 network client\n");
  printf("handed in by VOORBEELDSTUDENT\n");
  signal(SIGINT, sigint_handler);  // trap Ctrl^C signals
  parse_arguments(argc, argv[0]);
  addr = get_IP(argv[1]);
  initialize_firstmsg(&msg, argc, argv);
  server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  error_handling(server_fd, "Error while creating the socket");
  printf("The socket was created\n");  // to remove
  // Destination server values initialization.
  set_dest(&dest, addr);
  err = sendto(server_fd, &msg, sizeof(struct Firstmsg), 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
  error_handling(err, "Error while sending datagram to host");
  printf("First packet with path sent.\n");  // to remove
  printf("This the msg.filename: %s.", msg.filename);
  // No need to select since we can assume that the first packet is not lost
  err = recvfrom(server_fd, &conf, (size_t) sizeof(conf), 0, (struct sockaddr*) &dest, &dest_len);
  check_conf_error(&conf);  // in the case file or lib were not found.
  // open output
  audio_fd = aud_writeinit(conf.audio_rate, conf.audio_size, conf.channels);
  error_handling(audio_fd, "Error: unable to open audio output.");
  // open the library on the clientside if one is requested
  if (argv[3] && strcmp(argv[3],"")){
    // try to open the library, if one is requested
    pfunc = NULL;
    if (!pfunc){
      printf("failed to open the requested library. breaking hard\n");
      return -1;
    }
    printf("opened libraryfile %s\n",argv[3]);
  }
  else{
    pfunc = NULL;
    printf("not using a filter\n");
  }
  
  // start receiving data
  /* { */
  int bytesread, bytesmod, j = 0, timeoutcounter;
    unsigned int i = 0;
    char *modbuffer;
    printf("Before while loop for reading data\n");
    // start receiving
    {
      int bytesread, bytesmod, flag;
        char *modbuffer;
       
        while(1) { 
            //initialise or reset timeout
            FD_ZERO(&read_set);
            FD_SET(server_fd, &read_set);
            timeout.tv_sec = WAITING_TIME;
            timeout.tv_usec = 0;
  
            //wait for datamessage, if timeout ask for resend 
            select(server_fd+1, &read_set, NULL, NULL, &timeout);
            if (FD_ISSET(server_fd, &read_set)) {
                timeoutcounter=0; 
                //receive datamessage from server
                bytesread = recvfrom(server_fd, &audio_chunk, 
                    sizeof(struct Datamsg), 0, 
                    (struct sockaddr*) &dest, &dest_len);
                //send acknowledgement packet
                flag = 1;
                err = sendto(server_fd, &flag, sizeof(flag), 0, 
                    (struct sockaddr*) &dest, sizeof(struct sockaddr_in));   
          
                //check for end of soundfile and close server if so
                if (audio_chunk.endflag == 1) {
                    printf("Reached end of soundfile, thank you for using audioclient!\n");
                    if (audio_fd >= 0)    
                        close(audio_fd);
                    if (server_fd >= 0)
                        close(server_fd);
                    return 0;
                }
                write(audio_fd, audio_chunk.buffer, audio_chunk.length);
            }
        }
    }
}

