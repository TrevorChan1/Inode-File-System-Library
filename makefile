override CFLAGS := -Wall -Werror -std=gnu99 -O0 -g $(CFLAGS) -I.
CC = gcc

# Build the threads.o file
fs.o: fs.c fs.h

test: disk.c fs.o test.c
