// gui/screen.c

#include "screen.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

extern void Minecraft_closeScreenAndGrabMouse(void);
extern int  Minecraft_getGuiTexture(void);

static Tessellator TESSELLATOR;

void Screen_defaultKeyPressed(Screen* self, char eventCharacter, int eventKey) {
    (void)self; (void)eventCharacter;
    if (eventKey == GLFW_KEY_ESCAPE) {
        Minecraft_closeScreenAndGrabMouse();
    }
}

void Screen_fill(int x0, int y0, int x1, int y1, unsigned int col) {
    float a = ((col >> 24) & 0xFF) / 255.0f;
    float r = ((col >> 16) & 0xFF) / 255.0f;
    float g = ((col >> 8)  & 0xFF) / 255.0f;
    float b = (col & 0xFF) / 255.0f;

    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, a);

    Tessellator_begin(&TESSELLATOR);
    Tessellator_vertex(&TESSELLATOR, (float)x0, (float)y1, 0.0f);
    Tessellator_vertex(&TESSELLATOR, (float)x1, (float)y1, 0.0f);
    Tessellator_vertex(&TESSELLATOR, (float)x1, (float)y0, 0.0f);
    Tessellator_vertex(&TESSELLATOR, (float)x0, (float)y0, 0.0f);
    Tessellator_end(&TESSELLATOR);

    glEnable(GL_TEXTURE_2D);
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

    glDisable(GL_TEXTURE_2D);
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
    glEnable(GL_TEXTURE_2D);
}

void Screen_drawCenteredString(Screen* s, const char* str, int x, int y, unsigned int color) {
    int w = Font_width(s->font, str);
    Font_drawShadow(s->font, &TESSELLATOR, str, x - w / 2, y, (int)color);
}

void Screen_drawString(Screen* s, const char* str, int x, int y, unsigned int color) {
    Font_drawShadow(s->font, &TESSELLATOR, str, x, y, (int)color);
}

// u v w h are pixels in the 256x256 gui.png
static void drawButtonHalf(int textureId, int x, int y, int u, int v, int w, int h) {
    glBindTexture(GL_TEXTURE_2D, textureId);
    float u0 = u / 256.0f, v0 = v / 256.0f;
    float u1 = (u + w) / 256.0f, v1 = (v + h) / 256.0f;
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v1); glVertex2f((float)x,     (float)(y + h));
    glTexCoord2f(u1, v1); glVertex2f((float)(x + w), (float)(y + h));
    glTexCoord2f(u1, v0); glVertex2f((float)(x + w), (float)y);
    glTexCoord2f(u0, v0); glVertex2f((float)x,     (float)y);
    glEnd();
}

// two halves, the right one samples the right edge of the strip
// so narrow buttons don't stretch the texture
void Screen_renderButtons(Screen* self, Button* buttons, int count, int xMouse, int yMouse) {
    int guiTex = Minecraft_getGuiTexture();

    for (int i = 0; i < count; ++i) {
        Button* b = &buttons[i];
        if (!b->visible) continue;

        int hovered = (xMouse >= b->x && yMouse >= b->y && xMouse < b->x + b->w && yMouse < b->y + b->h);
        int state = !b->enabled ? 0 : (hovered ? 2 : 1); // 0=disabled, 1=normal, 2=hover

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        int halfW = b->w / 2;
        int v = 46 + state * 20;
        drawButtonHalf(guiTex, b->x, b->y, 0, v, halfW, b->h);
        drawButtonHalf(guiTex, b->x + halfW, b->y, 200 - halfW, v, halfW, b->h);

        if (!b->enabled) {
            Screen_drawCenteredString(self, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0xFFA0A0A0u);
        } else if (hovered) {
            Screen_drawCenteredString(self, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0x00FFFFA0u);
        } else {
            Screen_drawCenteredString(self, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0x00E0E0E0u);
        }
    }
}

// only enabled is checked here, not visible
int Screen_buttonClickedAt(Button* buttons, int count, int x, int y) {
    for (int i = 0; i < count; ++i) {
        Button* b = &buttons[i];
        if (!b->enabled) continue;
        if (x >= b->x && y >= b->y && x < b->x + b->w && y < b->y + b->h) return b->id;
    }
    return -1;
}
