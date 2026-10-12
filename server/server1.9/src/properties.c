// properties.c

#include "properties.h"
#include "jfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void Properties_init(Properties* p) {
    p->length = 11;
    p->count = 0;
    p->threshold = 8;
    p->table = (PropertiesEntry**)calloc(11, sizeof *p->table);
}

static int indexFor(int32_t hash, int length) {
    return (int)((hash & 0x7FFFFFFF) % length);
}

static void rehash(Properties* p) {
    int length = p->length * 2 + 1;
    PropertiesEntry** table = (PropertiesEntry**)calloc((size_t)length, sizeof *table);
    for (int i = p->length; i-- > 0;) {
        PropertiesEntry* old = p->table[i];
        while (old) {
            PropertiesEntry* e = old;
            old = old->next;
            int index = indexFor(e->hash, length);
            e->next = table[index];
            table[index] = e;
        }
    }
    free(p->table);
    p->table = table;
    p->length = length;
    p->threshold = (int)(length * 0.75f);
}

static void put(Properties* p, const JStr* key, const JStr* value) {
    int32_t hash = jstr_hash(key);
    int index = indexFor(hash, p->length);
    for (PropertiesEntry* e = p->table[index]; e; e = e->next) {
        if (e->hash == hash && jstr_eq(&e->key, key)) {
            jstr_copy(&e->value, value);
            return;
        }
    }
    if (p->count >= p->threshold) {
        rehash(p);
        index = indexFor(hash, p->length);
    }
    PropertiesEntry* e = (PropertiesEntry*)malloc(sizeof *e);
    e->hash = hash;
    jstr_copy(&e->key, key);
    jstr_copy(&e->value, value);
    e->next = p->table[index];
    p->table[index] = e;
    p->count++;
}

static const PropertiesEntry* lookup(const Properties* p, const JStr* key) {
    int32_t hash = jstr_hash(key);
    for (PropertiesEntry* e = p->table[indexFor(hash, p->length)]; e; e = e->next) {
        if (e->hash == hash && jstr_eq(&e->key, key)) return e;
    }
    return NULL;
}

void Properties_get(const Properties* p, const char* key, const char* def, JStr* out) {
    static JStr k;
    jstr_set(&k, key);
    const PropertiesEntry* e = lookup(p, &k);
    if (e) jstr_copy(out, &e->value);
    else jstr_set(out, def);
}

void Properties_set(Properties* p, const char* key, const JStr* value) {
    static JStr k;
    jstr_set(&k, key);
    put(p, &k, value);
}

/* load, Properties.LineReader and load0 */

typedef struct {
    const uint16_t* in;
    int limit, off;
    uint16_t* line;
} LineReader;

static int readLine(LineReader* r) {
    int len = 0;
    int skipWhiteSpace = 1, isCommentLine = 0, isNewLine = 1;
    int appendedLineBegin = 0, precedingBackslash = 0, skipLF = 0;
    for (;;) {
        if (r->off >= r->limit) return (len == 0 || isCommentLine) ? -1 : len;
        int c = r->in[r->off++];
        if (skipLF) {
            skipLF = 0;
            if (c == '\n') continue;
        }
        if (skipWhiteSpace) {
            if (c == ' ' || c == '\t' || c == '\f') continue;
            if (!appendedLineBegin && (c == '\r' || c == '\n')) continue;
            skipWhiteSpace = 0;
            appendedLineBegin = 0;
        }
        if (isNewLine) {
            isNewLine = 0;
            if (c == '#' || c == '!') {
                isCommentLine = 1;
                continue;
            }
        }
        if (c != '\n' && c != '\r') {
            r->line[len++] = (uint16_t)c;
            precedingBackslash = c == '\\' ? !precedingBackslash : 0;
        } else {
            if (isCommentLine || len == 0) {
                isCommentLine = 0;
                isNewLine = 1;
                skipWhiteSpace = 1;
                len = 0;
                continue;
            }
            if (r->off >= r->limit) return len;
            if (precedingBackslash) {
                len -= 1;
                skipWhiteSpace = 1;
                appendedLineBegin = 1;
                precedingBackslash = 0;
                if (c == '\r') skipLF = 1;
            } else {
                return len;
            }
        }
    }
}

static int hexValue(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// false on a malformed \uxxxx
static bool loadConvert(const uint16_t* in, int off, int len, JStr* out) {
    int end = off + len;
    out->n = 0;
    while (off < end) {
        int c = in[off++];
        if (c == '\\' && off < end) {
            c = in[off++];
            if (c == 'u') {
                int value = 0;
                for (int i = 0; i < 4; ++i) {
                    int h = off < end ? hexValue(in[off++]) : -1;
                    if (h < 0) return false;
                    value = (value << 4) + h;
                }
                c = value;
            } else if (c == 't') c = '\t';
            else if (c == 'r') c = '\r';
            else if (c == 'n') c = '\n';
            else if (c == 'f') c = '\f';
        }
        if (out->n < JSTR_MAX) out->c[out->n++] = (uint16_t)c;
    }
    return true;
}

bool Properties_load(Properties* p, const char* path) {
    char exception[512];
    int len = 0;
    uint8_t* data = jfile_read(path, &len, exception, sizeof exception);
    if (!data) return false;
    static JStr text, key, value;
    jstr_fromPlatform(&text, data, len);
    free(data);
    static uint16_t line[JSTR_MAX];
    LineReader r = { text.c, text.n, 0, line };
    int limit;
    while ((limit = readLine(&r)) >= 0) {
        int keyLen = 0, valueStart = limit, hasSep = 0, precedingBackslash = 0;
        while (keyLen < limit) {
            int c = line[keyLen];
            if ((c == '=' || c == ':') && !precedingBackslash) {
                valueStart = keyLen + 1;
                hasSep = 1;
                break;
            } else if ((c == ' ' || c == '\t' || c == '\f') && !precedingBackslash) {
                valueStart = keyLen + 1;
                break;
            }
            precedingBackslash = c == '\\' ? !precedingBackslash : 0;
            keyLen++;
        }
        while (valueStart < limit) {
            int c = line[valueStart];
            if (c != ' ' && c != '\t' && c != '\f') {
                if (!hasSep && (c == '=' || c == ':')) hasSep = 1;
                else break;
            }
            valueStart++;
        }
        if (!loadConvert(line, 0, keyLen, &key) || !loadConvert(line, valueStart, limit - valueStart, &value)) return false;
        put(p, &key, &value);
    }
    return true;
}

/* store, store0 */

static void saveConvert(const JStr* s, bool escapeSpace, JStr* out) {
    out->n = 0;
    for (int x = 0; x < s->n; ++x) {
        int c = s->c[x];
        char esc = 0;
        if (c > 61 && c < 127) {
            if (c == '\\') esc = '\\';
        } else if (c == ' ') {
            if (x == 0 || escapeSpace) esc = ' ';
        } else if (c == '\t') esc = 't';
        else if (c == '\n') esc = 'n';
        else if (c == '\r') esc = 'r';
        else if (c == '\f') esc = 'f';
        else if (c == '=' || c == ':' || c == '#' || c == '!') esc = (char)c;
        if (esc) {
            if (out->n + 2 > JSTR_MAX) break;
            out->c[out->n++] = '\\';
            out->c[out->n++] = (uint16_t)esc;
        } else if (out->n < JSTR_MAX) {
            out->c[out->n++] = (uint16_t)c;
        }
    }
}

static void writeJStr(FILE* f, const JStr* s) {
    static uint8_t bytes[JSTR_MAX * 3];
    int n = jstr_toPlatform(s, bytes, sizeof bytes);
    fwrite(bytes, 1, (size_t)n, f);
}

// Date.toString, the zone as GMT+hh:mm where the c library has no short name for it
static void dateString(char* out, size_t size) {
    time_t now = time(NULL);
    struct tm local = *localtime(&now);
    char zone[64];
    strftime(zone, sizeof zone, "%Z", &local);
    if (strlen(zone) > 5 || strchr(zone, ' ')) {
        struct tm utc = *gmtime(&now);
        utc.tm_isdst = local.tm_isdst;
        long offset = (long)difftime(now, mktime(&utc));
        long a = offset < 0 ? -offset : offset;
        snprintf(zone, sizeof zone, "GMT%c%02ld:%02ld", offset < 0 ? '-' : '+', a / 3600, a / 60 % 60);
    }
    static const char* DAYS[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    snprintf(out, size, "%s %s %02d %02d:%02d:%02d %s %d", DAYS[local.tm_wday], MONTHS[local.tm_mon], local.tm_mday,
             local.tm_hour, local.tm_min, local.tm_sec, zone, local.tm_year + 1900);
}

bool Properties_store(const Properties* p, const char* path, const char* comment) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    char date[96];
    dateString(date, sizeof date);
    fprintf(f, "#%s%s#%s%s", comment, JFILE_NEWLINE, date, JFILE_NEWLINE);
    static JStr key, value;
    // Hashtable.keys() runs from the last bucket down
    for (int i = p->length; i-- > 0;) {
        for (const PropertiesEntry* e = p->table[i]; e; e = e->next) {
            saveConvert(&e->key, true, &key);
            saveConvert(&e->value, false, &value);
            writeJStr(f, &key);
            fputc('=', f);
            writeJStr(f, &value);
            fputs(JFILE_NEWLINE, f);
        }
    }
    fclose(f);
    return true;
}
