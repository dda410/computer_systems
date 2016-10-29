CC = gcc
CFLAGS = -Wall  -g -ldl
CSHARED = -c -fPIC
LIBFLAGS = -Wall  -g -shared

all: client server libencrypt.so libvoldown.so libvol.so

client: client.c msg.h library.h
	${CC} ${CFLAGS} -o $@ client.c msg.h library.h audio.c

server: server.c msg.h library.h
	${CC} ${CFLAGS} -o $@ server.c msg.h library.h audio.c


libencrypt.so: libencrypt.o
	${CC} ${LIBFLAGS} -o $@ libencrypt.o 

libencrypt.o: libencrypt.c
	${CC} ${CSHARED} libencrypt.c

libvoldown.so: libvoldown.o
	${CC} ${LIBFLAGS} -o $@ libvoldown.o 

libvoldown.o: libvoldown.c
	${CC} ${CSHARED} libvoldown.c

libvol.so: libvol.o
	${CC} ${LIBFLAGS} -o $@ libvol.o 

libvol.o: libvol.c
	${CC} ${CSHARED} libvol.c

clean:
	rm client server *.so *.o
