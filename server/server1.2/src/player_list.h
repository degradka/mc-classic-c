// player_list.h: admins.txt, banned.txt and banned-ip.txt, a HashSet of lowercase names
// saved in HashSet order

#ifndef PLAYER_LIST_H
#define PLAYER_LIST_H

#include "jstring.h"
#include <stdbool.h>

typedef struct PlayerListEntry {
    int32_t hash;
    JStr key;
    struct PlayerListEntry* next;
} PlayerListEntry;

typedef struct {
    char name[32];
    char path[64];
    PlayerListEntry** table;
    int length;
    int size;
    int threshold;
} PlayerList;

void PlayerList_init(PlayerList* list, const char* name, const char* path);
void PlayerList_add(PlayerList* list, const JStr* name);
void PlayerList_remove(PlayerList* list, const JStr* name);
bool PlayerList_contains(const PlayerList* list, const JStr* name);

#endif
