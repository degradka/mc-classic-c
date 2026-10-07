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

static const char* ALLOWED =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ,.:-_'*!\"#%/()=+?[]{}<>\\@|$;";

static void ChatInputScreen_render(Screen* self, int xm, int ym) {
    (void)xm; (void)ym;
    ChatInputScreen* s = (ChatInputScreen*)self;

    // floats over the live game view, no full screen dim like other menus
    Screen_fill(2, self->height - 14, self->width - 2, self->height - 2, 0x80000000u);

    char line[72];
    // the cursor shows for 6 ticks, then hides for 6
    int blinkOn = s->counter / 6 % 2 == 0;
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
        // String.trim
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

    // room is left for the server's "name: " prefix
    int cap = 64 - ((int)strlen(Minecraft_getUserName()) + 2);
    if (cap > 64) cap = 64;
    if (cap < 0) cap = 0;

    if (eventKey == 0 && eventCharacter != 0 && s->length < cap && strchr(ALLOWED, eventCharacter)) {
        s->text[s->length++] = eventCharacter;
        s->text[s->length] = '\0';
    }
}

// clicking a name in the Tab list pastes it into the message
static void ChatInputScreen_mouseClicked(Screen* self, int x, int y, int button) {
    (void)x; (void)y;
    ChatInputScreen* s = (ChatInputScreen*)self;
    if (button != 0) return;

    const char* hovered = Minecraft_getHoveredTabName();
    if (!hovered) return;

    int cap = 64 - ((int)strlen(Minecraft_getUserName()) + 2);
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

static void ChatInputScreen_tick(Screen* self) {
    ((ChatInputScreen*)self)->counter++;
}

void ChatInputScreen_open(Font* font, int width, int height) {
    ChatInputScreen* s = &instance;
    s->screen.width  = width;
    s->screen.height = height;
    s->screen.font   = font;
    s->screen.render = ChatInputScreen_render;
    s->screen.init = NULL;
    s->screen.tick = ChatInputScreen_tick;
    s->screen.keyPressed = ChatInputScreen_keyPressed;
    s->screen.mouseClicked = ChatInputScreen_mouseClicked;
    s->screen.destroy = NULL;

    s->text[0] = '\0';
    s->length = 0;
    s->counter = 0;

    Minecraft_setScreen((Screen*)s);
}

bool ChatInputScreen_isThis(const Screen* screen) {
    return screen == (const Screen*)&instance;
}
