// gui/error_screen.c

#include "error_screen.h"
#include <stdio.h>

static Screen errorScreen;
static char title[128];
static char message[128];

static void ErrorScreen_render(Screen* s, int xm, int ym) {
    Screen_fillGradient(0, 0, s->width, s->height, 0xFF402020u, 0xFF501010u);
    Screen_drawCenteredString(s->font, title, s->width / 2, 90, 0xFFFFFF);
    Screen_drawCenteredString(s->font, message, s->width / 2, 110, 0xFFFFFF);
    Screen_render(s, xm, ym);
}

// keyPressed is empty, escape doesn't close it
static void ErrorScreen_keyPressed(Screen* s, char c, int key) {
    (void)s; (void)c; (void)key;
}

extern Screen* Minecraft_getScreen(void);

// nothing replaces an open error screen, so a second one must leave the first alone
Screen* ErrorScreen_new(const char* t, const char* m) {
    if (ErrorScreen_isOpen(Minecraft_getScreen())) return &errorScreen;
    Screen_construct(&errorScreen);
    errorScreen.render = ErrorScreen_render;
    errorScreen.keyPressed = ErrorScreen_keyPressed;
    snprintf(title, sizeof title, "%s", t);
    snprintf(message, sizeof message, "%s", m);
    return &errorScreen;
}

bool ErrorScreen_isOpen(const Screen* s) {
    return s == &errorScreen;
}
