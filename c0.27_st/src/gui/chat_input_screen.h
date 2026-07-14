// chat_input_screen.h: chat message text entry, opened with T (c0.0.16a_02)

#ifndef CHAT_INPUT_SCREEN_H
#define CHAT_INPUT_SCREEN_H

#include "screen.h"
#include <stdbool.h>

typedef struct {
    Screen screen;
    char text[65];
    int  length;
} ChatInputScreen;

// initializes the single static instance and makes it the active screen
void ChatInputScreen_open(Font* font, int width, int height);

// true if screen is the chat input screen's own singleton instance. Used by
// the HUD to show the wider chat scrollback while actively typing
bool ChatInputScreen_isThis(const Screen* screen);

#endif
