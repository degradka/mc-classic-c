// gui/controls_screen.c

#include "controls_screen.h"
#include "../options.h"
#include <stdio.h>

extern void Minecraft_setScreen(Screen* screen);
extern Options* Minecraft_getOptions(void);

static Screen controlsScreen;
static Screen* parent;
static int selectedKey;

static void ControlsScreen_init(Screen* s) {
    char msg[64];
    for (int i = 0; i < OPTIONS_KEY_COUNT; ++i) {
        Options_getKeyBinding(Minecraft_getOptions(), i, msg, sizeof msg);
        Screen_addButton(s, i, s->width / 2 - 155 + i % 2 * 160, s->height / 6 + 24 * (i >> 1), 150, msg);
    }
    Screen_addButton(s, 200, s->width / 2 - 100, s->height / 6 + 168, 200, "Done");
}

static void ControlsScreen_buttonClicked(Screen* s, Button* b) {
    for (int i = 0; i < OPTIONS_KEY_COUNT; ++i) {
        Options_getKeyBinding(Minecraft_getOptions(), i, s->buttons[i].msg, sizeof s->buttons[i].msg);
    }
    if (b->id == 200) {
        Minecraft_setScreen(parent);
        return;
    }
    selectedKey = b->id;
    char msg[60];
    Options_getKeyBinding(Minecraft_getOptions(), b->id, msg, sizeof msg);
    snprintf(b->msg, sizeof b->msg, "> %s <", msg);
}

// escape gets bound too while a binding waits
static void ControlsScreen_keyPressed(Screen* s, char c, int key) {
    if (selectedKey >= 0) {
        Options_setKeyBinding(Minecraft_getOptions(), selectedKey, key);
        Button* b = &s->buttons[selectedKey];
        Options_getKeyBinding(Minecraft_getOptions(), selectedKey, b->msg, sizeof b->msg);
        selectedKey = -1;
        return;
    }
    Screen_keyPressed(s, c, key);
}

static void ControlsScreen_render(Screen* s, int xm, int ym) {
    Screen_fillGradient(0, 0, s->width, s->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(s->font, "Controls", s->width / 2, 20, 0xFFFFFF);
    Screen_render(s, xm, ym);
}

Screen* ControlsScreen_new(Screen* p) {
    Screen_construct(&controlsScreen);
    controlsScreen.init = ControlsScreen_init;
    controlsScreen.buttonClicked = ControlsScreen_buttonClicked;
    controlsScreen.keyPressed = ControlsScreen_keyPressed;
    controlsScreen.render = ControlsScreen_render;
    parent = p;
    selectedKey = -1;
    return &controlsScreen;
}
