// net/level_send.h: background level compression, ported from
// server/a.java (LevelSendThread). A real OS thread, matching the original's
// architecture, so a big map never stalls the main tick loop

#ifndef LEVEL_SEND_H
#define LEVEL_SEND_H

struct Connection;

// copies blocks (the caller's buffer can be freed/reused immediately after
// this returns), compresses in the background, and publishes the result
// into conn->pendingLevelBytes/pendingLevelLen once done. The wire format is
// gzip containing a 4 byte big endian uncompressed length prefix followed by
// the raw block bytes, matching server/a.java's writeInt(len)+write(blocks)
// through a single GZIPOutputStream
void LevelSend_start(struct Connection* conn, const unsigned char* blocks, int len);

#endif
