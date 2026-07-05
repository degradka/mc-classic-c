// gui/pause_screen.c

#include "pause_screen.h"

// implemented in minecraft.c
extern void Minecraft_generateNewLevel(void);
extern void Minecraft_closeScreenAndGrabMouse(void);

static void PauseScreen_render(Screen* self, int xm, int ym) {
    PauseScreen* ps = (PauseScreen*)self;

    // c0.0.13a_03 raises the top color's alpha from 0x20 to 0x60, a darker
    // overlay than c0.0.13a's, matching the same background used by the new
    // save/load screens
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);

    for (int i = 0; i < 4; ++i) {
        Button* b = &ps->buttons[i];
        Screen_fill(b->x - 1, b->y - 1, b->x + b->w + 1, b->y + b->h + 1, 0xFF000000u);

        int hovered = (xm >= b->x && ym >= b->y && xm < b->x + b->w && ym < b->y + b->h);
        if (hovered) {
            Screen_fill(b->x - 1, b->y - 1, b->x + b->w + 1, b->y + b->h + 1, 0xFFA0A0A0u);
            Screen_fill(b->x, b->y, b->x + b->w, b->y + b->h, 0xFF8080A0u);
            Screen_drawCenteredString(self, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0x00FFFFA0u);
        } else {
            Screen_fill(b->x, b->y, b->x + b->w, b->y + b->h, 0xFF707070u);
            Screen_drawCenteredString(self, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0x00E0E0E0u);
        }
    }
}

static void buttonClicked(const Button* b) {
    if (b->id == 0) {
        Minecraft_generateNewLevel();
        Minecraft_closeScreenAndGrabMouse();
    }
    if (b->id == 3) {
        Minecraft_closeScreenAndGrabMouse();
    }
    // ids 1 (Save) and 2 (Load) are unwired in this version, matching the real source
}

static void PauseScreen_mouseClicked(Screen* self, int x, int y, int button) {
    PauseScreen* ps = (PauseScreen*)self;
    if (button != 0) return;
    for (int i = 0; i < 4; ++i) {
        Button* b = &ps->buttons[i];
        if (x >= b->x && y >= b->y && x < b->x + b->w && y < b->y + b->h) {
            buttonClicked(b);
        }
    }
}

void PauseScreen_init(PauseScreen* ps, Font* font, int width, int height) {
    ps->screen.width  = width;
    ps->screen.height = height;
    ps->screen.font   = font;
    ps->screen.render = PauseScreen_render;
    ps->screen.init = NULL;
    ps->screen.tick = NULL;
    ps->screen.keyPressed = Screen_defaultKeyPressed;
    ps->screen.mouseClicked = PauseScreen_mouseClicked;
    ps->screen.destroy = NULL;

    Button_init(&ps->buttons[0], 0, width / 2 - 100, height / 3 + 0,  200, 20, "Generate new level");
    Button_init(&ps->buttons[1], 1, width / 2 - 100, height / 3 + 32, 200, 20, "Save level..");
    Button_init(&ps->buttons[2], 2, width / 2 - 100, height / 3 + 64, 200, 20, "Load level..");
    Button_init(&ps->buttons[3], 3, width / 2 - 100, height / 3 + 96, 200, 20, "Back to game");
}
