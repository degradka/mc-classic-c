// level/fx/smolder.c

#include "smolder.h"
#include "../../jrandom.h"
#include "../level.h"
#include "../tile/tile.h"

extern void Minecraft_spawnSmoke(Level* lvl, float x, float y, float z);

void Smolder_init(Smolder* s, Level* level, int x, int y, int z) {
    Entity* e = &s->e;
    Entity_init(e, level);
    Entity_setSize(e, 0.0f, 0.0f);
    Entity_setPosition(e, (float)x + 0.5f, (float)y + 0.1f, (float)z + 0.5f);
    e->heightOffset = 1.5f;
    e->makeStepSound = false;
    s->life = 0;
    s->lifeTime = (int)(40.0 / (JRandom_mathRandom() * 0.8 + 0.2));
}

// smokes less as it ages, goes out when its time is up or nothing solid is under it
void Smolder_onTick(Smolder* s) {
    Entity* e = &s->e;
    float ox = (float)(JRandom_mathRandom() - 0.5);
    float oz = (float)(JRandom_mathRandom() - 0.5);
    float age = (float)s->life / (float)s->lifeTime;
    for (int i = 0; i < 4; ++i) {
        if (JRandom_mathRandom() > (double)age) Minecraft_spawnSmoke(e->level, e->x + ox, e->y, e->z + oz);
    }
    if (s->life++ < s->lifeTime) {
        int id = Level_getTile(e->level, (int)e->x, (int)(e->y - 0.3f), (int)e->z);
        if (id != 0 && gTiles[id] && gTiles[id]->isSolid(gTiles[id])) return;
    }
    Entity_remove(e);
}
