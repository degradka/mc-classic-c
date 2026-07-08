// gui/screen.c

#include "screen.h"
#include "../keys.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <stdlib.h>

static Tessellator TESSELLATOR;

extern void Minecraft_setScreen(Screen* screen);
extern void Minecraft_grabMouse(void);
extern int Minecraft_getGuiTexture(void);

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

void Screen_drawCenteredString(Font* font, const char* str, int x, int y, unsigned int color) {
    Font_drawShadow(font, &TESSELLATOR, str, x - Font_width(font, str) / 2, y, (int)color);
}

void Screen_drawString(Font* font, const char* str, int x, int y, unsigned int color) {
    Font_drawShadow(font, &TESSELLATOR, str, x, y, (int)color);
}

void Screen_blit(int x, int y, int u, int v, int w, int h, float zLevel) {
    float us = 0.00390625f;
    float vs = 0.00390625f;
    Tessellator_begin(&TESSELLATOR);
    Tessellator_vertexUV(&TESSELLATOR, (float)x, (float)(y + h), zLevel, (float)u * us, (float)(v + h) * vs);
    Tessellator_vertexUV(&TESSELLATOR, (float)(x + w), (float)(y + h), zLevel, (float)(u + w) * us, (float)(v + h) * vs);
    Tessellator_vertexUV(&TESSELLATOR, (float)(x + w), (float)y, zLevel, (float)(u + w) * us, (float)v * vs);
    Tessellator_vertexUV(&TESSELLATOR, (float)x, (float)y, zLevel, (float)u * us, (float)v * vs);
    Tessellator_end(&TESSELLATOR);
}

static void noInit(Screen* s) { (void)s; }

void Screen_construct(Screen* s) {
    s->buttonCount = 0;
    s->allowUserInput = false;
    s->render = Screen_render;
    s->keyPressed = Screen_keyPressed;
    s->mousePressed = Screen_mousePressed;
    s->buttonClicked = NULL;
    s->init = noInit;
    s->tick = NULL;
    s->closeScreen = NULL;
}

void Screen_setup(Screen* s, Font* font, int width, int height) {
    s->font = font;
    s->width = width;
    s->height = height;
    s->init(s);
}

Button* Screen_addButton(Screen* s, int id, int x, int y, int w, const char* msg) {
    if (s->buttonCount == s->buttonCapacity) {
        s->buttonCapacity = s->buttonCapacity ? s->buttonCapacity * 2 : 16;
        s->buttons = (Button*)realloc(s->buttons, (size_t)s->buttonCapacity * sizeof(Button));
    }
    Button* b = &s->buttons[s->buttonCount++];
    Button_init(b, id, x, y, w, msg);
    return b;
}

void Screen_clearButtons(Screen* s) {
    s->buttonCount = 0;
}

// a click that changes the screen still goes on to the buttons after it
void Screen_mousePressed(Screen* s, int x, int y, int button) {
    if (button != 0) return;
    for (int i = 0; i < s->buttonCount; i++) {
        Button* b = &s->buttons[i];
        if (!(b->enabled && x >= b->x && y >= b->y && x < b->x + b->w && y < b->y + b->h)) continue;
        if (s->buttonClicked) s->buttonClicked(s, b);
    }
}

// the left half and the right end of the 200 wide button in gui.png, the second row when hovered
void Screen_render(Screen* s, int xm, int ym) {
    for (int i = 0; i < s->buttonCount; ++i) {
        Button* b = &s->buttons[i];
        if (!b->visible) continue;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, Minecraft_getGuiTexture());
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        int state = 1;
        bool hover = xm >= b->x && ym >= b->y && xm < b->x + b->w && ym < b->y + b->h;
        if (!b->enabled) state = 0;
        else if (hover) state = 2;
        Screen_blit(b->x, b->y, 0, 46 + state * 20, b->w / 2, b->h, 0.0f);
        Screen_blit(b->x + b->w / 2, b->y, 200 - b->w / 2, 46 + state * 20, b->w / 2, b->h, 0.0f);
        if (!b->enabled) {
            Screen_drawCenteredString(s->font, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0xFFA0A0A0u);
        } else if (hover) {
            Screen_drawCenteredString(s->font, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0xFFFFA0u);
        } else {
            Screen_drawCenteredString(s->font, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0xE0E0E0u);
        }
    }
}

void Screen_keyPressed(Screen* s, char eventCharacter, int eventKey) {
    (void)s; (void)eventCharacter;
    if (eventKey == KEY_ESCAPE) {
        Minecraft_setScreen(NULL);
        Minecraft_grabMouse();
    }
}
