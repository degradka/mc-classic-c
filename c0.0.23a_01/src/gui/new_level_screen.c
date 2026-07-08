// gui/new_level_screen.c

#include "new_level_screen.h"

extern void Minecraft_generateNewLevel(int size);
extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_showPauseScreen(void);
extern void Minecraft_grabMouse(void);

static void NewLevelScreen_render(Screen* self, int xm, int ym) {
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(self, "Generate new level", self->width / 2, 40, 0xFFFFFF);
    Screen_render(self, xm, ym);
}

// cancel goes back to a new pause screen
static void NewLevelScreen_buttonClicked(Screen* self, Button* b) {
    (void)self;
    if (b->id == 3) {
        Minecraft_showPauseScreen();
        return;
    }
    Minecraft_generateNewLevel(b->id);
    Minecraft_setScreen(NULL);
    Minecraft_grabMouse();
}

void NewLevelScreen_init(NewLevelScreen* s, Font* font, int width, int height) {
    Screen* sc = &s->screen;
    Screen_initBase(sc, font, width, height);
    sc->render = NewLevelScreen_render;
    sc->buttonClicked = NewLevelScreen_buttonClicked;
    Screen_addButton(sc, 0, width / 2 - 100, height / 3,      200, 20, "Small");
    Screen_addButton(sc, 1, width / 2 - 100, height / 3 + 32, 200, 20, "Normal");
    Screen_addButton(sc, 2, width / 2 - 100, height / 3 + 64, 200, 20, "Huge");
    Screen_addButton(sc, 3, width / 2 - 100, height / 3 + 96, 200, 20, "Cancel");
}
