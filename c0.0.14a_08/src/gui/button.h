// button.h: clickable rectangle with a label, used by menu screens

#ifndef BUTTON_H
#define BUTTON_H

typedef struct {
    int id;
    int x, y, w, h;
    char msg[64];
} Button;

static inline void Button_init(Button* b, int id, int x, int y, int w, int h, const char* msg) {
    b->id = id; b->x = x; b->y = y; b->w = w; b->h = h;
    int i = 0;
    for (; msg[i] != '\0' && i < 63; ++i) b->msg[i] = msg[i];
    b->msg[i] = '\0';
}

#endif
