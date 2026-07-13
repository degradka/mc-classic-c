// java_hashmap.c

#include "java_hashmap.h"
#include <stdlib.h>

static int hashOf(int h) {
    unsigned int u = (unsigned int)h;
    u ^= (u >> 20) ^ (u >> 12);
    return (int)(u ^ (u >> 7) ^ (u >> 4));
}

void JHashMap_init(JHashMap* m) {
    m->capacity = 16;
    m->threshold = 12;
    m->size = 0;
    m->table = (JHashEntry**)calloc((size_t)m->capacity, sizeof(JHashEntry*));
}

void JHashMap_free(JHashMap* m) {
    for (int i = 0; i < m->capacity; i++) {
        JHashEntry* e = m->table[i];
        while (e) {
            JHashEntry* next = e->next;
            free(e);
            e = next;
        }
    }
    free(m->table);
    m->table = NULL;
    m->size = m->capacity = 0;
}

// new entries go to the front of their bucket, and moving them to a bigger table flips each chain
static void resize(JHashMap* m, int capacity) {
    JHashEntry** table = (JHashEntry**)calloc((size_t)capacity, sizeof(JHashEntry*));
    for (int j = 0; j < m->capacity; j++) {
        JHashEntry* e = m->table[j];
        while (e) {
            JHashEntry* next = e->next;
            int i = e->hash & (capacity - 1);
            e->next = table[i];
            table[i] = e;
            e = next;
        }
    }
    free(m->table);
    m->table = table;
    m->capacity = capacity;
    m->threshold = (int)((float)capacity * 0.75f);
}

void* JHashMap_put(JHashMap* m, int key, void* value) {
    int hash = hashOf(key);
    int i = hash & (m->capacity - 1);
    for (JHashEntry* e = m->table[i]; e; e = e->next) {
        if (e->hash == hash && e->key == key) {
            void* old = e->value;
            e->value = value;
            return old;
        }
    }
    JHashEntry* e = (JHashEntry*)malloc(sizeof(JHashEntry));
    e->key = key;
    e->hash = hash;
    e->value = value;
    e->next = m->table[i];
    m->table[i] = e;
    if (m->size++ >= m->threshold) resize(m, m->capacity * 2);
    return NULL;
}

void* JHashMap_get(const JHashMap* m, int key) {
    int hash = hashOf(key);
    for (JHashEntry* e = m->table[hash & (m->capacity - 1)]; e; e = e->next) {
        if (e->hash == hash && e->key == key) return e->value;
    }
    return NULL;
}

void* JHashMap_remove(JHashMap* m, int key) {
    int hash = hashOf(key);
    JHashEntry** link = &m->table[hash & (m->capacity - 1)];
    while (*link) {
        JHashEntry* e = *link;
        if (e->hash == hash && e->key == key) {
            void* value = e->value;
            *link = e->next;
            free(e);
            m->size--;
            return value;
        }
        link = &e->next;
    }
    return NULL;
}

int JHashMap_values(const JHashMap* m, void** out, int max) {
    int n = 0;
    for (int i = 0; i < m->capacity; i++) {
        for (JHashEntry* e = m->table[i]; e && n < max; e = e->next) out[n++] = e->value;
    }
    return n;
}
