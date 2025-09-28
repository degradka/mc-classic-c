// level/tile/liquid_tile.h

#ifndef LIQUID_TILE_H
#define LIQUID_TILE_H

#include "tile.h"

typedef struct LiquidTile {
    Tile base;
    int  liquidType;   // LIQ_WATER or LIQ_LAVA
    int  calmTileId;   // id of calm version
    int  tileId;       // id of flowing version (self for flowing; flowing-1 for calm)
    int  spreadSpeed;  // water=8, lava=2
} LiquidTile;

void LiquidTile_init(LiquidTile* t, int id, int liquidType, int isCalm);

#endif