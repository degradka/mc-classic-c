// item/item.h: a dropped item entity

#ifndef ITEM_ENTITY_H
#define ITEM_ENTITY_H

#include "../entity.h"

typedef struct {
    Entity e;
    int resource;   // tile id this represents
    float rot;
    int tickCount;
    int age;        // despawns at 6000 (5 minutes)

    bool  pickedUp;
    int   pickupTime;
    float pickupOrgX, pickupOrgY, pickupOrgZ;
    Entity* pickupTarget;
} Item;

void Item_init(Item* it, Level* level, float x, float y, float z, int resource);
void Item_onTick(Item* it);
void Item_render(const Item* it, float partialTicks);

// removed when the 3 tick animation ends
void Item_startPickup(Item* it, Entity* target);

#endif
