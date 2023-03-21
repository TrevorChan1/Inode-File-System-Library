override CFLAGS := -Wall -Werror -std=gnu99 -O0 -g $(CFLAGS) -I.
CC = gcc

# Build the threads.o file
fs.o: disk.c disk.h fs.c fs.h

# Build all of the test programs
checkprogs: $(test_files)

# Run the test programs
check: checkprogs
	tests/run_tests.sh $(test_files)
