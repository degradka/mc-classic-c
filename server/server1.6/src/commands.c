// commands.c

#include "commands.h"
#include "server.h"
#include "net/connection.h"
#include "net/packet.h"
#include "level/level.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

void Commands_handle(Connection* c, const char* text) {
    // single admin/non-admin gate for the entire command set, no
    // per-command permission granularity in the real source
    if (!PlayerList_contains(&c->server->admins, c->username)) {
        Connection_sendSystemMessage(c, "You're not a server admin!");
        return;
    }

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

    // deviation from the real source: a bare command with no argument (e.g.
    // "/ban" alone) throws an unhandled exception there that disconnects
    // the admin who typed it. User's call: a usage reply instead, matching
    // the same "don't reproduce dead end crash bugs" precedent as the
    // fullscreen bug and the singleplayer chat NPE
    if (strcmp(cmd, "/ban") == 0) {
        if (tokenCount < 2) { Connection_sendSystemMessage(c, "Usage: /ban <name>"); return; }
        Server_banByName(c->server, tokens[1]);
    } else if (strcmp(cmd, "/kick") == 0) {
        if (tokenCount < 2) { Connection_sendSystemMessage(c, "Usage: /kick <name>"); return; }
        Server_kickByName(c->server, tokens[1]);
    } else if (strcmp(cmd, "/banip") == 0) {
        if (tokenCount < 2) { Connection_sendSystemMessage(c, "Usage: /banip <name>"); return; }
        Server_banIpByName(c->server, tokens[1]);
    } else if (strcmp(cmd, "/unban") == 0) {
        if (tokenCount < 2) { Connection_sendSystemMessage(c, "Usage: /unban <name>"); return; }
        Server_unbanByName(c->server, tokens[1]);
    } else if (strcmp(cmd, "/op") == 0) {
        if (tokenCount < 2) { Connection_sendSystemMessage(c, "Usage: /op <name>"); return; }
        Server_opByName(c->server, tokens[1]);
    } else if (strcmp(cmd, "/deop") == 0) {
        if (tokenCount < 2) { Connection_sendSystemMessage(c, "Usage: /deop <name>"); return; }
        Server_deopByName(c->server, tokens[1]);
    } else if (strcmp(cmd, "/teleport") == 0) {
        // new in server1.4: teleports the ISSUER to the named player, not
        // the reverse. Reuses the existing Teleport packet, no protocol
        // change. Doesn't touch the issuer's own tracked server position,
        // matching the real source -- it self-corrects on their next Move
        if (tokenCount < 2) { Connection_sendSystemMessage(c, "Usage: /teleport <name>"); return; }
        Server_teleportToPlayer(c->server, c, tokens[1]);
    } else if (strcmp(cmd, "/setspawn") == 0) {
        // new in server1.3: sets the world spawn to the issuing admin's own
        // last known position, no argument. Yaw uses the same * 320 / 256
        // integer conversion as the real source, kept exactly as is rather
        // than normalized to the more usual * 360 / 256 seen elsewhere
        int rot = c->lastYaw * 320 / 256;
        Level_setSpawnPos(&c->server->level, c->lastX / 32, c->lastY / 32, c->lastZ / 32, (float)rot);
    } else if (strcmp(cmd, "/broadcast") == 0 || strcmp(cmd, "/say") == 0) {
        // rest of line, not further tokenized (unlike the single-arg
        // commands above), so the message can contain spaces -- matches
        // the real source taking everything after the command word rather
        // than just the next token
        const char* space = strchr(text, ' ');
        if (!space || !*(space + 1)) {
            Connection_sendSystemMessage(c, "Usage: /say <message>");
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
        Server_broadcastAll(c->server, pkt, n);
    } else {
        Connection_sendSystemMessage(c, "Unknown command!");
    }
}
