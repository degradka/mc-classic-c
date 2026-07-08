// gui/chat_screen.c

#include "chat_screen.h"
#include "../keys.h"
#include <string.h>
#include <stdio.h>

extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_sendChat(const char* text);
extern void Minecraft_setKeyRepeat(bool on);
extern const char* Minecraft_getUserName(void);
extern const char* Minecraft_hoveredUsername(void);

static const char* ALLOWED =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ,.:-_'*!\\\"#%/()=+?[]{}<>@|$;";

static Screen chatScreen;
static char typedMsg[128];
static int counter;

// the server puts "name: " in front, so the line leaves room for it
static int maxLength(void) {
    return 64 - ((int)strlen(Minecraft_getUserName()) + 2);
}

static void ChatScreen_init(Screen* s) {
    (void)s;
    Minecraft_setKeyRepeat(true);
}

static void ChatScreen_closeScreen(Screen* s) {
    (void)s;
    Minecraft_setKeyRepeat(false);
}

static void ChatScreen_tick(Screen* s) {
    (void)s;
    counter++;
}

static void ChatScreen_keyPressed(Screen* s, char c, int key) {
    (void)s;
    if (key == KEY_ESCAPE) {
        Minecraft_setScreen(NULL);
        return;
    }
    if (key == KEY_RETURN) {
        // String.trim
        char* start = typedMsg;
        while (*start != '\0' && (unsigned char)*start <= ' ') start++;
        int len = (int)strlen(start);
        while (len > 0 && (unsigned char)start[len - 1] <= ' ') len--;
        start[len] = '\0';
        if (len > 0) Minecraft_sendChat(start);
        Minecraft_setScreen(NULL);
        return;
    }
    int length = (int)strlen(typedMsg);
    if (key == KEY_BACK && length > 0) typedMsg[--length] = '\0';
    if (c != '\0' && strchr(ALLOWED, c) && length < maxLength()) {
        typedMsg[length] = c;
        typedMsg[length + 1] = '\0';
    }
}

static void ChatScreen_render(Screen* s, int xm, int ym) {
    (void)xm; (void)ym;
    Screen_fill(2, s->height - 14, s->width - 2, s->height - 2, 0x80000000u);
    char line[140];
    // the cursor shows for 6 ticks, then hides for 6
    snprintf(line, sizeof line, "> %s%s", typedMsg, counter / 6 % 2 == 0 ? "_" : "");
    Screen_drawString(s->font, line, 4, s->height - 12, 0xE0E0E0u);
}

// a left click on a name in the tab list adds it to the line
static void ChatScreen_mousePressed(Screen* s, int x, int y, int button) {
    (void)s; (void)x; (void)y;
    const char* name = Minecraft_hoveredUsername();
    if (button != 0 || !name) return;
    size_t len = strlen(typedMsg);
    if (len > 0 && typedMsg[len - 1] != ' ') strcat(typedMsg, " ");
    strncat(typedMsg, name, sizeof typedMsg - strlen(typedMsg) - 1);
    int max = maxLength();
    if ((int)strlen(typedMsg) > max) typedMsg[max] = '\0';
}

Screen* ChatScreen_new(void) {
    Screen_construct(&chatScreen);
    chatScreen.init = ChatScreen_init;
    chatScreen.closeScreen = ChatScreen_closeScreen;
    chatScreen.tick = ChatScreen_tick;
    chatScreen.keyPressed = ChatScreen_keyPressed;
    chatScreen.render = ChatScreen_render;
    chatScreen.mousePressed = ChatScreen_mousePressed;
    typedMsg[0] = '\0';
    counter = 0;
    return &chatScreen;
}

bool ChatScreen_isOpen(const Screen* s) {
    return s == &chatScreen;
}
