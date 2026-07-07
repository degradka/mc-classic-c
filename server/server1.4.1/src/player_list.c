// player_list.c

#include "player_list.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void toLower(char* out, const char* s, size_t outSize) {
    size_t i = 0;
    for (; s[i] && i < outSize - 1; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[i] = '\0';
}

static void save(const PlayerList* list) {
    FILE* f = fopen(list->path, "w");
    if (!f) return;
    for (int i = 0; i < list->count; i++) {
        fprintf(f, "%s\n", list->entries[i]);
    }
    fclose(f);
}

void PlayerList_init(PlayerList* list, const char* path) {
    snprintf(list->path, sizeof list->path, "%s", path);
    list->count = 0;

    FILE* f = fopen(path, "r");
    if (!f) {
        // matches the real source: create an empty file if missing
        f = fopen(path, "w");
        if (f) fclose(f);
        return;
    }

    char line[PLAYER_LIST_ENTRY_LEN];
    while (fgets(line, sizeof line, f) && list->count < PLAYER_LIST_MAX_ENTRIES) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        toLower(list->entries[list->count], line, PLAYER_LIST_ENTRY_LEN);
        list->count++;
    }
    fclose(f);
}

bool PlayerList_contains(const PlayerList* list, const char* name) {
    char lower[PLAYER_LIST_ENTRY_LEN];
    toLower(lower, name, sizeof lower);
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i], lower) == 0) return true;
    }
    return false;
}

void PlayerList_add(PlayerList* list, const char* name) {
    if (PlayerList_contains(list, name) || list->count >= PLAYER_LIST_MAX_ENTRIES) return;
    toLower(list->entries[list->count], name, PLAYER_LIST_ENTRY_LEN);
    list->count++;
    save(list);
}

void PlayerList_remove(PlayerList* list, const char* name) {
    char lower[PLAYER_LIST_ENTRY_LEN];
    toLower(lower, name, sizeof lower);
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i], lower) == 0) {
            memcpy(list->entries[i], list->entries[list->count - 1], PLAYER_LIST_ENTRY_LEN);
            list->count--;
            save(list);
            return;
        }
    }
}
