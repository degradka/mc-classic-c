// commands.h: unified admin command dispatcher, shared by stdin console
// input and in-game chat commands, ported from MinecraftServer's command
// dispatch method (previously in-game chat only, server1.4.1 and earlier)

#ifndef COMMANDS_H
#define COMMANDS_H

struct Connection;
struct MinecraftServer;

// text has no leading "/" (the in-game chat caller strips it; stdin console
// lines never had one to begin with), already trimmed. issuer NULL means
// console: skips the admin permission gate entirely (console is implicitly
// always privileged), and setspawn/teleport (which need a real player
// position) refuse with a console-only message instead of acting
void Commands_dispatch(struct MinecraftServer* srv, struct Connection* issuer, const char* text);

#endif
