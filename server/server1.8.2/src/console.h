// console.h: the thread that reads commands from stdin, the main loop runs them

#ifndef CONSOLE_H
#define CONSOLE_H

#include "jstring.h"
#include <stdbool.h>

void Console_start(void);
// the next line typed, false when there's none waiting
bool Console_poll(JStr* line);

#endif
