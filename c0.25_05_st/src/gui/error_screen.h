// gui/error_screen.h: a title and a message that can't be closed

#ifndef ERROR_SCREEN_H
#define ERROR_SCREEN_H

#include "screen.h"

Screen* ErrorScreen_new(const char* title, const char* message);
bool ErrorScreen_isOpen(const Screen* s);

#endif
