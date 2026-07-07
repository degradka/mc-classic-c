// gui/pause_screen.c

#include "pause_screen.h"

extern void Minecraft_showNewLevelScreen(void);
extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_grabMouse(void);
extern bool Minecraft_isNetworked(void);

static void PauseScreen_render(Screen* self, int xm, int ym) {
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(self, "Game menu", self->width / 2, 40, 0xFFFFFF);
    Screen_render(self, xm, ym);
}

// save and load need a logged in user, there never is one
static void PauseScreen_buttonClicked(Screen* self, Button* b) {
    (void)self;
    if (b->id == 0) {
        Minecraft_showNewLevelScreen();
    }
    if (b->id == 3) {
        Minecraft_setScreen(NULL);
        Minecraft_grabMouse();
    }
}

void PauseScreen_init(PauseScreen* ps, Font* font, int width, int height) {
    Screen* s = &ps->screen;
    Screen_initBase(s, font, width, height);
    s->render = PauseScreen_render;
    s->buttonClicked = PauseScreen_buttonClicked;

    Screen_addButton(s, 0, width / 2 - 100, height / 3,      200, 20, "Generate new level...");
    Screen_addButton(s, 1, width / 2 - 100, height / 3 + 32, 200, 20, "Save level..");
    Screen_addButton(s, 2, width / 2 - 100, height / 3 + 64, 200, 20, "Load level..");
    Screen_addButton(s, 3, width / 2 - 100, height / 3 + 96, 200, 20, "Back to game");
    s->buttons[1].enabled = false;
    s->buttons[2].enabled = false;
    if (Minecraft_isNetworked()) {
        s->buttons[0].enabled = false;
        s->buttons[1].enabled = false;
        s->buttons[2].enabled = false;
    }
}
