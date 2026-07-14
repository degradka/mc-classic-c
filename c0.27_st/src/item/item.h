// item/item.h: a dropped item on ground entity, c0.24_st_03, matches
// item/Item.java. Not an inventory slot representation, a small bouncy
// tumbling block icon entity that gets picked up on player touch

#ifndef ITEM_ENTITY_H
#define ITEM_ENTITY_H

#include "../entity.h"

typedef struct {
    Entity e;
    int resource;   // tile id this represents
    float rot;
    int tickCount;
    int age;        // despawns at 6000 (5 minutes)

    // c0.24_st_03: real source's TakeItemAnim is a short lived (3 tick)
    // separate Entity that just animates this same Item toward the player
    // then removes both itself and the Item. Folded directly into Item
    // instead of adding a whole second entity type/array for a 3 tick
    // visual flourish: same end result (the icon streaks to the player
    // then vanishes), one fewer moving part
    bool  pickedUp;
    int   pickupTime;
    float pickupOrgX, pickupOrgY, pickupOrgZ;
    Entity* pickupTarget;
} Item;

void Item_init(Item* it, Level* level, float x, float y, float z, int resource);
void Item_onTick(Item* it);
void Item_render(const Item* it, float partialTicks);

// starts the pickup animation toward target; the item is actually removed
// once it finishes (matches TakeItemAnim.tick()'s 3 tick timer)
void Item_startPickup(Item* it, Entity* target);

#endif
