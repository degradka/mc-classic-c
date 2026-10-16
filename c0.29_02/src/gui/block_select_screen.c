// gui/block_select_screen.c

#include "block_select_screen.h"
#include "../user.h"
#include "../level/tile/tile.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>

extern void Minecraft_setScreen(Screen* screen);

static BlockSelectScreen instance;
static Tessellator screenTess;

// x is a plain 24 wide cell, y is centered on the row
static int slotAt(Screen* self, int x, int y) {
    for (int i = 0; i < USER_CREATIVE_TILE_COUNT; ++i) {
        int cx = self->width / 2 + i % 9 * 24 + -108 - 3;
        int cy = self->height / 2 + i / 9 * 24 + -60 + 3;
        if (x < cx || x > cx + 24 || y < cy - 12 || y > cy + 12) continue;
        return i;
    }
    return -1;
}

static void BlockSelectScreen_render(Screen* self, int xm, int ym) {
    BlockSelectScreen* s = (BlockSelectScreen*)self;
    int hovered = slotAt(self, xm, ym);
    Screen_fillGradient(self->width / 2 - 120, 30, self->width / 2 + 120, 180, 0x90050500u, 0xC0303060u);
    if (hovered >= 0) {
        int hx = self->width / 2 + hovered % 9 * 24 + -108;
        int hy = self->height / 2 + hovered / 9 * 24 + -60;
        Screen_fillGradient(hx - 3, hy - 8, hx + 23, hy + 24 - 6, 0x90FFFFFFu, 0xC0FFFFFFu);
    }
    Screen_drawCenteredString(self, "Select block", self->width / 2, 40, 0xFFFFFFu);
    glBindTexture(GL_TEXTURE_2D, s->terrainTex);
    for (int i = 0; i < USER_CREATIVE_TILE_COUNT; ++i) {
        const Tile* tile = gTiles[USER_CREATIVE_TILES[i]];
        glPushMatrix();
        int cx = self->width / 2 + i % 9 * 24 + -108;
        int cy = self->height / 2 + i / 9 * 24 + -60;
        glTranslatef((float)cx, (float)cy, 0.0f);
        glScalef(10.0f, 10.0f, 10.0f);
        glTranslatef(1.0f, 0.5f, 8.0f);
        glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
        if (hovered == i) glScalef(1.6f, 1.6f, 1.6f);
        glTranslatef(-1.5f, 0.5f, 0.5f);
        glScalef(-1.0f, -1.0f, -1.0f);
        Tessellator_begin(&screenTess);
        tile->renderGuiItem(tile, &screenTess);
        Tessellator_end(&screenTess);
        glPopMatrix();
    }
}

static void BlockSelectScreen_mouseClicked(Screen* self, int x, int y, int button) {
    BlockSelectScreen* s = (BlockSelectScreen*)self;
    if (button != 0) return;
    Inventory_replaceSlotIndex(s->inventory, slotAt(self, x, y));
    Minecraft_setScreen(NULL);
}

void BlockSelectScreen_open(Font* font, int width, int height, Inventory* inventory, int terrainTex) {
    BlockSelectScreen* s = &instance;
    s->screen.width  = width;
    s->screen.height = height;
    s->screen.font   = font;
    s->screen.render = BlockSelectScreen_render;
    s->screen.init = NULL;
    s->screen.tick = NULL;
    s->screen.keyPressed = Screen_defaultKeyPressed;
    s->screen.mouseClicked = BlockSelectScreen_mouseClicked;
    s->screen.destroy = NULL;
    s->screen.allowUserInput = true;
    s->inventory = inventory;
    s->terrainTex = terrainTex;
    Minecraft_setScreen((Screen*)s);
}
