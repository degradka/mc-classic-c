// commands.c

#include "commands.h"
#include "server.h"
#include "net/connection.h"
#include "net/packet.h"
#include "level/level.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// issuer NULL (console) has no reply channel beyond the dispatch-line log in
// Commands_dispatch, matching the real source's console-targeted branches
static void reply(Connection* issuer, const char* text) {
    if (issuer) Connection_sendSystemMessage(issuer, text);
}

void Commands_dispatch(MinecraftServer* srv, Connection* issuer, const char* text) {
    // single admin/non-admin gate for the entire command set, no per-command
    // permission granularity in the real source. Console is implicitly
    // always privileged: the real dispatcher never gates its
    // console-originated calls, only in-game chat commands pass through
    // this check
    if (issuer && !PlayerList_contains(&srv->admins, issuer->username)) {
        Connection_sendSystemMessage(issuer, "You're not a server admin!");
        return;
    }

    // server1.6: every dispatched command is logged this way regardless of
    // source, matching the real source's unconditional "<name/console>
    // admins: <command>" log line at the top of the dispatcher
    Log_info("%s admins: %s", issuer ? issuer->username : "[console]", text);

    char buf[65];
    snprintf(buf, sizeof buf, "%s", text);
    char* tokens[8];
    int tokenCount = 0;
    char* tok = strtok(buf, " ");
    while (tok && tokenCount < 8) {
        tokens[tokenCount++] = tok;
        tok = strtok(NULL, " ");
    }
    if (tokenCount == 0) return;

    char cmd[32];
    snprintf(cmd, sizeof cmd, "%s", tokens[0]);
    for (char* p = cmd; *p; p++) *p = (char)tolower((unsigned char)*p);

    // deviation from the real source: a bare command missing its required
    // argument (e.g. "ban" alone) throws an unhandled exception there that
    // disconnects an in-game admin who typed it. User's call: a usage reply
    // instead, matching the same "don't reproduce dead end crash bugs"
    // precedent as the fullscreen bug and the singleplayer chat NPE
    if (strcmp(cmd, "ban") == 0) {
        if (tokenCount < 2) { reply(issuer, "Usage: /ban <name>"); return; }
        Server_banByName(srv, tokens[1]);
    } else if (strcmp(cmd, "kick") == 0) {
        if (tokenCount < 2) { reply(issuer, "Usage: /kick <name>"); return; }
        Server_kickByName(srv, tokens[1]);
    } else if (strcmp(cmd, "banip") == 0) {
        if (tokenCount < 2) { reply(issuer, "Usage: /banip <name>"); return; }
        Server_banIpByName(srv, tokens[1]);
    } else if (strcmp(cmd, "unban") == 0) {
        if (tokenCount < 2) { reply(issuer, "Usage: /unban <name>"); return; }
        Server_unbanByName(srv, tokens[1]);
    } else if (strcmp(cmd, "op") == 0) {
        if (tokenCount < 2) { reply(issuer, "Usage: /op <name>"); return; }
        Server_opByName(srv, tokens[1]);
    } else if (strcmp(cmd, "deop") == 0) {
        if (tokenCount < 2) { reply(issuer, "Usage: /deop <name>"); return; }
        Server_deopByName(srv, tokens[1]);
    } else if (strcmp(cmd, "teleport") == 0 || strcmp(cmd, "tp") == 0) {
        // new in server1.4: teleports the ISSUER to the named player, not
        // the reverse. server1.6: refuses console, which has no position of
        // its own to teleport from. server1.8.2: /tp is a one-word alias
        if (!issuer) { Log_info("Can't teleport from console!"); return; }
        if (tokenCount < 2) { reply(issuer, "Usage: /teleport <name>"); return; }
        Server_teleportToPlayer(srv, issuer, tokens[1]);
    } else if (strcmp(cmd, "solid") == 0) {
        // server1.8.2: toggles this connection's solid mode, no reply for
        // console since it has no connection of its own to toggle
        if (!issuer) return;
        issuer->solidMode = !issuer->solidMode;
        reply(issuer, issuer->solidMode ? "Now placing unbreakable stone" : "Now placing normal stone");
    } else if (strcmp(cmd, "setspawn") == 0) {
        // new in server1.3: sets the world spawn to the issuing admin's own
        // last known position, no argument. server1.6: refuses console,
        // which has no position of its own to spawn at
        if (!issuer) { Log_info("Can't set spawn from console!"); return; }
        int rot = issuer->lastYaw * 320 / 256;
        Level_setSpawnPos(&srv->level, issuer->lastX / 32, issuer->lastY / 32, issuer->lastZ / 32, (float)rot);
    } else if (strcmp(cmd, "broadcast") == 0 || strcmp(cmd, "say") == 0) {
        // rest of line, not further tokenized (unlike the single-arg
        // commands above), so the message can contain spaces
        const char* space = strchr(text, ' ');
        if (!space || !*(space + 1)) {
            reply(issuer, "Usage: /say <message>");
            return;
        }
        const char* message = space + 1;

        unsigned char pkt[1 + 1 + PACKET_STRING_LEN];
        int n = 0;
        pkt[n++] = (unsigned char)PACKET_MESSAGE;
        pkt[n++] = (unsigned char)0xFF; // -1: system message, /say and /broadcast are identical here
        size_t len = strlen(message);
        if (len > PACKET_STRING_LEN) len = PACKET_STRING_LEN;
        for (size_t i = 0; i < (size_t)PACKET_STRING_LEN; i++) pkt[n++] = (unsigned char)(i < len ? message[i] : ' ');
        Server_broadcastAll(srv, pkt, n);
    } else {
        // matches the real source: only a real in-game issuer gets an
        // "Unknown command!" reply. A console typo produces no extra
        // output beyond the dispatch-line log above
        if (issuer) reply(issuer, "Unknown command!");
    }
}
