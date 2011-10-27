/*
 * ioshell.c
 *
 * This program uses rdtsc rather than blktrace.
 */
#define	_GNU_SOURCE
#define	_POSIX_C_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>

#include "testlib.h"
#include "rdtsc.h"

#define	MAXLINE		128
#define	SPACE		"  "
#define	CPU_KHZ		1866655

#define	MIN(x, y)	(x) > (y) ? (y) : (x);
#define	MAX(x, y)	(x) > (y) ? (x) : (y);

static char linebuf[MAXLINE];
static char cmdbuf[MAXLINE];
static char devname[MAXLINE];
static flash_test_t *fft;
static flash_test_t fft_buf;

static int quit;

extern int errno;

/*
 * Command handling stuffs.
 */
struct shell_command_st {
	char *cmd;
	int (*cmd_func)(void);
	char *description;
};
typedef struct shell_command_st		shell_command_t;

struct rw_request_st {
	unsigned long start;
	unsigned long npages;
	unsigned long step;
	int times;
	int times_step;
	char reverse;
};
typedef struct rw_request_st	rw_request_t;

/*
 * Show outputs directly in this terminal.
 */
static char *outfilename;
static FILE *outfp;

#ifdef	__USE_BLKTRACE__
static char outbuf[MAXLINE];
static FILE *blkfifo;

static void fetch_blk_output(void)
{
	if (blkfifo == NULL)
		return;

fetch_again:
	fgets(outbuf, MAXLINE-1, blkfifo);
	if (outbuf[0] == '#')	/* We don't print the comment. */
		goto fetch_again;
}
#endif

/*
 * Function prototypes for command-line handling.
 */
static int cmd_help(void);
static int cmd_quit(void);
static int cmd_open(void);
static int cmd_close(void);
static int cmd_info(void);
static int cmd_read(void);
static int cmd_write(void);
static int cmd_output(void);
static int cmd_reset(void);
static int cmd_chunk(void);

static inline char *trimline(char *line);
static int line_empty(const char *line);
static int read_page_range(rw_request_t *req);
static inline int parse_command(void);

static shell_command_t command[] = {
	{ "help",	& cmd_help,	"Show help message." },
	{ "quit",	& cmd_quit,	"Quit the program." },
	{ "open",	& cmd_open,	"Open the device." },
	{ "close",	& cmd_close,	"Close the device." },
	{ "info",	& cmd_info,	"Shows the device information." },
	{ "read",	& cmd_read,	"Read one or more pages." },
	{ "write",	& cmd_write,	"Write one or more pages." },
	{ "output",     & cmd_output,   "Set output file option." },
	{ "reset",      & cmd_reset,    "Reset sequence number of output lines." },
	{ "chunk",	& cmd_chunk,	"Set IO chunk size." }
};


enum {
	CMD_HELP = 0, CMD_QUIT, CMD_OPEN, CMD_ClOSE, CMD_INFO, CMD_READ, CMD_WRITE,
	CMD_OUTPUT, CMD_RESET, CMD_CHUNK,
	N_COMMANDS
};

static int cmd_help(void)
{
	int i;

	printf("\n");
	for (i = 0; i < sizeof(command) / sizeof(shell_command_t); i++) {
		if (strlen(command[i].cmd) < 6)
			printf("  %s\t\t - %s\n", command[i].cmd, command[i].description);
		else
			printf("  %s\t - %s\n", command[i].cmd, command[i].description);
	}

	/*
	 * TODO:
	 * This method is changed. Update it.
	 */
	printf(	"\n" SPACE "When reading or writing pages you can specify"
		"\n" SPACE " - page number(s)"
		"\n" SPACE " - (n) number of times to read or write"
		"\n" SPACE " - (s) stepping value, if not specified assume 0\n"
		"\n" SPACE "For example:"
		"\n" SPACE " - 0n64s1 : { 0, 1, 2, ..., 64 }"
		"\n" SPACE " - 1n5s64 : { 1, 65, 129, 193, 257 }"
		"\n" SPACE " - 32n3   : { 32, 32, 32 }"
		"\n");

	return 0;
}

static int cmd_quit(void)
{
	quit = 1;

	if (fft != NULL)
		fft_exit(fft);

	return 0;
}

static int cmd_open(void)
{
	/*int dev_type = -1;*/

	if (fft != NULL) {
		printf(	"\n" SPACE "Device %s is already opened."
			"\n" SPACE "You have to close the device if you want to "
			"\n" SPACE "open another device!\n", devname);
		return 0;
	}

	printf("\n" SPACE "Device name: ");
	fflush(stdout);

	if (fgets(cmdbuf, MAXLINE-1, stdin) == NULL)
		return -1;

	cmdbuf[strlen(cmdbuf)-1] = '\0';
	strcpy(devname, cmdbuf);

#if 0
	do {
		int ch;

		printf(SPACE "Device type ('l' for large block, 's' for small block): ");
		fflush(stdout);

		if (fgets(cmdbuf, MAXLINE-1, stdin) == NULL)
			return -1;
		switch (ch = cmdbuf[0]) {
			case 'l': dev_type = DEVTYPE_LARGE; break;
			case 's': dev_type = DEVTYPE_SMALL; break;
			default: continue;
		}
	} while (dev_type < 0);
#endif

	fft = &fft_buf;

	if (fft_init(fft, devname, DEVTYPE_LARGE) < 0) {
		perror(SPACE "Cannot open device");
		return -1;
	}

	printf("\n" SPACE "Device %s is opened.\n", devname);

	return 0;
}

static int cmd_close(void)
{
	if (fft == NULL) {
		printf(SPACE "No device is opened.\n");
		return 0;
	}

	fft_exit(fft);
	fft = NULL;

	printf(SPACE "Device %s is closed.\n", devname);
	devname[0] = '\0';

	return 0;
}

static int cmd_info(void)
{
	if (fft == NULL) {
		printf(SPACE "No device is opened.\n");
		return 0;
	}

	printf("\n");
	fft_dump(fft);

	return 0;
}

static int cmd_chunk(void)
{
	unsigned long chunk_size = 0;

	if (fft == NULL) {
		printf(SPACE "No device is opened.\n");
		return 0;
	}

	printf("\n");

	printf(SPACE "Current IO chunk size is %d Bytes\n", fft->page_size);
	printf(SPACE "Input new chunk size in bytes (press ENTER if you don't want to change): ");
	fflush(stdout);

	if (fgets(cmdbuf, MAXLINE-1, stdin) == NULL)
		return -1;

	if (line_empty(cmdbuf))
		return 0;

	sscanf(cmdbuf, "%ld", &chunk_size);

	if (chunk_size > fft->dev_size) {
		printf(SPACE "It cannot be bigger than device size (%lld)\n", fft->dev_size);
		return 0;
	}

	fft->page_size = chunk_size;
	return 0;
}

static int cmd_read(void)
{
	int i, times;
	unsigned long pageno, step;
	unsigned long long tsc;
	rw_request_t req;

	if (fft == NULL) {
		printf(SPACE "No device is opened.\n");
		return 0;
	}
	printf("\n");

read_again:
	printf(SPACE "Pages to read: ");
	fflush(stdout);

	if (fgets(cmdbuf, MAXLINE-1, stdin) == NULL)
		return -1;

	if (line_empty(cmdbuf))
		goto read_again;

	if (read_page_range(&req) < 0) {
		printf("\n" SPACE "Invalid page range!\n");
		goto read_again;
	}

	step = req.step;
	if (req.reverse)
		step *= -1;

	printf("\n");

	for (times = 0; times < req.times; times++) {
		for (i = 0; i < req.npages; i++) {
			pageno = req.start + i*step + req.times_step * times;
			if (pageno > NPAGES(fft) - 1)
				continue;
			if ((tsc = fft_read_page(fft, pageno, NULL)) < 0) {
				perror(SPACE "Error while writing a page");
				return -1;
			}

#ifdef	__USE_BLKTRACE__
			fetch_blk_output();
			fputs(outbuf, outfp);
#else
			fprintf(outfp, "%lu\t%5.6lf\n", pageno, (double) tsc / CPU_KHZ * 1000);
#endif
		}
	}

	return 0;
}

static int cmd_write(void)
{
	int i, times;
	unsigned long pageno, step;
	unsigned long long tsc;
	rw_request_t req;


	if (fft == NULL) {
		printf(SPACE "No device is opened.\n");
		return 0;
	}
	printf("\n");

write_again:
	printf(SPACE "Pages to write: ");
	fflush(stdout);

	if (fgets(cmdbuf, MAXLINE-1, stdin) == NULL)
		return -1;

	if (line_empty(cmdbuf))
		goto write_again;

	if (read_page_range(&req) < 0) {
		printf("\n" SPACE "Invalid page range!\n");
		goto write_again;
	}

	step = req.step;
	if (req.reverse)
		step *= -1;

	printf("\n");

	for (times = 0; times < req.times; times++) {
		for (i = 0; i < req.npages; i++) {
			pageno = req.start + i*step + req.times_step * times;
			if (pageno > NPAGES(fft) - 1)
				continue;
			if ((tsc = fft_write_page(fft, pageno, NULL)) < 0) {
				perror(SPACE "Error while writing a page");
				return -1;
			}

#ifdef	__USE_BLKTRACE__
			fetch_blk_output();
			fputs(outbuf, outfp);
#else
			fprintf(outfp, "%lu\t%5.6lf\n", pageno, (double) tsc / CPU_KHZ * 1000);
#endif
		}
	}

	fflush(outfp);

	return 0;

}

static inline void show_current_output_setting(void)
{
	printf("\n");
	printf(SPACE "Current output = %s\n",
			outfp == stdout ? "stdout" : outfilename);
	printf("\n");
}

static int cmd_output(void)
{
	char *input;
	char *pos;

	show_current_output_setting();

	printf(SPACE "    1. Output to specified file.\n"
	       SPACE "    2. Output to stdout.\n"
	       SPACE "    3. Don't change current setting.\n\n"
	       SPACE "Select one: ");
	fflush(stdout);

	fgets(linebuf, MAXLINE-1, stdin);
	input = trimline(linebuf);

	switch (input[0]) {
	case '1':
output_file:
		pos = linebuf;
		pos += sprintf(pos, "result/");

		printf(SPACE "Output filename: ");
		fflush(stdout);

		fgets(pos, MAXLINE-1, stdin);
		linebuf[strlen(linebuf) - 1] = '\0';

		if (outfp && outfp != stdout) {
			fclose(outfp);
			free(outfilename);
		}

		if ((outfp = fopen(linebuf, "w+")) == NULL) {
			perror("Error while opening the file");
			goto output_file;
		}

		outfilename = strdup(linebuf);
		break;

	case '2':
		if (outfp && outfp != stdout) {
			fclose(outfp);
			free(outfilename);
		}
		outfilename = NULL;
		outfp = stdout;
	default:
		return 0;
	}

	show_current_output_setting();

	return 0;
}

static int cmd_reset(void)
{
#ifdef	__USE_BLKTRACE__
	FILE *fp;
	int pid;

	if ((fp = popen("pidof blkdata", "r")) == NULL) {
		perror("popen");
		return -1;
	}
	fscanf(fp, "%d", &pid);
	pclose(fp);

	printf("\nSend SIGUSR1 to blkdata(%d)\n", pid);

	if (kill(pid, SIGUSR1) < 0) {
		perror("kill");
		return -1;
	}
#endif

	return 0;
}

/*
 * Utility functions.
 */

static inline char *trimline(char *line)
{
	char *start = line;
	char *end = line + strlen(line) - 1;

	while (isspace(*start))
		start++;
	while (isspace(*end))
		end--;
	*(end + 1) = 0;

	return start;
}

static int line_empty(const char *line)
{
	int ch;

	while ((ch = *line++) != '\0')
		if (!isspace(ch))
			return 0;
	return 1;
}

/*
 * Page range format:
 *
 *	[1]. start_address
 *	(2). n + (number of pages)
 *	(3). s + (step width)
 *	(4). , (number of times)(+|-)
 *
 * example)
 *	0		: page 0
 *	0n12		: page 0,0,0,...,0
 *	0n12s1		: page 0,1,2,...,11
 *	0n12s4		: page 0,4,8
 *	0n12s5,2	: page 0,5,10, 0,5,10
 *	0n12s5,2+1	: page 0,5,10, 1,6,11
 *	0n12s5,2-1	: page 0,5,10,   4,9
 *
 */
static int read_page_range(rw_request_t *req)
{
	char *pos1, *pos2;
	char *line = trimline(cmdbuf);

	/* Reverse or not? */
	if ((pos1 = strchr(line, 'r')) == NULL) {
		req->reverse = 0;
		pos1 = line;
	}
	else {
		req->reverse = 1;
		*pos1 = '\0';
		pos1++;
	}

	/* times, times_step */
	if ((pos1 = strchr(pos1, ',')) != NULL) {
		*pos1 = '\0';
		pos1++;

		if ((pos2 = strchr(pos1, '+')) != NULL) {
			*pos2 = '\0';
			pos2++;
			if (isdigit(*pos1) && isdigit(*pos2)) {
				req->times = atoi(pos1);
				req->times_step = atoi(pos2);
			}
			else
				return -1;
		}
		else if ((pos2 = strchr(pos1, '-')) != NULL) {
			*pos2 = '\0';
			pos2++;
			if (isdigit(*pos1) && isdigit(*pos2)) {
				req->times = atoi(pos1);
				req->times_step = -1 * atoi(pos2);
			}
			else
				return -1;
		}
		else {
			if (isdigit(*pos1)) {
				req->times = atoi(pos1);
				req->times_step = 0;
			}
			else
				return -1;
		}
	}
	else {
		req->times = 1;
		req->times_step = 0;
	}

	/* start_address, number of pages, step */
	pos1 = strchr(line, 'n');
	pos2 = strchr(line, 's');

	if (pos1 == NULL && pos2 == NULL) {
		req->start = atol(line);
		req->npages = 1;
		req->step = 0;

		return 0;
	}

	if (pos1 == NULL)
		return -1;

	*pos1 = '\0';
	pos1++;
	if (isdigit(*pos1))
		req->npages = atoi(pos1);
	else
		return -1;

	if (pos2 == NULL) {
		req->step = 0;
	}
	else {
		*pos2 = '\0';
		pos2++;
		if (isdigit(*pos2))
			req->step = atol(pos2);
		else
			return -1;
	}

	req->start = atol(line);

	return 0;
}

static inline int is_line_empty(char *line)
{
	int i;
	for (i = 0; i < strlen(line); i++)
		if (!isspace(line[i]))
			return 0;
	return 1;
}

static inline int parse_command(void)
{
	int i;
	char *input = trimline(linebuf);

	if (is_line_empty(input))
		return N_COMMANDS;

	for (i = 0; i < N_COMMANDS; i++)
		if (!strncmp(command[i].cmd, input, strlen(command[i].cmd)))
			return i;

	return -1;
}

/*
 * Main program.
 */
int main(int argc, char **argv)
{
	int res = 0;
	pid_t pid;
	cpu_set_t cpu_mask;

	devname[0] = '\0';
	pid = getpid();

	CPU_ZERO(&cpu_mask);
	CPU_SET(0, &cpu_mask);

	printf("pid = [%d]\n", pid);

	/*
	 * Open the fifo file for gathering output.
	 */
#ifdef	__USE_BLKTRACE__
	if ((blkfifo = fopen("blkfifo", "r")) == NULL) {
		perror("fopen");
		return -1;
	}
#endif

	/*
	 * Default output file is stdout.
	 */
	outfp = stdout;

	do {
		int cmdno;
		int (*func) (void);

		printf("\n\x1b[32m[IO Shell%s\x1b[0m\x1b[33m%s\x1b[0m\x1b[32m]\x1b[0m ",
			devname[0] == '\0' ? "" : " @ ", devname);
		fflush(stdout);

		if (fgets(linebuf, MAXLINE-1, stdin) == NULL) {
			if (ferror(stdin)) {
				perror(SPACE "Error while reading a line");
				res = errno;
			}
			goto cleanup;
		}

		if ((cmdno = parse_command()) < 0) {
			printf(SPACE "Unknown command: %s\n", linebuf);
			continue;
		}
		if (cmdno == N_COMMANDS)
			continue;

		func = command[cmdno].cmd_func;

		if (func() < 0) {
			printf(SPACE "Error while execute command: %s\n", linebuf);
			continue;
		}

	} while (!quit);

cleanup:
	if (outfp && outfp != stdout) {
		fclose(outfp);
		free(outfilename);
	}

#ifdef	__USE_BLKTRACE__
	fclose(blkfifo);
#endif

	return res;
}

