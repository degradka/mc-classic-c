// gui/chat_input_screen.c

#include "chat_input_screen.h"
#include "../timer.h"
#include <GLFW/glfw3.h>
#include <string.h>
#include <stdio.h>

extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_sendChat(const char* text);

static ChatInputScreen instance;

// the same allowed character set as the (unimplemented) save-name entry
// screen in the real source, both hand roll this independently rather than
// sharing a base text field
static const char* ALLOWED =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ,.:-_'*!\"#%/()=+?[]{}<>";

static void ChatInputScreen_render(Screen* self, int xm, int ym) {
    (void)xm; (void)ym;
    ChatInputScreen* s = (ChatInputScreen*)self;

    // floats over the live game view, no full screen dim like other menus
    Screen_fill(2, self->height - 14, self->width - 2, self->height - 2, 0x80000000u);

    // real elapsed time, not a per call counter: this render call and the
    // screen's tick() both run once per rendered frame, not once per fixed
    // game tick, so a frame counted blink sped up or slowed down with the
    // framerate instead of keeping a steady ~300ms on/off rate
    char line[72];
    int blinkOn = (currentTimeMillis() / 300) % 2 == 0;
    snprintf(line, sizeof line, "> %s%s", s->text, blinkOn ? "_" : "");
    Screen_drawString(self, line, 4, self->height - 12, 0x00E0E0E0u);
}

static void ChatInputScreen_keyPressed(Screen* self, char eventCharacter, int eventKey) {
    ChatInputScreen* s = (ChatInputScreen*)self;

    if (eventKey == GLFW_KEY_ESCAPE) {
        Minecraft_setScreen(NULL);
        return;
    }
    if (eventKey == GLFW_KEY_ENTER) {
        // matches the real source's String.trim() before checking non-empty
        char* start = s->text;
        while (*start == ' ') start++;
        int len = (int)strlen(start);
        while (len > 0 && start[len - 1] == ' ') len--;
        start[len] = '\0';
        if (len > 0) Minecraft_sendChat(start);
        Minecraft_setScreen(NULL);
        return;
    }
    if (eventKey == GLFW_KEY_BACKSPACE) {
        if (s->length > 0) s->text[--s->length] = '\0';
        return;
    }

    if (eventKey == 0 && eventCharacter != 0 && s->length < 64 && strchr(ALLOWED, eventCharacter)) {
        s->text[s->length++] = eventCharacter;
        s->text[s->length] = '\0';
    }
}

void ChatInputScreen_open(Font* font, int width, int height) {
    ChatInputScreen* s = &instance;
    s->screen.width  = width;
    s->screen.height = height;
    s->screen.font   = font;
    s->screen.render = ChatInputScreen_render;
    s->screen.init = NULL;
    s->screen.tick = NULL;
    s->screen.keyPressed = ChatInputScreen_keyPressed;
    s->screen.mouseClicked = NULL;
    s->screen.destroy = NULL;

    s->text[0] = '\0';
    s->length = 0;

    Minecraft_setScreen((Screen*)s);
}
