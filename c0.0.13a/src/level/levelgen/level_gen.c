// level/levelgen/level_gen.c

#include "level_gen.h"
#include "../tile/tile.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

static inline float randf(void) {
    return (float)rand() / ((float)RAND_MAX + 1.0f);
}

static void buildBlocks(Level* level) {
    const int w = level->width, h = level->height, d = level->depth;
    const int dh = d / 2;
    const int rh = d / 3;

    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < d; ++y) {
            for (int z = 0; z < h; ++z) {
                int idx = (y * h + z) * w + x;
                int id = 0;
                if (y == dh)      id = TILE_GRASS.id;
                else if (y <= dh) id = TILE_DIRT.id;
                if (y <= rh)      id = TILE_ROCK.id;
                level->blocks[idx] = (byte)id;
            }
        }
    }
}

static void carveTunnels(Level* level) {
    const int w = level->width, h = level->height, d = level->depth;
    const int count = w * h * d / 256 / 64;

    for (int i = 0; i < count; ++i) {
        float x = randf() * w;
        float y = randf() * d;
        float z = randf() * h;
        int length = (int)(randf() + randf() * 150.0f);
        float dir1 = randf() * (float)M_PI * 2.0f;
        float dira1 = 0.0f;
        float dir2 = randf() * (float)M_PI * 2.0f;
        float dira2 = 0.0f;

        for (int l = 0; l < length; ++l) {
            x = (float)(x + sin(dir1) * cos(dir2));
            z = (float)(z + cos(dir1) * cos(dir2));
            y = (float)(y + sin(dir2));

            dir1 += dira1 * 0.2f;
            dira1 *= 0.9f;
            dira1 += randf() - randf();

            dir2 += dira2 * 0.5f;
            dir2 *= 0.5f;
            dira2 *= 0.9f;
            dira2 += randf() - randf();

            float size = (float)(sin(l * M_PI / length) * 2.5 + 1.0);

            for (int xx = (int)(x - size); xx <= (int)(x + size); ++xx) {
                for (int yy = (int)(y - size); yy <= (int)(y + size); ++yy) {
                    for (int zz = (int)(z - size); zz <= (int)(z + size); ++zz) {
                        float xd = xx - x;
                        float yd = yy - y;
                        float zd = zz - z;
                        float dd = xd * xd + yd * yd * 2.0f + zd * zd;
                        if (dd < size * size && xx >= 1 && yy >= 1 && zz >= 1 &&
                            xx < level->width - 1 && yy < level->depth - 1 && zz < level->height - 1) {
                            int ii = (yy * h + zz) * w + xx;
                            if (level->blocks[ii] == TILE_ROCK.id) {
                                level->blocks[ii] = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Growable stack flood fill, the same algorithm as the Java source's
// coordinate stack minus its fixed capacity buffer chaining workaround.
// A reallocated stack achieves identical fill behavior more simply.
static int growStack(int** stack, int* capacity) {
    int newCap = *capacity * 2;
    int* p = (int*)realloc(*stack, (size_t)newCap * sizeof(int));
    if (!p) return 0;
    *stack = p;
    *capacity = newCap;
    return 1;
}

static long long floodFillLiquid(Level* level, int x, int y, int z, int source, int target) {
    const int w = level->width, h = level->height;
    const int upStep = w * h;

    int capacity = 65536;
    int* stack = (int*)malloc((size_t)capacity * sizeof(int));
    if (!stack) return 0;
    int sp = 0;
    stack[sp++] = (y * h + z) * w + x;

    long long tiles = 0;

    while (sp > 0) {
        int cl = stack[--sp];

        int z0 = (cl / w) % h;
        int y0 = cl / upStep;
        int x0 = cl % w;

        while (x0 > 0 && level->blocks[cl - 1] == source) { x0--; cl--; }
        int x1 = x0;
        while (x1 < w && level->blocks[cl + (x1 - x0)] == source) x1++;

        int lastNorth = 0, lastSouth = 0, lastBelow = 0;
        tiles += (x1 - x0);

        for (int xx = x0; xx < x1; ++xx) {
            level->blocks[cl] = (byte)target;

            if (z0 > 0) {
                int north = (level->blocks[cl - w] == source);
                if (north && !lastNorth) {
                    if (sp == capacity && !growStack(&stack, &capacity)) { free(stack); return tiles; }
                    stack[sp++] = cl - w;
                }
                lastNorth = north;
            }
            if (z0 < h - 1) {
                int south = (level->blocks[cl + w] == source);
                if (south && !lastSouth) {
                    if (sp == capacity && !growStack(&stack, &capacity)) { free(stack); return tiles; }
                    stack[sp++] = cl + w;
                }
                lastSouth = south;
            }
            if (y0 > 0) {
                int belowId = level->blocks[cl - upStep];
                if (target == TILE_LAVA.id || target == TILE_CALM_LAVA.id) {
                    if (belowId == TILE_WATER.id || belowId == TILE_CALM_WATER.id) {
                        level->blocks[cl - upStep] = (byte)TILE_ROCK.id;
                    }
                }
                int below = (belowId == source);
                if (below && !lastBelow) {
                    if (sp == capacity && !growStack(&stack, &capacity)) { free(stack); return tiles; }
                    stack[sp++] = cl - upStep;
                }
                lastBelow = below;
            }
            cl++;
        }
    }

    free(stack);
    return tiles;
}

static void addWater(Level* level) {
    const int source = 0;
    const int target = TILE_CALM_WATER.id;
    long long tiles = 0;

    for (int x = 0; x < level->width; ++x) {
        tiles += floodFillLiquid(level, x, level->depth / 2 - 1, 0, source, target);
        tiles += floodFillLiquid(level, x, level->depth / 2 - 1, level->height - 1, source, target);
    }
    for (int zz = 0; zz < level->height; ++zz) {
        tiles += floodFillLiquid(level, 0, level->depth / 2 - 1, zz, source, target);
        tiles += floodFillLiquid(level, level->width - 1, level->depth / 2 - 1, zz, source, target);
    }
    for (int i = 0; i < level->width * level->height / 5000; ++i) {
        int j = rand() % level->width;
        int k = level->depth / 2 - 1;
        int zz = rand() % level->height;
        if (level->blocks[(k * level->height + zz) * level->width + j] == 0) {
            tiles += floodFillLiquid(level, j, k, zz, 0, target);
        }
    }
    printf("Flood filled %lld tiles\n", tiles);
}

static void addLava(Level* level) {
    int lavaCount = 0;
    int total = level->width * level->height * level->depth / 10000;
    for (int i = 0; i < total; ++i) {
        int x = rand() % level->width;
        int y = rand() % (level->depth / 2);
        int z = rand() % level->height;
        if (level->blocks[(y * level->height + z) * level->width + x] == 0) {
            lavaCount++;
            floodFillLiquid(level, x, y, z, 0, TILE_CALM_LAVA.id);
        }
    }
    printf("LavaCount: %d\n", lavaCount);
}

void LevelGen_generateMap(Level* level) {
    buildBlocks(level);
    carveTunnels(level);
    addWater(level);
    addLava(level);
}
