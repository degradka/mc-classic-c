// gui/chat_input_screen.h: chat entry, opened with T

#ifndef CHAT_INPUT_SCREEN_H
#define CHAT_INPUT_SCREEN_H

#include "screen.h"

typedef struct {
    Screen screen;
    char text[65];
    int  length;
} ChatInputScreen;

void ChatInputScreen_open(Font* font, int width, int height);

#endif
