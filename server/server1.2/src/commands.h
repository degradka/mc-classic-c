// commands.h: chat command parsing, ported from the "/" branch of
// server/d.java's Message packet handler

#ifndef COMMANDS_H
#define COMMANDS_H

struct Connection;

// text always starts with "/", already trimmed. Handles the admin gate,
// tokenizing, and dispatch to the matching MinecraftServer method
void Commands_handle(struct Connection* c, const char* text);

#endif
