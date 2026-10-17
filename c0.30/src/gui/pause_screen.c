// gui/pause_screen.c

#include "pause_screen.h"
#include "generate_level_screen.h"
#include "options_screen.h"

extern void Minecraft_closeScreenAndGrabMouse(void);
extern bool Minecraft_isMultiplayer(void);

static void PauseScreen_render(Screen* self, int xm, int ym) {
    PauseScreen* ps = (PauseScreen*)self;

    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(self, "Game menu", self->width / 2, 40, 0xFFFFFFu);
    Screen_renderButtons(self, ps->buttons, 5, xm, ym);
}

static void PauseScreen_mouseClicked(Screen* self, int x, int y, int button) {
    PauseScreen* ps = (PauseScreen*)self;
    if (button != 0) return;

    int id = Screen_buttonClickedAt(ps->buttons, 5, x, y);
    if (id == 0) {
        OptionsScreen_open(self, ps->options);
    } else if (id == 1) {
        GenerateNewLevelScreen_open(self);
    } else if (id == 4) {
        Minecraft_closeScreenAndGrabMouse();
    }
}

void PauseScreen_init(PauseScreen* ps, Font* font, int width, int height, Options* options) {
    ps->screen.width  = width;
    ps->screen.height = height;
    ps->screen.font   = font;
    ps->screen.render = PauseScreen_render;
    ps->screen.init = NULL;
    ps->screen.tick = NULL;
    ps->screen.keyPressed = Screen_defaultKeyPressed;
    ps->screen.mouseClicked = PauseScreen_mouseClicked;
    ps->screen.destroy = NULL;
    ps->options = options;

    Button_init(&ps->buttons[0], 0, width / 2 - 100, height / 4 + 0,   200, 20, "Options...");
    Button_init(&ps->buttons[1], 1, width / 2 - 100, height / 4 + 24,  200, 20, "Generate new level...");
    Button_init(&ps->buttons[2], 2, width / 2 - 100, height / 4 + 48,  200, 20, "Save level..");
    Button_init(&ps->buttons[3], 3, width / 2 - 100, height / 4 + 72,  200, 20, "Load level..");
    Button_init(&ps->buttons[4], 4, width / 2 - 100, height / 4 + 120, 200, 20, "Back to game");

    // greyed out without a login session, there's never one here
    ps->buttons[2].enabled = false;
    ps->buttons[3].enabled = false;

    if (Minecraft_isMultiplayer()) ps->buttons[1].enabled = false;
}
