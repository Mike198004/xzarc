/*
 * 
 * File: xzarc.c
 * Desc: .xz compressor/decompressor
 * Author: Genie (C) 2019
 *
 */
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "xzarc.h"

#define XZARC_OK		0
#define XZARC_INIT_ERR		1
#define XZARC_INPUT_ERR		2
#define XZARC_OUTPUT_ERR	3
#define XZARC_COMPRESS_ERR	4
#define XZARC_MODE_ERR		5
#define XZARC_DECOMPRESS_ERR	6

/* archiver working mode */
enum {
    XZARC_COMPRESS_MODE = 0,
    XZARC_DECOMPRESS_MODE = 1 };

static char *err_msg[] = {
    "Ok", "Failed init", "Failed input", "Failed output", "Failed compress", "Failed decompress" };

static char *prog_name;				/* program name */
static int compress_level = COMPRESSION_LEVEL;	/* default compression level */
static bool compress_extreme = false;		/* extreme false */
static int remove_in = 0;			/* by default, don't remove input files */

int lzma_compress_file(const char *in_fname, char *out_fname)
{
    int res = XZARC_OK, err = 0;
    uint8_t in_buf[IN_BUF_SIZE];
    uint8_t out_buf[OUT_BUF_SIZE];
    uint32_t lzma_set =  compress_level | (compress_extreme ? LZMA_PRESET_EXTREME : 0);
    lzma_check check = INTEGRITY_CHECK;
    lzma_stream lzma_strm = LZMA_STREAM_INIT;
    lzma_action action;
    lzma_ret xz_code;
    uint32_t in_len = 0;
    uint32_t out_len = 0, percent = 0;
    bool in_ok = false;
    bool out_ok = false;
    int in_file = 0;
    int out_file = 0;
    struct stat *info = NULL;
    uint64_t infile_size = 0;
    uint64_t outfile_size = 0;
    
    info = (struct stat *)malloc(sizeof(struct stat));
    if (!info)
	return 5;
    err = stat(in_fname, info);
    if (err < 0) {
	printf("lzma_compress_file(): input file not found!\n");
	return (XZARC_INPUT_ERR);
    }
    /* open files */
    in_file = open(in_fname, O_RDONLY);
    out_file = open(out_fname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if ((in_file <= 0) || (out_file <= 0)) {
	printf("lzma_compress_file(): error opening file!\n");
	free(info);
	return 1;
    }
    infile_size = info->st_size;
    printf("Using compression level %d, extreme mode %d\n",
	compress_level, compress_extreme);
    printf("Compressing file %s, size %lld bytes...",
	in_fname, (long long)info->st_size);
    /* init lzma compression */
    xz_code = lzma_easy_encoder(&lzma_strm, lzma_set, check);
    if (xz_code != LZMA_OK) {
	printf("lzma_easy_encoder(): error init encoder, code = %d!\n",
	    xz_code);
	free(info);
	return XZARC_INIT_ERR;
    }
    /* main reading loop */
    while((!in_ok) && (!out_ok)) {
	err = read(in_file, in_buf, IN_BUF_SIZE);
	if (err == 0) {
	    in_ok = true;
	}
	if (err < 0) {
	    in_ok = true;
	    res = XZARC_INPUT_ERR;
	}
	in_len = err;
	lzma_strm.next_in = in_buf;
	lzma_strm.avail_in = in_len;
	action = in_ok ? LZMA_FINISH : LZMA_RUN;
	do {	/* main encoding loop */
	    lzma_strm.next_out = out_buf;
	    lzma_strm.avail_out = OUT_BUF_SIZE;
	    /* compressing data */
	    xz_code = lzma_code(&lzma_strm, action);
	    if ((xz_code != LZMA_OK) && (xz_code != LZMA_STREAM_END)) {
		printf("lzma_compress_file(): error compressing data, code = %d!\n",
		    xz_code);
		out_ok = true;
		res = XZARC_COMPRESS_ERR;
	    } else {
		out_len = OUT_BUF_SIZE - lzma_strm.avail_out;
		err = write(out_file, out_buf, out_len);
		if (err < 0) {
		    out_ok = true;
		    res = XZARC_OUTPUT_ERR;
		}
	    }
	} while(lzma_strm.avail_out == 0);
    };
    lzma_end(&lzma_strm);
    close(in_file);
    close(out_file);
    printf("%s\n", err_msg[res]);

    /* checking output file */
    //memset(&info, 0, sizeof(struct stat));
    err = stat(out_fname, info);
    if (err == 0) {
	outfile_size = info->st_size;
	percent = 100-(((float)outfile_size/infile_size)*100);
	printf("Output file %s, size %lld bytes, compression ratio -%d%%\n",
	    out_fname, (long long)info->st_size, percent);
	printf("owner uid = %d, gid = %d, mode = %x\n", info->st_uid, info->st_gid, info->st_mode);
	if (remove_in) {
	    printf("Input file %s removed.\n", in_fname);
	    unlink(in_fname);
	}
    }
    free(info);

    return (res);
}

int lzma_decompress_file(char *in_fname, char *out_fname)
{
    int res = XZARC_OK, err = 0;
    int in_len = 0, out_len = 0;
    uint8_t in_buf[IN_BUF_SIZE];
    uint8_t out_buf[OUT_BUF_SIZE];
    const uint64_t mem_limit = UINT64_MAX;
    const uint32_t flags = LZMA_TELL_UNSUPPORTED_CHECK | LZMA_CONCATENATED;
    lzma_action action;
    lzma_ret xz_code;
    lzma_stream lzma_strm = LZMA_STREAM_INIT;
    int in_file = 0;
    int out_file = 0;
    int percent = 0;
    bool in_ok = false;
    bool out_ok = false;
    uint64_t infile_size = 0;
    uint64_t outfile_size = 0;
    struct stat *info = NULL;

    info = (struct stat*)malloc(sizeof(struct stat));
    err = stat(in_fname, info);
    if (err < 0) {
	printf("lzma_decompress_file(): input file not found!\n");
	free(info);
	return (XZARC_INPUT_ERR);
    }
    infile_size = info->st_size;
    in_file = open(in_fname, O_RDONLY);
    out_file = open(out_fname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if ((in_file <= 0) || (out_file <= 0)) {
	printf("lzma_decompress_file(): error opening file!\n");
	free(info);
	return 1;
    }
    
    printf("Decompressing file %s, size %lld bytes...",
	in_fname, (long long)info->st_size);
    /* init lzma decompression */
    xz_code = lzma_stream_decoder(&lzma_strm, mem_limit, flags);
    if (xz_code != LZMA_OK) {
	printf("lzma_easy_decoder(): error init encoder, code = %d!\n",
	    xz_code);
	free(info);
	return XZARC_INIT_ERR;
    }
    /* main reading loop */
    while((!in_ok) && (!out_ok)) {
	err = read(in_file, in_buf, IN_BUF_SIZE);
	if (err == 0) {
	    in_ok = true;
	}
	if (err < 0) {
	    in_ok = true;
	    res = XZARC_INPUT_ERR;
	}
	in_len = err;
	lzma_strm.next_in = in_buf;
	lzma_strm.avail_in = in_len;
	action = in_ok ? LZMA_FINISH : LZMA_RUN;
	do {	/* main decoding loop */
	    lzma_strm.next_out = out_buf;
	    lzma_strm.avail_out = OUT_BUF_SIZE;
	    /* decompressing data */
	    xz_code = lzma_code(&lzma_strm, action);
	    if ((xz_code != LZMA_OK) && (xz_code != LZMA_STREAM_END)) {
		printf("lzma_decompress_file(): error decompressing data, code = %d!\n",
		    xz_code);
		out_ok = true;
		res = XZARC_DECOMPRESS_ERR;
	    } else {
		out_len = OUT_BUF_SIZE - lzma_strm.avail_out;
		err = write(out_file, out_buf, out_len);
		if (err < 0) {
		    out_ok = true;
		    res = XZARC_OUTPUT_ERR;
		}
	    }
	} while(lzma_strm.avail_out == 0);
    };
    lzma_end(&lzma_strm);
    close(in_file);
    close(out_file);
    printf("%s\n", err_msg[res]);

    /* checking output file */
    //memset(&info, 0, sizeof(struct stat));
    err = stat(out_fname, info);
    if (err == 0) {
	outfile_size = info->st_size;
	percent = 100-(((float)infile_size/outfile_size)*100);
	printf("Output file %s, size %lld bytes, decompression ratio +%d%%\n",
	    out_fname, (long long)info->st_size, percent);
	printf("owner uid = %d, gid = %d, mode = %x\n", info->st_uid, info->st_gid, info->st_mode);
	if (remove_in) {
	    printf("Input file %s removed.\n", in_fname);
	    unlink(in_fname);
	}
    }
    free(info);

    return (res);
}

static void usage(void)
{
    printf("Usage: %s [-cdrl(0..9)h] <in_file>\n",
	prog_name);
}

static void version()
{
    printf("XZarc ver. %s Genie (C) 2019\n", XZARC_VER);
    exit(0);
}

static void help(void)
{
    usage();
    printf("Help:\n" \
	"-c : Compression mode;\n" \
	"-d : Decompression mode;\n" \
	"-l<level> : (0..9) compression mode level;\n" \
	"-r : remove input file;\n" \
	"-h : this help;\n" \
	"-v : print version;\n");
    exit(1);
}


static int check_file(char *fname)
{
    if (strstr(fname, ".xz") != NULL)
	return 1;
    else 
	return 0;
}

int main(int argc, char **argv, char **env)
{
    int res = 0, i = 0;
    char *in_file = NULL;
    char *out_file = NULL;
    int mode = 0, num_in_files = 0;	/* by default, compression mode */
    int opt = 0;
    char **in_files = NULL;	/* input files */
    
    prog_name = argv[0];
    if (argc < 2) {
	usage();
	return 1;
    }
    
    opt = getopt(argc, argv, "cdl:rhv");
    while(opt != -1) {
	switch(opt) {
	    case 'c':
		mode = XZARC_COMPRESS_MODE;
		break;
	    case 'd':
		mode = XZARC_DECOMPRESS_MODE;
		break;
	    case 'l':
		compress_level = atoi(optarg);
		if (compress_level > 9) {
		    printf("Error in compression level!\n");
		    usage();
		    exit(1);
		}
		if (compress_level > 5)
		    compress_extreme = true;
		break;
	    case 'r':
		remove_in = 1;
		break;
	    case 'h':
		help();
		break;
	    case 'v':
		version();
		break;
	    default:
		break;
	};
	opt = getopt(argc, argv, "cdl:rhv");
    };
    /* storing opts */
    in_files = argv + optind;
    num_in_files = argc - optind;
    /* checking archive mode */
    switch(mode) {
	case XZARC_COMPRESS_MODE:{
	    if (num_in_files < 1) {
		printf("Not found input file!\n");
		usage();
		exit(1);
	    }
	    for(i = 0;i < num_in_files;i++) {
		if (check_file(in_files[i])) {
		    printf("Error: input file %s has already .xz suffix!\n",
			in_files[i]);
		    return 1;
		}
		in_file = in_files[i];
		out_file = (char *)malloc(strlen(in_file) + 4);
		strcpy(out_file, in_file);
		strcat(out_file, ".xz");
		res = lzma_compress_file(in_file, out_file);
		free(out_file);
	    };
	    out_file = NULL;
	    break;
	};
	case XZARC_DECOMPRESS_MODE:{
	    if (num_in_files < 1) {
		printf("Not found input files!\n");
		usage();
		exit(1);
	    }
	    for(i = 0;i < num_in_files;i++) {
		if (!check_file(in_files[i])) {
		    printf("Error: input file %s has no .xz suffix!\n",
			in_files[i]);
		    return 1;
		}
		in_file = in_files[i];
		out_file = (char *)malloc(strlen(in_file));
		strncpy(out_file, in_file, strlen(in_file) - 3);
		res = lzma_decompress_file(in_file, out_file);
		free(out_file);
	    };
	    out_file = NULL;
	    break;
	};
	default:{
	    printf("Not found archive mode!\n");
	    res = XZARC_MODE_ERR;
	    break;
	};
    };
    if (out_file)
	free(out_file);
    
    return (res);
}
