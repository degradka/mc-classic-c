// gui/generate_level_screen.c

#include "generate_level_screen.h"

extern void Minecraft_generateNewLevelSized(int sizePreset);
extern void Minecraft_setScreen(Screen* screen);

static GenerateNewLevelScreen instance;

static void GenerateNewLevelScreen_render(Screen* self, int xm, int ym) {
    GenerateNewLevelScreen* s = (GenerateNewLevelScreen*)self;
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(self, "Generate new level", self->width / 2, 40, 0x00FFFFFFu);
    Screen_renderButtons(self, s->buttons, 4, xm, ym);
}

static void GenerateNewLevelScreen_mouseClicked(Screen* self, int x, int y, int button) {
    GenerateNewLevelScreen* s = (GenerateNewLevelScreen*)self;
    if (button != 0) return;

    int id = Screen_buttonClickedAt(s->buttons, 4, x, y);
    if (id == 3) {
        Minecraft_setScreen(s->previousScreen);
    } else if (id >= 0) {
        Minecraft_generateNewLevelSized(id);
        Minecraft_setScreen(NULL);
    }
}

void GenerateNewLevelScreen_open(Screen* previousScreen) {
    GenerateNewLevelScreen* s = &instance;
    s->screen.width  = previousScreen->width;
    s->screen.height = previousScreen->height;
    s->screen.font   = previousScreen->font;
    s->screen.render = GenerateNewLevelScreen_render;
    s->screen.init = NULL;
    s->screen.tick = NULL;
    s->screen.keyPressed = Screen_defaultKeyPressed;
    s->screen.mouseClicked = GenerateNewLevelScreen_mouseClicked;
    s->screen.destroy = NULL;
    s->previousScreen = previousScreen;

    // c0.24_st_03: layout changed from height/3+{0,32,64,96} to
    // height/4+{0,24,48,120} (confirmed against the real source, same kind
    // of relayout PauseScreen already got in c0.0.23a_01)
    int w = s->screen.width, h = s->screen.height;
    Button_init(&s->buttons[0], 0, w / 2 - 100, h / 4 + 0,   200, 20, "Small");
    Button_init(&s->buttons[1], 1, w / 2 - 100, h / 4 + 24,  200, 20, "Normal");
    Button_init(&s->buttons[2], 2, w / 2 - 100, h / 4 + 48,  200, 20, "Huge");
    Button_init(&s->buttons[3], 3, w / 2 - 100, h / 4 + 120, 200, 20, "Cancel");

    Minecraft_setScreen((Screen*)s);
}
