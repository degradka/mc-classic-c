// jstring.c

#include "jstring.h"
#include "jexc.h"
#include <stdio.h>
#include <string.h>
#include <wctype.h>

static void push(JStr* s, int u) {
    if (s->n < JSTR_MAX) s->c[s->n++] = (uint16_t)u;
}

static void pushCode(JStr* s, uint32_t cp) {
    if (cp >= 0x10000) {
        cp -= 0x10000;
        push(s, 0xD800 + (int)(cp >> 10));
        push(s, 0xDC00 + (int)(cp & 0x3FF));
    } else {
        push(s, (int)cp);
    }
}

static void decodeUtf8(JStr* s, const uint8_t* b, int len) {
    int i = 0;
    while (i < len) {
        int b0 = b[i];
        if (b0 < 0x80) { push(s, b0); i++; continue; }
        int need, lo = 0x80, hi = 0xBF;
        uint32_t cp;
        if (b0 >= 0xC2 && b0 <= 0xDF) { need = 1; cp = (uint32_t)(b0 & 0x1F); }
        else if (b0 >= 0xE0 && b0 <= 0xEF) {
            need = 2; cp = (uint32_t)(b0 & 0x0F);
            if (b0 == 0xE0) lo = 0xA0;
            if (b0 == 0xED) hi = 0x9F;
        } else if (b0 >= 0xF0 && b0 <= 0xF4) {
            need = 3; cp = (uint32_t)(b0 & 0x07);
            if (b0 == 0xF0) lo = 0x90;
            if (b0 == 0xF4) hi = 0x8F;
        } else { push(s, 0xFFFD); i++; continue; }
        int k = 1;
        for (; k <= need; ++k) {
            if (i + k >= len) break;
            int bk = b[i + k];
            if (bk < lo || bk > hi) break;
            cp = (cp << 6) | (uint32_t)(bk & 0x3F);
            lo = 0x80; hi = 0xBF;
        }
        if (k <= need) { push(s, 0xFFFD); i += k; continue; }
        pushCode(s, cp);
        i += need + 1;
    }
}

void jstr_set(JStr* s, const char* utf8) {
    s->n = 0;
    decodeUtf8(s, (const uint8_t*)utf8, (int)strlen(utf8));
}

void jstr_fromUtf8(JStr* s, const uint8_t* bytes, int len) {
    s->n = 0;
    decodeUtf8(s, bytes, len);
}

static int put(uint8_t* out, int cap, int n, int b) {
    if (n < cap) out[n] = (uint8_t)b;
    return n + 1;
}

int jstr_toUtf8(const JStr* s, uint8_t* out, int cap) {
    int n = 0;
    for (int i = 0; i < s->n; ++i) {
        uint32_t c = s->c[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s->n && s->c[i + 1] >= 0xDC00 && s->c[i + 1] <= 0xDFFF) {
            uint32_t cp = 0x10000 + ((c - 0xD800) << 10) + (s->c[i + 1] - 0xDC00u);
            i++;
            n = put(out, cap, n, 0xF0 | (int)(cp >> 18));
            n = put(out, cap, n, 0x80 | (int)((cp >> 12) & 0x3F));
            n = put(out, cap, n, 0x80 | (int)((cp >> 6) & 0x3F));
            n = put(out, cap, n, 0x80 | (int)(cp & 0x3F));
        } else if (c >= 0xD800 && c <= 0xDFFF) {
            n = put(out, cap, n, '?');
        } else if (c < 0x80) {
            n = put(out, cap, n, (int)c);
        } else if (c < 0x800) {
            n = put(out, cap, n, 0xC0 | (int)(c >> 6));
            n = put(out, cap, n, 0x80 | (int)(c & 0x3F));
        } else {
            n = put(out, cap, n, 0xE0 | (int)(c >> 12));
            n = put(out, cap, n, 0x80 | (int)((c >> 6) & 0x3F));
            n = put(out, cap, n, 0x80 | (int)(c & 0x3F));
        }
    }
    return n;
}

#if defined(_WIN32)
static const uint16_t CP1252[32] = {
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178
};
#endif

void jstr_fromPlatform(JStr* s, const uint8_t* bytes, int len) {
#if defined(_WIN32)
    s->n = 0;
    for (int i = 0; i < len; ++i) {
        int b = bytes[i];
        push(s, b >= 0x80 && b < 0xA0 ? CP1252[b - 0x80] : b);
    }
#else
    jstr_fromUtf8(s, bytes, len);
#endif
}

int jstr_toPlatform(const JStr* s, uint8_t* out, int cap) {
#if defined(_WIN32)
    int n = 0;
    for (int i = 0; i < s->n; ++i) {
        int c = s->c[i], b = '?';
        if (c < 0x80 || (c >= 0xA0 && c < 0x100)) b = c;
        else for (int k = 0; k < 32; ++k) if (CP1252[k] == c && c != 0xFFFD) { b = 0x80 + k; break; }
        n = put(out, cap, n, b);
    }
    return n;
#else
    return jstr_toUtf8(s, out, cap);
#endif
}

const char* jstr_c(const JStr* s, char* buf, int size) {
    int n = jstr_toUtf8(s, (uint8_t*)buf, size - 1);
    if (n > size - 1) n = size - 1;
    buf[n] = '\0';
    return buf;
}

void jstr_copy(JStr* dst, const JStr* src) {
    dst->n = src->n;
    memcpy(dst->c, src->c, (size_t)src->n * sizeof(uint16_t));
}

void jstr_cat(JStr* s, const JStr* t) {
    for (int i = 0; i < t->n; ++i) push(s, t->c[i]);
}

void jstr_catc(JStr* s, const char* utf8) {
    decodeUtf8(s, (const uint8_t*)utf8, (int)strlen(utf8));
}

void jstr_catInt(JStr* s, long long v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%lld", v);
    jstr_catc(s, buf);
}

void jstr_trim(JStr* s) {
    int a = 0, b = s->n;
    while (a < b && s->c[a] <= ' ') a++;
    while (b > a && s->c[b - 1] <= ' ') b--;
    memmove(s->c, s->c + a, (size_t)(b - a) * sizeof(uint16_t));
    s->n = b - a;
}

bool jstr_eq(const JStr* a, const JStr* b) {
    return a->n == b->n && memcmp(a->c, b->c, (size_t)a->n * sizeof(uint16_t)) == 0;
}

bool jstr_eqc(const JStr* a, const char* ascii) {
    int n = (int)strlen(ascii);
    if (a->n != n) return false;
    for (int i = 0; i < n; ++i) if (a->c[i] != (uint8_t)ascii[i]) return false;
    return true;
}

// Character.toLowerCase and toUpperCase, the wide ctype covers what's past latin-1
static int lowerUnit(int c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c < 0x80) return c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c < 0x100) return c;
    return (int)towlower((wint_t)c);
}

static int upperUnit(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c < 0x80) return c;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c == 0xFF) return 0x178;
    if (c == 0xB5) return 0x39C;
    if (c < 0x100) return c;
    return (int)towupper((wint_t)c);
}

bool jstr_eqIgnoreCase(const JStr* a, const JStr* b) {
    if (a->n != b->n) return false;
    for (int i = 0; i < a->n; ++i) {
        int c1 = a->c[i], c2 = b->c[i];
        if (c1 == c2) continue;
        int u1 = upperUnit(c1), u2 = upperUnit(c2);
        if (u1 == u2) continue;
        if (lowerUnit(u1) == lowerUnit(u2)) continue;
        return false;
    }
    return true;
}

void jstr_toLower(JStr* s) {
    for (int i = 0; i < s->n; ++i) s->c[i] = (uint16_t)lowerUnit(s->c[i]);
}

bool jstr_startsWithc(const JStr* s, const char* ascii) {
    int n = (int)strlen(ascii);
    if (s->n < n) return false;
    for (int i = 0; i < n; ++i) if (s->c[i] != (uint8_t)ascii[i]) return false;
    return true;
}

int32_t jstr_hash(const JStr* s) {
    uint32_t h = 0;
    for (int i = 0; i < s->n; ++i) h = 31 * h + s->c[i];
    return (int32_t)h;
}

void jstr_substring(JStr* out, const JStr* s, int begin) {
    if (begin < 0 || begin > s->n) {
        char text[96];
        snprintf(text, sizeof text, "java.lang.StringIndexOutOfBoundsException: String index out of range: %d",
                 begin < 0 ? begin : s->n - begin);
        jthrow(JEX_STRING_INDEX, text);
    }
    out->n = s->n - begin;
    memmove(out->c, s->c + begin, (size_t)out->n * sizeof(uint16_t));
}

int jstr_splitSpace(const JStr* s, JStr* parts, int max) {
    int count = 0, start = 0;
    bool found = false;
    for (int i = 0; i < s->n; ++i) {
        if (s->c[i] != ' ') continue;
        found = true;
        if (count < max) {
            parts[count].n = i - start;
            memcpy(parts[count].c, s->c + start, (size_t)(i - start) * sizeof(uint16_t));
        }
        count++;
        start = i + 1;
    }
    if (!found) {
        jstr_copy(&parts[0], s);
        return 1;
    }
    if (count < max) {
        parts[count].n = s->n - start;
        memcpy(parts[count].c, s->c + start, (size_t)(s->n - start) * sizeof(uint16_t));
    }
    count++;
    if (count > max) count = max;
    while (count > 0 && parts[count - 1].n == 0) count--;
    return count;
}
