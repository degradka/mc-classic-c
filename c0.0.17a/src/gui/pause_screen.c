// gui/pause_screen.c

#include "pause_screen.h"
#include "generate_level_screen.h"

// implemented in minecraft.c
extern void Minecraft_closeScreenAndGrabMouse(void);
extern bool Minecraft_isConnected(void);

static void PauseScreen_render(Screen* self, int xm, int ym) {
    PauseScreen* ps = (PauseScreen*)self;

    // c0.0.13a_03 raises the top color's alpha from 0x20 to 0x60, a darker
    // overlay than c0.0.13a's, matching the same background used by the new
    // save/load screens
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_renderButtons(self, ps->buttons, 4, xm, ym);
}

static void PauseScreen_mouseClicked(Screen* self, int x, int y, int button) {
    PauseScreen* ps = (PauseScreen*)self;
    if (button != 0) return;

    int id = Screen_buttonClickedAt(ps->buttons, 4, x, y);
    if (id == 0) {
        // c0.0.16a_02 routes this through a Small/Normal/Huge size picker
        // instead of regenerating immediately
        GenerateNewLevelScreen_open(self);
    } else if (id == 3) {
        Minecraft_closeScreenAndGrabMouse();
    }
    // ids 1 (Save) and 2 (Load) are unwired: the real screens they'd open
    // talk to a networked save/load backend that's permanently dead
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

    Button_init(&ps->buttons[0], 0, width / 2 - 100, height / 3 + 0,  200, 20, "Generate new level...");
    Button_init(&ps->buttons[1], 1, width / 2 - 100, height / 3 + 32, 200, 20, "Save level..");
    Button_init(&ps->buttons[2], 2, width / 2 - 100, height / 3 + 64, 200, 20, "Load level..");
    Button_init(&ps->buttons[3], 3, width / 2 - 100, height / 3 + 96, 200, 20, "Back to game");

    // c0.0.16a_02 greys these out unless a login session exists. Our desktop
    // build never has the applet supplied session the real client checks
    // for, so they stay permanently disabled here, on top of already being
    // unwired to a dead backend
    ps->buttons[1].enabled = false;
    ps->buttons[2].enabled = false;

    // c0.0.17a: Generate new level is now also disabled while connected to a
    // server, leaving only Back to game clickable in that case (Save/Load
    // were already always disabled above, in this port)
    if (Minecraft_isConnected()) ps->buttons[0].enabled = false;
}
