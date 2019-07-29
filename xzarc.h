#ifndef __XZARC_H__
#define __XZARC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <lzma.h>

#define XZARC_VER "0.7"
#define COMPRESSION_LEVEL	5
#define COMPRESSION_EXTREME	false
#define INTEGRITY_CHECK		LZMA_CHECK_CRC64

#define IN_BUF_SIZE	0x1000
#define OUT_BUF_SIZE	0x1000

int lzma_compress_file(const char *, char *);
int lzma_decompress_file(char *, char *);

#endif	/* __XZARC_H__ */
