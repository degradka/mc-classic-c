// level/fx/smolder.c: see smolder.h

#include "smolder.h"
#include "../level.h"
#include <stdlib.h>

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

// implemented in minecraft.c: spawns a SmokeParticle into particleEngine,
// shared with PrimedTnt's own per-tick fuse smoke
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
    // matches real source's own life/lifeTime EXACTLY: both are ints, so
    // this is integer division, truncating to 0 for this entity's entire
    // active lifetime (life is always < lifeTime while still ticking here).
    // Not a deliberately-decaying spawn-frequency fraction, but a genuine real
    // source quirk (Math.random() > 0 is true for virtually every roll), so
    // this spawns up to 4 SmokeParticles nearly every tick throughout,
    // preserved faithfully rather than "fixed" into the probably-intended
    // fade-out behavior
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
