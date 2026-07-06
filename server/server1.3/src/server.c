// server.c

#include "server.h"
#include "jfile.h"
#include "jmd5.h"
#include "jrandom.h"
#include "jtime.h"
#include "log.h"
#include "level/level_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* lists */

static void grow(void** items, int count, int* cap, size_t size) {
    if (count < *cap) return;
    *cap = *cap ? *cap * 2 : 16;
    *items = realloc(*items, (size_t)*cap * size);
}

static void removeAt(void* items, int* count, int i, size_t size) {
    char* p = (char*)items;
    memmove(p + (size_t)i * size, p + (size_t)(i + 1) * size, (size_t)(*count - i - 1) * size);
    (*count)--;
}

static void addPlayer(MinecraftServer* s, Player* p) {
    grow((void**)&s->players, s->playerCount, &s->playerCap, sizeof(Player*));
    s->players[s->playerCount++] = p;
}

static void release(MinecraftServer* s, SocketConnection* conn) {
    if (--conn->refs > 0) return;
    Player* p = conn->player;
    Conn_free(conn);
    if (!p) return;
    if (__atomic_load_n(&p->threads, __ATOMIC_SEQ_CST) == 0) {
        Player_free(p);
        return;
    }
    grow((void**)&s->dead, s->deadCount, &s->deadCap, sizeof(Player*));
    s->dead[s->deadCount++] = p;
}

static void freeDead(MinecraftServer* s) {
    for (int i = 0; i < s->deadCount; ++i) {
        if (__atomic_load_n(&s->dead[i]->threads, __ATOMIC_SEQ_CST) != 0) continue;
        Player_free(s->dead[i]);
        removeAt(s->dead, &s->deadCount, i--, sizeof(Player*));
    }
}

/* setup */

// Integer.parseInt, no plus sign
static bool parseInt(const JStr* s, int* out) {
    if (s->n == 0) return false;
    int i = 0;
    bool neg = false;
    if (s->c[0] == '-') {
        neg = true;
        i = 1;
        if (s->n == 1) return false;
    }
    long long v = 0;
    for (; i < s->n; ++i) {
        int c = s->c[i];
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
        if (v > 2147483648LL) return false;
    }
    if (neg) v = -v;
    if (v > 2147483647LL || v < -2147483648LL) return false;
    *out = (int)v;
    return true;
}

static void setInt(Properties* p, const char* key, int v) {
    JStr s;
    s.n = 0;
    jstr_catInt(&s, v);
    Properties_set(p, key, &s);
}

bool Server_init(MinecraftServer* s, char* exception, size_t size) {
    memset(s, 0, sizeof *s);
    PlayerList_init(&s->admins, "Admins", "admins.txt");
    PlayerList_init(&s->banned, "Banned", "banned.txt");
    PlayerList_init(&s->bannedIp, "Banned (IP)", "banned-ip.txt");
    PlayerList_init(&s->playerList, "Players", "players.txt");
    JRandom random;
    JRandom_initRandom(&random);
    s->salt.n = 0;
    jstr_catInt(&s->salt, JRandom_nextLong(&random));
    Properties_init(&s->properties);
    if (!Properties_load(&s->properties, "server.properties")) Log_warning("Failed to load server.properties!");

    Properties* p = &s->properties;
    JStr v;
    Properties_get(p, "server-name", "Minecraft Server", &s->serverName);
    Properties_get(p, "motd", "Welcome to my Minecraft Server!", &s->motd);
    Properties_get(p, "port", "25565", &v);
    bool ok = parseInt(&v, &s->port);
    if (ok) {
        Properties_get(p, "max-players", "16", &v);
        ok = parseInt(&v, &s->maxPlayers);
    }
    if (!ok) {
        Log_warning("server.properties is broken! Delete it or fix it!");
        exit(0);
    }
    Properties_get(p, "public", "true", &v);
    JStr t;
    jstr_set(&t, "true");
    s->isPublic = jstr_eqIgnoreCase(&v, &t);
    // off by default, it needs minecraft.net
    Properties_get(p, "verify-names", "false", &v);
    s->verifyNames = jstr_eqIgnoreCase(&v, &t);
    if (s->maxPlayers < 1) s->maxPlayers = 1;
    if (s->maxPlayers > 32) s->maxPlayers = 32;
    Properties_set(p, "server-name", &s->serverName);
    Properties_set(p, "motd", &s->motd);
    setInt(p, "max-players", s->maxPlayers);
    setInt(p, "port", s->port);
    jstr_set(&v, s->isPublic ? "true" : "false");
    Properties_set(p, "public", &v);
    jstr_set(&v, s->verifyNames ? "true" : "false");
    Properties_set(p, "verify-names", &v);
    if (!Properties_store(p, "server.properties", "Minecraft server properties")) Log_warning("Failed to save server.properties!");

    return NetSocket_listen(s->port, &s->listenSock, exception, size);
}

/* connections */

static void disconnect(MinecraftServer* s, SocketConnection* conn) {
    if (!conn->registered) return;
    Player* p = conn->player;
    PlayerList_remove(&s->playerList, &p->name);
    char who[512];
    Log_info("%s disconnected", Player_toString(p, who, sizeof who));
    conn->registered = false;
    for (int i = 0; i < s->playerCount; ++i) {
        if (s->players[i] != p) continue;
        removeAt(s->players, &s->playerCount, i, sizeof(Player*));
        break;
    }
    if (p->id >= 0) s->slots[p->id] = NULL;
    PacketValue v[1] = { { .i = p->id } };
    Server_broadcast(s, PACKET_DESPAWN, v);
}

void Server_closeLater(MinecraftServer* s, SocketConnection* conn) {
    grow((void**)&s->closing, s->closingCount, &s->closingCap, sizeof(PendingClose));
    s->closing[s->closingCount].conn = conn;
    s->closing[s->closingCount].ticks = 100;
    s->closingCount++;
    conn->refs++;
}

void Server_broadcast(MinecraftServer* s, int id, const PacketValue* values) {
    for (int i = 0; i < s->playerCount; ++i) {
        Jcatch e;
        JTRY(e) {
            Player_send(s->players[i], id, values);
            JEND(e);
        } else {
            Player_onException(s->players[i], &e);
        }
    }
}

void Server_broadcastExcept(MinecraftServer* s, const Player* except, int id, const PacketValue* values) {
    for (int i = 0; i < s->playerCount; ++i) {
        if (s->players[i] == except) continue;
        Jcatch e;
        JTRY(e) {
            Player_send(s->players[i], id, values);
            JEND(e);
        } else {
            Player_onException(s->players[i], &e);
        }
    }
}

static int freeSlot(const MinecraftServer* s) {
    for (int i = 0; i < s->maxPlayers; ++i) if (!s->slots[i]) return i;
    return -1;
}

static void refuse(MinecraftServer* s, SocketConnection* conn, const char* reason, const char* why) {
    JStr text;
    jstr_set(&text, reason);
    PacketValue v[1] = { { .s = &text } };
    Conn_send(conn, PACKET_DISCONNECT, v);
    char a[256];
    Log_info("%s tried to connect, %s", jstr_c(&conn->address, a, sizeof a), why);
    Server_closeLater(s, conn);
}

static void acceptAll(MinecraftServer* s) {
    sock_t sock;
    Jcatch e;
    JTRY(e) {
        while (NetSocket_accept(s->listenSock, &sock)) {
            SocketConnection* conn = Conn_create(sock);
            grow((void**)&s->conns, s->connCount, &s->connCap, sizeof(SocketConnection*));
            s->conns[s->connCount++] = conn;
            conn->refs = 1;
            if (PlayerList_contains(&s->bannedIp, &conn->address)) {
                refuse(s, conn, "You're banned!", "but is banned.");
                continue;
            }
            int same = 0;
            for (int i = 0; i < s->playerCount; ++i) {
                if (jstr_eq(&s->players[i]->conn->address, &conn->address)) ++same;
            }
            if (same >= 3) {
                char why[64];
                snprintf(why, sizeof why, "but is already connected %d times.", same);
                refuse(s, conn, "Too many connection!", why);
                continue;
            }
            int slot = freeSlot(s);
            if (slot < 0) {
                refuse(s, conn, "The server is full!", "but failed because the server was full.");
                continue;
            }
            Player* p = Player_create(s, conn, slot);
            char who[512];
            Log_info("%s connected", Player_toString(p, who, sizeof who));
            conn->registered = true;
            addPlayer(s, p);
            if (p->id >= 0) s->slots[p->id] = p;
        }
        JEND(e);
    } else {
        jthrow(JEX_RUNTIME, "java.lang.RuntimeException: IOException while ticking socketserver");
    }
}

static void readPackets(SocketConnection* conn) {
    if (!conn->hasSocket) jthrowNull();
    ByteBuffer* in = &conn->in;
    int n = NetSocket_read(conn->sock, in->data + in->pos, in->lim - in->pos);
    if (n > 0) in->pos += n;
    int handled = 0;
    while (in->pos > 0 && handled++ != 100) {
        BB_flip(in);
        int id = (int8_t)in->data[0];
        if (id < 0) jthrowIndex(id);
        if (id >= PACKET_COUNT) {
            char text[64];
            snprintf(text, sizeof text, "Bad command: %d", id);
            jthrowIO(text);
        }
        if (in->lim - in->pos >= PACKETS[id].size + 1) {
            in->pos++;
            static PacketValue values[7];
            static bool present[7];
            static JStr strings[7];
            static uint8_t bytes[1024];
            const char* fields = PACKETS[id].fields;
            for (int k = 0; fields[k]; ++k) present[k] = Conn_read(conn, fields[k], &values[k], &strings[k], bytes);
            if (!conn->player) jthrowNull();
            Player_handle(conn->player, id, values, present);
            if (!conn->connected) break;
            BB_compact(in);
            continue;
        }
        BB_compact(in);
        break;
    }
    Conn_flush(conn);
}

static void network(MinecraftServer* s) {
    for (int i = 0; i < s->playerCount; ++i) {
        Jcatch e;
        JTRY(e) {
            Player_tick(s->players[i]);
            JEND(e);
        } else {
            Player_onException(s->players[i], &e);
        }
    }
    acceptAll(s);
    for (int i = 0; i < s->connCount; ++i) {
        SocketConnection* conn = s->conns[i];
        Jcatch e;
        JTRY(e) {
            readPackets(conn);
            JEND(e);
        } else {
            if (conn->registered) Player_onException(conn->player, &e);
        }
        if (conn->connected) continue;
        Conn_close(conn);
        disconnect(s, conn);
        removeAt(s->conns, &s->connCount, i--, sizeof(SocketConnection*));
        release(s, conn);
    }
}

/* ticking */

static void tick(MinecraftServer* s) {
    Level_tick(s->level);
    for (int i = 0; i < s->closingCount; ++i) {
        PendingClose* c = &s->closing[i];
        disconnect(s, c->conn);
        Jcatch e;
        JTRY(e) {
            Jcatch io;
            JTRY(io) {
                Conn_flush(c->conn);
                JEND(io);
            } else {
                if (io.type != JEX_IO) jthrow(io.type, io.text);
            }
            if (c->ticks-- > 0) {
                JEND(e);
                continue;
            }
            Conn_close(c->conn);
            SocketConnection* conn = c->conn;
            removeAt(s->closing, &s->closingCount, i--, sizeof(PendingClose));
            release(s, conn);
            JEND(e);
        } else {
            Conn_close(c->conn);
        }
    }
}

void Server_save(MinecraftServer* s, bool quiet) {
    FILE* f = fopen("server_level.dat", "wb");
    if (!f) {
        if (quiet) return;
        char exception[512];
        jfile_notFound("server_level.dat", exception, sizeof exception);
        Log_severe("Failed to save the level! %s", exception);
        return;
    }
    LevelIO_save(s->level, f);
    fclose(f);
}

void Server_run(MinecraftServer* s) {
    Log_info("Now accepting input on %d", s->port);
    const int64_t tickNanos = 50000000LL, pingNanos = 500000000LL;
    int64_t lastPing = jtime_nanos();
    int64_t lastTick = jtime_nanos();
    int ticks = 0;
    Jcatch e;
    JTRY(e) {
        for (;;) {
            network(s);
            while (jtime_nanos() - lastTick > tickNanos) {
                lastTick += tickNanos;
                tick(s);
                if (ticks % 1200 == 0) {
                    Log_info("Saving level");
                    Log_info("Load: %d/%d", s->playerCount, s->maxPlayers);
                    Server_save(s, false);
                }
                // the heartbeat to minecraft.net every 900 ticks is left out, the list is gone
                ++ticks;
            }
            while (jtime_nanos() - lastPing > pingNanos) {
                lastPing += pingNanos;
                Server_broadcast(s, PACKET_PING, NULL);
            }
            freeDead(s);
            jtime_sleep(5);
        }
    } else {
        Log_severe("Error in main loop, server shutting down!");
        Log_stackTrace(e.text);
    }
}

void Server_tileChanged(void* ctx, int x, int y, int z) {
    MinecraftServer* s = (MinecraftServer*)ctx;
    PacketValue v[4] = { { .i = x }, { .i = y }, { .i = z }, { .i = Level_getTile(s->level, x, y, z) } };
    Server_broadcast(s, PACKET_SET_TILE, v);
}

/* admin commands */

static void announce(MinecraftServer* s, const JStr* name, const char* what) {
    JStr text;
    jstr_copy(&text, name);
    jstr_catc(&text, what);
    PacketValue v[2] = { { .i = -1 }, { .s = &text } };
    Server_broadcast(s, PACKET_MESSAGE, v);
}

void Server_kick(MinecraftServer* s, const JStr* name) {
    bool found = false;
    for (int i = 0; i < s->playerCount; ++i) {
        if (!jstr_eqIgnoreCase(&s->players[i]->name, name)) continue;
        found = true;
        Player_kick(s->players[i], "You were kicked");
    }
    if (found) announce(s, name, " got kicked from the server!");
}

void Server_ban(MinecraftServer* s, const JStr* name) {
    PlayerList_add(&s->banned, name);
    bool found = false;
    for (int i = 0; i < s->playerCount; ++i) {
        if (!jstr_eqIgnoreCase(&s->players[i]->name, name)) continue;
        found = true;
        Player_kick(s->players[i], "You were banned");
    }
    if (found) announce(s, name, " got banned!");
}

void Server_op(MinecraftServer* s, const JStr* name) {
    PlayerList_add(&s->admins, name);
    for (int i = 0; i < s->playerCount; ++i) {
        if (jstr_eqIgnoreCase(&s->players[i]->name, name)) Player_message(s->players[i], "You're now op!");
    }
}

void Server_deop(MinecraftServer* s, const JStr* name) {
    PlayerList_remove(&s->admins, name);
    for (int i = 0; i < s->playerCount; ++i) {
        if (jstr_eqIgnoreCase(&s->players[i]->name, name)) Player_message(s->players[i], "You're no longer op!");
    }
}

void Server_unban(MinecraftServer* s, const JStr* name) {
    PlayerList_remove(&s->banned, name);
}

// by name or ip, with or without the slash
void Server_banIp(MinecraftServer* s, const JStr* name) {
    bool found = false;
    JStr names, slashed;
    names.n = 0;
    jstr_set(&slashed, "/");
    jstr_cat(&slashed, name);
    for (int i = 0; i < s->playerCount; ++i) {
        Player* p = s->players[i];
        if (!jstr_eqIgnoreCase(&p->name, name) && !jstr_eqIgnoreCase(&p->conn->address, name) &&
            !jstr_eqIgnoreCase(&p->conn->address, &slashed)) continue;
        PlayerList_add(&s->bannedIp, &p->conn->address);
        Player_kick(p, "You were banned");
        // the string is compared to "" by reference, so only the first name gets a comma, in front
        if (names.n == 0 && !found) jstr_catc(&names, ", ");
        jstr_cat(&names, &p->name);
        found = true;
    }
    if (found) announce(s, &names, " got ip banned!");
}

Player* Server_findPlayer(MinecraftServer* s, const JStr* name) {
    for (int i = 0; i < s->playerCount; ++i) {
        if (jstr_eqIgnoreCase(&s->players[i]->name, name)) return s->players[i];
    }
    return NULL;
}

void Server_nameKey(const MinecraftServer* s, const JStr* name, JStr* out) {
    JStr text;
    jstr_copy(&text, &s->salt);
    jstr_cat(&text, name);
    static uint8_t bytes[JSTR_MAX * 3];
    int n = jstr_toPlatform(&text, bytes, sizeof bytes);
    // update(bytes, 0, length()) takes as many bytes as the string has chars
    if (n > text.n) n = text.n;
    uint8_t digest[16];
    jmd5(bytes, (size_t)n, digest);
    // BigInteger(1, digest).toString(16) has no leading zeros
    char hex[40];
    int k = 0;
    for (int i = 0; i < 16; ++i) k += snprintf(hex + k, sizeof hex - (size_t)k, "%02x", digest[i]);
    const char* h = hex;
    while (*h == '0' && h[1]) h++;
    jstr_set(out, h);
}
