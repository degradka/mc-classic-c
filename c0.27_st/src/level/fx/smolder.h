// level/fx/smolder.h: smoke left in every cell an explosion reached

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
