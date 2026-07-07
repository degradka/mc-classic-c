// net/connection.h: per-player session state and packet dispatch, ported
// from server/d.java (PlayerConnection)

#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include "net_socket.h"
#include <stdbool.h>

// only ever used as a pointer here, kept opaque to avoid a circular full
// include with server.h (which holds an array of Connection pointers)
struct MinecraftServer;

#define CONN_READ_BUFFER_SIZE   (256 * 1024)
#define CONN_WRITE_BUFFER_SIZE  (256 * 1024)
// packets that arrive for this connection before its own join sequence
// finishes get buffered here instead of interleaving mid-download, matching
// PlayerConnection.queuedPackets. Sized generously; normal traffic never
// gets close to this before a level transfer completes
#define CONN_QUEUE_BUFFER_SIZE  (256 * 1024)

typedef struct Connection {
    sock_t sock;
    bool open; // false once torn down, caller removes it from the server's list next tick
    char remoteAddress[64];

    struct MinecraftServer* server;

    unsigned char readBuf[CONN_READ_BUFFER_SIZE];
    int readLen;
    unsigned char writeBuf[CONN_WRITE_BUFFER_SIZE];
    int writeLen;

    bool loggedIn;
    bool spawned; // true once the join sequence (level send + spawn burst) fully completes
    char username[65];
    int playerId; // slot index once assigned, -1 until then

    // matches PendingDisconnect: a kick/ban writes its Disconnect packet then
    // waits this many more ticks before actually closing, so the message has
    // time to flush over the wire instead of closing out from under it
    bool pendingClose;
    int closeGraceTicks;

    // last position/rotation actually broadcast to other clients, 1/32 fixed
    // point and raw byte-angle units (not yet converted to degrees), used to
    // decide the cheapest applicable outgoing packet on the next update
    int lastX, lastY, lastZ;
    int lastYaw, lastPitch;
    int moveTickCounter;
    bool hasLastPos; // false until the first update, so it always sends a full resync first

    // outgoing packets for events that happen elsewhere while this
    // connection hasn't finished joining yet
    unsigned char queuedBuf[CONN_QUEUE_BUFFER_SIZE];
    int queuedLen;

    // background level gzip compression, handed off from a real OS thread
    // (see level_send.h) once finished; consumed by the chunked send driver.
    // The thread is detached (nothing needs to join it), so there's no
    // handle to keep here, just the result it eventually publishes
    volatile unsigned char* pendingLevelBytes;
    volatile int pendingLevelLen;
    int levelSendOffset; // how much of pendingLevelBytes has been chunked out so far
} Connection;

void Connection_init(Connection* c, struct MinecraftServer* server, sock_t sock);
// non-blocking read, dispatch up to 100 buffered packets, flush pending
// writes. Matches the per-connection body of MinecraftServer.tickNetwork()
void Connection_tick(Connection* c);
void Connection_close(Connection* c);

// sends immediately if the join sequence is done, otherwise buffers into
// queuedBuf, matching PlayerConnection.queueOrSend
void Connection_queueOrSend(Connection* c, const unsigned char* packetBytes, int len);
// always sends immediately regardless of join state, used only for this
// connection's own join sequence packets (login ack, level transfer, self spawn)
void Connection_sendDirect(Connection* c, const unsigned char* packetBytes, int len);

void Connection_kick(Connection* c, const char* reason);
void Connection_sendSystemMessage(Connection* c, const char* text);

#endif
