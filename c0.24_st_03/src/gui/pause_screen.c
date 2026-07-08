// gui/pause_screen.c

#include "pause_screen.h"
#include "generate_level_screen.h"
#include "options_screen.h"

// implemented in minecraft.c
extern void Minecraft_closeScreenAndGrabMouse(void);
extern bool Minecraft_isConnected(void);

static void PauseScreen_render(Screen* self, int xm, int ym) {
    PauseScreen* ps = (PauseScreen*)self;

    // c0.0.13a_03 raises the top color's alpha from 0x20 to 0x60, a darker
    // overlay than c0.0.13a's, matching the same background used by the new
    // save/load screens
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_renderButtons(self, ps->buttons, 5, xm, ym);
}

static void PauseScreen_mouseClicked(Screen* self, int x, int y, int button) {
    PauseScreen* ps = (PauseScreen*)self;
    if (button != 0) return;

    int id = Screen_buttonClickedAt(ps->buttons, 5, x, y);
    if (id == 0) {
        OptionsScreen_open(self, ps->options);
    } else if (id == 1) {
        // c0.0.16a_02 routes this through a Small/Normal/Huge size picker
        // instead of regenerating immediately
        GenerateNewLevelScreen_open(self);
    } else if (id == 4) {
        Minecraft_closeScreenAndGrabMouse();
    }
    // ids 2 (Save) and 3 (Load) are unwired: the real screens they'd open
    // talk to a networked save/load backend that's permanently dead
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

    // c0.0.23a_01: real source's own layout/ids, "Options..." added first and
    // everything else renumbered, y positions changed from height/3+i*32 to
    // height/4+{0,24,48,72,120} (the gap before "Back to game" is real too)
    Button_init(&ps->buttons[0], 0, width / 2 - 100, height / 4 + 0,   200, 20, "Options...");
    Button_init(&ps->buttons[1], 1, width / 2 - 100, height / 4 + 24,  200, 20, "Generate new level...");
    Button_init(&ps->buttons[2], 2, width / 2 - 100, height / 4 + 48,  200, 20, "Save level..");
    Button_init(&ps->buttons[3], 3, width / 2 - 100, height / 4 + 72,  200, 20, "Load level..");
    Button_init(&ps->buttons[4], 4, width / 2 - 100, height / 4 + 120, 200, 20, "Back to game");

    // c0.0.16a_02 greys these out unless a login session exists. Our desktop
    // build never has the applet supplied session the real client checks
    // for, so they stay permanently disabled here, on top of already being
    // unwired to a dead backend
    ps->buttons[2].enabled = false;
    ps->buttons[3].enabled = false;

    // c0.0.17a: Generate new level is now also disabled while connected to a
    // server, leaving only Options and Back to game clickable in that case
    // (Save/Load were already always disabled above, in this port)
    if (Minecraft_isConnected()) ps->buttons[1].enabled = false;
}
