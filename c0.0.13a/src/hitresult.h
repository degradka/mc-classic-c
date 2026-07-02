// hitresult.h — block raycast result

#ifndef HITRESULT_H
#define HITRESULT_H

#include <stdbool.h>

struct Player; typedef struct Player Player;

typedef struct HitResult {
    int x, y, z; // block position
    int o;       // unused/original offset field (kept for parity)
    int f;       // face 0..5
} HitResult;

void hitresult_create(HitResult* hitResult, int x, int y, int z, int o, int f);
bool HitResult_isCloserThan(const HitResult* h, const Player* player, const HitResult* o, int editMode);

#endif