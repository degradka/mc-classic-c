// jexc.c

#include "jexc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Jcatch* jTop = NULL;

void jthrow(int type, const char* text) {
    Jcatch* c = jTop;
    if (!c) {
        fprintf(stderr, "Exception in thread \"Thread-1\" %s\n", text);
        exit(1);
    }
    jTop = c->prev;
    c->type = type;
    snprintf(c->text, sizeof c->text, "%s", text);
    longjmp(c->buf, 1);
}

void jthrowNull(void) {
    jthrow(JEX_NULL, "java.lang.NullPointerException");
}

void jthrowIndex(int index) {
    char text[64];
    snprintf(text, sizeof text, "java.lang.ArrayIndexOutOfBoundsException: %d", index);
    jthrow(JEX_INDEX, text);
}

void jthrowIO(const char* message) {
    char text[512];
    snprintf(text, sizeof text, "java.io.IOException: %s", message);
    jthrow(JEX_IO, text);
}
