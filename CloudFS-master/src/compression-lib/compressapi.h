/**
  * @file compressapi.h
  * @author Mukul Kumar Singh
  * @date 13-MAR-2014
  * @brief Interface header for compression library
  *
  * This header file defines the interface for the compress library.
  * 
  * This file contains the deflate and infalte interface to compress and
  * decompress a file.
  */


#ifndef _COMPRESSAPI_H
#define _COMPRESSAPI_H

#include <stdio.h>

/** @brief This api is used to compress/deflate a file
  * 
  * This method compresses a file depending upon the given compression level.
  * 
  * @param source source file pointer of file to be compressed.
  * @param dest output file pointer where the compresed file should be saved
  * @param len length of the segment of the file, which should be compressed,
  *            this is relative to the current offset of the file.
  * @param level level of compression to be used while compressing the file
  *
  * @return returns Z_OK if success
  */
int def(FILE *source, FILE *dest, int len, int level);
/** @brief This api is used to decompress/inflate a file
  * 
  * This method decompresses a file.
  * 
  * @param source source file pointer of file to be compressed.
  * @param dest output file pointer where the compresed file should be saved
  * 
  * @return returns Z_OK if success, negative otherwise
  */
int inf(FILE *source, FILE *dest);

#endif
