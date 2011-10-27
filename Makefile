#
# Makefile
#

CC = gcc
CFLAGS = -g -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

# By default, this program uses rdtsc to measure timing.
# If you want to use blktrace rather than rdtsc turn this flag
# on.
CFLAGS += -D__USE_BLKTRACE__

TARGETS = ioshell blkdata

%: %.o
	$(CC) $(CFLAGS) -o  $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(TARGETS)

clean:
	rm -f *.o $(TARGETS)

ioshell: testlib.o ioshell.o
	$(CC) $(CFLAGS) -o $@ $^

blkdata: blkdata.o
	$(CC) $(CFLAGS) -o $@ $^

