/*
 * testlib.h
 *
 * Interface of flash I/O test library.
 */

#ifndef	__TESTLIB_H__
#define	__TESTLIB_H__

#define	DEVTYPE_UNKNOWN		0
#define	DEVTYPE_SMALL		1
#define	DEVTYPE_LARGE		2

/* Test descriptor. */
struct flash_test_st {
	int	fd;
	char	*name;
	int	dev_type;
	long long dev_size;
	int	page_size;
	int	pages_per_block;
	unsigned long sector_size;	/* TODO: two members should be swapped! */
	unsigned long nsectors;
};
typedef struct flash_test_st	flash_test_t;

#define	NPAGES(this)		((this)->dev_size / (this)->page_size)

#if 0
#define	BLOCKSIZE(this)		((this)->page_size * (this)->pages_per_block)
#define	NBLOCKS(this)		((this)->dev_size / BLOCKSIZE(this))
#define NPAGES(this)		(NBLOCKS(this) * (this)->pages_per_block)

#define	DEVTYPE(this)		((this)->dev_type)
#define	STRDEVTYPE(this)	(DEVTYPE(this) == DEVTYPE_SMALL ? "Small block"	\
				: (DEVTYPE(this) == DEVTYPE_LARGE ? "Large block" : "Unknown"))

#define	fft_dump(this)								\
		do {								\
			printf(	"Name = %s\n"					\
				"Size = %lld Bytes (%lld MBytes)\n"		\
				"Type = %s (1 Block = %d Byte page x %d)\n"	\
				"# of Blocks = %lld\n"				\
				"# of Pages = %lld\n",				\
				(this)->name, (this)->dev_size,			\
				(this)->dev_size >> 20,				\
				STRDEVTYPE(this), (this)->page_size,		\
				(this)->pages_per_block,			\
				NBLOCKS(this), NPAGES(this));			\
		} while (0)
#endif

#define	fft_dump(this)								\
		do {								\
			printf(	"Name = %s\n"					\
				"Size = %lld Bytes (%lld MB)\n"			\
				"Sector size = %ld\n"				\
				"# sectors = %ld\n"				\
				"Chunk size = %d\n"				\
				"# of chunks = %lld\n",				\
				(this)->name, (this)->dev_size,			\
				(this)->dev_size >> 20,				\
				(this)->nsectors, (this)->sector_size,		\
				(this)->page_size, (this)->dev_size / (this)->page_size);	\
		} while (0)


/* Initialize/De-initialize and I/O. */
int fft_init(flash_test_t *this, const char *devname, const int type);
int fft_exit(flash_test_t *this);
unsigned long long fft_read_page(flash_test_t *this, unsigned long pageno, void *buf);
unsigned long long fft_write_page(flash_test_t *this, unsigned long pageno, void *buf);

/* Timer rouines. */

struct timeval;

extern struct timeval elapsed_time;
extern unsigned long elapsed_time_usec;

void timer_start(void);
void timer_end(void);

#define	dio_align(buf)		\
		((unsigned char *) ((unsigned long) buf - ((unsigned long)(buf) % 512) + 512))

#endif	/* __TESTLIB_H__ */

