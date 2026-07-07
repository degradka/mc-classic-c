// properties.h: Properties for server.properties
// a Hashtable underneath, store() writes the keys in its order

#ifndef PROPERTIES_H
#define PROPERTIES_H

#include "jstring.h"
#include <stdbool.h>

typedef struct PropertiesEntry {
    int32_t hash;
    JStr key, value;
    struct PropertiesEntry* next;
} PropertiesEntry;

typedef struct {
    PropertiesEntry** table;
    int length;
    int count;
    int threshold;
} Properties;

void Properties_init(Properties* p);
// false if the file can't be read, what loaded before a bad \u escape stays
bool Properties_load(Properties* p, const char* path);
bool Properties_store(const Properties* p, const char* path, const char* comment);
void Properties_get(const Properties* p, const char* key, const char* def, JStr* out);
void Properties_set(Properties* p, const char* key, const JStr* value);

#endif
