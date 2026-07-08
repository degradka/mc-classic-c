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
    free(p->actions);
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

static void cheat(Player* p, const char* what) {
    char reason[64];
    snprintf(reason, sizeof reason, "Cheat detected: %s", what);
    Player_kick(p, reason);
}

static void handleLogin(Player* p, const PacketValue* v, const bool* present) {
    if (!present[0] || !present[1]) jthrowNull();
    int protocol = v[0].i;
    static JStr name, key;
    jstr_copy(&name, v[1].s);
    jstr_trim(&name);
    for (int i = 0; i < name.n; ++i) {
        if (name.c[i] >= ' ' && name.c[i] <= 0x7F) continue;
        cheat(p, "Bad name!");
        return;
    }
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
    if (protocol != 6) {
        Player_kick(p, "Wrong protocol version.");
        return;
    }
    if (PlayerList_contains(&p->server->banned, &name)) {
        Player_kick(p, "You're banned!");
        return;
    }
    p->loggedIn = true;
    jstr_copy(&p->name, &name);
    // 100 tells the client it's an op
    PacketValue reply[4] = { { .i = 6 }, { .s = &p->server->serverName }, { .s = &p->server->motd },
                             { .i = PlayerList_contains(&p->server->admins, &name) ? 100 : 0 } };
    sendDirect(p, PACKET_LOGIN, reply);
    startLevelJob(p);
    PlayerList_add(&p->server->playerList, &name);
}

// what a client may place, anything else is a cheat
static const int PLACEABLE[32] = {
    TILE_ROCK, TILE_STONEBRICK, TILE_DIRT, TILE_WOOD, TILE_LOG, TILE_LEAVES, TILE_BUSH, TILE_DANDELION, TILE_ROSE,
    TILE_BROWN_MUSHROOM, TILE_RED_MUSHROOM, TILE_SAND, TILE_GRAVEL, TILE_GLASS, TILE_SPONGE, TILE_GOLD,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36
};

static void broadcastText(MinecraftServer* s, const JStr* text) {
    PacketValue v[2] = { { .i = -1 }, { .s = text } };
    Server_broadcast(s, PACKET_MESSAGE, v);
}

static void queueAction(Player* p, int id, const PacketValue* v, const bool* present) {
    if (p->actionCount > 1200) {
        cheat(p, "Too much lag");
        return;
    }
    if (p->actionHead + p->actionCount == p->actionCap) {
        if (p->actionHead > 0) {
            memmove(p->actions, p->actions + p->actionHead, (size_t)p->actionCount * sizeof(PendingAction));
            p->actionHead = 0;
        } else {
            p->actionCap = p->actionCap ? p->actionCap * 2 : 64;
            p->actions = (PendingAction*)realloc(p->actions, (size_t)p->actionCap * sizeof(PendingAction));
        }
    }
    PendingAction* a = &p->actions[p->actionHead + p->actionCount++];
    a->id = id;
    for (int i = 0; i < 6; ++i) {
        a->values[i] = v[i].i;
        a->present[i] = i < (int)strlen(PACKETS[id].fields) && present[i];
    }
}

static void place(Player* p, const PendingAction* a) {
    for (int i = 0; i < 5; ++i) if (!a->present[i]) jthrowNull();
    int x = a->values[0], y = a->values[1], z = a->values[2], mode = a->values[3], type = a->values[4];
    if (++p->clicks == 100) {
        cheat(p, "Too much clicking!");
        return;
    }
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
    // the tile object is looked up in the list, an id without a tile is never in it
    const Tile* tile = Tile_get(type);
    bool allowed = false;
    for (int i = 0; i < 32 && tile && !allowed; ++i) if (PLACEABLE[i] == tile->id) allowed = true;
    if (!allowed) {
        cheat(p, "Tile type");
        return;
    }
    if (x < 0 || y < 0 || z < 0 || x >= level->width || y >= level->depth || z >= level->height) return;
    if (mode == 0) {
        // only admins break bedrock
        if (Level_getTile(level, x, y, z) != TILE_UNBREAKABLE || PlayerList_contains(&p->server->admins, &p->name)) {
            Level_setTile(level, x, y, z, 0);
        }
        return;
    }
    // only into air or a liquid, anything goes
    const Tile* t = Tile_get(Level_getTile(level, x, y, z));
    if (t == NULL || t->id == TILE_WATER || t->id == TILE_CALM_WATER || t->id == TILE_LAVA || t->id == TILE_CALM_LAVA) {
        Level_setTile(level, x, y, z, p->solid && type == TILE_ROCK ? TILE_UNBREAKABLE : type);
        const Tile* placed = Tile_req(type);
        if (placed->onPlace) placed->onPlace(placed, level, x, y, z);
    }
}

// true when the tick may go on to the next queued action, a move to a new spot ends it
static bool move(Player* p, const PendingAction* a) {
    for (int i = 0; i < 6; ++i) if (!a->present[i]) jthrowNull();
    int x = a->values[1], y = a->values[2], z = a->values[3], yaw = a->values[4], pitch = a->values[5];
    if (x == p->x && y == p->y && z == p->z && yaw == p->yaw && pitch == p->pitch) return true;
    bool samePlace = x == p->x && y == p->y && z == p->z;
    int moves = p->moves;
    p->moves = (int)((unsigned)moves + 1u);
    if (moves % 2 != 0) return samePlace;
    int dx = x - p->x, dy = y - p->y, dz = z - p->z;
    if (dx >= 128 || dx < -128 || dy >= 128 || dy < -128 || dz >= 128 || dz < -128 || p->moves % 20 <= 1) {
        p->x = x;
        p->y = y;
        p->z = z;
        p->yaw = yaw;
        p->pitch = pitch;
        PacketValue o[6] = { { .i = p->id }, { .i = x }, { .i = y }, { .i = z }, { .i = yaw }, { .i = pitch } };
        Server_broadcastExcept(p->server, p, PACKET_TELEPORT, o);
        return false;
    }
    if (samePlace) {
        p->yaw = yaw;
        p->pitch = pitch;
        PacketValue o[3] = { { .i = p->id }, { .i = yaw }, { .i = pitch } };
        Server_broadcastExcept(p->server, p, PACKET_LOOK, o);
    } else if (yaw == p->yaw && pitch == p->pitch) {
        p->x = x;
        p->y = y;
        p->z = z;
        PacketValue o[4] = { { .i = p->id }, { .i = dx }, { .i = dy }, { .i = dz } };
        Server_broadcastExcept(p->server, p, PACKET_MOVE, o);
    } else {
        p->x = x;
        p->y = y;
        p->z = z;
        p->yaw = yaw;
        p->pitch = pitch;
        PacketValue o[6] = { { .i = p->id }, { .i = dx }, { .i = dy }, { .i = dz }, { .i = yaw }, { .i = pitch } };
        Server_broadcastExcept(p->server, p, PACKET_MOVE_LOOK, o);
    }
    return samePlace;
}

static void chat(Player* p, const JStr* text) {
    static JStr message;
    jstr_copy(&message, text);
    jstr_trim(&message);
    p->chatter += (message.n + 15) << 2;
    if (p->chatter > 600) {
        p->chatter = 760;
        Player_message(p, "Too much chatter! Muted for eight seconds.");
        char n[JSTR_MAX * 3];
        Log_info("Muting %s for chatting too much", jstr_c(&p->name, n, sizeof n));
        return;
    }
    for (int i = 0; i < message.n; ++i) {
        if (message.c[i] >= ' ' && message.c[i] <= 0x7F) continue;
        cheat(p, "Bad chat message!");
        return;
    }
    if (jstr_startsWithc(&message, "/")) {
        if (!PlayerList_contains(&p->server->admins, &p->name)) {
            Player_message(p, "You're not a server admin!");
            return;
        }
        static JStr command;
        jstr_substring(&command, &message, 1);
        Server_command(p->server, p, &command);
        return;
    }
    char n[JSTR_MAX * 3], m[JSTR_MAX * 3];
    Log_info("%s says: %s", jstr_c(&p->name, n, sizeof n), jstr_c(&message, m, sizeof m));
    static JStr out;
    jstr_copy(&out, &p->name);
    jstr_catc(&out, ": ");
    jstr_cat(&out, &message);
    PacketValue v[2] = { { .i = p->id }, { .s = &out } };
    Server_broadcast(p->server, PACKET_MESSAGE, v);
}

void Player_handle(Player* p, int id, const PacketValue* values, const bool* present) {
    if (p->kicked) return;
    if (id == PACKET_LOGIN) {
        handleLogin(p, values, present);
        return;
    }
    if (id == PACKET_PING) return;
    if (!p->loggedIn || !p->spawned) return;
    if (id == PACKET_PLACE || id == PACKET_TELEPORT) {
        queueAction(p, id, values, present);
    } else if (id == PACKET_MESSAGE) {
        if (!present[1]) jthrowNull();
        static JStr message;
        jstr_copy(&message, values[1].s);
        jstr_trim(&message);
        if (message.n > 0) chat(p, &message);
    }
}

void Player_kick(Player* p, const char* reason) {
    static JStr text;
    jstr_set(&text, reason);
    PacketValue v[1] = { { .s = &text } };
    sendDirect(p, PACKET_DISCONNECT, v);
    char who[512];
    Log_info("Kicking %s: %s", Player_toString(p, who, sizeof who), reason);
    Server_closeLater(p->server, p->conn);
    p->kicked = true;
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
    if (p->clicks >= 2) p->clicks -= 2;
    if (p->chatter > 0) {
        p->chatter--;
        if (p->chatter == 600) {
            Player_message(p, "You can now talk again.");
            p->chatter = 300;
        }
    }
    bool more = true;
    while (p->actionCount > 0 && more) {
        PendingAction a = p->actions[p->actionHead++];
        p->actionCount--;
        if (!a.present[0]) jthrowNull();
        if (a.id == PACKET_PLACE) {
            place(p, &a);
            more = true;
        } else {
            more = move(p, &a);
        }
    }
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
        Log_stackTrace(e->text);
    }
    JStr left;
    jstr_copy(&left, &p->name);
    jstr_catc(&left, " left the game");
    PacketValue v[2] = { { .i = -1 }, { .s = &left } };
    Server_broadcastExcept(p->server, p, PACKET_MESSAGE, v);
    Conn_close(p->conn);
}
