// inventory_screen.c: c0.0.20a_02 "select block" screen, opened with B

#include "inventory_screen.h"
#include "../level/tile/tile.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

extern void Minecraft_setScreen(Screen* screen);

static InventoryScreen instance;
static Tessellator screenTess;

#define GRID_COLUMNS 8

// matches the real source exactly: X range is a plain 24px cell, Y range is
// centered on the anchor (+/-12), not top aligned like X
static int hitTest(Screen* self, int x, int y) {
    for (int i = 0; i < PLACEABLE_TILE_COUNT; ++i) {
        int cx = self->width / 2 + (i % GRID_COLUMNS) * 24 - 96;
        int cy = self->height / 2 + (i / GRID_COLUMNS) * 24 - 48;
        if (x < cx || x > cx + 24 || y < cy - 12 || y > cy + 12) continue;
        return i;
    }
    return -1;
}

static void renderSlotIcon(InventoryScreen* s, int screenX, int screenY, int tileId, int hovered) {
    glPushMatrix();
    glTranslatef((float)screenX, (float)screenY, 0.0f);
    glScalef(10.0f, 10.0f, 10.0f);
    glTranslatef(1.0f, 0.5f, 8.0f);
    glRotatef(-30.0f, 1, 0, 0);
    glRotatef(45.0f, 0, 1, 0);
    if (hovered) glScalef(1.6f, 1.6f, 1.6f);
    glTranslatef(-1.5f, 0.5f, 0.5f);
    glScalef(-1.0f, -1.0f, -1.0f);

    Tessellator_begin(&screenTess);
    const Tile* tile = gTiles[tileId];
    if (tile && tile->render) tile->render(tile, &screenTess, s->level, 0, -2, 0, 0);
    Tessellator_end(&screenTess);

    glPopMatrix();
}

static void InventoryScreen_render(Screen* self, int xMouse, int yMouse) {
    InventoryScreen* s = (InventoryScreen*)self;

    // the isometric tile icons below sit close to local z=0, so the backdrop
    // needs to be pushed further back or it wins the depth test and hides
    // them, matching how the hotbar's own background bar sits at z=-90,
    // behind its own icons
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -95.0f);
    Screen_fillGradient(0, 0, self->width, self->height, 0x60050500u, 0xA0303060u);
    glPopMatrix();

    Screen_drawCenteredString(self, "Select block", self->width / 2, 40, 0xFFFFFFu);

    int hovered = hitTest(self, xMouse, yMouse);

    glEnable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s->textureId);
    for (int i = 0; i < PLACEABLE_TILE_COUNT; ++i) {
        int cx = self->width / 2 + (i % GRID_COLUMNS) * 24 - 96;
        int cy = self->height / 2 + (i / GRID_COLUMNS) * 24 - 48;
        renderSlotIcon(s, cx, cy, PLACEABLE_TILE_IDS[i], i == hovered);
    }
    glDisable(GL_TEXTURE_2D);
}

static void InventoryScreen_mouseClicked(Screen* self, int x, int y, int button) {
    (void)button;
    InventoryScreen* s = (InventoryScreen*)self;
    int index = hitTest(self, x, y);
    if (index < 0) return;

    int tileId = PLACEABLE_TILE_IDS[index];
    int existingSlot = -1;
    for (int i = 0; i < 9; ++i) if (s->hotbar[i] == tileId) existingSlot = i;

    if (existingSlot >= 0) s->hotbar[existingSlot] = s->hotbar[*s->selectedSlot];
    s->hotbar[*s->selectedSlot] = tileId;

    Minecraft_setScreen(NULL);
}

void InventoryScreen_open(Font* font, int width, int height, Level* level, int* hotbar, int* selectedSlot, int textureId) {
    InventoryScreen* s = &instance;
    s->screen.width  = width;
    s->screen.height = height;
    s->screen.font   = font;
    s->screen.render = InventoryScreen_render;
    s->screen.init = NULL;
    s->screen.tick = NULL;
    s->screen.keyPressed = Screen_defaultKeyPressed;
    s->screen.mouseClicked = InventoryScreen_mouseClicked;
    s->screen.destroy = NULL;

    s->level = level;
    s->hotbar = hotbar;
    s->selectedSlot = selectedSlot;
    s->textureId = textureId;

    Minecraft_setScreen((Screen*)s);
}
