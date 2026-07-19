// level/fx/smolder.c

#include "smolder.h"
#include "../level.h"
#include <stdlib.h>

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

extern void Minecraft_spawnSmokeParticle(Level* lvl, float x, float y, float z);

void Smolder_init(Smolder* s, Level* level, int x, int y, int z) {
    Entity* e = &s->e;
    Entity_init(e, level);
    Entity_setSize(e, 0.0f, 0.0f);
    Entity_setPosition(e, (float)x + 0.5f, (float)y + 0.1f, (float)z + 0.5f);
    e->heightOffset = 1.5f;
    e->makeStepSound = false;
    s->lifeTime = (int)(40.0 / (frand01() * 0.8 + 0.2));
    s->life = 0;
}

void Smolder_onTick(Smolder* s) {
    Entity* e = &s->e;

    float f1 = frand01() - 0.5f;
    float f2 = frand01() - 0.5f;
    int f3 = 0;
    for (int k = 0; k < 4; ++k) {
        if (frand01() > (float)f3) {
            Minecraft_spawnSmokeParticle(e->level, e->x + f1, e->y, e->z + f2);
        }
    }

    int kx = (int)e->x;
    int iy = (int)(e->y - 0.3f);
    int jz = (int)e->z;
    int tileId = Level_getTile(e->level, kx, iy, jz);

    bool lifeExpired = (s->life++ >= s->lifeTime);
    if (lifeExpired || tileId == 0 || !Level_isSolidTile(e->level, kx, iy, jz)) {
        Entity_remove(e);
    }
}
