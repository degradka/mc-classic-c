// level/levelgen/level_gen.c

#include "level_gen.h"
#include "../tile/tile.h"
#include "synth/synth.h"
#include "synth/perlin_noise.h"
#include "synth/distort.h"
#include "../../log.h"
#include "../../jtime.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// the LevelGen's Random, the server makes one LevelGen
static JRandom genRandom;

typedef struct {
    int width, height, depth;
    int8_t* blocks;
} Gen;

static int* raise(Gen* g) {
    PerlinNoise p1, p2, p3, p4, p5;
    Distort d1, d2;
    PerlinNoise_init(&p1, &genRandom, 8);
    PerlinNoise_init(&p2, &genRandom, 8);
    Distort_init(&d1, &p1.synth, &p2.synth);
    PerlinNoise_init(&p3, &genRandom, 8);
    PerlinNoise_init(&p4, &genRandom, 8);
    Distort_init(&d2, &p3.synth, &p4.synth);
    PerlinNoise_init(&p5, &genRandom, 8);

    int* heightmap = (int*)malloc((size_t)g->width * g->height * sizeof(int));
    float f = 1.3f;
    for (int x = 0; x < g->width; ++x) {
        for (int z = 0; z < g->height; ++z) {
            double h1 = d1.synth.getValue(&d1.synth, (double)((float)x * f), (double)((float)z * f)) / 8.0 - 8.0;
            double h2 = d2.synth.getValue(&d2.synth, (double)((float)x * f), (double)((float)z * f)) / 6.0 + 6.0;
            double c = p5.synth.getValue(&p5.synth, x, z) / 8.0;
            if (c > 0.0) h2 = h1;
            double h = (h1 > h2 ? h1 : h2) / 2.0;
            if (h < 0.0) h *= 0.8;
            heightmap[x + z * g->width] = (int)h;
        }
    }
    PerlinNoise_destroy(&p1); PerlinNoise_destroy(&p2); PerlinNoise_destroy(&p3);
    PerlinNoise_destroy(&p4); PerlinNoise_destroy(&p5);
    return heightmap;
}

static void erode(Gen* g, int* heightmap) {
    PerlinNoise p1, p2, p3, p4;
    Distort d1, d2;
    PerlinNoise_init(&p1, &genRandom, 8);
    PerlinNoise_init(&p2, &genRandom, 8);
    Distort_init(&d1, &p1.synth, &p2.synth);
    PerlinNoise_init(&p3, &genRandom, 8);
    PerlinNoise_init(&p4, &genRandom, 8);
    Distort_init(&d2, &p3.synth, &p4.synth);

    for (int x = 0; x < g->width; ++x) {
        for (int z = 0; z < g->height; ++z) {
            double e = d1.synth.getValue(&d1.synth, x << 1, z << 1) / 8.0;
            int o = d2.synth.getValue(&d2.synth, x << 1, z << 1) > 0.0 ? 1 : 0;
            if (e > 2.0) {
                int h = heightmap[x + z * g->width];
                heightmap[x + z * g->width] = ((h - o) / 2 << 1) + o;
            }
        }
    }
    PerlinNoise_destroy(&p1); PerlinNoise_destroy(&p2); PerlinNoise_destroy(&p3); PerlinNoise_destroy(&p4);
}

// dirt down to the heightmap, rock under a noisy depth, no grass yet
static void soil(Gen* g, int* heightmap) {
    int w = g->width, h = g->height, d = g->depth;
    PerlinNoise p;
    PerlinNoise_init(&p, &genRandom, 8);
    for (int x = 0; x < w; ++x) {
        for (int z = 0; z < h; ++z) {
            int n = (int)(p.synth.getValue(&p.synth, x, z) / 24.0) - 4;
            int dh = heightmap[x + z * w] + d / 2;
            int rh = dh + n;
            heightmap[x + z * w] = dh > rh ? dh : rh;
            for (int y = 0; y < d; ++y) {
                int i = (y * g->height + z) * g->width + x;
                int id = 0;
                if (y <= dh) id = TILE_DIRT;
                if (y <= rh) id = TILE_ROCK;
                g->blocks[i] = (int8_t)id;
            }
        }
    }
    PerlinNoise_destroy(&p);
}

static void carveTunnels(Gen* g) {
    int w = g->width, h = g->height, d = g->depth;
    int count = w * h * d / 256 / 64;

    for (int i = 0; i < count; ++i) {
        float x = JRandom_nextFloat(&genRandom) * (float)w;
        float y = JRandom_nextFloat(&genRandom) * (float)d;
        float z = JRandom_nextFloat(&genRandom) * (float)h;
        int length = (int)((JRandom_nextFloat(&genRandom) + JRandom_nextFloat(&genRandom)) * 75.0f);
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
            // 3 in 10 steps carve nothing, the rest carve a little off the path
            if (JRandom_nextFloat(&genRandom) < 0.3f) continue;
            float cx = x + JRandom_nextFloat(&genRandom) * 4.0f - 2.0f;
            float cy = y + JRandom_nextFloat(&genRandom) * 4.0f - 2.0f;
            float cz = z + JRandom_nextFloat(&genRandom) * 4.0f - 2.0f;
            float size = (float)(sin(l * M_PI / length) * 2.5 + 1.0);

            for (int xx = (int)(cx - size); xx <= (int)(cx + size); ++xx)
            for (int yy = (int)(cy - size); yy <= (int)(cy + size); ++yy)
            for (int zz = (int)(cz - size); zz <= (int)(cz + size); ++zz) {
                float xd = (float)xx - cx;
                float yd = (float)yy - cy;
                float zd = (float)zz - cz;
                float dd = xd * xd + yd * yd * 2.0f + zd * zd;
                if (dd < size * size && xx >= 1 && yy >= 1 && zz >= 1 &&
                    xx < g->width - 1 && yy < g->depth - 1 && zz < g->height - 1) {
                    int ii = (yy * g->height + zz) * g->width + xx;
                    if (g->blocks[ii] == TILE_ROCK) g->blocks[ii] = 0;
                }
            }
        }
    }
}

static void addOre(Gen* g, int tile, int chance, int step) {
    (void)step;
    int8_t ore = (int8_t)tile;
    int w = g->width, h = g->height, d = g->depth;
    int count = w * h * d / 256 / 64 * chance / 100;
    for (int i = 0; i < count; ++i) {
        float x = JRandom_nextFloat(&genRandom) * (float)w;
        float y = JRandom_nextFloat(&genRandom) * (float)d;
        float z = JRandom_nextFloat(&genRandom) * (float)h;
        int length = (int)((JRandom_nextFloat(&genRandom) + JRandom_nextFloat(&genRandom)) * 75.0f * (float)chance / 100.0f);
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
            float size = (float)(sin(l * M_PI / length) * chance / 100.0 + 1.0);
            for (int xx = (int)(x - size); xx <= (int)(x + size); ++xx)
            for (int yy = (int)(y - size); yy <= (int)(y + size); ++yy)
            for (int zz = (int)(z - size); zz <= (int)(z + size); ++zz) {
                float xd = (float)xx - x;
                float yd = (float)yy - y;
                float zd = (float)zz - z;
                float dd = xd * xd + yd * yd * 2.0f + zd * zd;
                if (dd < size * size && xx >= 1 && yy >= 1 && zz >= 1 &&
                    xx < g->width - 1 && yy < g->depth - 1 && zz < g->height - 1) {
                    int ii = (yy * g->height + zz) * g->width + xx;
                    if (g->blocks[ii] == TILE_ROCK) g->blocks[ii] = ore;
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
            level->blocks[cl] = (int8_t)target;

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
                if (target == TILE_LAVA || target == TILE_CALM_LAVA) {
                    if (belowId == TILE_WATER || belowId == TILE_CALM_WATER) {
                        level->blocks[cl - upStep] = (int8_t)TILE_ROCK;
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


static void addWater(Gen* g) {
    long long start = jtime_nanos();
    int target = TILE_CALM_WATER;
    long long tiles = 0;
    for (int x = 0; x < g->width; ++x) {
        tiles += floodFillLiquid(g, x, g->depth / 2 - 1, 0, 0, target);
        tiles += floodFillLiquid(g, x, g->depth / 2 - 1, g->height - 1, 0, target);
    }
    for (int z = 0; z < g->height; ++z) {
        tiles += floodFillLiquid(g, 0, g->depth / 2 - 1, z, 0, target);
        tiles += floodFillLiquid(g, g->width - 1, g->depth / 2 - 1, z, 0, target);
    }
    int count = g->width * g->height / 200;
    for (int i = 0; i < count; ++i) {
        int x = JRandom_nextIntN(&genRandom, g->width);
        int y = g->depth / 2 - 1 - JRandom_nextIntN(&genRandom, 3);
        int z = JRandom_nextIntN(&genRandom, g->height);
        if (g->blocks[(y * g->height + z) * g->width + x] == 0) tiles += floodFillLiquid(g, x, y, z, 0, target);
    }
    char ms[64];
    jtime_doubleString((double)(jtime_nanos() - start) / 1000000.0, ms, sizeof ms);
    printf("Flood filled %lld tiles in %s ms\n", tiles, ms);
    fflush(stdout);
}

static void addLava(Gen* g) {
    int lavaCount = 0;
    int count = g->width * g->height * g->depth / 10000;
    for (int i = 0; i < count; ++i) {
        int x = JRandom_nextIntN(&genRandom, g->width);
        int y = JRandom_nextIntN(&genRandom, g->depth / 2 - 4);
        int z = JRandom_nextIntN(&genRandom, g->height);
        if (g->blocks[(y * g->height + z) * g->width + x] == 0) {
            lavaCount++;
            floodFillLiquid(g, x, y, z, 0, TILE_CALM_LAVA);
        }
    }
    printf("LavaCount: %d\n", lavaCount);
    fflush(stdout);
}

// grass on top, sand near the water level, gravel under water
static void grow(Gen* g, const int* heightmap) {
    int w = g->width, h = g->height, d = g->depth;
    PerlinNoise p1, p2;
    PerlinNoise_init(&p1, &genRandom, 8);
    PerlinNoise_init(&p2, &genRandom, 8);
    for (int x = 0; x < w; ++x) {
        for (int z = 0; z < h; ++z) {
            bool sand = p1.synth.getValue(&p1.synth, x, z) > 8.0;
            bool gravel = p2.synth.getValue(&p2.synth, x, z) > 12.0;
            int y = heightmap[x + z * w];
            int i = (y * g->height + z) * g->width + x;
            int above = g->blocks[((y + 1) * g->height + z) * g->width + x] & 0xFF;
            if ((above == TILE_WATER || above == TILE_CALM_WATER) && y <= d / 2 - 1 && gravel) {
                g->blocks[i] = (int8_t)TILE_GRAVEL;
            }
            if (above != 0) continue;
            int id = TILE_GRASS;
            if (y <= d / 2 - 1 && sand) id = TILE_SAND;
            g->blocks[i] = (int8_t)id;
        }
    }
    PerlinNoise_destroy(&p1);
    PerlinNoise_destroy(&p2);
}

static void plant(Gen* g, const int* heightmap) {
    int w = g->width;
    int count = g->width * g->height / 4000;
    for (int i = 0; i < count; ++i) {
        int x0 = JRandom_nextIntN(&genRandom, g->width);
        int z0 = JRandom_nextIntN(&genRandom, g->height);
        for (int j = 0; j < 20; ++j) {
            int x = x0;
            int z = z0;
            for (int k = 0; k < 20; ++k) {
                // both steps happen before either is checked
                x += JRandom_nextIntN(&genRandom, 6) - JRandom_nextIntN(&genRandom, 6);
                z += JRandom_nextIntN(&genRandom, 6) - JRandom_nextIntN(&genRandom, 6);
                if (x < 0 || z < 0 || x >= g->width || z >= g->height) continue;
                int y = heightmap[x + z * w] + 1;
                int trunk = JRandom_nextIntN(&genRandom, 3) + 4;
                bool ok = true;
                for (int yy = y; yy <= y + 1 + trunk; ++yy) {
                    int r = 1;
                    if (yy >= y + 1 + trunk - 2) r = 2;
                    for (int xx = x - r; xx <= x + r && ok; ++xx) {
                        for (int zz = z - r; zz <= z + r && ok; ++zz) {
                            if (xx >= 0 && yy >= 0 && zz >= 0 && xx < g->width && yy < g->depth && zz < g->height) {
                                if ((g->blocks[(yy * g->height + zz) * g->width + xx] & 0xFF) != 0) ok = false;
                            } else {
                                ok = false;
                            }
                        }
                    }
                }
                if (!ok) continue;
                int idx = (y * g->height + z) * g->width + x;
                int below = g->blocks[((y - 1) * g->height + z) * g->width + x] & 0xFF;
                if (below != TILE_GRASS || y >= g->depth - trunk - 1) continue;
                g->blocks[idx - 1 * g->width * g->height] = (int8_t)TILE_DIRT;
                for (int yy = y - 3 + trunk; yy <= y + trunk; ++yy) {
                    int dy = yy - (y + trunk);
                    int r = 1 - dy / 2;
                    for (int xx = x - r; xx <= x + r; ++xx) {
                        int dx = xx - x;
                        for (int zz = z - r; zz <= z + r; ++zz) {
                            int dz = zz - z;
                            // a corner is left out half the time, always on the top layer
                            if (abs(dx) == r && abs(dz) == r && (JRandom_nextIntN(&genRandom, 2) == 0 || dy == 0)) continue;
                            g->blocks[(yy * g->height + zz) * g->width + xx] = (int8_t)TILE_LEAVES;
                        }
                    }
                }
                for (int t = 0; t < trunk; ++t) {
                    g->blocks[idx + t * g->width * g->height] = (int8_t)TILE_LOG;
                }
            }
        }
    }
}

// the server always makes 256 x 256 x 64
Level* LevelGen_generateLevel(const char* creator) {
    JRandom_initRandom(&genRandom);
    Log_info("Generating level");
    Gen g = { 256, 256, 64, NULL };
    g.blocks = (int8_t*)calloc((size_t)256 * 256 * 64 + 1, 1);
    int* heightmap = raise(&g);
    erode(&g, heightmap);
    soil(&g, heightmap);
    carveTunnels(&g);
    addOre(&g, TILE_COAL_ORE, 90, 1);
    addOre(&g, TILE_IRON_ORE, 70, 2);
    addOre(&g, TILE_GOLD_ORE, 50, 3);
    addWater(&g);
    addLava(&g);
    grow(&g, heightmap);
    plant(&g, heightmap);
    free(heightmap);

    Level* level = Level_create();
    Level_setData(level, 256, 64, 256, g.blocks);
    level->createTime = jtime_millis();
    JStr s;
    jstr_set(&s, creator);
    level->creator = (JStr*)malloc(sizeof(JStr));
    jstr_copy(level->creator, &s);
    jstr_set(&s, "A Nice World");
    level->name = (JStr*)malloc(sizeof(JStr));
    jstr_copy(level->name, &s);
    return level;
}
