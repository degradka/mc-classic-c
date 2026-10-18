// jstring.h: strings as utf-16 units, for names, chat and the player lists

#ifndef JSTRING_H
#define JSTRING_H

#include <stdbool.h>
#include <stdint.h>

#define JSTR_MAX 1024

typedef struct {
    int n;
    uint16_t c[JSTR_MAX];
} JStr;

void jstr_set(JStr* s, const char* utf8);
// new String(bytes, "UTF-8"), bad sequences become U+FFFD
void jstr_fromUtf8(JStr* s, const uint8_t* bytes, int len);
// getBytes("UTF-8"), returns the full length, writes at most cap
int  jstr_toUtf8(const JStr* s, uint8_t* out, int cap);
// FileReader and FileWriter, cp1252 on windows and utf-8 elsewhere
void jstr_fromPlatform(JStr* s, const uint8_t* bytes, int len);
int  jstr_toPlatform(const JStr* s, uint8_t* out, int cap);
const char* jstr_c(const JStr* s, char* buf, int size);

void jstr_copy(JStr* dst, const JStr* src);
void jstr_cat(JStr* s, const JStr* t);
void jstr_catc(JStr* s, const char* utf8);
void jstr_catInt(JStr* s, long long v);

void jstr_trim(JStr* s);
bool jstr_eq(const JStr* a, const JStr* b);
bool jstr_eqc(const JStr* a, const char* ascii);
bool jstr_eqIgnoreCase(const JStr* a, const JStr* b);
void jstr_toLower(JStr* s);
bool jstr_startsWithc(const JStr* s, const char* ascii);
int32_t jstr_hash(const JStr* s);
// substring(begin), throws StringIndexOutOfBoundsException
void jstr_substring(JStr* out, const JStr* s, int begin);
// split(" "), returns how many parts, at most max
int  jstr_splitSpace(const JStr* s, JStr* parts, int max);

#endif
