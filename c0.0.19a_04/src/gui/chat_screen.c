// gui/chat_screen.c

#include "chat_screen.h"
#include <GLFW/glfw3.h>
#include <string.h>
#include <stdio.h>

extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_sendChat(const char* text);
extern void Minecraft_setKeyRepeat(bool on);
extern const char* Minecraft_getUserName(void);

static const char* ALLOWED =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ,.:-_'*!\\\"#%/()=+?[]{}<>@|$";

static void ChatScreen_render(Screen* self, int xm, int ym) {
    (void)xm; (void)ym;
    ChatScreen* s = (ChatScreen*)self;
    Screen_fill(2, self->height - 14, self->width - 2, self->height - 2, 0x80000000u);
    char line[80];
    // the cursor shows for 6 ticks, then hides for 6
    snprintf(line, sizeof line, "> %s%s", s->text, s->counter / 6 % 2 == 0 ? "_" : "");
    Screen_drawString(self, line, 4, self->height - 12, 0xE0E0E0u);
}

// glfw sends typed characters as their own events with key 0
static void ChatScreen_keyPressed(Screen* self, char c, int key) {
    ChatScreen* s = (ChatScreen*)self;
    if (key == GLFW_KEY_ESCAPE) {
        Minecraft_setScreen(NULL);
        return;
    }
    if (key == GLFW_KEY_ENTER) {
        // String.trim
        char* start = s->text;
        while (*start != '\0' && (unsigned char)*start <= ' ') start++;
        int len = (int)strlen(start);
        while (len > 0 && (unsigned char)start[len - 1] <= ' ') len--;
        start[len] = '\0';
        if (len > 0) Minecraft_sendChat(start);
        Minecraft_setScreen(NULL);
        return;
    }
    if (key == GLFW_KEY_BACKSPACE && s->length > 0) {
        s->text[--s->length] = '\0';
    }
    // the server puts "name: " in front, so the line leaves room for it
    if (c != '\0' && strchr(ALLOWED, c) && s->length < 64 - ((int)strlen(Minecraft_getUserName()) + 2)) {
        s->text[s->length++] = c;
        s->text[s->length] = '\0';
    }
}

static void ChatScreen_tick(Screen* self) {
    ((ChatScreen*)self)->counter++;
}

static void ChatScreen_opened(Screen* self) {
    (void)self;
    Minecraft_setKeyRepeat(true);
}

static void ChatScreen_removed(Screen* self) {
    (void)self;
    Minecraft_setKeyRepeat(false);
}

void ChatScreen_init(ChatScreen* s, Font* font, int width, int height) {
    Screen_initBase(&s->screen, font, width, height);
    s->screen.render = ChatScreen_render;
    s->screen.keyPressed = ChatScreen_keyPressed;
    s->screen.tick = ChatScreen_tick;
    s->screen.opened = ChatScreen_opened;
    s->screen.removed = ChatScreen_removed;
    s->text[0] = '\0';
    s->length = 0;
    s->counter = 0;
}
