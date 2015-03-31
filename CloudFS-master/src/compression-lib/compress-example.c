/* zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.4  11 December 2005  Mark Adler */

/* Version history:
   1.0  30 Oct 2004  First version
   1.1   8 Nov 2004  Add void casting for unused return values
                     Use switch statement for inflate() return values
   1.2   9 Nov 2004  Add assertions to document zlib guarantees
   1.3   6 Apr 2005  Remove incorrect assertion in inf()
   1.4  11 Dec 2005  Add hack to avoid MSDOS end-of-line conversions
                     Avoid some compiler warnings for input and output buffers
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"
#include "compressapi.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

/* report a zlib or i/o error */
void zerr(int ret)
{
    fputs("zpipe: ", stderr);
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            fputs("error reading stdin\n", stderr);
        if (ferror(stdout))
            fputs("error writing stdout\n", stderr);
        break;
    case Z_STREAM_ERROR:
        fputs("invalid compression level\n", stderr);
        break;
    case Z_DATA_ERROR:
        fputs("invalid or incomplete deflate data\n", stderr);
        break;
    case Z_MEM_ERROR:
        fputs("out of memory\n", stderr);
        break;
    case Z_VERSION_ERROR:
        fputs("zlib version mismatch!\n", stderr);
    }
}

/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    int ret = 0;
	int len = 0;
	FILE *infp = NULL;
	FILE *outfp = NULL;

	if (argc == 3) {
		printf("compressing\n");
		infp = fopen(argv[1], "r");
		outfp = fopen(argv[2], "w");
	} else if (argc == 4) {
		printf("de compressing\n");
		infp = fopen(argv[2], "r");
		outfp = fopen(argv[3], "w");
	} else {
		printf("invalid\n");
		return 0;
	}

    /* do compression if no arguments */
    if (argc == 3) {
		fseek(infp, 0, SEEK_END);
		len = ftell(infp);
		fseek(infp, 0, SEEK_SET);
		
        ret = def(infp, outfp, len, Z_DEFAULT_COMPRESSION);
    }

    /* do decompression if -d specified */
    else if (argc == 4 && strcmp(argv[1], "-d") == 0) {
        ret = inf(infp, outfp);
    }

	return ret;
}
