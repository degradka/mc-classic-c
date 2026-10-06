// gui/message_screen.h: two line message for connection errors and kicks

#ifndef MESSAGE_SCREEN_H
#define MESSAGE_SCREEN_H

#include "screen.h"

typedef struct {
    Screen screen;
    char title[80];
    char message[80];
} MessageScreen;

void MessageScreen_open(Font* font, int width, int height, const char* title, const char* message);

#endif
