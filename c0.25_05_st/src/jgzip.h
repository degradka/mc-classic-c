// jgzip.h: GZIPOutputStream and GZIPInputStream

#ifndef JGZIP_H
#define JGZIP_H

#include <stddef.h>
#include <stdint.h>

// a 10 byte header, a level 6 deflate stream, the crc and the size
uint8_t* jgzip_compress(const uint8_t* data, size_t len, size_t* outLen);
uint8_t* jgzip_unfinished(const uint8_t* data, size_t len, size_t* outLen);
// what inflates before the stream ends or breaks, NULL with the exception's toString if the header is wrong
uint8_t* jgzip_decompress(const uint8_t* data, size_t len, size_t* outLen, char* exception, size_t size);

#endif
