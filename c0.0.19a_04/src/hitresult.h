// hitresult.h: picked block and face

#ifndef HITRESULT_H
#define HITRESULT_H

#include <stdbool.h>

struct Player; typedef struct Player Player;

typedef struct HitResult {
    int x, y, z; // block position
    int o;       // the type name, always 0
    int f;       // face 0..5
} HitResult;

void hitresult_create(HitResult* hitResult, int x, int y, int z, int o, int f);
bool HitResult_isCloserThan(const HitResult* h, const Player* player, const HitResult* o, int editMode);

#endif