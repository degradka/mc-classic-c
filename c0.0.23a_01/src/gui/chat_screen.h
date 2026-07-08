// gui/chat_screen.h: the chat line at the bottom of the screen

#ifndef CHAT_SCREEN_H
#define CHAT_SCREEN_H

#include "screen.h"

Screen* ChatScreen_new(void);
bool ChatScreen_isOpen(const Screen* s);

#endif
