// stdin_reader.h: background stdin line reader for console admin commands,
// new in server1.6 (real distribution's server/i.java)

#ifndef STDIN_READER_H
#define STDIN_READER_H

struct MinecraftServer;

// starts the background reader thread. Call once, from Server_init
void StdinReader_start(struct MinecraftServer* srv);

// drains and dispatches every line queued so far (each via
// Commands_dispatch with a NULL issuer). Call once per outer loop
// iteration, matching MinecraftServer.c()'s synchronized drain
void StdinReader_poll(struct MinecraftServer* srv);

#endif
