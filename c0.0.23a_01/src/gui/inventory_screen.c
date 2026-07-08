// gui/inventory_screen.c

#include "inventory_screen.h"
#include "../level/tile/tile.h"
#include <GL/glew.h>

extern void Minecraft_setScreen(Screen* screen);
extern const int Minecraft_creativeTiles[];
extern const int Minecraft_creativeTileCount;
extern void Minecraft_inventorySetTile(int id);
extern const Level* Minecraft_getLevel(void);
extern int Minecraft_getTerrainTexture(void);

static Screen inventoryScreen;
static Tessellator sTess;

// the hit boxes sit 3 left and 3 down from where the blocks are drawn
static int getTileAtSlot(const Screen* s, int x, int y) {
    for (int i = 0; i < Minecraft_creativeTileCount; ++i) {
        int bx = s->width / 2 + i % 8 * 24 - 96 - 3;
        int by = s->height / 2 + i / 8 * 24 - 48 + 3;
        if (x < bx || x > bx + 24 || y < by - 12 || y > by + 12) continue;
        return i;
    }
    return -1;
}

static void InventoryScreen_render(Screen* s, int xm, int ym) {
    int slot = getTileAtSlot(s, xm, ym);
    Screen_fillGradient(s->width / 2 - 120, 30, s->width / 2 + 120, 180, 0x90050500u, 0xC0303060u);
    if (slot >= 0) {
        int x = s->width / 2 + slot % 8 * 24 - 96;
        int y = s->height / 2 + slot / 8 * 24 - 48;
        Screen_fillGradient(x - 3, y - 8, x + 23, y + 24 - 6, 0x90FFFFFFu, 0xC0FFFFFFu);
    }
    Screen_drawCenteredString(s->font, "Select block", s->width / 2, 40, 0xFFFFFF);
    glBindTexture(GL_TEXTURE_2D, Minecraft_getTerrainTexture());
    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < Minecraft_creativeTileCount; ++i) {
        const Tile* tile = gTiles[Minecraft_creativeTiles[i]];
        glPushMatrix();
        int x = s->width / 2 + i % 8 * 24 - 96;
        int y = s->height / 2 + i / 8 * 24 - 48;
        glTranslatef((float)x, (float)y, 0.0f);
        glScalef(10.0f, 10.0f, 10.0f);
        glTranslatef(1.0f, 0.5f, 8.0f);
        glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
        if (slot == i) glScalef(1.6f, 1.6f, 1.6f);
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
static void InventoryScreen_mousePressed(Screen* s, int x, int y, int button) {
    if (button != 0) return;
    int slot = getTileAtSlot(s, x, y);
    if (slot >= 0) Minecraft_inventorySetTile(Minecraft_creativeTiles[slot]);
    Minecraft_setScreen(NULL);
}

Screen* InventoryScreen_new(void) {
    Screen_construct(&inventoryScreen);
    inventoryScreen.allowUserInput = true;
    inventoryScreen.render = InventoryScreen_render;
    inventoryScreen.mousePressed = InventoryScreen_mousePressed;
    return &inventoryScreen;
}
