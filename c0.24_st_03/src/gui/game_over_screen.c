// gui/game_over_screen.c

#include "game_over_screen.h"
#include "generate_level_screen.h"
#include <stdio.h>

extern void Minecraft_setScreen(Screen* screen);

static void GameOverScreen_render(Screen* self, int xm, int ym) {
    GameOverScreen* s = (GameOverScreen*)self;

    // exact real-source constants: 0x60500000 and (unsigned)-1602211792 == 0xA0803030
    Screen_fillGradient(0, 0, self->width, self->height, 0x60500000u, 0xA0803030u);

    // c0.24_st_03: title is drawn at 2x scale, so its logical position is
    // halved before the scale doubles it back on screen (matches the real
    // source's p.a(this.f, "Game over!", this.b/2/2, 30, ...) inside a
    // glScalef(2,2,2) block)
    glPushMatrix();
    glScalef(2.0f, 2.0f, 2.0f);
    Screen_drawCenteredString(self, "Game over!", self->width / 2 / 2, 30, 0xFFFFFFu);
    glPopMatrix();

    char scoreMsg[48];
    snprintf(scoreMsg, sizeof scoreMsg, "Score: &e%d", s->score);
    Screen_drawCenteredString(self, scoreMsg, self->width / 2, 100, 0xFFFFFFu);

    Screen_renderButtons(self, s->buttons, 1, xm, ym);
}

static void GameOverScreen_mouseClicked(Screen* self, int x, int y, int button) {
    GameOverScreen* s = (GameOverScreen*)self;
    if (button != 0) return;

    // real source's click handler also checks a button id 0, but no button
    // using id 0 is ever added here, only the id 1 "Generate new level..."
    // one, so that branch is unreachable and left out
    int id = Screen_buttonClickedAt(s->buttons, 1, x, y);
    if (id == 1) {
        GenerateNewLevelScreen_open(self);
    }
}

void GameOverScreen_init(GameOverScreen* s, Font* font, int width, int height, int score) {
    s->screen.width  = width;
    s->screen.height = height;
    s->screen.font   = font;
    s->screen.render = GameOverScreen_render;
    s->screen.init = NULL;
    s->screen.tick = NULL;
    s->screen.keyPressed = Screen_defaultKeyPressed;
    s->screen.mouseClicked = GameOverScreen_mouseClicked;
    s->screen.destroy = NULL;
    s->score = score;

    Button_init(&s->buttons[0], 1, width / 2 - 100, height / 4 + 72, 200, 20, "Generate new level...");
}
