// inventory.c

#include "inventory.h"
#include "user.h"

void Inventory_init(Inventory* inv) {
    for (int i = 0; i < 9; i++) {
        inv->id[i] = -1;
        inv->count[i] = 0;
        inv->flash[i] = 0;
    }
    inv->selected = 0;
}

void Inventory_onTick(Inventory* inv) {
    for (int i = 0; i < 9; i++) {
        if (inv->flash[i] > 0) inv->flash[i]--;
    }
}

int Inventory_getSelected(const Inventory* inv) {
    return inv->id[inv->selected];
}

int Inventory_findSlot(const Inventory* inv, int tileId) {
    for (int i = 0; i < 9; i++) {
        if (inv->id[i] == tileId) return i;
    }
    return -1;
}

void Inventory_scroll(Inventory* inv, int dir) {
    if (dir > 0) dir = 1;
    if (dir < 0) dir = -1;
    inv->selected -= dir;
    while (inv->selected < 0) inv->selected += 9;
    while (inv->selected >= 9) inv->selected -= 9;
}

bool Inventory_consume(Inventory* inv, int tileId) {
    int slot = Inventory_findSlot(inv, tileId);
    if (slot < 0) return false;
    inv->count[slot]--;
    if (inv->count[slot] <= 0) inv->id[slot] = -1;
    return true;
}

bool Inventory_addResource(Inventory* inv, int tileId) {
    int slot = Inventory_findSlot(inv, tileId);
    if (slot < 0) slot = Inventory_findSlot(inv, -1);
    if (slot < 0) return false;
    if (inv->count[slot] >= 99) return false;
    inv->id[slot] = tileId;
    inv->count[slot]++;
    inv->flash[slot] = 5;
    return true;
}

void Inventory_replaceSlot(Inventory* inv, int tileId) {
    int slot = Inventory_findSlot(inv, tileId);
    if (slot >= 0) inv->id[slot] = inv->id[inv->selected];
    inv->id[inv->selected] = tileId;
}

void Inventory_replaceSlotIndex(Inventory* inv, int index) {
    if (index >= 0) Inventory_replaceSlot(inv, USER_CREATIVE_TILES[index]);
}

void Inventory_grabTexture(Inventory* inv, int tileId, bool creative) {
    int slot = Inventory_findSlot(inv, tileId);
    if (slot >= 0) {
        inv->selected = slot;
        return;
    }
    if (!creative || tileId <= 0) return;
    for (int i = 0; i < USER_CREATIVE_TILE_COUNT; ++i) {
        if (USER_CREATIVE_TILES[i] == tileId) {
            Inventory_replaceSlot(inv, tileId);
            return;
        }
    }
}
