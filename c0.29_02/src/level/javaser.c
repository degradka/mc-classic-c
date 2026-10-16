// level/javaser.c

#include "javaser.h"
#include <stdlib.h>
#include <string.h>

#define BASE_HANDLE 0x7E0000

enum {
    TC_NULL = 0x70, TC_REFERENCE = 0x71, TC_CLASSDESC = 0x72, TC_OBJECT = 0x73, TC_STRING = 0x74,
    TC_ARRAY = 0x75, TC_BLOCKDATA = 0x77, TC_ENDBLOCKDATA = 0x78, TC_RESET = 0x79, TC_BLOCKDATALONG = 0x7A,
    TC_LONGSTRING = 0x7C
};

/* reader */

struct JStream {
    const unsigned char* data;
    size_t len, pos;
    bool bad;
    void** handles;
    int nhandles, capHandles;
    void** owned;       // everything allocated, freed together
    int nowned, capOwned;
    JClass** classes;   // every class descriptor, in stream order
    int nclasses, capClasses;
};

static void* own(JStream* s, void* p) {
    if (!p) { s->bad = true; return NULL; }
    if (s->nowned == s->capOwned) {
        s->capOwned = s->capOwned ? s->capOwned * 2 : 256;
        void** n = (void**)realloc(s->owned, (size_t)s->capOwned * sizeof(void*));
        if (!n) { free(p); s->bad = true; return NULL; }
        s->owned = n;
    }
    s->owned[s->nowned++] = p;
    return p;
}

static void* alloc(JStream* s, size_t n) { return own(s, calloc(1, n ? n : 1)); }

static int handle(JStream* s, void* p) {
    if (s->nhandles == s->capHandles) {
        s->capHandles = s->capHandles ? s->capHandles * 2 : 256;
        void** n = (void**)realloc(s->handles, (size_t)s->capHandles * sizeof(void*));
        if (!n) { s->bad = true; return -1; }
        s->handles = n;
    }
    s->handles[s->nhandles] = p;
    return s->nhandles++;
}

static int rd1(JStream* s) {
    if (s->pos >= s->len) { s->bad = true; return 0; }
    return s->data[s->pos++];
}

static int64_t rdn(JStream* s, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 8) | (uint64_t)rd1(s);
    return (int64_t)v;
}

static int rd2(JStream* s) { return (int)(uint16_t)rdn(s, 2); }
static int32_t rd4(JStream* s) { return (int32_t)(uint32_t)rdn(s, 4); }

static char* rdutf(JStream* s, size_t n) {
    if (s->pos + n > s->len) { s->bad = true; return NULL; }
    char* str = (char*)alloc(s, n + 1);
    if (!str) return NULL;
    memcpy(str, s->data + s->pos, n);
    s->pos += n;
    return str;
}

static void* content(JStream* s);

static JClass* classDesc(JStream* s) {
    int tc = rd1(s);
    if (tc == TC_NULL) return NULL;
    if (tc == TC_REFERENCE) {
        int h = rd4(s) - BASE_HANDLE;
        if (h < 0 || h >= s->nhandles) { s->bad = true; return NULL; }
        return (JClass*)s->handles[h];
    }
    if (tc != TC_CLASSDESC) { s->bad = true; return NULL; }
    JClass* c = (JClass*)alloc(s, sizeof(JClass));
    if (!c) return NULL;
    c->name = rdutf(s, (size_t)rd2(s));
    c->suid = rdn(s, 8);
    handle(s, c);
    if (s->nclasses == s->capClasses) {
        int cap = s->capClasses ? s->capClasses * 2 : 32;
        JClass** n = (JClass**)realloc(s->classes, (size_t)cap * sizeof(JClass*));
        if (!n) { s->bad = true; return NULL; }
        s->classes = n;
        s->capClasses = cap;
    }
    s->classes[s->nclasses++] = c;
    c->flags = rd1(s);
    c->nfields = rd2(s);
    c->fields = (JField*)alloc(s, (size_t)c->nfields * sizeof(JField));
    for (int i = 0; i < c->nfields && !s->bad; ++i) {
        c->fields[i].type = (char)rd1(s);
        c->fields[i].name = rdutf(s, (size_t)rd2(s));
        if (c->fields[i].type == 'L' || c->fields[i].type == '[') {
            JNode* t = (JNode*)content(s);
            c->fields[i].cls = t && t->kind == JN_STRING ? t->str : "";
        }
    }
    while (!s->bad && s->pos < s->len && s->data[s->pos] != TC_ENDBLOCKDATA) content(s);
    rd1(s);
    c->super = classDesc(s);
    return c;
}

static int64_t prim(JStream* s, char t, double* d) {
    switch (t) {
        case 'B': return (int8_t)rd1(s);
        case 'Z': return rd1(s) != 0;
        case 'C': return rd2(s);
        case 'S': return (int16_t)rd2(s);
        case 'I': return rd4(s);
        case 'J': return rdn(s, 8);
        case 'F': { uint32_t b = (uint32_t)rd4(s); float f; memcpy(&f, &b, 4); *d = f; return 0; }
        case 'D': { uint64_t b = (uint64_t)rdn(s, 8); memcpy(d, &b, 8); return 0; }
    }
    s->bad = true;
    return 0;
}

static void annotation(JStream* s, JNode* n) {
    int count = n->nextra, cap = count + 16;
    JNode** list = (JNode**)malloc((size_t)cap * sizeof(JNode*));
    if (!list) { s->bad = true; return; }
    for (int i = 0; i < count; ++i) list[i] = n->extra[i];
    while (!s->bad) {
        int tc = s->pos < s->len ? s->data[s->pos] : -1;
        if (tc == TC_ENDBLOCKDATA) { s->pos++; break; }
        if (tc == TC_BLOCKDATA) { s->pos++; s->pos += (size_t)rd1(s); continue; }
        if (tc == TC_BLOCKDATALONG) { s->pos++; s->pos += (size_t)(uint32_t)rd4(s); continue; }
        JNode* c = (JNode*)content(s);
        if (count == cap) {
            cap *= 2;
            JNode** g = (JNode**)realloc(list, (size_t)cap * sizeof(JNode*));
            if (!g) { s->bad = true; break; }
            list = g;
        }
        list[count++] = c;
    }
    n->extra = (JNode**)own(s, list);
    n->nextra = count;
}

static void* content(JStream* s) {
    int tc = rd1(s);
    switch (tc) {
        case TC_NULL: return NULL;
        case TC_RESET: return content(s);
        case TC_REFERENCE: {
            int h = rd4(s) - BASE_HANDLE;
            if (h < 0 || h >= s->nhandles) { s->bad = true; return NULL; }
            return s->handles[h];
        }
        case TC_STRING:
        case TC_LONGSTRING: {
            JNode* n = (JNode*)alloc(s, sizeof(JNode));
            if (!n) return NULL;
            n->kind = JN_STRING;
            n->str = rdutf(s, tc == TC_STRING ? (size_t)rd2(s) : (size_t)rdn(s, 8));
            handle(s, n);
            return n;
        }
        case TC_CLASSDESC:
            s->pos--;
            return classDesc(s);
        case TC_ARRAY: {
            JNode* n = (JNode*)alloc(s, sizeof(JNode));
            if (!n) return NULL;
            n->kind = JN_ARRAY;
            n->cls = classDesc(s);
            handle(s, n);
            n->len = rd4(s);
            if (!n->cls || !n->cls->name || n->len < 0 || (size_t)n->len > s->len) { s->bad = true; return n; }
            char et = n->cls->name[1];
            if (et == 'B') {
                n->bytes = (unsigned char*)alloc(s, (size_t)n->len);
                if (n->bytes && s->pos + (size_t)n->len <= s->len) memcpy(n->bytes, s->data + s->pos, (size_t)n->len);
                else s->bad = true;
                s->pos += (size_t)n->len;
            } else if (et == 'L' || et == '[') {
                n->elems = (JNode**)alloc(s, (size_t)n->len * sizeof(JNode*));
                for (int i = 0; i < n->len && !s->bad; ++i) n->elems[i] = (JNode*)content(s);
            } else {
                n->prims = (int64_t*)alloc(s, (size_t)n->len * sizeof(int64_t));
                double d = 0;
                for (int i = 0; i < n->len && !s->bad; ++i) n->prims[i] = prim(s, et, &d);
            }
            return n;
        }
        case TC_OBJECT: {
            JNode* n = (JNode*)alloc(s, sizeof(JNode));
            if (!n) return NULL;
            n->kind = JN_OBJECT;
            n->cls = classDesc(s);
            handle(s, n);
            JClass* chain[32];
            int depth = 0;
            for (JClass* c = n->cls; c && depth < 32; c = c->super) chain[depth++] = c;
            for (int k = 0; k < depth; ++k) n->nvals += chain[k]->nfields;
            n->vals = (JValue*)alloc(s, (size_t)n->nvals * sizeof(JValue));
            int v = 0;
            for (int k = depth - 1; k >= 0 && !s->bad; --k) {
                JClass* c = chain[k];
                for (int i = 0; i < c->nfields && !s->bad; ++i, ++v) {
                    JValue* val = &n->vals[v];
                    val->type = c->fields[i].type;
                    if (val->type == 'L' || val->type == '[') val->ref = (JNode*)content(s);
                    else val->i = prim(s, val->type, &val->d);
                }
                if (c->flags & 1) annotation(s, n);
            }
            return n;
        }
    }
    s->bad = true;
    return NULL;
}

JStream* JStream_read(const unsigned char* data, size_t len, JNode** root) {
    JStream* s = (JStream*)calloc(1, sizeof(JStream));
    if (!s) return NULL;
    s->data = data;
    s->len = len;
    *root = NULL;
    if (rd2(s) != 0xACED || rd2(s) != 5) { JStream_free(s); return NULL; }
    JNode* n = (JNode*)content(s);
    if (s->bad || !n || n->kind != JN_OBJECT) { JStream_free(s); return NULL; }
    *root = n;
    return s;
}

void JStream_free(JStream* s) {
    if (!s) return;
    for (int i = 0; i < s->nowned; ++i) free(s->owned[i]);
    free(s->owned);
    free(s->handles);
    free(s->classes);
    free(s);
}

const char* JStream_unknownClass(const JStream* s, const char* const* known, int n) {
    for (int i = 0; i < s->nclasses; ++i) {
        const char* name = s->classes[i]->name;
        bool found = false;
        for (int k = 0; k < n && !found; ++k) found = name && strcmp(name, known[k]) == 0;
        if (!found) return name ? name : "";
    }
    return NULL;
}

const char* JStream_suidMismatch(const JStream* s, const char* const* names, const int64_t* suids, int n) {
    for (int i = 0; i < s->nclasses; ++i) {
        const JClass* c = s->classes[i];
        for (int k = 0; k < n; ++k) {
            if (c->name && strcmp(c->name, names[k]) == 0 && c->suid != suids[k]) return c->name;
        }
    }
    return NULL;
}

bool JNode_is(const JNode* n, const char* cls) {
    return n && n->cls && n->cls->name && strcmp(n->cls->name, cls) == 0;
}

const JValue* JNode_field(const JNode* n, const char* cls, const char* name) {
    if (!n || n->kind != JN_OBJECT) return NULL;
    int end = n->nvals;
    for (const JClass* c = n->cls; c; c = c->super) {
        int start = end - c->nfields;
        if (!cls || (c->name && strcmp(c->name, cls) == 0)) {
            for (int i = 0; i < c->nfields; ++i) {
                if (strcmp(c->fields[i].name, name) == 0) return &n->vals[start + i];
            }
        }
        end = start;
    }
    return NULL;
}

int64_t JNode_int(const JNode* n, const char* cls, const char* name) {
    const JValue* v = JNode_field(n, cls, name);
    return v ? v->i : 0;
}

float JNode_float(const JNode* n, const char* cls, const char* name) {
    const JValue* v = JNode_field(n, cls, name);
    return v ? (float)v->d : 0.0f;
}

double JNode_double(const JNode* n, const char* cls, const char* name) {
    const JValue* v = JNode_field(n, cls, name);
    return v ? v->d : 0.0;
}

JNode* JNode_obj(const JNode* n, const char* cls, const char* name) {
    const JValue* v = JNode_field(n, cls, name);
    return v ? v->ref : NULL;
}

/* writer */

void JWriter_init(JWriter* w) {
    memset(w, 0, sizeof *w);
    JW_i2(w, 0xACED);
    JW_i2(w, 5);
}

void JWriter_free(JWriter* w) {
    for (int i = 0; i < w->handles; ++i) free(w->strings[i]);
    free(w->strings);
    free(w->ids);
    free(w->buf);
    memset(w, 0, sizeof *w);
}

static void put(JWriter* w, const void* p, size_t n) {
    if (w->len + n > w->cap) {
        size_t cap = w->cap ? w->cap * 2 : 65536;
        while (cap < w->len + n) cap *= 2;
        unsigned char* b = (unsigned char*)realloc(w->buf, cap);
        if (!b) return;
        w->buf = b;
        w->cap = cap;
    }
    memcpy(w->buf + w->len, p, n);
    w->len += n;
}

void JW_u1(JWriter* w, int v) { unsigned char b = (unsigned char)v; put(w, &b, 1); }
void JW_i2(JWriter* w, int v) { JW_u1(w, v >> 8); JW_u1(w, v); }
void JW_i4(JWriter* w, int32_t v) { JW_i2(w, (int)((uint32_t)v >> 16)); JW_i2(w, v & 0xFFFF); }
void JW_i8(JWriter* w, int64_t v) { JW_i4(w, (int32_t)((uint64_t)v >> 32)); JW_i4(w, (int32_t)(uint32_t)v); }
void JW_f4(JWriter* w, float v) { uint32_t b; memcpy(&b, &v, 4); JW_i4(w, (int32_t)b); }
void JW_d8(JWriter* w, double v) { uint64_t b; memcpy(&b, &v, 8); JW_i8(w, (int64_t)b); }
void JW_null(JWriter* w) { JW_u1(w, TC_NULL); }

static void newHandle(JWriter* w, const void* id, const char* str) {
    if (w->handles == w->idCap) {
        w->idCap = w->idCap ? w->idCap * 2 : 1024;
        w->ids = (const void**)realloc((void*)w->ids, (size_t)w->idCap * sizeof(void*));
        w->strings = (char**)realloc(w->strings, (size_t)w->idCap * sizeof(char*));
    }
    w->ids[w->handles] = id;
    w->strings[w->handles] = NULL;
    if (str) {
        size_t n = strlen(str);
        w->strings[w->handles] = (char*)malloc(n + 1);
        if (w->strings[w->handles]) memcpy(w->strings[w->handles], str, n + 1);
    }
    w->handles++;
}

bool JW_ref(JWriter* w, const void* id) {
    if (!id) return false;
    for (int i = 0; i < w->handles; ++i) {
        if (w->ids[i] == id) {
            JW_u1(w, TC_REFERENCE);
            JW_i4(w, BASE_HANDLE + i);
            return true;
        }
    }
    return false;
}

static void utf(JWriter* w, const char* s) {
    size_t n = strlen(s);
    JW_i2(w, (int)n);
    put(w, s, n);
}

void JW_string(JWriter* w, const char* s) {
    if (!s) { JW_null(w); return; }
    for (int i = 0; i < w->handles; ++i) {
        if (w->strings[i] && strcmp(w->strings[i], s) == 0) {
            JW_u1(w, TC_REFERENCE);
            JW_i4(w, BASE_HANDLE + i);
            return;
        }
    }
    JW_u1(w, TC_STRING);
    utf(w, s);
    newHandle(w, NULL, s);
}

static void writeClassDesc(JWriter* w, const JDesc* d) {
    if (!d) { JW_null(w); return; }
    if (JW_ref(w, d)) return;
    JW_u1(w, TC_CLASSDESC);
    utf(w, d->name);
    JW_i8(w, d->suid);
    newHandle(w, d, NULL);
    JW_u1(w, d->flags);
    JW_i2(w, d->nfields);
    for (int i = 0; i < d->nfields; ++i) {
        JW_u1(w, d->fields[i].type);
        utf(w, d->fields[i].name);
        if (d->fields[i].type == 'L' || d->fields[i].type == '[') JW_string(w, d->fields[i].cls);
    }
    JW_u1(w, TC_ENDBLOCKDATA);
    writeClassDesc(w, d->super);
}

void JW_object(JWriter* w, const JDesc* d, const void* id) {
    JW_u1(w, TC_OBJECT);
    writeClassDesc(w, d);
    newHandle(w, id, NULL);
}

void JW_array(JWriter* w, const JDesc* d, const void* id, int n) {
    JW_u1(w, TC_ARRAY);
    writeClassDesc(w, d);
    newHandle(w, id, NULL);
    JW_i4(w, n);
}

static const JDesc INT_ARRAY = { "[I", 5600894804908749477LL, 2, 0, NULL, NULL };
static const JDesc BYTE_ARRAY = { "[B", -5984413125824719648LL, 2, 0, NULL, NULL };

void JW_intArray(JWriter* w, const int* v, int n) {
    JW_array(w, &INT_ARRAY, NULL, n);
    for (int i = 0; i < n; ++i) JW_i4(w, v[i]);
}

void JW_byteArray(JWriter* w, const unsigned char* v, int n) {
    JW_array(w, &BYTE_ARRAY, NULL, n);
    put(w, v, (size_t)n);
}

void JW_block(JWriter* w, const unsigned char* v, int n) {
    JW_u1(w, TC_BLOCKDATA);
    JW_u1(w, n);
    put(w, v, (size_t)n);
}

void JW_endBlock(JWriter* w) { JW_u1(w, TC_ENDBLOCKDATA); }
