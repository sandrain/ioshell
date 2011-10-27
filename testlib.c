/*
 * testlib.c
 *
 */
#define _GNU_SOURCE
#define	_XOPEN_SOURCE	500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>

#include "testlib.h"
#include "rdtsc.h"

extern int errno;

/* I/O buffer. */
static unsigned char prebuf[65536];
static unsigned char *iobuf;

/* Timer values. */
static struct timezone dummy;
static struct timeval before, after;
struct timeval elapsed_time;
unsigned long elapsed_time_usec;

#if 0
static int page_sizes[3] = { -1, 512, 2048 };
static int pages_per_blocks[3] = { -1, 32, 64 };
#endif


int fft_init(flash_test_t *this, const char *devname, const int type)
{
	unsigned long nsectors, sector_size;

	if (this == NULL || devname == NULL || (type < 0 || type > 2))
		return -1;

	iobuf = dio_align(prebuf);

	if ((this->fd = open(devname, O_RDWR | O_DIRECT)) < 0)
		return errno;
	if ((this->name = strdup(devname)) == NULL)
		return errno;

#if 0
	this->dev_type = type;
	this->page_size = page_sizes[type];
	this->pages_per_block = pages_per_blocks[type];
#endif
	this->dev_type = 'l';
	this->page_size = 4096;
	this->pages_per_block = 0;

	if (ioctl(this->fd, BLKGETSIZE, &sector_size) < 0) {
		return errno;
	}
	if (ioctl(this->fd, BLKSSZGET, &nsectors) < 0) {
		return errno;
	}
	this->dev_size = sector_size;
	this->dev_size *= nsectors;

	this->sector_size = sector_size;
	this->nsectors = nsectors;

	return 0;
}

int fft_exit(flash_test_t *this)
{
	free(this->name);
	close(this->fd);

	return 0;
}

unsigned long long fft_read_page(flash_test_t *this, unsigned long pageno, void *buf)
{
	unsigned char *usebuf = buf ? buf : iobuf;
	off_t offset = pageno * this->page_size;
	unsigned long long before, after;
	unsigned long long ret;

	before = rdtsc();
	ret = pread(this->fd, (void *) usebuf, this->page_size, offset);
	after  = rdtsc();

	return ret < 0 ? ret : after - before;
}

unsigned long long fft_write_page(flash_test_t *this, unsigned long pageno, void *buf)
{
	unsigned char *usebuf = buf ? buf : iobuf;
	off_t offset = pageno * this->page_size;
	unsigned long long before, after;
	unsigned long long ret;

	before = rdtsc();
	ret = pwrite(this->fd, (void *) usebuf, this->page_size, offset);
	after = rdtsc();

	return ret < 0 ? ret : after - before;
}


void timer_start(void)
{
	gettimeofday(&before, &dummy);
}

void timer_end(void)
{
	gettimeofday(&after, &dummy);

	elapsed_time.tv_sec = after.tv_sec - before.tv_sec;
	elapsed_time.tv_usec = after.tv_usec - before.tv_usec;
	if (elapsed_time.tv_usec < 0) {
		elapsed_time.tv_sec -= 1;
		elapsed_time.tv_usec += 1000000;
	}
	elapsed_time_usec = elapsed_time.tv_sec*1000000 + elapsed_time.tv_usec;
}

