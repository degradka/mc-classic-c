// gui/message_screen.c

#include "message_screen.h"
#include <stdio.h>

extern void Minecraft_setScreen(Screen* screen);

static MessageScreen instance;

static void MessageScreen_render(Screen* self, int xm, int ym) {
    (void)xm; (void)ym;
    MessageScreen* s = (MessageScreen*)self;
    Screen_fillGradient(0, 0, self->width, self->height, 0xFF402020u, 0xFF501010u);
    Screen_drawCenteredString(self, s->title, self->width / 2, 90, 0x00FFFFFFu);
    Screen_drawCenteredString(self, s->message, self->width / 2, 110, 0x00FFFFFFu);
}

// uncloseable: connect failure/disconnect is a dead end here, matching the
// real source's empty keyPressed override
static void MessageScreen_keyPressed(Screen* self, char eventCharacter, int eventKey) {
    (void)self; (void)eventCharacter; (void)eventKey;
}

void MessageScreen_open(Font* font, int width, int height, const char* title, const char* message) {
    MessageScreen* s = &instance;
    s->screen.width  = width;
    s->screen.height = height;
    s->screen.font   = font;
    s->screen.render = MessageScreen_render;
    s->screen.init = NULL;
    s->screen.tick = NULL;
    s->screen.keyPressed = MessageScreen_keyPressed;
    s->screen.mouseClicked = NULL;
    s->screen.destroy = NULL;

    snprintf(s->title, sizeof s->title, "%s", title);
    snprintf(s->message, sizeof s->message, "%s", message);

    Minecraft_setScreen((Screen*)s);
}
