// net/connection.c

#include "connection.h"
#include "packet.h"
#include "net_socket.h"
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern void Minecraft_beginLevelLoading(const char* title);
extern void Minecraft_levelLoadUpdate(const char* status);
extern void Minecraft_levelLoadProgress(int percent);
extern void Minecraft_clearLevel(void);
extern void Minecraft_installNetworkLevel(int w, int d, int h, unsigned char* blocks);
extern void Minecraft_addChatLine(const char* text);
extern void Minecraft_networkSetBlock(int x, int y, int z, int type);
extern void Minecraft_showErrorScreen(const char* title, const char* message);
extern bool Minecraft_networkSelfSpawn(int x, int y, int z, int yaw, int pitch);
extern void Minecraft_networkSelfTeleport(int x, int y, int z, int yaw, int pitch);
extern bool Minecraft_spawnNetworkPlayer(int id, const char* name, int x, int y, int z, float yRot, float xRot);
extern void Minecraft_teleportNetworkPlayer(int id, int x, int y, int z, float yRot, float xRot);
extern void Minecraft_queueNetworkPlayerMoveLook(int id, int dx, int dy, int dz, float yRot, float xRot);
extern void Minecraft_queueNetworkPlayerMove(int id, int dx, int dy, int dz);
extern void Minecraft_queueNetworkPlayerLook(int id, float yRot, float xRot);
extern bool Minecraft_despawnNetworkPlayer(int id);

static void writeByte(NetConnection* c, unsigned char v) {
    if (c->writeLen < NET_BUFFER_SIZE) c->writeBuf[c->writeLen++] = v;
}

static void writeShort(NetConnection* c, int v) {
    writeByte(c, (unsigned char)((unsigned short)v >> 8));
    writeByte(c, (unsigned char)v);
}

// utf-8 bytes, cut at 64 and padded with spaces
static void writeString(NetConnection* c, const char* s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) {
        writeByte(c, i < len ? (unsigned char)s[i] : (unsigned char)' ');
    }
}

static int readShort(const unsigned char* p) {
    return (short)((p[0] << 8) | p[1]);
}

// String.trim, it cuts everything up to the space on both ends
static void readString(const unsigned char* p, char* out) {
    int start = 0, end = PACKET_STRING_LEN;
    while (start < end && p[start] <= ' ') start++;
    while (end > start && p[end - 1] <= ' ') end--;
    memcpy(out, p + start, (size_t)(end - start));
    out[end - start] = '\0';
}

static float angle(signed char b) {
    return (float)(b * 360) / 256.0f;
}

void NetConnection_open(NetConnection* c, const char* host, int port, const char* username, const char* mppass) {
    memset(c, 0, sizeof *c);
    snprintf(c->username, sizeof c->username, "%s", username);
    snprintf(c->mppass, sizeof c->mppass, "%s", mppass);
    snprintf(c->host, sizeof c->host, "%s", host);
    c->port = port;
    c->pending = true;
}

int NetConnection_finishConnect(NetConnection* c) {
    if (!c->pending) return 0;
    c->pending = false;
    sock_t sock;
    int r = NetSocket_connect(&sock, c->host, c->port);
    if (r <= 0) return r;
    c->sock = (uintptr_t)sock;
    c->hasSocket = true;
    c->connected = true;
    c->levelBufCapacity = 256 * 1024;
    c->levelBuf = (unsigned char*)malloc((size_t)c->levelBufCapacity);

    writeByte(c, (unsigned char)PACKET_LOGIN);
    writeByte(c, 5);
    writeString(c, c->username);
    writeString(c, c->mppass);
    c->loggedIn = true;
    return 1;
}

bool NetConnection_isOpen(const NetConnection* c) {
    return c->hasSocket && c->connected;
}

void NetConnection_close(NetConnection* c) {
    if (!c->connected) return;
    if (c->writeLen > 0) NetSocket_write((sock_t)c->sock, c->writeBuf, c->writeLen);
    NetSocket_close((sock_t)c->sock);
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

// LevelIO.c: a gzipped int length and then the blocks
static unsigned char* gunzipLevel(const unsigned char* src, int srcLen, int* outLen) {
    z_stream strm;
    memset(&strm, 0, sizeof strm);
    if (inflateInit2(&strm, 15 + 16) != Z_OK) return NULL;
    unsigned char header[4];
    strm.next_in = (Bytef*)src;
    strm.avail_in = (uInt)srcLen;
    strm.next_out = header;
    strm.avail_out = 4;
    int ret = inflate(&strm, Z_SYNC_FLUSH);
    if (strm.avail_out != 0 || (ret != Z_OK && ret != Z_STREAM_END)) { inflateEnd(&strm); return NULL; }
    int len = (int)(((unsigned)header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3]);
    if (len < 0) { inflateEnd(&strm); return NULL; }
    unsigned char* blocks = (unsigned char*)malloc((size_t)(len > 0 ? len : 1));
    strm.next_out = blocks;
    strm.avail_out = (uInt)len;
    while (strm.avail_out > 0) {
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_END) break;
        if (ret != Z_OK) { free(blocks); inflateEnd(&strm); return NULL; }
    }
    inflateEnd(&strm);
    if (strm.avail_out != 0) { free(blocks); return NULL; }
    *outLen = len;
    return blocks;
}

// false when it throws
static bool handle(NetConnection* c, int id, const unsigned char* f) {
    switch (id) {
        case PACKET_LOGIN: {
            char name[65], motd[65];
            readString(f + 1, name);
            readString(f + 1 + PACKET_STRING_LEN, motd);
            Minecraft_beginLevelLoading(name);
            Minecraft_levelLoadUpdate(motd);
            break;
        }
        case PACKET_PING:
            break;
        case PACKET_LEVEL_INIT:
            Minecraft_clearLevel();
            c->levelBufLen = 0;
            break;
        case PACKET_LEVEL_CHUNK: {
            int len = readShort(f);
            Minecraft_levelLoadProgress((signed char)f[2 + PACKET_ARRAY_LEN]);
            if (len < 0 || len > PACKET_ARRAY_LEN) return false;
            appendLevelBytes(c, f + 2, len);
            break;
        }
        case PACKET_LEVEL_FINALIZE: {
            int len = 0;
            unsigned char* blocks = gunzipLevel(c->levelBuf, c->levelBufLen, &len);
            c->levelBufLen = 0;
            if (!blocks) return false;
            int w = readShort(f), d = readShort(f + 2), h = readShort(f + 4);
            if (w <= 0 || d <= 0 || h <= 0 || len < w * d * h) { free(blocks); return false; }
            Minecraft_installNetworkLevel(w, d, h, blocks);
            break;
        }
        case PACKET_SET_BLOCK_SC:
            Minecraft_networkSetBlock(readShort(f), readShort(f + 2), readShort(f + 4), (signed char)f[6]);
            break;
        case PACKET_SPAWN_PLAYER: {
            signed char pid = (signed char)f[0];
            char name[65];
            readString(f + 1, name);
            const unsigned char* g = f + 1 + PACKET_STRING_LEN;
            int x = readShort(g), y = readShort(g + 2), z = readShort(g + 4);
            float yRot = -angle((signed char)g[6]);
            float xRot = angle((signed char)g[7]);
            if (pid >= 0) {
                if (!Minecraft_spawnNetworkPlayer(pid, name, x, y, z, yRot, xRot)) return false;
            } else {
                if (!Minecraft_networkSelfSpawn(x, y, z, (signed char)g[6], (signed char)g[7])) return false;
            }
            break;
        }
        case PACKET_TELEPORT: {
            signed char pid = (signed char)f[0];
            if (pid < 0) {
                Minecraft_networkSelfTeleport(readShort(f + 1), readShort(f + 3), readShort(f + 5), (signed char)f[7], (signed char)f[8]);
            } else {
                Minecraft_teleportNetworkPlayer(pid, readShort(f + 1), readShort(f + 3), readShort(f + 5),
                                                -angle((signed char)f[7]), angle((signed char)f[8]));
            }
            break;
        }
        case PACKET_MOVE_LOOK: {
            signed char pid = (signed char)f[0];
            if (pid >= 0) {
                Minecraft_queueNetworkPlayerMoveLook(pid, (signed char)f[1], (signed char)f[2], (signed char)f[3],
                                                     -angle((signed char)f[4]), angle((signed char)f[5]));
            }
            break;
        }
        case PACKET_MOVE: {
            signed char pid = (signed char)f[0];
            if (pid >= 0) Minecraft_queueNetworkPlayerMove(pid, (signed char)f[1], (signed char)f[2], (signed char)f[3]);
            break;
        }
        case PACKET_LOOK: {
            signed char pid = (signed char)f[0];
            if (pid >= 0) Minecraft_queueNetworkPlayerLook(pid, -angle((signed char)f[1]), angle((signed char)f[2]));
            break;
        }
        case PACKET_DESPAWN_PLAYER: {
            signed char pid = (signed char)f[0];
            if (pid >= 0 && !Minecraft_despawnNetworkPlayer(pid)) return false;
            break;
        }
        case PACKET_MESSAGE: {
            signed char pid = (signed char)f[0];
            char text[65];
            readString(f + 1, text);
            if (pid < 0) {
                char line[72];
                snprintf(line, sizeof line, "&e%s", text);
                Minecraft_addChatLine(line);
            } else {
                Minecraft_addChatLine(text);
            }
            break;
        }
        case PACKET_DISCONNECT: {
            char reason[65];
            readString(f, reason);
            Minecraft_showErrorScreen("Connection lost", reason);
            break;
        }
    }
    return true;
}

NetResult NetConnection_tick(NetConnection* c) {
    if (!c->connected) return NET_OK;

    int space = NET_BUFFER_SIZE - c->readLen;
    if (space > 0) {
        int n = NetSocket_read((sock_t)c->sock, c->readBuf + c->readLen, space);
        if (n > 0) c->readLen += n;
        else if (n < 0) return NET_THROW;
    }

    int pos = 0;
    int handled = 0;
    while (pos < c->readLen && handled++ != 100) {
        int id = c->readBuf[pos];
        // the packet table is indexed with a signed byte, anything unknown throws
        if (id >= PACKET_COUNT) return NET_THROW;
        int available = c->readLen - pos;
        if (available < PacketFullLen[id] + 1) break;
        if (!handle(c, id, c->readBuf + pos + 1)) return NET_THROW;
        pos += PacketFullLen[id] + 1;
        if (!c->connected) break;
    }
    if (pos > 0) {
        memmove(c->readBuf, c->readBuf + pos, (size_t)(c->readLen - pos));
        c->readLen -= pos;
    }

    if (c->connected && c->writeLen > 0) {
        int n = NetSocket_write((sock_t)c->sock, c->writeBuf, c->writeLen);
        if (n < 0) return NET_THROW;
        if (n > 0) {
            memmove(c->writeBuf, c->writeBuf + n, (size_t)(c->writeLen - n));
            c->writeLen -= n;
        }
    }
    return NET_OK;
}

void NetConnection_sendSetBlock(NetConnection* c, int x, int y, int z, int mode, int type) {
    if (!c->connected) return;
    writeByte(c, (unsigned char)PACKET_SET_BLOCK_CS);
    writeShort(c, x);
    writeShort(c, y);
    writeShort(c, z);
    writeByte(c, (unsigned char)mode);
    writeByte(c, (unsigned char)type);
}

void NetConnection_sendMessage(NetConnection* c, const char* text) {
    if (!c->connected) return;
    writeByte(c, (unsigned char)PACKET_MESSAGE);
    writeByte(c, (unsigned char)0xFF);
    writeString(c, text);
}

void NetConnection_sendPosition(NetConnection* c, float x, float y, float z, float yRot, float xRot) {
    if (!c->connected) return;
    writeByte(c, (unsigned char)PACKET_TELEPORT);
    writeByte(c, (unsigned char)0xFF);
    writeShort(c, (int)(x * 32.0f));
    writeShort(c, (int)(y * 32.0f));
    writeShort(c, (int)(z * 32.0f));
    writeByte(c, (unsigned char)((int)(yRot * 256.0f / 360.0f) & 0xFF));
    writeByte(c, (unsigned char)((int)(xRot * 256.0f / 360.0f) & 0xFF));
}
