// gui/error_screen.c

#include "error_screen.h"
#include <stdio.h>

static void ErrorScreen_render(Screen* self, int xm, int ym) {
    ErrorScreen* s = (ErrorScreen*)self;
    Screen_fillGradient(0, 0, self->width, self->height, 0xFF402020u, 0xFF501010u);
    Screen_drawCenteredString(self, s->title, self->width / 2, 90, 0xFFFFFF);
    if (s->nullMessage) return;
    Screen_drawCenteredString(self, s->message, self->width / 2, 110, 0xFFFFFF);
    Screen_render(self, xm, ym);
}

// keyPressed is empty, escape doesn't close it
static void ErrorScreen_keyPressed(Screen* self, char c, int key) {
    (void)self; (void)c; (void)key;
}

void ErrorScreen_init(ErrorScreen* s, Font* font, int width, int height, const char* title, const char* message) {
    Screen_initBase(&s->screen, font, width, height);
    s->screen.render = ErrorScreen_render;
    s->screen.keyPressed = ErrorScreen_keyPressed;
    snprintf(s->title, sizeof s->title, "%s", title);
    s->nullMessage = message == NULL;
    snprintf(s->message, sizeof s->message, "%s", message ? message : "");
}
