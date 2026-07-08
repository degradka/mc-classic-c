// net/connection.c

#include "connection.h"
#include "packet.h"
#include "level_send.h"
#include "../server.h"
#include "../level/level.h"
#include "../level/tile/tile.h"
#include "../log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// implemented in commands.c. Only ever called for text starting with "/",
// matching the real source's own routing (a plain String.startsWith check)
extern void Commands_dispatch(MinecraftServer* srv, Connection* issuer, const char* text);

/* wire format helpers, identical convention to the client's net/connection.c */

static void writeByte(Connection* c, unsigned char v) {
    if (c->writeLen < CONN_WRITE_BUFFER_SIZE) c->writeBuf[c->writeLen++] = v;
}
static void writeU16(Connection* c, unsigned short v) {
    writeByte(c, (unsigned char)(v >> 8));
    writeByte(c, (unsigned char)v);
}
static void writeString(Connection* c, const char* s) {
    size_t len = strlen(s);
    if (len > PACKET_STRING_LEN) len = PACKET_STRING_LEN;
    for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) {
        writeByte(c, i < len ? (unsigned char)s[i] : (unsigned char)' ');
    }
}

static unsigned short readU16(const unsigned char* p) {
    return (unsigned short)((p[0] << 8) | p[1]);
}
static void readString(const unsigned char* p, char* out, size_t outCapacity) {
    int len = PACKET_STRING_LEN;
    while (len > 0 && p[len - 1] == ' ') len--;
    if ((size_t)len >= outCapacity) len = (int)outCapacity - 1;
    memcpy(out, p, (size_t)len);
    out[len] = '\0';
}

// byte angle helpers, matching the client's asymmetric convention exactly:
// incoming decode negates yaw, outgoing encode does not (confirmed real,
// not a transcription slip, see c0.0.16a_02/src/net/connection.c)
static float angleYawIn(signed char b)   { return (-(float)b * 360.0f) / 256.0f; }
static float anglePitchIn(signed char b) { return ((float)b * 360.0f) / 256.0f; }
static signed char angleOut(float degrees) { return (signed char)((int)(degrees * 256.0f / 360.0f) & 0xFF); }

/* lifecycle */

void Connection_init(Connection* c, MinecraftServer* server, sock_t sock) {
    memset(c, 0, sizeof *c);
    c->sock = sock;
    c->open = true;
    c->server = server;
    c->playerId = -1;
    NetSocket_getRemoteAddress(sock, c->remoteAddress, sizeof c->remoteAddress);
    NetSocket_configure(sock);
}

void Connection_close(Connection* c) {
    if (!c->open) return;
    c->open = false;
    NetSocket_close(c->sock);
    free((void*)c->pendingLevelBytes);
    c->pendingLevelBytes = NULL;
}

void Connection_sendDirect(Connection* c, const unsigned char* packetBytes, int len) {
    for (int i = 0; i < len && c->writeLen < CONN_WRITE_BUFFER_SIZE; i++) {
        c->writeBuf[c->writeLen++] = packetBytes[i];
    }
}

void Connection_queueOrSend(Connection* c, const unsigned char* packetBytes, int len) {
    if (c->spawned) {
        Connection_sendDirect(c, packetBytes, len);
        return;
    }
    for (int i = 0; i < len && c->queuedLen < CONN_QUEUE_BUFFER_SIZE; i++) {
        c->queuedBuf[c->queuedLen++] = packetBytes[i];
    }
}

void Connection_kick(Connection* c, const char* reason) {
    if (c->pendingClose) return; // already kicked, don't restart the grace period
    Log_info("Kicking %s: %s", c->username[0] ? c->username : c->remoteAddress, reason);

    writeByte(c, (unsigned char)PACKET_DISCONNECT);
    writeString(c, reason);

    // matches PendingDisconnect: a 100 tick grace period (doubled from
    // server1.2's 40) rather than an immediate close, so this Disconnect
    // packet actually reaches the client before the socket goes away.
    // Connection_tick counts it down
    c->pendingClose = true;
    c->closeGraceTicks = 100;
    c->loggedIn = false; // stop processing any further packets from this connection
}

void Connection_sendSystemMessage(Connection* c, const char* text) {
    unsigned char pkt[1 + 1 + PACKET_STRING_LEN];
    int n = 0;
    pkt[n++] = (unsigned char)PACKET_MESSAGE;
    pkt[n++] = (unsigned char)0xFF; // -1: system/server sender
    size_t len = strlen(text);
    if (len > PACKET_STRING_LEN) len = PACKET_STRING_LEN;
    for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) pkt[n++] = (unsigned char)(i < len ? text[i] : ' ');
    Connection_queueOrSend(c, pkt, n);
}

/* per-packet handling */

static void handleLogin(Connection* c, const unsigned char* f) {
    signed char protocolVersion = (signed char)f[0];
    char username[65];
    readString(f + 1, username, sizeof username);

    Log_info("%s connected", c->remoteAddress);

    // new in server1.4.1: reject control/non-printable characters in the
    // username before it ever reaches another client, matching the real
    // fix for "bad names connecting and crashing all players"
    for (const char* p = username; *p; p++) {
        if ((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7F) {
            Connection_kick(c, "Bad name!");
            return;
        }
    }

    // matches the real ordering: an existing session under the same name is
    // kicked before this connection's own protocol/ban checks even run
    Server_kickExistingSessionByName(c->server, username);
    Log_info("%s logged in as %s", c->remoteAddress, username);

    if (protocolVersion != 6) { // server1.8.2: bumped from 5, wire shapes unchanged
        Connection_kick(c, "Wrong protocol version.");
        return;
    }
    if (PlayerList_contains(&c->server->bannedNames, username)) {
        Connection_kick(c, "You're banned!");
        return;
    }

    snprintf(c->username, sizeof c->username, "%s", username);
    c->loggedIn = true;
    PlayerList_add(&c->server->onlinePlayers, username); // new in server1.3

    // login is bidirectional: this same packet id, reused for the reply
    writeByte(c, (unsigned char)PACKET_LOGIN);
    writeByte(c, 6); // server1.8.2: bumped from 5
    writeString(c, c->server->serverName);
    writeString(c, c->server->motd);
    // server1.8.2: new trailing userType byte, 100 for an admin, 0 otherwise.
    // Gates whether the client's own local Bedrock break guard allows it
    writeByte(c, PlayerList_contains(&c->server->admins, username) ? 100 : 0);

    size_t total = (size_t)c->server->level.width * c->server->level.height * c->server->level.depth;
    LevelSend_start(c, c->server->level.blocks, (int)total);
}

// server1.6: enqueues a SetBlock item instead of validating/applying it
// immediately; Connection_onGameTick's drain does that once per real tick
static void handleSetBlock(Connection* c, const unsigned char* f) {
    if (!c->spawned) return;
    // matches the real source's shared cap check, done at enqueue time
    // ("kick if size() > 400 before adding"), not at drain time
    if (c->actionQueueCount > 400) {
        Connection_kick(c, "Cheat detected: Too much lag");
        return;
    }
    QueuedAction* a = &c->actionQueue[(c->actionQueueHead + c->actionQueueCount) % CONN_ACTION_QUEUE_CAP];
    a->isSetBlock = true;
    a->sbX = readU16(f); a->sbY = readU16(f + 2); a->sbZ = readU16(f + 4);
    a->sbMode = f[6];
    a->sbType = f[7];
    c->actionQueueCount++;
}

// the real per-item SetBlock validation/apply logic, unchanged from before
// this version's queueing rework, now running once per drained queue item
// instead of once per received packet
static void processSetBlockItem(Connection* c, int x, int y, int z, int mode, int type) {
    // server1.6: new click-rate limiter, incremented once per drained
    // SetBlock item (decayed by 2 every tick in Connection_onGameTick). A
    // kick here only ends this one item's processing; the real source
    // still drains every other queued SetBlock item in the same tick
    c->clickCounter++;
    if (c->clickCounter == 100) {
        Connection_kick(c, "Cheat detected: Too much clicking!");
        return;
    }

    // distance check: matches the real source, computed against the
    // connection's last known raw fixed point position (1/32 block units)
    // with the same eye height offset, before either mode is considered
    float dx = (float)x - (float)c->lastX / 32.0f;
    float dy = (float)y - (float)c->lastY / 32.0f - 1.62f;
    float dz = (float)z - (float)c->lastZ / 32.0f;
    if (dx * dx + dy * dy + dz * dz >= 8.0f * 8.0f) {
        Connection_kick(c, "Cheat detected: Distance");
        return;
    }

    // the whitelist check runs before the mode check in the real source, so
    // it also gates breaking, not just placing: a player whose currently
    // selected tile isn't on the list gets kicked even while only breaking.
    // confirmed against the real decompile, not a guess, kept as is
    bool whitelisted = false;
    for (int i = 0; i < PLACEABLE_TILE_COUNT; ++i) {
        if (PLACEABLE_TILE_IDS[i] == type) { whitelisted = true; break; }
    }
    if (!whitelisted) {
        Connection_kick(c, "Cheat detected: Tile type");
        return;
    }

    // new in server1.4.1: guard against out-of-bounds coordinates from a
    // malformed/malicious SetBlock packet, inserted here in the real source
    // between the whitelist check and the actual tile read/write. x bounds
    // against width, y (vertical) against depth, z against height, matching
    // this codebase's established width/height/depth axis convention
    if (x < 0 || y < 0 || z < 0 ||
        x >= c->server->level.width || y >= c->server->level.depth || z >= c->server->level.height) {
        return;
    }

    if (mode == 0) {
        // server1.8.2: server1.6 had no protection at all here, letting any
        // player destroy Bedrock. Now refused unless the requester is an admin
        int tileHere = Level_getTile(&c->server->level, x, y, z);
        if (tileHere != TILE_BEDROCK.id || PlayerList_contains(&c->server->admins, c->username)) {
            level_setTile(&c->server->level, x, y, z, 0);
        }
        return;
    }

    // matches the real placement guard: liquids can't be hand placed
    const Tile* t = (type >= 0 && type < 256) ? gTiles[type] : NULL;
    bool disallowed = (type == TILE_WATER.id || type == TILE_CALM_WATER.id ||
                       type == TILE_LAVA.id  || type == TILE_CALM_LAVA.id);
    if (t && !disallowed) {
        // server1.8.2: /solid toggles this per connection; while active, a
        // request to place Rock silently places Bedrock instead
        int placeType = (c->solidMode && type == TILE_ROCK.id) ? TILE_BEDROCK.id : type;
        level_setTile(&c->server->level, x, y, z, placeType);
    }
    // re-broadcast happens via the Level block-change listener (Server_onBlockChanged),
    // not here directly, matching the real source's decoupled design
}

// server1.6: enqueues a Move/Teleport item instead of processing it
// immediately; Connection_onGameTick's drain does that once per real tick
static void handleTeleportSelf(Connection* c, const unsigned char* f) {
    if (!c->spawned) return;
    if (c->actionQueueCount > 400) {
        Connection_kick(c, "Cheat detected: Too much lag");
        return;
    }
    QueuedAction* a = &c->actionQueue[(c->actionQueueHead + c->actionQueueCount) % CONN_ACTION_QUEUE_CAP];
    a->isSetBlock = false;
    a->mvX = (short)readU16(f + 1); a->mvY = (short)readU16(f + 3); a->mvZ = (short)readU16(f + 5);
    a->mvYaw = (signed char)f[7]; a->mvPitch = (signed char)f[8];
    c->actionQueueCount++;
}

// the throttled movement rebroadcast: every other dequeued update is acted
// on, and the cheapest applicable packet (full resync / move+look / move
// only / look only) is chosen based on what actually changed and how far it
// moved. Hand-traced directly from the real (raw bytecode) per-connection
// tick method, confirmed byte exact, superseding an earlier version of
// this project's port that was reconstructed from research prose because
// JD-Core failed to decompile this method on either side. Returns true if
// the drain loop should continue to the next queued item this tick, false
// if it should stop and defer the rest to next tick, matching the real
// source's own loop-continue flag exactly, including the otherwise
// easy-to-miss detail that a periodic resync always stops the drain even
// when the position itself didn't change
static bool processMoveItem(Connection* c, int x, int y, int z, signed char yawByte, signed char pitchByte) {
    if (!c->hasLastPos) {
        c->lastX = x; c->lastY = y; c->lastZ = z;
        c->lastYaw = yawByte; c->lastPitch = pitchByte;
        c->hasLastPos = true;
    }

    // nothing at all changed: no broadcast, no counter increment, keep draining
    if (x == c->lastX && y == c->lastY && z == c->lastZ &&
        yawByte == c->lastYaw && pitchByte == c->lastPitch) {
        return true;
    }

    bool posUnchanged = (x == c->lastX && y == c->lastY && z == c->lastZ);

    c->moveTickCounter++;
    if (c->moveTickCounter % 2 != 0) {
        // skipped update: no broadcast, but the loop-continue flag is still
        // resolved from the position-only comparison above
        return posUnchanged;
    }

    int dx = x - c->lastX, dy = y - c->lastY, dz = z - c->lastZ;
    bool outOfDeltaRange = (dx < -128 || dx > 127 || dy < -128 || dy > 127 || dz < -128 || dz > 127);
    bool periodicResync = (c->moveTickCounter % 20 <= 1);

    unsigned char pkt[16];
    int n = 0;
    if (outOfDeltaRange || periodicResync) {
        pkt[n++] = (unsigned char)PACKET_TELEPORT;
        pkt[n++] = (unsigned char)c->playerId;
        pkt[n++] = (unsigned char)(x >> 8); pkt[n++] = (unsigned char)x;
        pkt[n++] = (unsigned char)(y >> 8); pkt[n++] = (unsigned char)y;
        pkt[n++] = (unsigned char)(z >> 8); pkt[n++] = (unsigned char)z;
        pkt[n++] = (unsigned char)yawByte;
        pkt[n++] = (unsigned char)pitchByte;
        Server_broadcastExcept(c->server, c, pkt, n);
        c->lastX = x; c->lastY = y; c->lastZ = z;
        c->lastYaw = yawByte; c->lastPitch = pitchByte;
        return false; // a resync always stops the drain, even if position itself didn't change
    }

    if (posUnchanged) {
        pkt[n++] = (unsigned char)PACKET_LOOK;
        pkt[n++] = (unsigned char)c->playerId;
        pkt[n++] = (unsigned char)yawByte;
        pkt[n++] = (unsigned char)pitchByte;
        Server_broadcastExcept(c->server, c, pkt, n);
        c->lastYaw = yawByte; c->lastPitch = pitchByte;
        return true;
    }

    if (yawByte == c->lastYaw && pitchByte == c->lastPitch) {
        pkt[n++] = (unsigned char)PACKET_MOVE;
        pkt[n++] = (unsigned char)c->playerId;
        pkt[n++] = (unsigned char)dx; pkt[n++] = (unsigned char)dy; pkt[n++] = (unsigned char)dz;
        Server_broadcastExcept(c->server, c, pkt, n);
        c->lastX = x; c->lastY = y; c->lastZ = z;
        return false;
    }

    pkt[n++] = (unsigned char)PACKET_MOVE_LOOK;
    pkt[n++] = (unsigned char)c->playerId;
    pkt[n++] = (unsigned char)dx; pkt[n++] = (unsigned char)dy; pkt[n++] = (unsigned char)dz;
    pkt[n++] = (unsigned char)yawByte;
    pkt[n++] = (unsigned char)pitchByte;
    Server_broadcastExcept(c->server, c, pkt, n);
    c->lastX = x; c->lastY = y; c->lastZ = z;
    c->lastYaw = yawByte; c->lastPitch = pitchByte;
    return false;
}

static void handleMessage(Connection* c, const unsigned char* f) {
    if (!c->spawned) return;
    char text[65];
    readString(f + 1, text, sizeof text);

    char* start = text;
    while (*start == ' ') start++;
    int len = (int)strlen(start);
    while (len > 0 && start[len - 1] == ' ') len--;
    start[len] = '\0';
    if (len == 0) return;

    // server1.6: chat throttle, evaluated before any bad-character check or
    // routing, matching the real source's own ordering exactly (trim
    // first, then this). Once the running total exceeds 600 it's set to
    // 760 and this message is dropped entirely, with a system message sent
    // to the sender only (not broadcast, despite reading like a public
    // announcement). While the counter stays elevated, every further chat
    // attempt keeps adding to it and re-triggering the same drop, decaying
    // by 1 per real tick (see Connection_onGameTick) until it clears
    c->chatMuteCounter += (len + 15) * 4;
    if (c->chatMuteCounter > 600) {
        c->chatMuteCounter = 760;
        Connection_sendSystemMessage(c, "Too much chatter! Muted for eight seconds.");
        Log_info("Muting %s for chatting too much", c->username);
        return;
    }

    // server1.6: fixed here, now correctly returns after kicking.
    // server1.4.1's version kicked but fell through to process the line
    // anyway, faithfully replicated there since it was a real (if harmless)
    // upstream bug; no longer needs that treatment now that it's fixed
    for (const char* p = start; *p; p++) {
        if ((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7F) {
            Connection_kick(c, "Cheat detected: Bad chat message!");
            return;
        }
    }

    if (start[0] == '/') {
        // server1.6: leading "/" stripped before reaching the shared
        // dispatcher (was matched with the slash still attached before)
        Commands_dispatch(c->server, c, start + 1);
        return;
    }

    Log_info("%s says: %s", c->username, start);
    char formatted[128];
    snprintf(formatted, sizeof formatted, "%s: %s", c->username, start);

    unsigned char pkt[1 + 1 + PACKET_STRING_LEN];
    int n = 0;
    pkt[n++] = (unsigned char)PACKET_MESSAGE;
    pkt[n++] = (unsigned char)c->playerId;
    size_t flen = strlen(formatted);
    if (flen > PACKET_STRING_LEN) flen = PACKET_STRING_LEN;
    for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) {
        pkt[n++] = (unsigned char)(i < flen ? formatted[i] : ' ');
    }
    Server_broadcastAll(c->server, pkt, n);
}

/* dispatch */

// returns bytes consumed, 0 if incomplete (wait for more data), -1 on a
// malformed/unknown packet id
static int dispatchOne(Connection* c, const unsigned char* p, int available) {
    if (available < 1) return 0;
    int id = p[0];
    if (id < 0 || id >= PACKET_COUNT) return -1;

    int need = PacketPayloadLen[id] + 1;
    if (available < need) return 0;

    const unsigned char* f = p + 1;

    switch (id) {
        case PACKET_LOGIN:
            if (!c->loggedIn) handleLogin(c, f);
            break;
        case PACKET_PING:
            break; // no-op either direction
        case PACKET_SET_BLOCK_CS:
            handleSetBlock(c, f);
            break;
        case PACKET_TELEPORT:
            handleTeleportSelf(c, f);
            break;
        case PACKET_MESSAGE:
            handleMessage(c, f);
            break;
        // LevelInit/LevelChunk/LevelFinalize/SpawnPlayer/MoveAndLook/Move/
        // Look/DespawnPlayer/Disconnect are all server-to-client only in
        // practice; an incoming one is just ignored rather than treated as
        // an error, matching the real source's default/else branch
        default:
            break;
    }

    return need;
}

// drives the chunked level send once the background gzip thread has
// published a result, matching PlayerConnection.flushLevelSend(). Runs a
// bounded slice per call so a huge level doesn't stall the tick loop either
static void driveLevelSend(Connection* c) {
    if (!c->pendingLevelBytes) return;

    if (c->levelSendOffset == 0) {
        writeByte(c, (unsigned char)PACKET_LEVEL_INIT);
    }

    int remaining = c->pendingLevelLen - c->levelSendOffset;
    int chunksThisCall = 0;
    while (remaining > 0 && chunksThisCall < 20) {
        int chunkLen = remaining > PACKET_ARRAY_LEN ? PACKET_ARRAY_LEN : remaining;
        writeByte(c, (unsigned char)PACKET_LEVEL_CHUNK);
        writeU16(c, (unsigned short)chunkLen);
        for (int i = 0; i < PACKET_ARRAY_LEN; i++) {
            writeByte(c, i < chunkLen ? c->pendingLevelBytes[c->levelSendOffset + i] : 0);
        }
        c->levelSendOffset += chunkLen;
        int percent = (c->levelSendOffset * 100) / c->pendingLevelLen;
        writeByte(c, (unsigned char)percent);
        remaining -= chunkLen;
        chunksThisCall++;
    }

    if (remaining > 0) return; // more to send next tick

    free((void*)c->pendingLevelBytes);
    c->pendingLevelBytes = NULL;

    Level* level = &c->server->level;
    writeByte(c, (unsigned char)PACKET_LEVEL_FINALIZE);
    writeU16(c, (unsigned short)level->width);
    writeU16(c, (unsigned short)level->depth);
    writeU16(c, (unsigned short)level->height);

    // self spawn (id -1) at the level's spawn point
    writeByte(c, (unsigned char)PACKET_SPAWN_PLAYER);
    writeByte(c, (unsigned char)0xFF);
    writeString(c, c->username);
    int sx = (level->xSpawn << 5) + 16, sy = (level->ySpawn << 5) + 16, sz = (level->zSpawn << 5) + 16;
    writeU16(c, (unsigned short)sx); writeU16(c, (unsigned short)sy); writeU16(c, (unsigned short)sz);
    writeByte(c, (unsigned char)angleOut(level->rotSpawn));
    writeByte(c, 0);

    c->lastX = sx; c->lastY = sy; c->lastZ = sz;
    c->lastYaw = 0; c->lastPitch = 0;
    c->hasLastPos = true;

    // announce to everyone else with the real slot id, then tell the new
    // client about everyone already online, matching the real join order
    unsigned char spawnPkt[1 + 1 + PACKET_STRING_LEN + 2 + 2 + 2 + 1 + 1];
    int n = 0;
    spawnPkt[n++] = (unsigned char)PACKET_SPAWN_PLAYER;
    spawnPkt[n++] = (unsigned char)c->playerId;
    size_t ulen = strlen(c->username);
    if (ulen > PACKET_STRING_LEN) ulen = PACKET_STRING_LEN;
    for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) spawnPkt[n++] = (unsigned char)(i < ulen ? c->username[i] : ' ');
    spawnPkt[n++] = (unsigned char)(sx >> 8); spawnPkt[n++] = (unsigned char)sx;
    spawnPkt[n++] = (unsigned char)(sy >> 8); spawnPkt[n++] = (unsigned char)sy;
    spawnPkt[n++] = (unsigned char)(sz >> 8); spawnPkt[n++] = (unsigned char)sz;
    spawnPkt[n++] = (unsigned char)angleOut(level->rotSpawn);
    spawnPkt[n++] = 0;
    Server_broadcastExcept(c->server, c, spawnPkt, n);

    char joinMsg[80];
    snprintf(joinMsg, sizeof joinMsg, "%s joined the game", c->username);
    unsigned char joinPkt[1 + 1 + PACKET_STRING_LEN];
    n = 0;
    joinPkt[n++] = (unsigned char)PACKET_MESSAGE;
    joinPkt[n++] = (unsigned char)0xFF;
    size_t jlen = strlen(joinMsg);
    if (jlen > PACKET_STRING_LEN) jlen = PACKET_STRING_LEN;
    for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) joinPkt[n++] = (unsigned char)(i < jlen ? joinMsg[i] : ' ');
    Server_broadcastAll(c->server, joinPkt, n);

    for (int i = 0; i < c->server->maxPlayers; i++) {
        Connection* other = c->server->playerSlots[i];
        if (!other || other == c || !other->spawned) continue;
        unsigned char otherPkt[1 + 1 + PACKET_STRING_LEN + 2 + 2 + 2 + 1 + 1];
        int m = 0;
        otherPkt[m++] = (unsigned char)PACKET_SPAWN_PLAYER;
        otherPkt[m++] = (unsigned char)other->playerId;
        size_t olen = strlen(other->username);
        if (olen > PACKET_STRING_LEN) olen = PACKET_STRING_LEN;
        for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) otherPkt[m++] = (unsigned char)(i < olen ? other->username[i] : ' ');
        otherPkt[m++] = (unsigned char)(other->lastX >> 8); otherPkt[m++] = (unsigned char)other->lastX;
        otherPkt[m++] = (unsigned char)(other->lastY >> 8); otherPkt[m++] = (unsigned char)other->lastY;
        otherPkt[m++] = (unsigned char)(other->lastZ >> 8); otherPkt[m++] = (unsigned char)other->lastZ;
        otherPkt[m++] = (unsigned char)other->lastYaw;
        otherPkt[m++] = (unsigned char)other->lastPitch;
        Connection_sendDirect(c, otherPkt, m);
    }

    c->spawned = true;
    Connection_sendDirect(c, c->queuedBuf, c->queuedLen);
    c->queuedLen = 0;
}

void Connection_tick(Connection* c) {
    if (!c->open) return;

    if (c->pendingClose) {
        // just drain the outgoing buffer (the kick/ban Disconnect packet)
        // and count down, matching PendingDisconnect, no more reading or
        // dispatching packets from a connection that's on its way out
        if (c->writeLen > 0) {
            int n = NetSocket_write(c->sock, c->writeBuf, c->writeLen);
            if (n > 0) {
                memmove(c->writeBuf, c->writeBuf + n, (size_t)(c->writeLen - n));
                c->writeLen -= n;
            }
        }
        if (--c->closeGraceTicks <= 0) Connection_close(c);
        return;
    }

    driveLevelSend(c);

    int spaceLeft = CONN_READ_BUFFER_SIZE - c->readLen;
    if (spaceLeft > 0) {
        int n = NetSocket_read(c->sock, c->readBuf + c->readLen, spaceLeft);
        if (n > 0) {
            c->readLen += n;
        } else if (n < 0) {
            Log_warn("%s lost connection suddenly", c->username[0] ? c->username : c->remoteAddress);
            Connection_close(c);
            return;
        }
    }

    int consumedTotal = 0;
    for (int packets = 0; packets < 100 && consumedTotal < c->readLen; packets++) {
        int consumed = dispatchOne(c, c->readBuf + consumedTotal, c->readLen - consumedTotal);
        if (consumed < 0) {
            Log_warn("%s: bad command, dropping connection", c->username[0] ? c->username : c->remoteAddress);
            Connection_close(c);
            return;
        }
        if (consumed == 0) break;
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
            Connection_close(c);
        }
    }
}

void Connection_onGameTick(Connection* c) {
    if (!c->open || c->pendingClose) return;

    // click counter decay, matches d.a()'s "if (r >= 2) r -= 2"
    if (c->clickCounter >= 2) c->clickCounter -= 2;

    // chat mute counter decay/resolution, matches d.a()'s "if (p > 0) { p--;
    // if (p == 600) {...; p = 300;} }". The reply goes to this connection
    // only, not a public broadcast, same correction as the mute message above
    if (c->chatMuteCounter > 0) {
        c->chatMuteCounter--;
        if (c->chatMuteCounter == 600) {
            Connection_sendSystemMessage(c, "You can now talk again.");
            c->chatMuteCounter = 300;
        }
    }

    // drains the shared SetBlock/Move queue: SetBlock items never stop the
    // drain (all backlog SetBlock items resolve in one tick), Move items
    // stop the drain the moment one represents actual movement (or a
    // periodic resync fires), deferring the rest to the next tick, matching
    // the real source's own loop-continue flag exactly
    bool keepDraining = true;
    while (keepDraining && c->actionQueueCount > 0) {
        QueuedAction item = c->actionQueue[c->actionQueueHead];
        c->actionQueueHead = (c->actionQueueHead + 1) % CONN_ACTION_QUEUE_CAP;
        c->actionQueueCount--;

        if (item.isSetBlock) {
            processSetBlockItem(c, item.sbX, item.sbY, item.sbZ, item.sbMode, item.sbType);
            keepDraining = true;
        } else {
            keepDraining = processMoveItem(c, item.mvX, item.mvY, item.mvZ, item.mvYaw, item.mvPitch);
        }
    }
}
