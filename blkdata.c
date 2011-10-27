/*
 * blkdata.c
 *
 * Line format:
 *   8,0    0        3     0.000393417  6781  D   W 0 + 4 [test_seq]
 *   8,0    0        4     0.000759113     2  C   W 0 + 4 [0]
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define	MAXLINE		128
#define	PAGESIZE	2048
#define SECTORSIZE	512

static char linebuf[MAXLINE];
static unsigned long i;

static void sig_SIGUSR1(int arg)
{
	i = 0;
}

static char *strtrim(char *str)
{
	char *tmp = str;
	while (isspace(*tmp))
		tmp++;
	return tmp;
}

int main(int argc, char **argv)
{
	unsigned long linenum = 0;
	int issued_flag = 0;
	FILE *fp;
	unsigned long count = 0;
	float max_time = 0.00F;

	if (argc < 2)
		fp = stdin;
	else
		if ((fp = fopen(argv[1], "r")) == NULL) {
			perror("fopen() failed");
			return -1;
		}

	if (signal(SIGUSR1, sig_SIGUSR1) == SIG_ERR) {
		perror("signal");
		fclose(fp);
		return -1;
	}

	printf("# SEQ\tOP\tCHUNK\tTIME\n");

	while (fgets(linebuf, MAXLINE-1, fp) != NULL) {
		char *line = strtrim(linebuf);
		char action, command;
		int major, minor, cpuid, seq, pid;
		int cpu_issue, cpu_complete;
		unsigned long start_offset, length;
		unsigned long issue[2], complete[2];
		double timestamp;
		double time_issue, time_complete;

		++linenum;

		if (line[0] == '#')
			continue;	/* Ignore the line. */

		/* We reached the end of trace data. */
		if (!strncmp("CPU", line, strlen("CPU"))) {
			break;
		}

		sscanf(line, "%d,%d %d %d %lf %d %c %c %lu + %lu",
			&major, &minor, &cpuid, &seq, &timestamp, &pid, &action,
			&command, &start_offset, &length);
		if (command == 'N')
			continue;

		if (action == 'D') {
			issued_flag = 1;
			time_issue = timestamp;
			cpu_issue = cpuid;
			issue[0] = start_offset;
			issue[1] = length;
		}
		else if (action == 'C') {
			double current;
			/*
			unsigned long pageno = start_offset * SECTORSIZE / PAGESIZE;
			*/
			unsigned long chunkno = start_offset / length;

			time_complete = timestamp;
			current = (time_complete - time_issue) * 1000000;
			cpu_complete = cpuid;
			complete[0] = start_offset;
			complete[1] = length;

			/*
			if (length * SECTORSIZE != PAGESIZE) {
				fprintf(stderr, "length(%lu) is not page size(%d).\n",
						length * SECTORSIZE, PAGESIZE);
				goto err;
			}
			*/
			if (!issued_flag)
				goto err;

			/*
			printf("%lu\t[%c]\t%lu\t%10.3lf\n", i++, command, pageno, current);
			*/
			printf("%lu\t[%c]\t%lu\t%10.3lf\n", i++, command, chunkno, current);
			fflush(stdout);	/* Is this line really needed? */

			issued_flag = 0;
			if (max_time < current)
				max_time = current;
			count++;
		}
		else {
err:
			fprintf(stderr, "file format wrong: %s(linenum = %lu)\n", line, linenum);
			fclose(fp);
			return -1;
		}
	}
	if (ferror(fp)) {
		perror("fgets() failed");
		fclose(fp);
		return -1;
	}
	fprintf(stderr, "xrange [0:%lu]\n", count);
	fprintf(stderr, "yrange [0:%.3f]\n", max_time);

	fclose(fp);
	return 0;
}

