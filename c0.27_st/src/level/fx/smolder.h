// level/fx/smolder.h: matches level/fx/Smolder.java, an invisible entity
// Level_explode spawns once per destroyed block. Periodically puffs
// SmokeParticle above the crater and self-removes once its own lifetime
// runs out or the block beneath it stops being solid (e.g. it was hovering
// over a block that itself later got destroyed too)

#ifndef SMOLDER_H
#define SMOLDER_H

#include "../../entity.h"

typedef struct {
    Entity e;
    int life;
    int lifeTime;
} Smolder;

void Smolder_init(Smolder* s, Level* level, int x, int y, int z);
void Smolder_onTick(Smolder* s);

#endif
