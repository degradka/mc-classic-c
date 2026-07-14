// jgzip.c

#include "jgzip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

uint8_t* jgzip_compress(const uint8_t* data, size_t len, size_t* outLen) {
    z_stream z;
    memset(&z, 0, sizeof z);
    deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    size_t cap = deflateBound(&z, (uLong)len) + 18;
    uint8_t* out = (uint8_t*)malloc(cap);
    static const uint8_t HEADER[10] = { 0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 0 };
    memcpy(out, HEADER, 10);
    z.next_in = (Bytef*)data;
    z.avail_in = (uInt)len;
    z.next_out = out + 10;
    z.avail_out = (uInt)(cap - 18);
    deflate(&z, Z_FINISH);
    size_t n = 10 + z.total_out;
    deflateEnd(&z);
    uint32_t crc = (uint32_t)crc32(0L, data, (uInt)len);
    uint32_t size = (uint32_t)len;
    for (int i = 0; i < 4; ++i) out[n++] = (uint8_t)(crc >> (8 * i));
    for (int i = 0; i < 4; ++i) out[n++] = (uint8_t)(size >> (8 * i));
    *outLen = n;
    return out;
}

uint8_t* jgzip_decompress(const uint8_t* data, size_t len, size_t* outLen, char* exception, size_t size) {
    if (len < 2 || data[0] != 0x1F || data[1] != 0x8B) {
        snprintf(exception, size, len < 2 ? "java.io.EOFException" : "java.io.IOException: Not in GZIP format");
        return NULL;
    }
    z_stream z;
    memset(&z, 0, sizeof z);
    inflateInit2(&z, 16 + 15);
    size_t cap = len * 4 + 4096, n = 0;
    uint8_t* out = (uint8_t*)malloc(cap);
    z.next_in = (Bytef*)data;
    z.avail_in = (uInt)len;
    for (;;) {
        if (n == cap) {
            cap *= 2;
            out = (uint8_t*)realloc(out, cap);
        }
        z.next_out = out + n;
        z.avail_out = (uInt)(cap - n);
        int r = inflate(&z, Z_NO_FLUSH);
        n = z.total_out;
        if (r != Z_OK) break;
    }
    inflateEnd(&z);
    *outLen = n;
    return out;
}

// what a GZIPOutputStream that never got closed has written, the deflater's output so far and no end
uint8_t* jgzip_unfinished(const uint8_t* data, size_t len, size_t* outLen) {
    z_stream z;
    memset(&z, 0, sizeof z);
    deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    size_t cap = deflateBound(&z, (uLong)len) + 10;
    uint8_t* out = (uint8_t*)malloc(cap);
    static const uint8_t HEADER[10] = { 0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 0 };
    memcpy(out, HEADER, 10);
    z.next_in = (Bytef*)data;
    z.avail_in = (uInt)len;
    z.next_out = out + 10;
    z.avail_out = (uInt)(cap - 10);
    deflate(&z, Z_NO_FLUSH);
    *outLen = 10 + z.total_out;
    deflateEnd(&z);
    return out;
}
