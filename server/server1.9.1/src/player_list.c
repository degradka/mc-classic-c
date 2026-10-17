// player_list.c

#include "player_list.h"
#include "jfile.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// HashMap.hash
static int32_t spread(int32_t h) {
    uint32_t u = (uint32_t)h;
    u ^= (u >> 20) ^ (u >> 12);
    return (int32_t)(u ^ (u >> 7) ^ (u >> 4));
}

static PlayerListEntry* find(const PlayerList* list, const JStr* key, int32_t hash) {
    for (PlayerListEntry* e = list->table[hash & (list->length - 1)]; e; e = e->next) {
        if (e->hash == hash && jstr_eq(&e->key, key)) return e;
    }
    return NULL;
}

static void resize(PlayerList* list, int length) {
    PlayerListEntry** table = (PlayerListEntry**)calloc((size_t)length, sizeof *table);
    for (int j = 0; j < list->length; ++j) {
        PlayerListEntry* e = list->table[j];
        while (e) {
            PlayerListEntry* next = e->next;
            int i = e->hash & (length - 1);
            e->next = table[i];
            table[i] = e;
            e = next;
        }
    }
    free(list->table);
    list->table = table;
    list->length = length;
    list->threshold = (int)(length * 0.75f);
}

static void put(PlayerList* list, const JStr* key) {
    int32_t hash = spread(jstr_hash(key));
    if (find(list, key, hash)) return;
    PlayerListEntry* e = (PlayerListEntry*)malloc(sizeof *e);
    int i = hash & (list->length - 1);
    e->hash = hash;
    jstr_copy(&e->key, key);
    e->next = list->table[i];
    list->table[i] = e;
    if (list->size++ >= list->threshold) resize(list, list->length * 2);
}

static void save(const PlayerList* list) {
    FILE* f = fopen(list->path, "wb");
    if (!f) {
        char exception[512];
        jfile_notFound(list->path, exception, sizeof exception);
        Log_warning("Failed to save player list \"%s\". (%s)", list->name, exception);
        return;
    }
    static uint8_t bytes[JSTR_MAX * 3];
    for (int i = 0; i < list->length; ++i) {
        for (PlayerListEntry* e = list->table[i]; e; e = e->next) {
            int n = jstr_toPlatform(&e->key, bytes, sizeof bytes);
            fwrite(bytes, 1, (size_t)n, f);
            fputs(JFILE_NEWLINE, f);
        }
    }
    fclose(f);
}

void PlayerList_init(PlayerList* list, const char* name, const char* path) {
    snprintf(list->name, sizeof list->name, "%s", name);
    snprintf(list->path, sizeof list->path, "%s", path);
    list->length = 16;
    list->size = 0;
    list->threshold = 12;
    list->table = (PlayerListEntry**)calloc(16, sizeof *list->table);

    char exception[512];
    int len = 0;
    uint8_t* data = jfile_read(path, &len, exception, sizeof exception);
    if (!data) {
        FILE* f = fopen(path, "ab");
        if (f) fclose(f);
        Log_warning("Failed to load player list \"%s\". (%s)", name, exception);
        return;
    }
    // BufferedReader.readLine: \n, \r or \r\n ends a line, an empty last one isn't a line
    static JStr line;
    int start = 0;
    for (int i = 0; i <= len; ++i) {
        if (i < len && data[i] != '\n' && data[i] != '\r') continue;
        if (i == len && start == len) break;
        jstr_fromPlatform(&line, data + start, i - start);
        jstr_toLower(&line);
        put(list, &line);
        if (i < len && data[i] == '\r' && i + 1 < len && data[i + 1] == '\n') i++;
        start = i + 1;
    }
    free(data);
}

void PlayerList_add(PlayerList* list, const JStr* name) {
    static JStr key;
    jstr_copy(&key, name);
    jstr_toLower(&key);
    put(list, &key);
    save(list);
}

void PlayerList_remove(PlayerList* list, const JStr* name) {
    static JStr key;
    jstr_copy(&key, name);
    jstr_toLower(&key);
    int32_t hash = spread(jstr_hash(&key));
    PlayerListEntry** link = &list->table[hash & (list->length - 1)];
    while (*link) {
        PlayerListEntry* e = *link;
        if (e->hash == hash && jstr_eq(&e->key, &key)) {
            *link = e->next;
            free(e);
            list->size--;
            break;
        }
        link = &e->next;
    }
    save(list);
}

bool PlayerList_contains(const PlayerList* list, const JStr* name) {
    static JStr key;
    jstr_copy(&key, name);
    jstr_toLower(&key);
    return find(list, &key, spread(jstr_hash(&key))) != NULL;
}
