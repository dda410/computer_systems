CC = gcc
CFLAGS = -Wall -Werror -g -ldl
CSHARED = -c -fPIC
LIBFLAGS = -Wall -Werror -g -shared

# linking the client and server binaries with the dynamic libraries
all: client server libencrypt.so libvol.so

client: client.c msg.h library.h
	${CC} ${CFLAGS} -o $@ client.c msg.h library.h audio.c

server: server.c msg.h library.h
	${CC} ${CFLAGS} -o $@ server.c msg.h library.h audio.c

# dynamic libraries compilation rules
libencrypt.so: libencrypt.o
	${CC} ${LIBFLAGS} -o $@ libencrypt.o 

libencrypt.o: libencrypt.c
	${CC} ${CSHARED} libencrypt.c

libvol.so: libvol.o
	${CC} ${LIBFLAGS} -o $@ libvol.o 

libvol.o: libvol.c
	${CC} ${CSHARED} libvol.c

# removing compiled binaries and libraries
clean:
	rm client server *.so *.o
