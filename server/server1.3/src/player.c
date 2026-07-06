// player.c

#include "player.h"
#include "server.h"
#include "log.h"
#include "jgzip.h"
#include "jtime.h"
#include "level/level.h"
#include "level/tile/tile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <pthread.h>
#endif

Player* Player_create(MinecraftServer* server, SocketConnection* conn, int id) {
    Player* p = (Player*)calloc(1, sizeof *p);
    p->server = server;
    p->conn = conn;
    p->id = id;
    p->connectTime = jtime_millis();
    conn->player = p;
    Level* level = server->level;
    p->x = (level->xSpawn << 5) + 16;
    p->y = (level->ySpawn << 5) + 16;
    p->z = (level->zSpawn << 5) + 16;
    p->yaw = (int)(level->rotSpawn * 256.0f / 360.0f);
    p->pitch = 0;
    return p;
}

static void freeQueue(Player* p) {
    for (int i = 0; i < p->queueSize; ++i) {
        for (int k = 0; k < 7; ++k) free(p->queue[i].strings[k]);
    }
    free(p->queue);
    p->queue = NULL;
    p->queueSize = p->queueCap = 0;
}

void Player_free(Player* p) {
    freeQueue(p);
    for (LevelBuffer* b = p->buffers; b;) {
        LevelBuffer* next = b->next;
        free(b->data);
        free(b);
        b = next;
    }
    free(p);
}

const char* Player_toString(const Player* p, char* buf, int size) {
    char a[JSTR_MAX * 3], n[JSTR_MAX * 3];
    jstr_c(&p->conn->address, a, sizeof a);
    if (!p->loggedIn) snprintf(buf, (size_t)size, "%s", a);
    else snprintf(buf, (size_t)size, "%s (%s)", jstr_c(&p->name, n, sizeof n), a);
    return buf;
}

/* the level send thread, it waits, gzips the copy with its length in front and waits again */

typedef struct {
    Player* player;
    int8_t* blocks;
    int len;
} LevelJob;

static void runJob(LevelJob* job) {
    jtime_sleep(500);
    uint8_t* raw = (uint8_t*)malloc((size_t)job->len + 4);
    raw[0] = (uint8_t)(job->len >> 24);
    raw[1] = (uint8_t)(job->len >> 16);
    raw[2] = (uint8_t)(job->len >> 8);
    raw[3] = (uint8_t)job->len;
    memcpy(raw + 4, job->blocks, (size_t)job->len);
    size_t n = 0;
    uint8_t* gz = jgzip_compress(raw, (size_t)job->len + 4, &n);
    free(raw);
    free(job->blocks);
    jtime_sleep(500);
    Player* p = job->player;
    free(job);
    LevelBuffer* b = (LevelBuffer*)malloc(sizeof *b);
    b->data = gz;
    b->len = (int)n;
    b->next = __atomic_load_n(&p->buffers, __ATOMIC_SEQ_CST);
    while (!__atomic_compare_exchange_n(&p->buffers, &b->next, b, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {}
    __atomic_store_n(&p->level, b, __ATOMIC_SEQ_CST);
    __atomic_sub_fetch(&p->threads, 1, __ATOMIC_SEQ_CST);
}

#if defined(_WIN32)
static DWORD WINAPI jobMain(LPVOID arg) { runJob((LevelJob*)arg); return 0; }
#else
static void* jobMain(void* arg) { runJob((LevelJob*)arg); return NULL; }
#endif

static void startLevelJob(Player* p) {
    Level* level = p->server->level;
    LevelJob* job = (LevelJob*)malloc(sizeof *job);
    job->player = p;
    job->blocks = Level_copyBlocks(level);
    job->len = level->width * level->height * level->depth;
    __atomic_add_fetch(&p->threads, 1, __ATOMIC_SEQ_CST);
#if defined(_WIN32)
    HANDLE h = CreateThread(NULL, 0, jobMain, job, 0, NULL);
    if (h) CloseHandle(h);
#else
    pthread_t t;
    if (pthread_create(&t, NULL, jobMain, job) == 0) pthread_detach(t);
#endif
}

/* packets */

static void sendDirect(Player* p, int id, const PacketValue* v) {
    Conn_send(p->conn, id, v);
}

static void handleLogin(Player* p, const PacketValue* v, const bool* present) {
    if (!present[0] || !present[1]) jthrowNull();
    int protocol = v[0].i;
    static JStr name, key;
    jstr_copy(&name, v[1].s);
    jstr_trim(&name);
    if (p->server->verifyNames) {
        if (!present[2]) jthrowNull();
        Server_nameKey(p->server, &name, &key);
        if (!jstr_eq(v[2].s, &key)) {
            Player_kick(p, "Illegal name.");
            return;
        }
    }
    Player* other = Server_findPlayer(p->server, &name);
    if (other) Player_kick(other, "You logged in from another computer.");
    char who[512], n[JSTR_MAX * 3];
    Log_info("%s logged in as %s", Player_toString(p, who, sizeof who), jstr_c(&name, n, sizeof n));
    if (protocol != 4) {
        Player_kick(p, "Wrong protocol version.");
        return;
    }
    if (PlayerList_contains(&p->server->banned, &name)) {
        Player_kick(p, "You're banned!");
        return;
    }
    p->loggedIn = true;
    jstr_copy(&p->name, &name);
    PacketValue reply[3] = { { .i = 4 }, { .s = &p->server->serverName }, { .s = &p->server->motd } };
    sendDirect(p, PACKET_LOGIN, reply);
    startLevelJob(p);
    PlayerList_add(&p->server->playerList, &name);
}

static void cheat(Player* p, const char* what) {
    char reason[64];
    snprintf(reason, sizeof reason, "Cheat detected: %s", what);
    Player_kick(p, reason);
}

// what a client may place, anything else is a cheat
static const int PLACEABLE[9] = { TILE_ROCK, TILE_DIRT, TILE_STONEBRICK, TILE_WOOD, TILE_BUSH, TILE_LOG, TILE_LEAVES, TILE_SAND, TILE_GRAVEL };

static void handlePlace(Player* p, const PacketValue* v, const bool* present) {
    for (int i = 0; i < 5; ++i) if (!present[i]) jthrowNull();
    int x = v[0].i, y = v[1].i, z = v[2].i, mode = v[3].i, type = v[4].i;
    Level* level = p->server->level;
    // from the eyes of the last position the others were sent
    float dx = (float)x - (float)p->x / 32.0f;
    float dy = (float)y - ((float)p->y / 32.0f - 1.62f);
    float dz = (float)z - (float)p->z / 32.0f;
    float dist = dx * dx + dy * dy + dz * dz;
    if (dist >= 8.0f * 8.0f) {
        char d[64];
        jtime_doubleString(sqrt((double)dist), d, sizeof d);
        printf("Distance: %s\n", d);
        fflush(stdout);
        cheat(p, "Distance");
        return;
    }
    bool allowed = false;
    for (int i = 0; i < 9 && !allowed; ++i) if (PLACEABLE[i] == type) allowed = true;
    if (!allowed) {
        cheat(p, "Tile type");
        return;
    }
    if (mode == 0) {
        Level_setTile(level, x, y, z, 0);
        return;
    }
    // only into air or a liquid, anything goes
    const Tile* t = Tile_get(Level_getTile(level, x, y, z));
    if (t == NULL || t->id == TILE_WATER || t->id == TILE_CALM_WATER || t->id == TILE_LAVA || t->id == TILE_CALM_LAVA) {
        Level_setTile(level, x, y, z, type);
        const Tile* placed = Tile_req(type);
        if (placed->onPlace) placed->onPlace(placed, level, x, y, z);
    }
}

static void broadcastText(MinecraftServer* s, const JStr* text) {
    PacketValue v[2] = { { .i = -1 }, { .s = text } };
    Server_broadcast(s, PACKET_MESSAGE, v);
}

static void handleCommand(Player* p, const JStr* message) {
    MinecraftServer* s = p->server;
    if (!PlayerList_contains(&s->admins, &p->name)) {
        Player_message(p, "You're not a server admin!");
        return;
    }
    static JStr parts[128], cmd, rest;
    int count = jstr_splitSpace(message, parts, 128);
    jstr_copy(&cmd, &parts[0]);
    jstr_toLower(&cmd);
    const JStr* arg = count > 1 ? &parts[1] : NULL;
    if (jstr_eqc(&cmd, "/ban")) {
        if (!arg) jthrowIndex(1);
        Server_ban(s, arg);
    } else if (jstr_eqc(&cmd, "/kick")) {
        if (!arg) jthrowIndex(1);
        Server_kick(s, arg);
    } else if (jstr_eqc(&cmd, "/banip")) {
        if (!arg) jthrowIndex(1);
        Server_banIp(s, arg);
    } else if (jstr_eqc(&cmd, "/unban")) {
        if (!arg) jthrowIndex(1);
        Server_unban(s, arg);
    } else if (jstr_eqc(&cmd, "/op")) {
        if (!arg) jthrowIndex(1);
        Server_op(s, arg);
    } else if (jstr_eqc(&cmd, "/deop")) {
        if (!arg) jthrowIndex(1);
        Server_deop(s, arg);
    } else if (jstr_eqc(&cmd, "/setspawn")) {
        Level* level = s->level;
        level->xSpawn = p->x / 32;
        level->ySpawn = p->y / 32;
        level->zSpawn = p->z / 32;
        level->rotSpawn = (float)(p->yaw * 320 / 256);
    } else if (jstr_eqc(&cmd, "/broadcast")) {
        jstr_substring(&rest, message, 11);
        jstr_trim(&rest);
        broadcastText(s, &rest);
    } else if (jstr_eqc(&cmd, "/say")) {
        jstr_substring(&rest, message, 5);
        jstr_trim(&rest);
        broadcastText(s, &rest);
    } else {
        Player_message(p, "Unknown command!");
    }
}

static void handleMessage(Player* p, const PacketValue* v, const bool* present) {
    if (!present[1]) jthrowNull();
    static JStr message;
    jstr_copy(&message, v[1].s);
    jstr_trim(&message);
    if (message.n <= 0) return;
    if (jstr_startsWithc(&message, "/")) {
        handleCommand(p, &message);
        return;
    }
    char n[JSTR_MAX * 3], m[JSTR_MAX * 3];
    Log_info("%s says: %s", jstr_c(&p->name, n, sizeof n), jstr_c(&message, m, sizeof m));
    static JStr text;
    jstr_copy(&text, &p->name);
    jstr_catc(&text, ": ");
    jstr_cat(&text, &message);
    PacketValue out[2] = { { .i = p->id }, { .s = &text } };
    Server_broadcast(p->server, PACKET_MESSAGE, out);
}

static void handleTeleport(Player* p, const PacketValue* v, const bool* present) {
    if (!present[0]) jthrowNull();
    for (int i = 5; i >= 1; --i) if (!present[i]) jthrowNull();
    int pitch = v[5].i, yaw = v[4].i, z = v[3].i, y = v[2].i, x = v[1].i;
    // every other one, and only if something changed
    int moves = p->moves;
    p->moves = (int)((unsigned)moves + 1u);
    if (moves % 2 != 0) return;
    if (x == p->x && y == p->y && z == p->z && yaw == p->yaw && pitch == p->pitch) return;
    int dx = x - p->x, dy = y - p->y, dz = z - p->z;
    if (dx >= 128 || dx < -128 || dy >= 128 || dy < -128 || dz >= 128 || dz < -128 || p->moves % 20 <= 1) {
        p->x = x;
        p->y = y;
        p->z = z;
        p->yaw = yaw;
        p->pitch = pitch;
        PacketValue o[6] = { { .i = p->id }, { .i = x }, { .i = y }, { .i = z }, { .i = yaw }, { .i = pitch } };
        Server_broadcastExcept(p->server, p, PACKET_TELEPORT, o);
        return;
    }
    if (x == p->x && y == p->y && z == p->z) {
        p->yaw = yaw;
        p->pitch = pitch;
        PacketValue o[3] = { { .i = p->id }, { .i = yaw }, { .i = pitch } };
        Server_broadcastExcept(p->server, p, PACKET_LOOK, o);
        return;
    }
    if (yaw == p->yaw && pitch == p->pitch) {
        p->x = x;
        p->y = y;
        p->z = z;
        PacketValue o[4] = { { .i = p->id }, { .i = dx }, { .i = dy }, { .i = dz } };
        Server_broadcastExcept(p->server, p, PACKET_MOVE, o);
        return;
    }
    p->x = x;
    p->y = y;
    p->z = z;
    p->yaw = yaw;
    p->pitch = pitch;
    PacketValue o[6] = { { .i = p->id }, { .i = dx }, { .i = dy }, { .i = dz }, { .i = yaw }, { .i = pitch } };
    Server_broadcastExcept(p->server, p, PACKET_MOVE_LOOK, o);
}

void Player_handle(Player* p, int id, const PacketValue* values, const bool* present) {
    if (id == PACKET_LOGIN) handleLogin(p, values, present);
    else if (id == PACKET_PING) return;
    else if (id == PACKET_PLACE) handlePlace(p, values, present);
    else if (id == PACKET_MESSAGE) handleMessage(p, values, present);
    else if (id == PACKET_TELEPORT && p->spawned) handleTeleport(p, values, present);
}

void Player_kick(Player* p, const char* reason) {
    static JStr text;
    jstr_set(&text, reason);
    PacketValue v[1] = { { .s = &text } };
    sendDirect(p, PACKET_DISCONNECT, v);
    char who[512];
    Log_info("Kicking %s: %s", Player_toString(p, who, sizeof who), reason);
    Server_closeLater(p->server, p->conn);
}

void Player_message(Player* p, const char* text) {
    static JStr s;
    jstr_set(&s, text);
    PacketValue v[2] = { { .i = -1 }, { .s = &s } };
    Player_send(p, PACKET_MESSAGE, v);
}

static void sendSpawn(Player* p, const Player* o) {
    PacketValue v[7] = { { .i = o->id }, { .s = &o->name }, { .i = o->x }, { .i = o->y }, { .i = o->z }, { .i = o->yaw }, { .i = o->pitch } };
    sendDirect(p, PACKET_SPAWN, v);
}

void Player_tick(Player* p) {
    if (!p->loggedIn && jtime_millis() - p->connectTime > 5000) {
        Player_kick(p, "You need to log in!");
        return;
    }
    LevelBuffer* b = __atomic_load_n(&p->level, __ATOMIC_SEQ_CST);
    if (!b) return;
    const uint8_t* data = b->data;
    int len = b->len;
    MinecraftServer* s = p->server;
    Level* level = s->level;
    // one 1024 byte array, the last chunk keeps the old tail
    static uint8_t chunk[1024];
    sendDirect(p, PACKET_LEVEL_INIT, NULL);
    int sent = 0, left = len;
    while (left > 0) {
        int n = left > 1024 ? 1024 : left;
        memcpy(chunk, data + sent, (size_t)n);
        PacketValue v[3] = { { .i = n }, { .bytes = chunk, .len = 1024 }, { .i = (sent + n) * 100 / len } };
        sendDirect(p, PACKET_LEVEL_CHUNK, v);
        left -= n;
        sent += n;
    }
    PacketValue fin[3] = { { .i = level->width }, { .i = level->depth }, { .i = level->height } };
    sendDirect(p, PACKET_LEVEL_FINAL, fin);
    PacketValue self[7] = { { .i = -1 }, { .s = &p->name }, { .i = p->x }, { .i = p->y }, { .i = p->z }, { .i = p->yaw }, { .i = p->pitch } };
    sendDirect(p, PACKET_SPAWN, self);
    PacketValue others[7] = { { .i = p->id }, { .s = &p->name }, { .i = (level->xSpawn << 5) + 16 }, { .i = (level->ySpawn << 5) + 16 },
                              { .i = (level->zSpawn << 5) + 16 }, { .i = (int)(level->rotSpawn * 256.0f / 360.0f) }, { .i = 0 } };
    Server_broadcastExcept(s, p, PACKET_SPAWN, others);
    static JStr joined;
    jstr_copy(&joined, &p->name);
    jstr_catc(&joined, " joined the game");
    broadcastText(s, &joined);
    for (int i = 0; i < s->playerCount; ++i) {
        Player* o = s->players[i];
        if (o == NULL || o == p || !o->loggedIn) continue;
        sendSpawn(p, o);
    }
    p->spawned = true;
    // a second level sent to the same player finds the list already gone
    if (p->queueGone) jthrowNull();
    for (int i = 0; i < p->queueSize; ++i) {
        QueuedPacket* q = &p->queue[i];
        PacketValue v[7];
        for (int k = 0; k < 7; ++k) {
            v[k].i = q->ints[k];
            v[k].s = q->strings[k];
        }
        Player_send(p, q->id, v);
    }
    freeQueue(p);
    p->queueGone = true;
    __atomic_store_n(&p->level, NULL, __ATOMIC_SEQ_CST);
}

void Player_send(Player* p, int id, const PacketValue* values) {
    if (!p->spawned) {
        if (p->queueSize == p->queueCap) {
            p->queueCap = p->queueCap ? p->queueCap * 2 : 16;
            p->queue = (QueuedPacket*)realloc(p->queue, (size_t)p->queueCap * sizeof(QueuedPacket));
        }
        QueuedPacket* q = &p->queue[p->queueSize++];
        memset(q, 0, sizeof *q);
        q->id = id;
        const char* fields = PACKETS[id].fields;
        for (int k = 0; fields[k]; ++k) {
            q->ints[k] = values[k].i;
            if (fields[k] == 'T') {
                q->strings[k] = (JStr*)malloc(sizeof(JStr));
                jstr_copy(q->strings[k], values[k].s);
            }
        }
        return;
    }
    Conn_send(p->conn, id, values);
}

void Player_onException(Player* p, const Jcatch* e) {
    char who[512];
    Player_toString(p, who, sizeof who);
    if (e->type == JEX_IO) {
        Log_info("%s lost connection suddenly. (%s)", who, e->text);
    } else {
        Log_warning("%s:%s", who, e->text);
        Log_warning("Exception handling %s!", who);
    }
    JStr left;
    jstr_copy(&left, &p->name);
    jstr_catc(&left, " left the game");
    PacketValue v[2] = { { .i = -1 }, { .s = &left } };
    Server_broadcastExcept(p->server, p, PACKET_MESSAGE, v);
    Conn_close(p->conn);
}
