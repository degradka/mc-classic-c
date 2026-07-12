// inventory.h: player's survival inventory (c0.24_st_03), matches
// player/d.java. 9 slots, empty at first (no more Creative style fixed
// hotbar), a slot holds one tile id and a count (max 99), replenished only
// by picking up dropped Item entities, spent one at a time on placement.

#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>

typedef struct {
    int id[9];      // tile id per slot, -1 = empty
    int count[9];   // stack count per slot, matches Player.d's own 99 cap
    int flash[9];   // c0.24_st_03: pickup "pop" flash timer, matches d.c[],
                     // ticked down in Inventory_onTick; task #61's HUD reads it
    int selected;    // 0..8
} Inventory;

void Inventory_init(Inventory* inv);
void Inventory_onTick(Inventory* inv);

// matches d.a(): the tile id in the currently selected slot, or -1
int  Inventory_getSelected(const Inventory* inv);
// matches d.a(int): first slot index holding tileId, or -1. Also used with
// tileId=-1 to find the first empty slot, same trick as the real source
int  Inventory_findSlot(const Inventory* inv, int tileId);
// matches d.b(int): scroll the selection by wheel direction, clamped to a
// single step and wrapped
void Inventory_scroll(Inventory* inv, int dir);
// matches d.c(int): spends one of tileId, clearing the slot at 0. Returns
// false if none held (nothing to place)
bool Inventory_consume(Inventory* inv, int tileId);
// matches Player.addResource(int): merges into an existing stack of tileId
// if any, else the first empty slot; false (item stays on the ground) if no
// slot is available or the matched/target stack is already at the 99 cap
bool Inventory_addResource(Inventory* inv, int tileId);

#endif
