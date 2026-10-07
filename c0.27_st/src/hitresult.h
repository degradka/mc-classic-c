// hitresult.h: block raycast result

#ifndef HITRESULT_H
#define HITRESULT_H

#include <stdbool.h>

struct Player; typedef struct Player Player;
struct Entity; typedef struct Entity Entity;

#define HITRESULT_TILE   0
#define HITRESULT_ENTITY 1

typedef struct HitResult {
    int type;    // HITRESULT_TILE or HITRESULT_ENTITY
    int x, y, z; // block position, only meaningful for a tile hit
    int o;       // unused
    int f;       // face 0..5, only meaningful for a tile hit
    Entity* entity; // only meaningful for an entity hit
} HitResult;

void hitresult_create(HitResult* hitResult, int x, int y, int z, int o, int f);
void hitresult_createEntity(HitResult* hitResult, Entity* entity);
bool HitResult_isCloserThan(const HitResult* h, const Player* player, const HitResult* o, int editMode);

#endif