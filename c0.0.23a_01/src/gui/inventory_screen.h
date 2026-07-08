// inventory_screen.h: c0.0.20a_02 "select block" screen, opened with B

#ifndef INVENTORY_SCREEN_H
#define INVENTORY_SCREEN_H

#include "screen.h"
#include "../level/level.h"
#include <stdbool.h>

typedef struct {
    Screen screen; // first member: an InventoryScreen* can be passed where a Screen* is expected
    Level* level;
    int* hotbar;        // 9 element array, shared with minecraft.c's gHotbar
    int* selectedSlot;  // shared with minecraft.c's gSelectedSlot
    int textureId;      // terrain.png, for the isometric tile icons
} InventoryScreen;

void InventoryScreen_open(Font* font, int width, int height, Level* level, int* hotbar, int* selectedSlot, int textureId);

// true if screen is the Select block screen's own singleton instance. c0.0.21a:
// this is the one screen that keeps gameplay movement/hotbar switching alive
// while open (the real source's Screen.e passthrough flag, set only here)
bool InventoryScreen_isThis(const Screen* screen);

#endif
