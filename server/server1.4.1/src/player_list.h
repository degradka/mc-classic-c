// player_list.h: a named, persisted set of usernames (or IPs), backing
// admins.txt/banned.txt/banned-ip.txt. Ported from server/b.java

#ifndef PLAYER_LIST_H
#define PLAYER_LIST_H

#include <stdbool.h>

#define PLAYER_LIST_MAX_ENTRIES 256
#define PLAYER_LIST_ENTRY_LEN   64

typedef struct {
    char path[256];
    // every real call site constructs this case-insensitive (caseSensitive
    // always false), so that's the only mode this port implements
    char entries[PLAYER_LIST_MAX_ENTRIES][PLAYER_LIST_ENTRY_LEN];
    int count;
} PlayerList;

// loads from path, creating an empty file if it doesn't exist yet
void PlayerList_init(PlayerList* list, const char* path);
// every add/remove immediately rewrites the whole file, matching the real
// source's lack of batching. Order is not preserved (the real source backs
// this with a HashSet), not a bug to "fix" if a diff looks reshuffled
void PlayerList_add(PlayerList* list, const char* name);
void PlayerList_remove(PlayerList* list, const char* name);
bool PlayerList_contains(const PlayerList* list, const char* name);

#endif
