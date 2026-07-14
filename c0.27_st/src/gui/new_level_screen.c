// gui/new_level_screen.c

#include "new_level_screen.h"

extern void Minecraft_generateNewLevel(int size);
extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_grabMouse(void);

static Screen newLevelScreen;
static Screen* parent;

static void NewLevelScreen_init(Screen* s) {
    Screen_clearButtons(s);
    Screen_addButton(s, 0, s->width / 2 - 100, s->height / 3, 200, "Small");
    Screen_addButton(s, 1, s->width / 2 - 100, s->height / 3 + 32, 200, "Normal");
    Screen_addButton(s, 2, s->width / 2 - 100, s->height / 3 + 64, 200, "Huge");
    Screen_addButton(s, 3, s->width / 2 - 100, s->height / 3 + 96, 200, "Cancel");
}

static void NewLevelScreen_buttonClicked(Screen* s, Button* b) {
    (void)s;
    if (b->id == 3) {
        Minecraft_setScreen(parent);
        return;
    }
    Minecraft_generateNewLevel(b->id);
    Minecraft_setScreen(NULL);
    Minecraft_grabMouse();
}

static void NewLevelScreen_render(Screen* s, int xm, int ym) {
    Screen_fillGradient(0, 0, s->width, s->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(s->font, "Generate new level", s->width / 2, 40, 0xFFFFFF);
    Screen_render(s, xm, ym);
}

Screen* NewLevelScreen_new(Screen* p) {
    Screen_construct(&newLevelScreen);
    newLevelScreen.init = NewLevelScreen_init;
    newLevelScreen.buttonClicked = NewLevelScreen_buttonClicked;
    newLevelScreen.render = NewLevelScreen_render;
    parent = p;
    return &newLevelScreen;
}
