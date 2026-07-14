// gui/controls_screen.c

#include "controls_screen.h"
#include <stdio.h>

extern void Minecraft_setScreen(Screen* screen);

static ControlsScreen instance;

static void refreshBindingLabels(ControlsScreen* s) {
    for (int i = 0; i < OPTIONS_KEY_COUNT; ++i) {
        Options_keyBindingLabel(s->options, i, s->buttons[i].msg, sizeof s->buttons[i].msg);
    }
}

static void ControlsScreen_render(Screen* self, int xm, int ym) {
    ControlsScreen* s = (ControlsScreen*)self;
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(self, "Controls", self->width / 2, 20, 0xFFFFFFu);
    Screen_renderButtons(self, s->buttons, OPTIONS_KEY_COUNT + 1, xm, ym);
}

static void ControlsScreen_mouseClicked(Screen* self, int x, int y, int button) {
    ControlsScreen* s = (ControlsScreen*)self;
    if (button != 0) return;

    int id = Screen_buttonClickedAt(s->buttons, OPTIONS_KEY_COUNT + 1, x, y);
    if (id < 0) return;

    // matches the real source: clicking any binding first restores every
    // label to its normal (non capturing) text, undoing a stale "> ... <"
    // prompt left over from a previously started, never finished capture
    refreshBindingLabels(s);

    if (id == 200) {
        Minecraft_setScreen(s->previousScreen);
        return;
    }

    s->capturingIndex = id;
    char label[72]; // headroom over msg's 64: "> " + up to 63 chars + " <" + NUL
    snprintf(label, sizeof label, "> %s <", s->buttons[id].msg);
    // precision caps the copy at msg's own 63 usable chars, so the compiler
    // can see this can never overflow regardless of how long label got above
    snprintf(s->buttons[id].msg, sizeof s->buttons[id].msg, "%.63s", label);
}

static void ControlsScreen_keyPressed(Screen* self, char eventCharacter, int eventKey) {
    (void)eventCharacter;
    ControlsScreen* s = (ControlsScreen*)self;

    // eventKey 0 means this call came from the char callback (a typed
    // character, not a raw key), which fires right after the real key
    // callback already consumed the capture below, so it's ignored here
    if (s->capturingIndex >= 0 && eventKey != 0) {
        Options_setKeyBinding(s->options, s->capturingIndex, eventKey);
        Options_keyBindingLabel(s->options, s->capturingIndex,
                                 s->buttons[s->capturingIndex].msg, sizeof s->buttons[s->capturingIndex].msg);
        s->capturingIndex = -1;
        return;
    }
    Screen_defaultKeyPressed(self, eventCharacter, eventKey);
}

void ControlsScreen_open(Screen* previousScreen, Options* options) {
    ControlsScreen* s = &instance;
    s->screen.width  = previousScreen->width;
    s->screen.height = previousScreen->height;
    s->screen.font   = previousScreen->font;
    s->screen.render = ControlsScreen_render;
    s->screen.init = NULL;
    s->screen.tick = NULL;
    s->screen.keyPressed = ControlsScreen_keyPressed;
    s->screen.mouseClicked = ControlsScreen_mouseClicked;
    s->screen.destroy = NULL;
    s->previousScreen = previousScreen;
    s->options = options;
    s->capturingIndex = -1;

    int w = s->screen.width, h = s->screen.height;
    for (int i = 0; i < OPTIONS_KEY_COUNT; ++i) {
        int bx = w / 2 - 155 + (i % 2) * 160;
        int by = h / 6 + 24 * (i / 2);
        Button_init(&s->buttons[i], i, bx, by, 150, 20, "");
    }
    refreshBindingLabels(s);
    Button_init(&s->buttons[OPTIONS_KEY_COUNT], 200, w / 2 - 100, h / 6 + 168, 200, 20, "Done");

    Minecraft_setScreen((Screen*)s);
}
