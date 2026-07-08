// gui/block_select_screen.c

#include "block_select_screen.h"
#include "../level/tile/tile.h"
#include <GL/glew.h>

extern void Minecraft_setScreen(Screen* screen);
extern const int Minecraft_blockList[];
extern const int Minecraft_blockListCount;
extern void Minecraft_inventoryReplace(int id);
extern const Level* Minecraft_getLevel(void);
extern int Minecraft_getTerrainTexture(void);

static Tessellator sTess;

// 8 to a row, 24 apart
static int blockAt(const Screen* s, int x, int y) {
    for (int i = 0; i < Minecraft_blockListCount; ++i) {
        int bx = s->width / 2 + i % 8 * 24 - 96;
        int by = s->height / 2 + i / 8 * 24 - 48;
        if (x < bx || x > bx + 24 || y < by - 12 || y > by + 12) continue;
        return i;
    }
    return -1;
}

static void BlockSelectScreen_render(Screen* self, int xm, int ym) {
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    Screen_drawCenteredString(self, "Select block", self->width / 2, 40, 0xFFFFFF);
    int hover = blockAt(self, xm, ym);
    glBindTexture(GL_TEXTURE_2D, Minecraft_getTerrainTexture());
    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < Minecraft_blockListCount; ++i) {
        const Tile* tile = gTiles[Minecraft_blockList[i]];
        glPushMatrix();
        int bx = self->width / 2 + i % 8 * 24 - 96;
        int by = self->height / 2 + i / 8 * 24 - 48;
        glTranslatef((float)bx, (float)by, 0.0f);
        glScalef(10.0f, 10.0f, 10.0f);
        glTranslatef(1.0f, 0.5f, 8.0f);
        glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
        if (hover == i) glScalef(1.6f, 1.6f, 1.6f);
        glTranslatef(-1.5f, 0.5f, 0.5f);
        glScalef(-1.0f, -1.0f, -1.0f);
        Tessellator_begin(&sTess);
        tile->render(tile, &sTess, Minecraft_getLevel(), 0, -2, 0, 0);
        Tessellator_end(&sTess);
        glPopMatrix();
    }
    glDisable(GL_TEXTURE_2D);
}

// any left click closes it
static void BlockSelectScreen_mouseClicked(Screen* self, int x, int y, int button) {
    if (button != 0) return;
    int i = blockAt(self, x, y);
    if (i >= 0) Minecraft_inventoryReplace(Minecraft_blockList[i]);
    Minecraft_setScreen(NULL);
}

void BlockSelectScreen_init(BlockSelectScreen* s, Font* font, int width, int height) {
    Screen_initBase(&s->screen, font, width, height);
    s->screen.render = BlockSelectScreen_render;
    s->screen.mouseClicked = BlockSelectScreen_mouseClicked;
}
