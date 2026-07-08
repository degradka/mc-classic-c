// gui/button.h

#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

typedef struct {
    int id;
    int x, y, w, h;
    char msg[64];
    bool enabled;
    bool visible;
} Button;

// the height it's given is ignored, every button is 20 high
static inline void Button_init(Button* b, int id, int x, int y, int w, const char* msg) {
    b->id = id; b->x = x; b->y = y; b->w = w; b->h = 20;
    int i = 0;
    for (; msg[i] != '\0' && i < 63; ++i) b->msg[i] = msg[i];
    b->msg[i] = '\0';
    b->enabled = true;
    b->visible = true;
}

#endif
