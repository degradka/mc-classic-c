// jexc.h: exceptions with setjmp, the server reacts to them where it would catch one
// leaving a JTRY body any other way than its end or a throw needs a JEND first

#ifndef JEXC_H
#define JEXC_H

#include <setjmp.h>

enum {
    JEX_IO = 1,
    JEX_RUNTIME,
    JEX_NULL,
    JEX_INDEX,
    JEX_STRING_INDEX,
    JEX_UNDERFLOW,
    JEX_OVERFLOW,
    JEX_NUMBER_FORMAT
};

typedef struct Jcatch {
    jmp_buf buf;
    struct Jcatch* prev;
    int type;
    char text[512];
} Jcatch;

extern Jcatch* jTop;

#define JTRY(c) (c).prev = jTop; jTop = &(c); if (setjmp((c).buf) == 0)
#define JEND(c) (jTop = (c).prev)

void jthrow(int type, const char* text);
void jthrowNull(void);
void jthrowIndex(int index);
void jthrowIO(const char* message);

#endif
