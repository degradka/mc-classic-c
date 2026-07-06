// message_screen.h: generic 2 line message, connect failure/disconnect (c0.0.16a_02)

#ifndef MESSAGE_SCREEN_H
#define MESSAGE_SCREEN_H

#include "screen.h"

typedef struct {
    Screen screen;
    char title[80];
    char message[80];
} MessageScreen;

// initializes the single static instance and makes it the active screen.
// uncloseable: Escape does nothing, matching the real source
void MessageScreen_open(Font* font, int width, int height, const char* title, const char* message);

#endif
