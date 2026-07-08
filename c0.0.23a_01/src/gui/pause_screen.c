// gui/pause_screen.c

#include "pause_screen.h"
#include "options_screen.h"
#include "new_level_screen.h"

extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_grabMouse(void);
extern bool Minecraft_isNetworked(void);
extern bool Minecraft_hasUser(void);

static Screen pauseScreen;

static void PauseScreen_init(Screen* s) {
    Screen_clearButtons(s);
    Screen_addButton(s, 0, s->width / 2 - 100, s->height / 4, 200, "Options...");
    Screen_addButton(s, 1, s->width / 2 - 100, s->height / 4 + 24, 200, "Generate new level...");
    Screen_addButton(s, 2, s->width / 2 - 100, s->height / 4 + 48, 200, "Save level..");
    Screen_addButton(s, 3, s->width / 2 - 100, s->height / 4 + 72, 200, "Load level..");
    Screen_addButton(s, 4, s->width / 2 - 100, s->height / 4 + 120, 200, "Back to game");
    if (!Minecraft_hasUser()) {
        s->buttons[2].enabled = false;
        s->buttons[3].enabled = false;
    }
    if (Minecraft_isNetworked()) {
        s->buttons[1].enabled = false;
        s->buttons[2].enabled = false;
        s->buttons[3].enabled = false;
    }
}

// save and load go to minecraft.net, they're never offered
static void PauseScreen_buttonClicked(Screen* s, Button* b) {
    if (b->id == 0) Minecraft_setScreen(OptionsScreen_new(s));
    if (b->id == 1) Minecraft_setScreen(NewLevelScreen_new(s));
    if (b->id == 4) {
        Minecraft_setScreen(NULL);
        Minecraft_grabMouse();
    }
}

static void PauseScreen_render(Screen* s, int xm, int ym) {
    Screen_fillGradient(0, 0, s->width, s->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(s->font, "Game menu", s->width / 2, 40, 0xFFFFFF);
    Screen_render(s, xm, ym);
}

Screen* PauseScreen_new(void) {
    Screen_construct(&pauseScreen);
    pauseScreen.init = PauseScreen_init;
    pauseScreen.buttonClicked = PauseScreen_buttonClicked;
    pauseScreen.render = PauseScreen_render;
    return &pauseScreen;
}

bool PauseScreen_isOpen(const Screen* s) {
    return s == &pauseScreen;
}
