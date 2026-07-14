// inventory.h: player's survival inventory (c0.24_st_03), matches
// player/Inventory.java (renamed from the old obfuscated player/d.java as of
// c0.25_05_st; this port's own shape already matched the new one, addResource
// and the per tick popTime countdown already lived here rather than inlined
// in Player). 9 slots, empty at first (no more Creative style fixed hotbar),
// a slot holds one tile id and a count (max 99), replenished only by picking
// up dropped Item entities, spent one at a time on placement.

#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>

typedef struct {
    int id[9];      // tile id per slot, -1 = empty
    int count[9];   // stack count per slot, matches Inventory's own 99 cap
    int flash[9];   // c0.24_st_03: pickup "pop" flash timer, matches
                     // Inventory's own popTime[], ticked down in
                     // Inventory_onTick and read by the hotbar's own HUD draw
    int selected;    // 0..8
} Inventory;

void Inventory_init(Inventory* inv);
void Inventory_onTick(Inventory* inv);

// matches Inventory.getSelected(): the tile id in the currently selected
// slot, or -1
int  Inventory_getSelected(const Inventory* inv);
// matches Inventory's own still unnamed private slot search (a(int) in the
// decompile): first slot index holding tileId, or -1. Also used with
// tileId=-1 to find the first empty slot, same trick as the real source
int  Inventory_findSlot(const Inventory* inv, int tileId);
// matches Inventory.swapPaint(int): scroll the selection by wheel direction,
// clamped to a single step and wrapped
void Inventory_scroll(Inventory* inv, int dir);
// matches Inventory.removeResource(int): spends one of tileId, clearing the
// slot at 0. Returns false if none held (nothing to place)
bool Inventory_consume(Inventory* inv, int tileId);
// matches Inventory.addResource(int), moved here from Player as of
// c0.25_05_st (this port's own shape already matched): merges into an
// existing stack of tileId if any, else the first empty slot; false (item
// stays on the ground) if no slot is available or the matched/target stack
// is already at the 99 cap
bool Inventory_addResource(Inventory* inv, int tileId);

#endif
