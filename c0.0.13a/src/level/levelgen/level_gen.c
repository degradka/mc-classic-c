// level/levelgen/level_gen.c

#include "level_gen.h"
#include "../tile/tile.h"
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern void Minecraft_beginLevelLoading(const char* title);
extern void Minecraft_levelLoadUpdate(const char* status);

// the generator keeps one Random for the whole game
static JRandom genRandom;
static bool genRandomReady = false;

// the level being built
typedef struct {
    int width, height, depth;
    byte* blocks;
} Gen;

static void buildBlocks(Gen* level) {
    const int w = level->width, h = level->height, d = level->depth;

    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < d; ++y) {
            for (int z = 0; z < h; ++z) {
                int dh = d / 2;
                int rh = d / 3;
                int idx = (y * h + z) * w + x;
                int id = 0;
                if (y == dh && y >= d / 2 - 1) id = TILE_GRASS.id;
                else if (y <= dh) id = TILE_DIRT.id;
                if (y <= rh) id = TILE_ROCK.id;
                level->blocks[idx] = (byte)id;
            }
        }
    }
}

static void carveTunnels(Gen* level) {
    const int w = level->width, h = level->height, d = level->depth;
    const int count = w * h * d / 256 / 64;

    for (int i = 0; i < count; ++i) {
        float x = JRandom_nextFloat(&genRandom) * (float)w;
        float y = JRandom_nextFloat(&genRandom) * (float)d;
        float z = JRandom_nextFloat(&genRandom) * (float)h;
        int length = (int)(JRandom_nextFloat(&genRandom) + JRandom_nextFloat(&genRandom) * 150.0f);
        float dir1 = (float)(JRandom_nextFloat(&genRandom) * M_PI * 2.0);
        float dira1 = 0.0f;
        float dir2 = (float)(JRandom_nextFloat(&genRandom) * M_PI * 2.0);
        float dira2 = 0.0f;

        for (int l = 0; l < length; ++l) {
            x = (float)(x + sin(dir1) * cos(dir2));
            z = (float)(z + cos(dir1) * cos(dir2));
            y = (float)(y + sin(dir2));

            dir1 += dira1 * 0.2f;
            dira1 *= 0.9f;
            dira1 += JRandom_nextFloat(&genRandom) - JRandom_nextFloat(&genRandom);

            dir2 += dira2 * 0.5f;
            dir2 *= 0.5f;
            dira2 *= 0.9f;
            dira2 += JRandom_nextFloat(&genRandom) - JRandom_nextFloat(&genRandom);

            float size = (float)(sin(l * M_PI / length) * 2.5 + 1.0);

            for (int xx = (int)(x - size); xx <= (int)(x + size); ++xx) {
                for (int yy = (int)(y - size); yy <= (int)(y + size); ++yy) {
                    for (int zz = (int)(z - size); zz <= (int)(z + size); ++zz) {
                        float xd = (float)xx - x;
                        float yd = (float)yy - y;
                        float zd = (float)zz - z;
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

// growable stack flood fill, it keeps the fill order of chained coordinate stacks
static int growStack(int** stack, int* capacity) {
    int newCap = *capacity * 2;
    int* p = (int*)realloc(*stack, (size_t)newCap * sizeof(int));
    if (!p) return 0;
    *stack = p;
    *capacity = newCap;
    return 1;
}

static long long floodFillLiquid(Gen* level, int x, int y, int z, int source, int target) {
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

static void addWater(Gen* level) {
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
        int j = JRandom_nextIntN(&genRandom, level->width);
        int k = level->depth / 2 - 1;
        int zz = JRandom_nextIntN(&genRandom, level->height);
        if (level->blocks[(k * level->height + zz) * level->width + j] == 0) {
            tiles += floodFillLiquid(level, j, k, zz, 0, target);
        }
    }
    printf("Flood filled %lld tiles\n", tiles);
}

static void addLava(Gen* level) {
    int lavaCount = 0;
    int total = level->width * level->height * level->depth / 10000;
    for (int i = 0; i < total; ++i) {
        int x = JRandom_nextIntN(&genRandom, level->width);
        int y = JRandom_nextIntN(&genRandom, level->depth / 2);
        int z = JRandom_nextIntN(&genRandom, level->height);
        if (level->blocks[(y * level->height + z) * level->width + x] == 0) {
            lavaCount++;
            floodFillLiquid(level, x, y, z, 0, TILE_CALM_LAVA.id);
        }
    }
    printf("LavaCount: %d\n", lavaCount);
}

bool LevelGen_generateLevel(Level* level, const char* userName, int width, int height, int depth) {
    if (!genRandomReady) { JRandom_initRandom(&genRandom); genRandomReady = true; }

    Minecraft_beginLevelLoading("Generating level");
    Gen gen = { width, height, depth, NULL };
    gen.blocks = (byte*)calloc((size_t)width * height * depth, 1);
    if (!gen.blocks) {
        fprintf(stderr, "Failed to allocate level memory\n");
        exit(EXIT_FAILURE);
    }

    // buildHeightmap only returns an empty array in this version
    Minecraft_levelLoadUpdate("Raising..");
    Minecraft_levelLoadUpdate("Eroding..");
    buildBlocks(&gen);
    Minecraft_levelLoadUpdate("Carving..");
    carveTunnels(&gen);
    Minecraft_levelLoadUpdate("Watering..");
    addWater(&gen);
    Minecraft_levelLoadUpdate("Melting..");
    addLava(&gen);

    Level_setData(level, width, depth, height, gen.blocks);
    level->createTime = (long long)time(NULL) * 1000;
    snprintf(level->creator, sizeof(level->creator), "%s", userName);
    snprintf(level->name, sizeof(level->name), "A Nice World");
    return true;
}
