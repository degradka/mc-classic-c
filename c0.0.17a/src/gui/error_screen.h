// gui/error_screen.h: a title and a message that can't be closed

#ifndef ERROR_SCREEN_H
#define ERROR_SCREEN_H

#include "screen.h"

typedef struct {
    Screen screen; // must be first so ErrorScreen* can be used as Screen*
    char title[128];
    char message[128];
    // the message can be null, drawing it throws and the frame never shows
    bool nullMessage;
} ErrorScreen;

void ErrorScreen_init(ErrorScreen* s, Font* font, int width, int height, const char* title, const char* message);

#endif
