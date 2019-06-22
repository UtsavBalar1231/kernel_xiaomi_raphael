/*
 * mkswap.c - format swap device (Linux v1 only)
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* from Linux 2.6.23 */
/*
 * Magic header for a swap area. ... Note that the first
 * kilobyte is reserved for boot loader or disk label stuff.
 */
struct swap_header_v1 {
/*	char     bootbits[1024];    Space for disklabel etc. */
	uint32_t version;        /* second kbyte, word 0 */
	uint32_t last_page;      /* 1 */
	uint32_t nr_badpages;    /* 2 */
	char     sws_uuid[16];   /* 3,4,5,6 */
	char     sws_volume[16]; /* 7,8,9,10 */
	uint32_t padding[117];   /* 11..127 */
	uint32_t badpages[1];    /* 128 */
	/* total 129 32-bit words in 2nd kilobyte */
} __attribute__((__may_alias__));

#define NWORDS 129
#define COMMON_BUFSIZE 1024

static struct swap_header_v1 *hdr;

struct BUG_sizes {
	char swap_header_v1_wrong[sizeof(*hdr)  != (NWORDS * 4) ? -1 : 1];
	char bufsiz1_is_too_small[COMMON_BUFSIZE < (NWORDS * 4) ? -1 : 1];
};

/* Stored without terminating NUL */
static const char SWAPSPACE2[sizeof("SWAPSPACE2")-1] __attribute__((aligned(1))) = "SWAPSPACE2";

static loff_t get_volume_size_in_bytes(int fd)
{
	loff_t result;

	/* more portable than BLKGETSIZE[64] */
	result = lseek(fd, 0, SEEK_END);

	lseek(fd, 0, SEEK_SET);

	/* Prevent things like this:
	 * $ dd if=/dev/zero of=foo count=1 bs=1024
	 * $ mkswap foo
	 * Setting up swapspace version 1, size = 18446744073709548544 bytes
	 *
	 * Picked 16k arbitrarily: */
	if (result < 16*1024) {
		fprintf(stderr, "image is too small\n");
		exit(1);
	}

	return result;
}

int main(int argc, char **argv)
{
	int fd;
	int pagesize;
	off_t len;
	const char *label = "";

	if (argc < 2) {
		fprintf(stderr, "Usage: %s /path/to/swapfile\n", argv[0]);
		exit(1);
	}

	fd = open(argv[1], O_WRONLY);
	if (fd == -1) {
		perror("Failed to open file");
		exit(1);
	}

	/* Figure out how big the device is */
	len = get_volume_size_in_bytes(fd);
	pagesize = getpagesize();
	len -= pagesize;

	hdr = (struct swap_header_v1*)calloc(1, 1024);

	/* Announce our intentions */
	printf("Setting up swapspace version 1, size = %jd bytes\n", (intmax_t)len);

	/* hdr is zero-filled so far. Clear the first kbyte, or else
	 * mkswap-ing former FAT partition does NOT erase its signature.
	 *
	 * util-linux-ng 2.17.2 claims to erase it only if it does not see
	 * a partition table and is not run on whole disk. -f forces it.
	 */
	write(fd, hdr, 1024);

	/* Fill the header. */
	hdr->version = 1;
	hdr->last_page = (uint32_t)(len / pagesize);

	strncpy(hdr->sws_volume, label, 16);

	/* Write the header.  Sync to disk because some kernel versions check
	 * signature on disk (not in cache) during swapon. */
	write(fd, hdr, NWORDS * 4);
	lseek(fd, pagesize - 10, SEEK_SET);
	write(fd, SWAPSPACE2, 10);
	fsync(fd);

	close(fd);

	return 0;
}
