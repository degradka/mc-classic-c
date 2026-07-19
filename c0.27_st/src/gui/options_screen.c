// gui/options_screen.c

#include "options_screen.h"
#include "controls_screen.h"

extern void Minecraft_setScreen(Screen* screen);

static OptionsScreen instance;

static void refreshToggleLabels(OptionsScreen* s) {
    for (int i = 0; i < OPTIONS_TOGGLE_COUNT; ++i) {
        Options_toggleLabel(s->options, i, s->buttons[i].msg, sizeof s->buttons[i].msg);
    }
}

static void OptionsScreen_render(Screen* self, int xm, int ym) {
    OptionsScreen* s = (OptionsScreen*)self;
    // matches the real source: the same generic menu backdrop every other
    // screen but the Select block screen uses, full screen, not a bounded box
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(self, "Options", self->width / 2, 20, 0xFFFFFFu);
    Screen_renderButtons(self, s->buttons, OPTIONS_TOGGLE_COUNT + 2, xm, ym);
}

static void OptionsScreen_mouseClicked(Screen* self, int x, int y, int button) {
    OptionsScreen* s = (OptionsScreen*)self;
    if (button != 0) return;

    int id = Screen_buttonClickedAt(s->buttons, OPTIONS_TOGGLE_COUNT + 2, x, y);
    if (id < 0) return;

    if (id < OPTIONS_TOGGLE_COUNT) {
        Options_toggleValue(s->options, id, 1);
        refreshToggleLabels(s);
    } else if (id == 10) {
        ControlsScreen_open(self, s->options);
    } else if (id == 20) {
        Minecraft_setScreen(s->previousScreen);
    }
}

void OptionsScreen_open(Screen* previousScreen, Options* options) {
    OptionsScreen* s = &instance;
    s->screen.width  = previousScreen->width;
    s->screen.height = previousScreen->height;
    s->screen.font   = previousScreen->font;
    s->screen.render = OptionsScreen_render;
    s->screen.init = NULL;
    s->screen.tick = NULL;
    s->screen.keyPressed = Screen_defaultKeyPressed;
    s->screen.mouseClicked = OptionsScreen_mouseClicked;
    s->screen.destroy = NULL;
    s->previousScreen = previousScreen;
    s->options = options;

    // matches c/e.java's own a() exactly: a 2 column grid, 150x20 buttons,
    // 160px column spacing, 24px row spacing (was wrongly a single 200-wide
    // column before, a pre-existing layout bug unrelated to which toggles
    // exist, only now surfaced by actually reading the real layout code)
    int w = s->screen.width, h = s->screen.height;
    for (int i = 0; i < OPTIONS_TOGGLE_COUNT; ++i) {
        Button_init(&s->buttons[i], i, w / 2 - 155 + (i % 2) * 160, h / 6 + 24 * (i / 2), 150, 20, "");
    }
    refreshToggleLabels(s);
    Button_init(&s->buttons[OPTIONS_TOGGLE_COUNT],     10, w / 2 - 100, h / 6 + 132, 200, 20, "Controls...");
    Button_init(&s->buttons[OPTIONS_TOGGLE_COUNT + 1], 20, w / 2 - 100, h / 6 + 168, 200, 20, "Done");

    Minecraft_setScreen((Screen*)s);
}
