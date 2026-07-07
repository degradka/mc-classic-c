// net/connection.c

#include "connection.h"
#include "packet.h"
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <pthread.h>
#endif

extern void Minecraft_beginLevelLoading(const char* title);
extern void Minecraft_levelLoadUpdate(const char* status);
extern void Minecraft_levelLoadProgress(int percent);
extern void Minecraft_installNetworkLevel(int w, int h, int d, const unsigned char* blocks);
extern void Minecraft_addChatLine(const char* text);
extern void Minecraft_networkSetBlock(int x, int y, int z, int type);
extern void Minecraft_networkTeleportSelf(float x, float y, float z, float yaw, float pitch);
extern void Minecraft_setLevelSpawnPos(int x, int y, int z, float rot);
extern void Minecraft_openMessageScreen(const char* title, const char* message);
extern void Minecraft_spawnNetworkPlayer(int id, const char* name, int xRaw, int yRaw, int zRaw, float yaw, float pitch);
extern void Minecraft_teleportNetworkPlayer(int id, int xRaw, int yRaw, int zRaw, float yaw, float pitch);
extern void Minecraft_queueNetworkPlayerMoveLook(int id, int dx, int dy, int dz, float yaw, float pitch);
extern void Minecraft_queueNetworkPlayerMove(int id, int dx, int dy, int dz);
extern void Minecraft_queueNetworkPlayerLook(int id, float yaw, float pitch);
extern void Minecraft_despawnNetworkPlayer(int id);

/* wire format helpers */

static void writeByte(NetConnection* c, unsigned char v) {
    if (c->writeLen < NET_WRITE_BUFFER_SIZE) c->writeBuf[c->writeLen++] = v;
}

static void writeU16(NetConnection* c, unsigned short v) {
    writeByte(c, (unsigned char)(v >> 8));
    writeByte(c, (unsigned char)v);
}

static void writeString(NetConnection* c, const char* s) {
    size_t len = strlen(s);
    if (len > PACKET_STRING_LEN) len = PACKET_STRING_LEN;
    for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) {
        writeByte(c, i < len ? (unsigned char)s[i] : (unsigned char)' ');
    }
}

static unsigned short readU16(const unsigned char* p) {
    return (unsigned short)((p[0] << 8) | p[1]);
}

// fixed 64 byte field, space padded on the wire, trimmed like Java's String.trim()
static void readString(const unsigned char* p, char* out, size_t outCapacity) {
    int len = PACKET_STRING_LEN;
    while (len > 0 && p[len - 1] == ' ') len--;
    if ((size_t)len >= outCapacity) len = (int)outCapacity - 1;
    memcpy(out, p, (size_t)len);
    out[len] = '\0';
}

// byte angle to degrees, yaw negated on conversion, matches SpawnPlayer/Teleport decode
static float angleYaw(signed char b)   { return (-(float)b * 360.0f) / 256.0f; }
static float anglePitch(signed char b) { return ((float)b * 360.0f) / 256.0f; }

/* connect / disconnect */

// background thread body: matches net/c.java becoming a Thread subclass in
// c0.0.17a that does the actual (still blocking, same 8 second timeout
// deviation as before) connect off the render thread, publishing the result
// via connectResult once done. Only touches connectingSock/connectHost/
// connectPort until the very last line, so the main thread never sees a
// half written result
static void runConnect(NetConnection* c) {
    sock_t sock;
    int ok = NetSocket_connect(&sock, c->connectHost, c->connectPort);
    if (ok) c->connectingSock = sock;
    c->connectResult = ok ? 1 : 2; // publish last
}

#if defined(_WIN32)
static DWORD WINAPI connectThreadMain(LPVOID arg) { runConnect((NetConnection*)arg); return 0; }
#else
static void* connectThreadMain(void* arg) { runConnect((NetConnection*)arg); return NULL; }
#endif

void NetConnection_beginConnect(NetConnection* c, const char* host, int port, const char* username) {
    memset(c, 0, sizeof *c);
    snprintf(c->connectHost, sizeof c->connectHost, "%s", host);
    c->connectPort = port;
    snprintf(c->connectUsername, sizeof c->connectUsername, "%s", username);
    c->connectResult = 0;

#if defined(_WIN32)
    HANDLE h = CreateThread(NULL, 0, connectThreadMain, c, 0, NULL);
    if (h) CloseHandle(h); // detached: nothing needs to join it
#else
    pthread_t t;
    if (pthread_create(&t, NULL, connectThreadMain, c) == 0) pthread_detach(t);
#endif

    Minecraft_beginLevelLoading("Connecting..");
}

bool NetConnection_pollConnecting(NetConnection* c) {
    if (c->connectResult == 0) return false; // still in flight

    if (c->connectResult == 2) {
        Minecraft_openMessageScreen("Failed to connect",
            "You failed to connect to the server. It's probably down!");
        return true;
    }

    c->sock = c->connectingSock;
    c->connected = true;
    c->levelBufCapacity = 256 * 1024;
    c->levelBuf = (unsigned char*)malloc((size_t)c->levelBufCapacity);

    writeByte(c, (unsigned char)PACKET_LOGIN);
    writeByte(c, 4); // protocol version
    writeString(c, c->connectUsername);
    writeString(c, "--"); // session id placeholder, never actually used by the server
    return true;
}

void NetConnection_close(NetConnection* c) {
    if (!c->connected) return;
    // best effort flush before closing, matches com.mojang.a.a.a()
    if (c->writeLen > 0) NetSocket_write(c->sock, c->writeBuf, c->writeLen);
    NetSocket_close(c->sock);
    c->connected = false;
    free(c->levelBuf);
    c->levelBuf = NULL;
}

static void appendLevelBytes(NetConnection* c, const unsigned char* data, int len) {
    if (c->levelBufLen + len > c->levelBufCapacity) {
        while (c->levelBufLen + len > c->levelBufCapacity) c->levelBufCapacity *= 2;
        c->levelBuf = (unsigned char*)realloc(c->levelBuf, (size_t)c->levelBufCapacity);
    }
    memcpy(c->levelBuf + c->levelBufLen, data, (size_t)len);
    c->levelBufLen += len;
}

static bool gunzipBuffer(const unsigned char* src, int srcLen, unsigned char* dst, int dstLen) {
    z_stream strm;
    memset(&strm, 0, sizeof strm);
    if (inflateInit2(&strm, 15 + 16) != Z_OK) return false; // 15+16 = auto-detect gzip header

    strm.next_in   = (Bytef*)src;
    strm.avail_in  = (uInt)srcLen;
    strm.next_out  = (Bytef*)dst;
    strm.avail_out = (uInt)dstLen;

    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    return ret == Z_STREAM_END;
}

/* dispatch */

// returns bytes consumed, or -1 on a malformed/unknown packet id
static int dispatchOne(NetConnection* c, const unsigned char* p, int available) {
    if (available < 1) return 0;
    int id = p[0];
    if (id < 0 || id >= PACKET_COUNT) return -1;

    int need = PacketPayloadLen[id] + 1;
    if (available < need) return 0; // wait for more data

    const unsigned char* f = p + 1; // fields start after the id byte

    switch (id) {
        case PACKET_LOGIN: { // server's login reply: serverName, motd
            char serverName[65], motd[65];
            readString(f + 1, serverName, sizeof serverName);
            readString(f + 1 + PACKET_STRING_LEN, motd, sizeof motd);
            Minecraft_beginLevelLoading(serverName);
            Minecraft_levelLoadUpdate(motd);
            break;
        }
        case PACKET_PING:
            break; // sent periodically by the server, nothing to do with it
        case PACKET_LEVEL_INIT:
            c->levelBufLen = 0;
            break;
        case PACKET_LEVEL_CHUNK: {
            unsigned short len = readU16(f);
            const unsigned char* chunk = f + 2;
            unsigned char percent = f[2 + PACKET_ARRAY_LEN];
            appendLevelBytes(c, chunk, len);
            Minecraft_levelLoadProgress(percent);
            break;
        }
        case PACKET_LEVEL_FINALIZE: {
            // wire order is width, depth, height -- matches this codebase's
            // own (Notch-inherited) naming where depth is the vertical axis
            // and height is a horizontal one, confirmed against both the
            // client's Level.setData(w, depth, height, ...) and the
            // server's send call sending (width, depth, height) in that order
            int width = readU16(f), depth = readU16(f + 2), height = readU16(f + 4);
            int rawSize = width * height * depth;
            // the gzipped stream isn't just the raw blocks: a DataOutputStream
            // wraps the GZIPOutputStream on the server side and writes a
            // 4 byte big endian length BEFORE the block bytes, both inside
            // the compressed data. Confirmed against the client's own
            // LevelIO.c(), which reads that same int first via readInt().
            unsigned char* raw = (unsigned char*)malloc((size_t)rawSize + 4);
            if (raw && gunzipBuffer(c->levelBuf, c->levelBufLen, raw, rawSize + 4)) {
                Minecraft_installNetworkLevel(width, height, depth, raw + 4);
            }
            free(raw);
            c->levelBufLen = 0;
            break;
        }
        case PACKET_SET_BLOCK_SC: {
            int x = readU16(f), y = readU16(f + 2), z = readU16(f + 4);
            int type = f[6];
            Minecraft_networkSetBlock(x, y, z, type);
            break;
        }
        case PACKET_SPAWN_PLAYER: {
            signed char sid = (signed char)f[0];
            char name[65];
            readString(f + 1, name, sizeof name);
            int sx = (short)readU16(f + 1 + PACKET_STRING_LEN);
            int sy = (short)readU16(f + 1 + PACKET_STRING_LEN + 2);
            int sz = (short)readU16(f + 1 + PACKET_STRING_LEN + 4);
            signed char syaw   = (signed char)f[1 + PACKET_STRING_LEN + 6];
            signed char spitch = (signed char)f[1 + PACKET_STRING_LEN + 7];
            float yaw = angleYaw(syaw), pitch = anglePitch(spitch);
            if (sid >= 0) {
                Minecraft_spawnNetworkPlayer(sid, name, sx, sy, sz, yaw, pitch);
            } else {
                // id < 0: this is the server telling us where we spawn.
                // c0.0.17a also updates the level's own spawn point here, so
                // a later respawn returns to the server's spawn instead of
                // nowhere. The stored spawn yaw and the yaw actually applied
                // to the player use two different formulas, confirmed as a
                // genuine inconsistency in the real source rather than a
                // transcription slip: the stored spawn point uses integer
                // math and a 320 constant with no negation, while the
                // applied facing uses float math and 360 with no negation
                // either, unlike every other yaw conversion in this file
                // (angleYaw negates). Neither matches the other or the
                // shared angleYaw() helper, and both are kept exactly as is
                float appliedYaw = ((float)syaw * 360.0f) / 256.0f;
                int spawnYaw = ((int)syaw * 320) / 256;
                Minecraft_setLevelSpawnPos(sx / 32, sy / 32, sz / 32, (float)spawnYaw);
                Minecraft_networkTeleportSelf(sx / 32.0f, sy / 32.0f, sz / 32.0f, appliedYaw, pitch);
            }
            break;
        }
        case PACKET_TELEPORT: {
            signed char sid = (signed char)f[0];
            int sx = (short)readU16(f + 1), sy = (short)readU16(f + 3), sz = (short)readU16(f + 5);
            signed char syaw = (signed char)f[7], spitch = (signed char)f[8];
            if (sid >= 0) {
                Minecraft_teleportNetworkPlayer(sid, sx, sy, sz, angleYaw(syaw), anglePitch(spitch));
            } else {
                // c0.0.18a_02: id < 0 was previously silently ignored (dead
                // sentinel with no sender). Now the real source uses it to
                // support the new server /teleport command, moving the local
                // player. Yaw here is NOT negated, matching the real source's
                // (b4 * 360) / 256.0F exactly -- unlike angleYaw's negated
                // conversion used for other players, this is a deliberate
                // asymmetry, not a mistake
                float yaw = ((float)syaw * 360.0f) / 256.0f;
                Minecraft_networkTeleportSelf(sx / 32.0f, sy / 32.0f, sz / 32.0f, yaw, anglePitch(spitch));
            }
            break;
        }
        case PACKET_MOVE_LOOK: {
            signed char sid = (signed char)f[0];
            signed char dx = (signed char)f[1], dy = (signed char)f[2], dz = (signed char)f[3];
            signed char syaw = (signed char)f[4], spitch = (signed char)f[5];
            Minecraft_queueNetworkPlayerMoveLook(sid, dx, dy, dz, angleYaw(syaw), anglePitch(spitch));
            break;
        }
        case PACKET_MOVE: {
            signed char sid = (signed char)f[0];
            signed char dx = (signed char)f[1], dy = (signed char)f[2], dz = (signed char)f[3];
            Minecraft_queueNetworkPlayerMove(sid, dx, dy, dz);
            break;
        }
        case PACKET_LOOK: {
            signed char sid = (signed char)f[0];
            signed char syaw = (signed char)f[1], spitch = (signed char)f[2];
            Minecraft_queueNetworkPlayerLook(sid, angleYaw(syaw), anglePitch(spitch));
            break;
        }
        case PACKET_DESPAWN_PLAYER: {
            signed char sid = (signed char)f[0];
            Minecraft_despawnNetworkPlayer(sid);
            break;
        }
        case PACKET_MESSAGE: {
            signed char sid = (signed char)f[0];
            char text[65];
            readString(f + 1, text, sizeof text);
            if (sid < 0) {
                char withPrefix[72];
                snprintf(withPrefix, sizeof withPrefix, "&e%s", text);
                Minecraft_addChatLine(withPrefix);
            } else {
                Minecraft_addChatLine(text);
            }
            break;
        }
        case PACKET_DISCONNECT: {
            char reason[65];
            readString(f, reason, sizeof reason);
            Minecraft_openMessageScreen("Connection lost", reason);
            break;
        }
    }

    return need;
}

void NetConnection_tick(NetConnection* c) {
    if (!c->connected) return;

    int spaceLeft = NET_READ_BUFFER_SIZE - c->readLen;
    if (spaceLeft > 0) {
        int n = NetSocket_read(c->sock, c->readBuf + c->readLen, spaceLeft);
        if (n > 0) c->readLen += n;
        else if (n < 0) {
            Minecraft_openMessageScreen("Disconnected!", "You've lost connection to the server");
            NetConnection_close(c);
            return;
        }
    }

    int consumedTotal = 0;
    for (int packets = 0; packets < 100 && consumedTotal < c->readLen; packets++) {
        int consumed = dispatchOne(c, c->readBuf + consumedTotal, c->readLen - consumedTotal);
        if (consumed < 0) {
            Minecraft_openMessageScreen("Disconnected!", "You've lost connection to the server");
            NetConnection_close(c);
            return;
        }
        if (consumed == 0) break; // incomplete packet, wait for more data
        consumedTotal += consumed;
    }
    if (consumedTotal > 0) {
        memmove(c->readBuf, c->readBuf + consumedTotal, (size_t)(c->readLen - consumedTotal));
        c->readLen -= consumedTotal;
    }

    if (c->writeLen > 0) {
        int n = NetSocket_write(c->sock, c->writeBuf, c->writeLen);
        if (n > 0) {
            memmove(c->writeBuf, c->writeBuf + n, (size_t)(c->writeLen - n));
            c->writeLen -= n;
        } else if (n < 0) {
            Minecraft_openMessageScreen("Disconnected!", "You've lost connection to the server");
            NetConnection_close(c);
        }
    }
}

/* outgoing */

void NetConnection_sendSetBlock(NetConnection* c, int x, int y, int z, int mode, int type) {
    if (!c->connected) return;
    writeByte(c, (unsigned char)PACKET_SET_BLOCK_CS);
    writeU16(c, (unsigned short)x);
    writeU16(c, (unsigned short)y);
    writeU16(c, (unsigned short)z);
    writeByte(c, (unsigned char)mode);
    writeByte(c, (unsigned char)type);
}

void NetConnection_sendMessage(NetConnection* c, const char* text) {
    if (!c->connected) return;
    writeByte(c, (unsigned char)PACKET_MESSAGE);
    writeByte(c, (unsigned char)0xFF); // -1 as an unsigned byte: outgoing chat is always the local player
    writeString(c, text);
}

void NetConnection_sendTeleportSelf(NetConnection* c, float x, float y, float z, float yaw, float pitch) {
    if (!c->connected) return;
    writeByte(c, (unsigned char)PACKET_TELEPORT);
    writeByte(c, (unsigned char)0xFF); // -1
    writeU16(c, (unsigned short)(short)(x * 32.0f));
    writeU16(c, (unsigned short)(short)(y * 32.0f));
    writeU16(c, (unsigned short)(short)(z * 32.0f));
    // outgoing angles are NOT negated, unlike incoming decode (angleYaw above) --
    // confirmed asymmetric in the real source, not a transcription slip
    writeByte(c, (unsigned char)((int)(yaw * 256.0f / 360.0f) & 0xFF));
    writeByte(c, (unsigned char)((int)(pitch * 256.0f / 360.0f) & 0xFF));
}
