// net/level_send.c

#include "level_send.h"
#include "connection.h"
#include <zlib.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <pthread.h>
#endif

typedef struct {
    Connection* conn;
    unsigned char* blocks;
    int len;
} LevelSendJob;

static void runJob(LevelSendJob* job) {
    uLong bound = compressBound((uLong)job->len + 4) + 64;
    unsigned char* out = (unsigned char*)malloc(bound);
    if (!out) { free(job->blocks); free(job); return; }

    z_stream strm;
    memset(&strm, 0, sizeof strm);
    // 15+16 = zlib's windowBits convention for producing a gzip wrapped
    // stream instead of a raw zlib one
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(out); free(job->blocks); free(job);
        return;
    }

    unsigned char lenPrefix[4] = {
        (unsigned char)(job->len >> 24), (unsigned char)(job->len >> 16),
        (unsigned char)(job->len >> 8),  (unsigned char)job->len
    };

    strm.next_out = out;
    strm.avail_out = (uInt)bound;

    // two deflate() calls into the same stream, matching the real source
    // writing the length prefix then the block bytes through one continuous
    // GZIPOutputStream -- both land in a single gzip member either way
    strm.next_in = lenPrefix;
    strm.avail_in = 4;
    deflate(&strm, Z_NO_FLUSH);

    strm.next_in = job->blocks;
    strm.avail_in = (uInt)job->len;
    deflate(&strm, Z_FINISH);

    int compressedLen = (int)(bound - strm.avail_out);
    deflateEnd(&strm);
    free(job->blocks);

    // publish length before the pointer, so the moment the main thread sees
    // pendingLevelBytes go non-null, pendingLevelLen is already valid
    job->conn->pendingLevelLen = compressedLen;
    job->conn->pendingLevelBytes = out;

    free(job);
}

#if defined(_WIN32)
static DWORD WINAPI threadMain(LPVOID arg) { runJob((LevelSendJob*)arg); return 0; }
#else
static void* threadMain(void* arg) { runJob((LevelSendJob*)arg); return NULL; }
#endif

void LevelSend_start(Connection* conn, const unsigned char* blocks, int len) {
    LevelSendJob* job = (LevelSendJob*)malloc(sizeof *job);
    job->conn = conn;
    job->blocks = (unsigned char*)malloc((size_t)len);
    memcpy(job->blocks, blocks, (size_t)len);
    job->len = len;

#if defined(_WIN32)
    HANDLE h = CreateThread(NULL, 0, threadMain, job, 0, NULL);
    if (h) CloseHandle(h); // detached: nothing needs to join it
#else
    pthread_t t;
    if (pthread_create(&t, NULL, threadMain, job) == 0) pthread_detach(t);
#endif
}
