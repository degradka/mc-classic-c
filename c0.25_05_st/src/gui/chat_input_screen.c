// gui/chat_input_screen.c

#include "chat_input_screen.h"
#include "../timer.h"
#include <GLFW/glfw3.h>
#include <string.h>
#include <stdio.h>

extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_sendChat(const char* text);
extern const char* Minecraft_getUserName(void);
extern const char* Minecraft_getHoveredTabName(void);

static ChatInputScreen instance;

// the same allowed character set as the (unimplemented) save-name entry
// screen in the real source, both hand roll this independently rather than
// sharing a base text field. c0.0.18a_02 added \, @, |, $ on top of this.
// c0.0.19a_04 added ;
static const char* ALLOWED =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ,.:-_'*!\"#%/()=+?[]{}<>\\@|$;";

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

    // c0.0.17a: the budget shrinks by the local username's length (plus a
    // small constant), reserving room for the server's own "name: " prefix,
    // instead of always allowing a flat 64 chars for the message body alone.
    // Clamped to the text buffer's real capacity, matching the formula's
    // intent rather than a literal translation that could overflow it
    int cap = 64 - (int)strlen(Minecraft_getUserName()) + 2;
    if (cap > 64) cap = 64;
    if (cap < 0) cap = 0;

    if (eventKey == 0 && eventCharacter != 0 && s->length < cap && strchr(ALLOWED, eventCharacter)) {
        s->text[s->length++] = eventCharacter;
        s->text[s->length] = '\0';
    }
}

// c0.0.23a_01 (wiki confirmed, matches c/b.java): clicking a name hovered in
// the Tab overlay pastes it into the message being typed, with a separating
// space if the current text is non-empty and doesn't already end with one
static void ChatInputScreen_mouseClicked(Screen* self, int x, int y, int button) {
    (void)x; (void)y;
    ChatInputScreen* s = (ChatInputScreen*)self;
    if (button != 0) return;

    const char* hovered = Minecraft_getHoveredTabName();
    if (!hovered) return;

    int cap = 64 - (int)strlen(Minecraft_getUserName()) + 2;
    if (cap > 64) cap = 64;
    if (cap < 0) cap = 0;

    if (s->length > 0 && s->length < cap && s->text[s->length - 1] != ' ') {
        s->text[s->length++] = ' ';
    }

    for (int i = 0; hovered[i] != '\0' && s->length < cap; ++i) {
        s->text[s->length++] = hovered[i];
    }
    s->text[s->length] = '\0';
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
    s->screen.mouseClicked = ChatInputScreen_mouseClicked;
    s->screen.destroy = NULL;

    s->text[0] = '\0';
    s->length = 0;

    Minecraft_setScreen((Screen*)s);
}

bool ChatInputScreen_isThis(const Screen* screen) {
    return screen == (const Screen*)&instance;
}
