// gui/screen.c

#include "screen.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// implemented in minecraft.c
extern void Minecraft_closeScreenAndGrabMouse(void);

static Tessellator TESSELLATOR;

// c0.0.13a_03 gives the base Screen a real keyPressed default: Escape closes
// the current screen and grabs the mouse again, matching the real source.
// c0.0.13a's base Screen.keyPressed was an empty stub, this is new here.
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

    // GL_ALPHA_TEST is left enabled globally (glAlphaFunc(GL_GREATER, 0.5))
    // for cutout textured world geometry, so low alpha translucent 2D fills
    // need it off or they get discarded outright below the 0.5 threshold.
    GLboolean wasAlpha = glIsEnabled(GL_ALPHA_TEST);
    if (wasAlpha) glDisable(GL_ALPHA_TEST);

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
    if (wasAlpha) glEnable(GL_ALPHA_TEST);
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

    GLboolean wasAlpha = glIsEnabled(GL_ALPHA_TEST);
    if (wasAlpha) glDisable(GL_ALPHA_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS); // Java: GL11.glBegin(7) == GL_QUADS, not GL_POLYGON
    glColor4f(r1, g1, b1, a1);
    glVertex2f((float)x1, (float)y0);
    glVertex2f((float)x0, (float)y0);
    glColor4f(r2, g2, b2, a2);
    glVertex2f((float)x0, (float)y1);
    glVertex2f((float)x1, (float)y1);
    glEnd();

    glDisable(GL_BLEND);
    if (wasAlpha) glEnable(GL_ALPHA_TEST);
}

void Screen_drawCenteredString(Screen* s, const char* str, int x, int y, unsigned int color) {
    int w = Font_width(s->font, str);
    Font_drawShadow(s->font, &TESSELLATOR, str, x - w / 2, y, (int)color);
}

void Screen_drawString(Screen* s, const char* str, int x, int y, unsigned int color) {
    Font_drawShadow(s->font, &TESSELLATOR, str, x, y, (int)color);
}

void Screen_renderButtons(Screen* self, Button* buttons, int count, int xMouse, int yMouse) {
    for (int i = 0; i < count; ++i) {
        Button* b = &buttons[i];
        if (!b->visible) continue;

        if (!b->enabled) {
            Screen_fill(b->x - 1, b->y - 1, b->x + b->w + 1, b->y + b->h + 1, 0xFF8080A0u);
            Screen_fill(b->x, b->y, b->x + b->w, b->y + b->h, 0xFF909090u);
            Screen_drawCenteredString(self, b->msg, b->x + b->w / 2, b->y + (b->h - 8) / 2, 0xFFA0A0A0u);
            continue;
        }

        Screen_fill(b->x - 1, b->y - 1, b->x + b->w + 1, b->y + b->h + 1, 0xFF000000u);
        int hovered = (xMouse >= b->x && yMouse >= b->y && xMouse < b->x + b->w && yMouse < b->y + b->h);
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

// the real click dispatch only checks visibility, not enabled, so a greyed
// out button still fires its click handler if something is drawn there
int Screen_buttonClickedAt(Button* buttons, int count, int x, int y) {
    for (int i = 0; i < count; ++i) {
        Button* b = &buttons[i];
        if (!b->visible) continue;
        if (x >= b->x && y >= b->y && x < b->x + b->w && y < b->y + b->h) return b->id;
    }
    return -1;
}
