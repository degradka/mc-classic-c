// java_hashmap.h: HashMap with int keys, only kept for its iteration order

#ifndef JAVA_HASHMAP_H
#define JAVA_HASHMAP_H

typedef struct JHashEntry {
    int key;
    int hash;
    void* value;
    struct JHashEntry* next;
} JHashEntry;

typedef struct {
    JHashEntry** table;
    int capacity;
    int size;
    int threshold;
} JHashMap;

void  JHashMap_init(JHashMap* m);
void  JHashMap_free(JHashMap* m);
// returns the old value or NULL
void* JHashMap_put(JHashMap* m, int key, void* value);
void* JHashMap_get(const JHashMap* m, int key);
void* JHashMap_remove(JHashMap* m, int key);
// the values in the order values() hands them out, returns how many
int   JHashMap_values(const JHashMap* m, void** out, int max);

#endif
