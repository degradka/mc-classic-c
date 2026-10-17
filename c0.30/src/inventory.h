// inventory.h: survival inventory, 9 slots of up to 99

#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>

typedef struct {
    int id[9];      // tile id per slot, -1 = empty
    int count[9];   // up to 99
    int flash[9];   // Inventory.popTime, the pickup pop
    int selected;    // 0..8
} Inventory;

void Inventory_init(Inventory* inv);
void Inventory_onTick(Inventory* inv);

int  Inventory_getSelected(const Inventory* inv);
// first slot holding tileId, -1 finds the first empty one
int  Inventory_findSlot(const Inventory* inv, int tileId);
// Inventory.swapPaint, one step either way, wrapping
void Inventory_scroll(Inventory* inv, int dir);
// Inventory.removeResource, false when there's none
bool Inventory_consume(Inventory* inv, int tileId);
// false when there's no room, the item stays on the ground
bool Inventory_addResource(Inventory* inv, int tileId);
// Inventory.replaceSlot(Tile), puts the tile in the selected slot and moves the old one to where the tile was
void Inventory_replaceSlot(Inventory* inv, int tileId);
// Inventory.replaceSlot(int), an index into the creative list, -1 does nothing
void Inventory_replaceSlotIndex(Inventory* inv, int index);
// Inventory.grabTexture, pick block; creative swaps the tile in when it isn't on the hotbar
void Inventory_grabTexture(Inventory* inv, int tileId, bool creative);

#endif
