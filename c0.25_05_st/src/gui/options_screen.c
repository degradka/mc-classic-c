// gui/options_screen.c

#include "options_screen.h"
#include "controls_screen.h"
#include "../options.h"

extern void Minecraft_setScreen(Screen* screen);
extern Options* Minecraft_getOptions(void);

static Screen optionsScreen;
static Screen* parent;

// the buttons are cleared, coming back from the controls would stack a second set that flips every toggle twice
static void OptionsScreen_init(Screen* s) {
    Screen_clearButtons(s);
    char msg[64];
    for (int i = 0; i < 5; ++i) {
        Options_getOption(Minecraft_getOptions(), i, msg, sizeof msg);
        Screen_addButton(s, i, s->width / 2 - 100, s->height / 6 + i * 24, 200, msg);
    }
    Screen_addButton(s, 10, s->width / 2 - 100, s->height / 6 + 120 + 12, 200, "Controls...");
    Screen_addButton(s, 20, s->width / 2 - 100, s->height / 6 + 168, 200, "Done");
}

static void OptionsScreen_buttonClicked(Screen* s, Button* b) {
    if (!b->enabled) return;
    if (b->id < 5) {
        Options_setOption(Minecraft_getOptions(), b->id, 1);
        Options_getOption(Minecraft_getOptions(), b->id, b->msg, sizeof b->msg);
    }
    if (b->id == 10) Minecraft_setScreen(ControlsScreen_new(s));
    if (b->id == 20) Minecraft_setScreen(parent);
}

static void OptionsScreen_render(Screen* s, int xm, int ym) {
    Screen_fillGradient(0, 0, s->width, s->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(s->font, "Options", s->width / 2, 20, 0xFFFFFF);
    Screen_render(s, xm, ym);
}

Screen* OptionsScreen_new(Screen* p) {
    Screen_construct(&optionsScreen);
    optionsScreen.init = OptionsScreen_init;
    optionsScreen.buttonClicked = OptionsScreen_buttonClicked;
    optionsScreen.render = OptionsScreen_render;
    parent = p;
    return &optionsScreen;
}
