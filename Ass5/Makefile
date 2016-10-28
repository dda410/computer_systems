CC = gcc
CFLAGS = -Wall  -g -ldl
CSHARED = -c -fPIC
LIBFLAGS = -Wall  -g -shared

all: client server libblank.so libvoldown.so libvolup.so

client: client.c msg.h library.h
	${CC} ${CFLAGS} -o $@ client.c msg.h library.h audio.c

server: server.c msg.h library.h
	${CC} ${CFLAGS} -o $@ server.c msg.h library.h audio.c


libblank.so: libblank.o
	${CC} ${LIBFLAGS} -o $@ libblank.o 

libblank.o: libblank.c
	${CC} ${CSHARED} libblank.c

libvoldown.so: libvoldown.o
	${CC} ${LIBFLAGS} -o $@ libvoldown.o 

libvoldown.o: libvoldown.c
	${CC} ${CSHARED} libvoldown.c

libvolup.so: libvolup.o
	${CC} ${LIBFLAGS} -o $@ libvolup.o 

libvolup.o: libvolup.c
	${CC} ${CSHARED} libvolup.c

clean:
	rm client server *.so *.o
