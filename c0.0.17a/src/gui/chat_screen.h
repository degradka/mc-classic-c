// gui/chat_screen.h: the chat line at the bottom of the screen

#ifndef CHAT_SCREEN_H
#define CHAT_SCREEN_H

#include "screen.h"

typedef struct {
    Screen screen; // must be first so ChatScreen* can be used as Screen*
    char text[72];
    int length;
    int counter;
} ChatScreen;

void ChatScreen_init(ChatScreen* s, Font* font, int width, int height);

#endif
