// level/javaser.h: just enough object serialization for level.dat version 2

#ifndef JAVASER_H
#define JAVASER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct JField {
    char type;          // B C D F I J S Z, L for objects, [ for arrays
    const char* name;
    const char* cls;    // type string for L and [, like "Ljava/lang/String;"
} JField;

typedef struct JDesc {
    const char* name;
    int64_t suid;
    int flags;          // 2 serializable, 3 with its own writeObject
    int nfields;
    const JField* fields;
    const struct JDesc* super;
} JDesc;

/* reader */

typedef struct JNode JNode;

typedef struct JValue {
    char type;
    int64_t i;
    double d;
    JNode* ref;
} JValue;

enum { JN_OBJECT, JN_ARRAY, JN_STRING, JN_CLASS };

typedef struct JClass {
    char* name;
    int64_t suid;
    int flags;
    int nfields;
    JField* fields;     // names and type strings owned by the class
    struct JClass* super;
} JClass;

struct JNode {
    int kind;
    JClass* cls;
    JValue* vals;       // object: fields of every class, top super first
    int nvals;
    JNode** extra;      // object: objects written by a custom writeObject
    int nextra;
    int len;            // array length
    int64_t* prims;     // primitive array
    unsigned char* bytes; // byte array
    JNode** elems;      // object array
    char* str;
};

typedef struct JStream JStream;

// parses a whole stream, returns the first object or null
JStream* JStream_read(const unsigned char* data, size_t len, JNode** root);
// the first declared class that isn't in known, NULL when there is none
const char* JStream_unknownClass(const JStream* s, const char* const* known, int n);
// the first class whose serialVersionUID differs from the one listed for its name
const char* JStream_suidMismatch(const JStream* s, const char* const* names, const int64_t* suids, int n);
void JStream_free(JStream* s);

bool JNode_is(const JNode* n, const char* cls);
// looks up a field, cls null means the most derived class that has it
const JValue* JNode_field(const JNode* n, const char* cls, const char* name);
int64_t JNode_int(const JNode* n, const char* cls, const char* name);
float JNode_float(const JNode* n, const char* cls, const char* name);
double JNode_double(const JNode* n, const char* cls, const char* name);
JNode* JNode_obj(const JNode* n, const char* cls, const char* name);

/* writer */

typedef struct JWriter {
    unsigned char* buf;
    size_t len, cap;
    int handles;
    const void** ids;   // what each handle stands for
    int idCap;
    char** strings;     // string handles, null where the handle is something else
} JWriter;

void JWriter_init(JWriter* w);
void JWriter_free(JWriter* w);
void JW_u1(JWriter* w, int v);
void JW_i2(JWriter* w, int v);
void JW_i4(JWriter* w, int32_t v);
void JW_i8(JWriter* w, int64_t v);
void JW_f4(JWriter* w, float v);
void JW_d8(JWriter* w, double v);
void JW_null(JWriter* w);
// writes a back reference if id was written before
bool JW_ref(JWriter* w, const void* id);
// starts an object, its field values follow in class order
void JW_object(JWriter* w, const JDesc* d, const void* id);
void JW_string(JWriter* w, const char* s);
void JW_intArray(JWriter* w, const int* v, int n);
void JW_byteArray(JWriter* w, const unsigned char* v, int n);
// starts an object array, n elements follow
void JW_array(JWriter* w, const JDesc* d, const void* id, int n);
void JW_block(JWriter* w, const unsigned char* v, int n);
void JW_endBlock(JWriter* w);

#endif
