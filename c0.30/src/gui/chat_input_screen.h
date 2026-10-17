// gui/chat_input_screen.h: chat entry, opened with T

#ifndef CHAT_INPUT_SCREEN_H
#define CHAT_INPUT_SCREEN_H

#include "screen.h"
#include <stdbool.h>

typedef struct {
    Screen screen;
    char text[65];
    int  length;
    int  counter;
} ChatInputScreen;

void ChatInputScreen_open(Font* font, int width, int height);

// the hud shows more chat lines while typing
bool ChatInputScreen_isThis(const Screen* screen);

#endif
