// gui/screen.c

#include "screen.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

static Tessellator TESSELLATOR;

void Screen_fill(int x0, int y0, int x1, int y1, unsigned int col) {
    float a = ((col >> 24) & 0xFF) / 255.0f;
    float r = ((col >> 16) & 0xFF) / 255.0f;
    float g = ((col >> 8)  & 0xFF) / 255.0f;
    float b = (col & 0xFF) / 255.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, a);

    Tessellator_begin(&TESSELLATOR);
    Tessellator_vertex(&TESSELLATOR, (float)x0, (float)y1, 0.0f);
    Tessellator_vertex(&TESSELLATOR, (float)x1, (float)y1, 0.0f);
    Tessellator_vertex(&TESSELLATOR, (float)x1, (float)y0, 0.0f);
    Tessellator_vertex(&TESSELLATOR, (float)x0, (float)y0, 0.0f);
    Tessellator_end(&TESSELLATOR);

    glDisable(GL_BLEND);
}

void Screen_fillGradient(int x0, int y0, int x1, int y1, unsigned int col1, unsigned int col2) {
    float a1 = ((col1 >> 24) & 0xFF) / 255.0f;
    float r1 = ((col1 >> 16) & 0xFF) / 255.0f;
    float g1 = ((col1 >> 8)  & 0xFF) / 255.0f;
    float b1 = (col1 & 0xFF) / 255.0f;

    float a2 = ((col2 >> 24) & 0xFF) / 255.0f;
    float r2 = ((col2 >> 16) & 0xFF) / 255.0f;
    float g2 = ((col2 >> 8)  & 0xFF) / 255.0f;
    float b2 = (col2 & 0xFF) / 255.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
    glColor4f(r1, g1, b1, a1);
    glVertex2f((float)x1, (float)y0);
    glVertex2f((float)x0, (float)y0);
    glColor4f(r2, g2, b2, a2);
    glVertex2f((float)x0, (float)y1);
    glVertex2f((float)x1, (float)y1);
    glEnd();

    glDisable(GL_BLEND);
}

void Screen_drawCenteredString(Screen* s, const char* str, int x, int y, unsigned int color) {
    int w = Font_width(s->font, str);
    Font_drawShadow(s->font, &TESSELLATOR, str, x - w / 2, y, (int)color);
}

void Screen_drawString(Screen* s, const char* str, int x, int y, unsigned int color) {
    Font_drawShadow(s->font, &TESSELLATOR, str, x, y, (int)color);
}

extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_grabMouse(void);

void Screen_initBase(Screen* s, Font* font, int width, int height) {
    s->width = width;
    s->height = height;
    s->font = font;
    s->buttonCount = 0;
    s->render = Screen_render;
    s->keyPressed = Screen_keyPressed;
    s->buttonClicked = NULL;
    s->mouseClicked = Screen_mouseClicked;
    s->tick = NULL;
    s->opened = NULL;
    s->removed = NULL;
}

void Screen_addButton(Screen* s, int id, int x, int y, int w, int h, const char* msg) {
    if (s->buttonCount < SCREEN_MAX_BUTTONS) Button_init(&s->buttons[s->buttonCount++], id, x, y, w, h, msg);
}

void Screen_mouseClicked(Screen* s, int x, int y, int button) {
    if (button != 0) return;
    for (int i = 0; i < s->buttonCount; i++) {
        Button* b = &s->buttons[i];
        if (!b->enabled || x < b->x || y < b->y || x >= b->x + b->w || y >= b->y + b->h) continue;
        if (s->buttonClicked) s->buttonClicked(s, b);
    }
}

void Screen_render(Screen* s, int xm, int ym) {
    for (int i = 0; i < s->buttonCount; ++i) {
        Button* b = &s->buttons[i];
        if (!b->visible) continue;
        if (!b->enabled) {
            Screen_fill(b->x - 1, b->y - 1, b->x + b->w + 1, b->y + b->h + 1, 0xFF8080A0u);
            Screen_fill(b->x, b->y, b->x + b->w, b->y + b->h, 0xFF909090u);
            Screen_drawCenteredString(s, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0xFFA0A0A0u);
            continue;
        }
        Screen_fill(b->x - 1, b->y - 1, b->x + b->w + 1, b->y + b->h + 1, 0xFF000000u);
        if (xm >= b->x && ym >= b->y && xm < b->x + b->w && ym < b->y + b->h) {
            Screen_fill(b->x - 1, b->y - 1, b->x + b->w + 1, b->y + b->h + 1, 0xFFA0A0A0u);
            Screen_fill(b->x, b->y, b->x + b->w, b->y + b->h, 0xFF8080A0u);
            Screen_drawCenteredString(s, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0xFFFFA0u);
            continue;
        }
        Screen_fill(b->x, b->y, b->x + b->w, b->y + b->h, 0xFF707070u);
        Screen_drawCenteredString(s, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0xE0E0E0u);
    }
}

void Screen_keyPressed(Screen* s, char eventCharacter, int eventKey) {
    (void)s; (void)eventCharacter;
    if (eventKey == GLFW_KEY_ESCAPE) {
        Minecraft_setScreen(NULL);
        Minecraft_grabMouse();
    }
}
