CC = gcc
CFLAGS = -Wall
SRC = $(wildcard *.c)
PROGS = $(patsubst %.c,%,$(SRC))

# This compiles all the .c source files in the current directory individually,
# outputting the relative binary without the .c extension
all: $(PROGS)

depend:
	makedepend $(SRC);
clean:
	rm $(PROGS);
