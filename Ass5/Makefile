# Sysprog Assignment 3 Makefile by Peter Peerdeman, 1536494
# This program makes the audioserver and 
# audioclient programs including the plugin filter libraries

# Variable Definitions
CC = gcc
CFLAGS = -Wall  -g -ldl
CSHARED = -c -fPIC
LIBFLAGS = -Wall  -g -shared

# Targets
all: client server libblank.so

client: client.c msg.h library.h
	${CC} ${CFLAGS} -o $@ client.c msg.h library.h audio.c

server: server.c msg.h library.h
	${CC} ${CFLAGS} -o $@ server.c msg.h library.h audio.c

# External Filter Libraries
libblank.so: libblank.o
	${CC} ${LIBFLAGS} -o $@ libblank.o 

libblank.o: libblank.c
	${CC} ${CSHARED} libblank.c
# House-cleaning
clean:
	rm client server *.so *.o
