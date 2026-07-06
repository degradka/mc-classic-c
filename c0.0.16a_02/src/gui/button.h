// button.h: clickable rectangle with a label, used by menu screens

#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

typedef struct {
    int id;
    int x, y, w, h;
    char msg[64];
    bool enabled; // c0.0.16a_02: greyed out, bordered look, still visible
    bool visible; // c0.0.16a_02: not drawn or clickable at all
} Button;

static inline void Button_init(Button* b, int id, int x, int y, int w, int h, const char* msg) {
    b->id = id; b->x = x; b->y = y; b->w = w; b->h = h;
    b->enabled = true;
    b->visible = true;
    int i = 0;
    for (; msg[i] != '\0' && i < 63; ++i) b->msg[i] = msg[i];
    b->msg[i] = '\0';
}

#endif
