// level/levelgen/level_gen.c: LevelGen.generateLevel

#include "level_gen.h"
#include "../tile/tile.h"
#include "../../character/creature.h"
#include "../../jrandom.h"
#include "synth/synth.h"
#include "synth/perlin_noise.h"
#include "synth/distort.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

extern void Minecraft_beginLevelLoading(const char* title);
extern void Minecraft_levelLoadUpdate(const char* status);
extern void Minecraft_levelLoadProgress(int percent);
extern const char* Minecraft_getUserName(void);
// false if the mob's box overlaps another entity
extern bool Minecraft_spawnMob(Level* lvl, int kind, float x, float y, float z);

typedef struct {
    Level* level;
    int w, h, d, g;
    byte* blocks;
    JRandom random;
} Gen;

static void setNextPhase(int percent) {
    Minecraft_levelLoadProgress(percent);
}

// these phases never run with a single step, dividing by zero would throw
static int steps(int n) {
    return n > 1 ? n - 1 : 1;
}

static int nextIntDiff(JRandom* r, int bound) {
    int a = JRandom_nextIntN(r, bound);
    int b = JRandom_nextIntN(r, bound);
    return a - b;
}

static float nextFloatDiff(JRandom* r) {
    float a = JRandom_nextFloat(r);
    float b = JRandom_nextFloat(r);
    return a - b;
}

static int* raise(Gen* gen) {
    JRandom* r = &gen->random;
    PerlinNoise a1, a2, b1, b2, selector;
    PerlinNoise_init(&a1, r, 8);
    PerlinNoise_init(&a2, r, 8);
    Distort low;
    Distort_init(&low, &a1.synth, &a2.synth);
    PerlinNoise_init(&b1, r, 8);
    PerlinNoise_init(&b2, r, 8);
    Distort high;
    Distort_init(&high, &b1.synth, &b2.synth);
    PerlinNoise_init(&selector, r, 6);

    int* heightmap = (int*)malloc((size_t)gen->w * gen->h * sizeof(int));
    const float scale = 1.3f;
    for (int x = 0; x < gen->w; ++x) {
        setNextPhase(x * 100 / steps(gen->w));
        for (int z = 0; z < gen->h; ++z) {
            double lowH = low.synth.getValue(&low.synth, (double)((float)x * scale), (double)((float)z * scale)) / 6.0 + (double)-4;
            double highH = high.synth.getValue(&high.synth, (double)((float)x * scale), (double)((float)z * scale)) / 5.0 + 10.0 + (double)-4;
            double pick = selector.synth.getValue(&selector.synth, x, z) / 8.0;
            if (pick > 0.0) highH = lowH;
            double height = fmax(lowH, highH) / 2.0;
            if (height < 0.0) height *= 0.8;
            heightmap[x + z * gen->w] = (int)height;
        }
    }

    PerlinNoise_destroy(&a1); PerlinNoise_destroy(&a2);
    PerlinNoise_destroy(&b1); PerlinNoise_destroy(&b2);
    PerlinNoise_destroy(&selector);
    return heightmap;
}

static void erode(Gen* gen, int* heightmap) {
    JRandom* r = &gen->random;
    PerlinNoise c1, c2, d1, d2;
    PerlinNoise_init(&c1, r, 8);
    PerlinNoise_init(&c2, r, 8);
    Distort amount;
    Distort_init(&amount, &c1.synth, &c2.synth);
    PerlinNoise_init(&d1, r, 8);
    PerlinNoise_init(&d2, r, 8);
    Distort parity;
    Distort_init(&parity, &d1.synth, &d2.synth);

    for (int x = 0; x < gen->w; ++x) {
        setNextPhase(x * 100 / steps(gen->w));
        for (int z = 0; z < gen->h; ++z) {
            double a = amount.synth.getValue(&amount.synth, x << 1, z << 1) / 8.0;
            int odd = parity.synth.getValue(&parity.synth, x << 1, z << 1) > 0.0 ? 1 : 0;
            if (!(a > 2.0)) continue;
            int hgt = heightmap[x + z * gen->w];
            heightmap[x + z * gen->w] = ((hgt - odd) / 2 << 1) + odd;
        }
    }

    PerlinNoise_destroy(&c1); PerlinNoise_destroy(&c2);
    PerlinNoise_destroy(&d1); PerlinNoise_destroy(&d2);
}

static void soil(Gen* gen, int* heightmap) {
    PerlinNoise rockNoise;
    PerlinNoise_init(&rockNoise, &gen->random, 8);
    const int w = gen->w, h = gen->h, d = gen->d;
    for (int x = 0; x < w; ++x) {
        setNextPhase(x * 100 / steps(w));
        for (int z = 0; z < h; ++z) {
            int rockDepth = (int)(rockNoise.synth.getValue(&rockNoise.synth, x, z) / 24.0) - 4;
            int dirtTop = heightmap[x + z * w] + gen->g;
            int rockTop = dirtTop + rockDepth;
            int top = dirtTop > rockTop ? dirtTop : rockTop;
            if (top > d - 2) top = d - 2;
            if (top < 1) top = 1;
            heightmap[x + z * w] = top;
            for (int y = 0; y < d; ++y) {
                int id = 0;
                if (y <= dirtTop) id = TILE_DIRT.id;
                if (y <= rockTop) id = TILE_ROCK.id;
                if (y == 0) id = TILE_LAVA.id;
                gen->blocks[(y * h + z) * w + x] = (byte)id;
            }
        }
    }
    PerlinNoise_destroy(&rockNoise);
}

static void carveCaves(Gen* gen) {
    JRandom* r = &gen->random;
    const int w = gen->w, h = gen->h, d = gen->d;
    int count = w * h * d / 256 / 64 << 1;
    for (int i = 0; i < count; ++i) {
        setNextPhase(i * 100 / steps(count) / 4);
        float x = JRandom_nextFloat(r) * (float)w;
        float y = JRandom_nextFloat(r) * (float)d;
        float z = JRandom_nextFloat(r) * (float)h;
        int length = (int)((JRandom_nextFloat(r) + JRandom_nextFloat(r)) * 200.0f);
        float yaw = (float)((double)JRandom_nextFloat(r) * M_PI * 2.0);
        float yawSpeed = 0.0f;
        float pitch = (float)((double)JRandom_nextFloat(r) * M_PI * 2.0);
        float pitchSpeed = 0.0f;
        float radiusScale = JRandom_nextFloat(r) * JRandom_nextFloat(r);
        for (int step = 0; step < length; ++step) {
            x = (float)((double)x + sin(yaw) * cos(pitch));
            z = (float)((double)z + cos(yaw) * cos(pitch));
            y = (float)((double)y + sin(pitch));
            yaw += yawSpeed * 0.2f;
            yawSpeed *= 0.9f;
            yawSpeed += nextFloatDiff(r);
            pitch += pitchSpeed * 0.5f;
            pitch *= 0.5f;
            pitchSpeed *= 0.75f;
            pitchSpeed += nextFloatDiff(r);
            if (JRandom_nextFloat(r) < 0.25f) continue;
            float cx = x + (JRandom_nextFloat(r) * 4.0f - 2.0f) * 0.2f;
            float cy = y + (JRandom_nextFloat(r) * 4.0f - 2.0f) * 0.2f;
            float cz = z + (JRandom_nextFloat(r) * 4.0f - 2.0f) * 0.2f;
            float radius = ((float)d - cy) / (float)d;
            radius = 1.2f + (radius * 3.5f + 1.0f) * radiusScale;
            radius = (float)(sin((double)step * M_PI / (double)length) * (double)radius);
            for (int bx = (int)(cx - radius); bx <= (int)(cx + radius); ++bx) {
                for (int by = (int)(cy - radius); by <= (int)(cy + radius); ++by) {
                    for (int bz = (int)(cz - radius); bz <= (int)(cz + radius); ++bz) {
                        float dx = (float)bx - cx;
                        float dy = (float)by - cy;
                        float dz = (float)bz - cz;
                        float dist = dx * dx + dy * dy * 2.0f + dz * dz;
                        if (!(dist < radius * radius) || bx < 1 || by < 1 || bz < 1 || bx >= w - 1 || by >= d - 1 || bz >= h - 1) continue;
                        int idx = (by * h + bz) * w + bx;
                        if (gen->blocks[idx] != TILE_ROCK.id) continue;
                        gen->blocks[idx] = 0;
                    }
                }
            }
        }
    }
}

// LevelGen.carveTunnels, ore veins wandering through rock
static void placeOre(Gen* gen, int oreId, int percent, int phase) {
    JRandom* r = &gen->random;
    const int w = gen->w, h = gen->h, d = gen->d;
    int count = w * h * d / 256 / 64 * percent / 100;
    for (int i = 0; i < count; ++i) {
        setNextPhase(i * 100 / steps(count) / 4 + phase * 100 / 4);
        float x = JRandom_nextFloat(r) * (float)w;
        float y = JRandom_nextFloat(r) * (float)d;
        float z = JRandom_nextFloat(r) * (float)h;
        int length = (int)((JRandom_nextFloat(r) + JRandom_nextFloat(r)) * 75.0f * (float)percent / 100.0f);
        float yaw = (float)((double)JRandom_nextFloat(r) * M_PI * 2.0);
        float yawSpeed = 0.0f;
        float pitch = (float)((double)JRandom_nextFloat(r) * M_PI * 2.0);
        float pitchSpeed = 0.0f;
        for (int step = 0; step < length; ++step) {
            x = (float)((double)x + sin(yaw) * cos(pitch));
            z = (float)((double)z + cos(yaw) * cos(pitch));
            y = (float)((double)y + sin(pitch));
            yaw += yawSpeed * 0.2f;
            yawSpeed *= 0.9f;
            yawSpeed += nextFloatDiff(r);
            pitch += pitchSpeed * 0.5f;
            pitch *= 0.5f;
            pitchSpeed *= 0.9f;
            pitchSpeed += nextFloatDiff(r);
            float radius = (float)(sin((double)step * M_PI / (double)length) * (double)percent / 100.0 + 1.0);
            for (int bx = (int)(x - radius); bx <= (int)(x + radius); ++bx) {
                for (int by = (int)(y - radius); by <= (int)(y + radius); ++by) {
                    for (int bz = (int)(z - radius); bz <= (int)(z + radius); ++bz) {
                        float dx = (float)bx - x;
                        float dy = (float)by - y;
                        float dz = (float)bz - z;
                        float dist = dx * dx + dy * dy * 2.0f + dz * dz;
                        if (!(dist < radius * radius) || bx < 1 || by < 1 || bz < 1 || bx >= w - 1 || by >= d - 1 || bz >= h - 1) continue;
                        int idx = (by * h + bz) * w + bx;
                        if (gen->blocks[idx] != TILE_ROCK.id) continue;
                        gen->blocks[idx] = (byte)oreId;
                    }
                }
            }
        }
    }
}

// growable coordinate stack, it keeps the fill order of chained stacks
static long long floodFillLiquid(Gen* gen, int x, int y, int z, int target) {
    const int w = gen->w, h = gen->h;
    const int upStep = w * h;
    byte* blocks = gen->blocks;

    int capacity = 0x100000;
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

        while (x0 > 0 && blocks[cl - 1] == 0) { x0--; cl--; }
        int x1 = x0;
        while (x1 < w && blocks[cl + (x1 - x0)] == 0) x1++;

        int lastNorth = 0, lastSouth = 0, lastBelow = 0;
        tiles += (x1 - x0);
        for (int xx = x0; xx < x1; ++xx) {
            blocks[cl] = (byte)target;
            if (z0 > 0) {
                int north = blocks[cl - w] == 0;
                if (north && !lastNorth) {
                    if (sp == capacity) { capacity *= 2; stack = (int*)realloc(stack, (size_t)capacity * sizeof(int)); }
                    stack[sp++] = cl - w;
                }
                lastNorth = north;
            }
            if (z0 < h - 1) {
                int south = blocks[cl + w] == 0;
                if (south && !lastSouth) {
                    if (sp == capacity) { capacity *= 2; stack = (int*)realloc(stack, (size_t)capacity * sizeof(int)); }
                    stack[sp++] = cl + w;
                }
                lastSouth = south;
            }
            if (y0 > 0) {
                int belowId = blocks[cl - upStep];
                if ((target == TILE_LAVA.id || target == TILE_CALM_LAVA.id) &&
                    (belowId == TILE_WATER.id || belowId == TILE_CALM_WATER.id)) {
                    blocks[cl - upStep] = (byte)TILE_ROCK.id;
                }
                int below = belowId == 0;
                if (below && !lastBelow) {
                    if (sp == capacity) { capacity *= 2; stack = (int*)realloc(stack, (size_t)capacity * sizeof(int)); }
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

static void addWater(Gen* gen) {
    JRandom* r = &gen->random;
    const int w = gen->w, h = gen->h, d = gen->d;
    const int water = TILE_CALM_WATER.id;
    long long tiles = 0;
    setNextPhase(0);
    for (int x = 0; x < w; ++x) {
        tiles += floodFillLiquid(gen, x, d / 2 - 1, 0, water);
        tiles += floodFillLiquid(gen, x, d / 2 - 1, h - 1, water);
    }
    for (int z = 0; z < h; ++z) {
        tiles += floodFillLiquid(gen, 0, d / 2 - 1, z, water);
        tiles += floodFillLiquid(gen, w - 1, d / 2 - 1, z, water);
    }
    int count = w * h / 8000;
    for (int i = 0; i < count; ++i) {
        if (i % 100 == 0) setNextPhase(i * 100 / steps(count));
        int x = JRandom_nextIntN(r, w);
        int y = gen->g - 1 - JRandom_nextIntN(r, 2);
        int z = JRandom_nextIntN(r, h);
        if (gen->blocks[(y * h + z) * w + x] != 0) continue;
        tiles += floodFillLiquid(gen, x, y, z, water);
    }
    setNextPhase(100);
    printf("Flood filled %lld tiles\n", tiles);
}

static void addLava(Gen* gen) {
    JRandom* r = &gen->random;
    const int w = gen->w, h = gen->h, d = gen->d;
    int lavaCount = 0;
    int count = w * h * d / 20000;
    for (int i = 0; i < count; ++i) {
        if (i % 100 == 0) setNextPhase(i * 100 / steps(count));
        int x = JRandom_nextIntN(r, w);
        int y = (int)(JRandom_nextFloat(r) * JRandom_nextFloat(r) * (float)(gen->g - 3));
        int z = JRandom_nextIntN(r, h);
        if (gen->blocks[(y * h + z) * w + x] != 0) continue;
        lavaCount++;
        floodFillLiquid(gen, x, y, z, TILE_CALM_LAVA.id);
    }
    setNextPhase(100);
    printf("LavaCount: %d\n", lavaCount);
}

// gravel on lake and sea beds, grass on dry ground, sand on low dry ground
static void addBeaches(Gen* gen, const int* heightmap) {
    const int w = gen->w, h = gen->h, d = gen->d;
    PerlinNoise sandNoise, gravelNoise;
    PerlinNoise_init(&sandNoise, &gen->random, 8);
    PerlinNoise_init(&gravelNoise, &gen->random, 8);
    for (int x = 0; x < w; ++x) {
        setNextPhase(x * 100 / steps(w));
        for (int z = 0; z < h; ++z) {
            bool sand = sandNoise.synth.getValue(&sandNoise.synth, x, z) > 8.0;
            bool gravel = gravelNoise.synth.getValue(&gravelNoise.synth, x, z) > 12.0;
            int top = heightmap[x + z * w];
            int idx = (top * h + z) * w + x;
            int above = gen->blocks[((top + 1) * h + z) * w + x] & 0xFF;
            if ((above == TILE_WATER.id || above == TILE_CALM_WATER.id) && top <= d / 2 - 1 && gravel) {
                gen->blocks[idx] = (byte)TILE_GRAVEL.id;
            }
            if (above != 0) continue;
            int id = TILE_GRASS.id;
            if (top <= d / 2 - 1 && sand) id = TILE_SAND.id;
            gen->blocks[idx] = (byte)id;
        }
    }
    PerlinNoise_destroy(&sandNoise);
    PerlinNoise_destroy(&gravelNoise);
}

static void plantFlowers(Gen* gen, const int* heightmap) {
    JRandom* r = &gen->random;
    const int w = gen->w, h = gen->h;
    int count = w * h / 3000;
    for (int i = 0; i < count; ++i) {
        int kind = JRandom_nextIntN(r, 2);
        setNextPhase(i * 50 / steps(count));
        int bx = JRandom_nextIntN(r, w);
        int bz = JRandom_nextIntN(r, h);
        for (int c = 0; c < 10; ++c) {
            int x = bx, z = bz;
            for (int s = 0; s < 5; ++s) {
                x += nextIntDiff(r, 6);
                z += nextIntDiff(r, 6);
                if (x < 0 || z < 0 || x >= w || z >= h) continue;
                int y = heightmap[x + z * w] + 1;
                if ((gen->blocks[(y * h + z) * w + x] & 0xFF) != 0) continue;
                if ((gen->blocks[((y - 1) * h + z) * w + x] & 0xFF) != TILE_GRASS.id) continue;
                gen->blocks[(y * h + z) * w + x] = (byte)(kind == 0 ? TILE_DANDELION.id : TILE_ROSE.id);
            }
        }
    }
}

static void plantMushrooms(Gen* gen, const int* heightmap) {
    JRandom* r = &gen->random;
    const int w = gen->w, h = gen->h, d = gen->d;
    int placed = 0;
    int count = w * h * d / 2000;
    for (int i = 0; i < count; ++i) {
        int kind = JRandom_nextIntN(r, 2);
        setNextPhase(i * 50 / steps(count) + 50);
        int bx = JRandom_nextIntN(r, w);
        int by = JRandom_nextIntN(r, d);
        int bz = JRandom_nextIntN(r, h);
        for (int c = 0; c < 20; ++c) {
            int x = bx, y = by, z = bz;
            for (int s = 0; s < 5; ++s) {
                x += nextIntDiff(r, 6);
                y += nextIntDiff(r, 2);
                z += nextIntDiff(r, 6);
                if (x < 0 || z < 0 || y < 1 || x >= w || z >= h || y >= heightmap[x + z * w] - 1) continue;
                if ((gen->blocks[(y * h + z) * w + x] & 0xFF) != 0) continue;
                if ((gen->blocks[((y - 1) * h + z) * w + x] & 0xFF) != TILE_ROCK.id) continue;
                gen->blocks[(y * h + z) * w + x] = (byte)(kind == 0 ? TILE_MUSHROOM_BROWN.id : TILE_MUSHROOM_RED.id);
                placed++;
            }
        }
    }
    printf("Added %d mushrooms\n", placed);
}

static void plantTrees(Gen* gen, const int* heightmap) {
    JRandom* r = &gen->random;
    const int w = gen->w, h = gen->h;
    int count = w * h / 4000;
    for (int i = 0; i < count; ++i) {
        setNextPhase(i * 50 / steps(count) + 50);
        int bx = JRandom_nextIntN(r, w);
        int bz = JRandom_nextIntN(r, h);
        for (int c = 0; c < 20; ++c) {
            int x = bx, z = bz;
            for (int s = 0; s < 20; ++s) {
                x += nextIntDiff(r, 6);
                z += nextIntDiff(r, 6);
                if (x < 0 || z < 0 || x >= w || z >= h) continue;
                int y = heightmap[x + z * w] + 1;
                if (JRandom_nextIntN(r, 4) != 0) continue;
                Level_maybeGrowTree(gen->level, x, y, z);
            }
        }
    }
}

// Level.maybeSpawnMobs, used by world gen and by Level.tick while playing
int LevelGen_maybeSpawnMobs(Level* level, int attempts, const Entity* referenceEntity, bool reportProgress) {
    JRandom* r = &level->random;
    const int w = level->width, h = level->height, d = level->depth;
    // the rolls 0 1 2 3 are Zombie, Skeleton, Pig, Creeper
    static const int kindForRoll[5] = { CREATURE_ZOMBIE, CREATURE_SKELETON, CREATURE_PIG, CREATURE_CREEPER, CREATURE_SPIDER };
    int placed = 0;
    for (int a = 0; a < attempts; ++a) {
        if (reportProgress) setNextPhase(a * 100 / steps(attempts));
        int kind = kindForRoll[JRandom_nextIntN(r, 5)];
        int sx = JRandom_nextIntN(r, w);
        float f1 = JRandom_nextFloat(r);
        float f2 = JRandom_nextFloat(r);
        int sy = (int)(fminf(f1, f2) * (float)d);
        int sz = JRandom_nextIntN(r, h);
        if (Level_isSolidTile(level, sx, sy, sz)) continue;
        int siteId = Level_getTile(level, sx, sy, sz);
        if (siteId > 0 && gTiles[siteId] && gTiles[siteId]->liquidType != LIQUID_NONE) continue;
        if (Level_isLit(level, sx, sy, sz) && JRandom_nextIntN(r, 5) != 0) continue;

        for (int cluster = 0; cluster < 3; ++cluster) {
            int x = sx, y = sy, z = sz;
            for (int step = 0; step < 3; ++step) {
                x += nextIntDiff(r, 6);
                z += nextIntDiff(r, 6);
                y += nextIntDiff(r, 1);
                if (x < 0 || z < 1 || y < 0 || y >= d - 2 || x >= w || z >= h) continue;
                if (!Level_isSolidTile(level, x, y - 1, z)) continue;
                if (Level_isSolidTile(level, x, y, z) || Level_isSolidTile(level, x, y + 1, z)) continue;
                float fx = (float)x + 0.5f;
                float fy = (float)y + 1.0f;
                float fz = (float)z + 0.5f;
                float dx, dy, dz;
                if (referenceEntity) {
                    dx = fx - referenceEntity->x;
                    dy = fy - referenceEntity->y;
                    dz = fz - referenceEntity->z;
                } else {
                    dx = fx - (float)level->xSpawn;
                    dy = fy - (float)level->ySpawn;
                    dz = fz - (float)level->zSpawn;
                }
                if (dx * dx + dy * dy + dz * dz < 256.0f) continue;
                if (Minecraft_spawnMob(level, kind, fx, fy, fz)) placed++;
            }
        }
    }
    return placed;
}

void LevelGen_generateMap(Level* level) {
    Gen gen;
    gen.level = level;
    gen.w = level->width;
    gen.h = level->height;
    gen.d = level->depth;
    gen.g = 32;
    gen.blocks = level->blocks;
    JRandom_initRandom(&gen.random);

    Minecraft_beginLevelLoading("Generating level");
    Minecraft_levelLoadUpdate("Raising..");
    int* heightmap = raise(&gen);
    Minecraft_levelLoadUpdate("Eroding..");
    erode(&gen, heightmap);
    Minecraft_levelLoadUpdate("Soiling..");
    soil(&gen, heightmap);
    Minecraft_levelLoadUpdate("Carving..");
    carveCaves(&gen);
    placeOre(&gen, TILE_COAL_ORE.id, 90, 1);
    placeOre(&gen, TILE_IRON_ORE.id, 70, 2);
    placeOre(&gen, TILE_GOLD_ORE.id, 50, 3);
    Minecraft_levelLoadUpdate("Watering..");
    addWater(&gen);
    Minecraft_levelLoadUpdate("Melting..");
    addLava(&gen);
    Minecraft_levelLoadUpdate("Growing..");
    addBeaches(&gen, heightmap);
    Minecraft_levelLoadUpdate("Planting..");
    plantFlowers(&gen, heightmap);
    plantMushrooms(&gen, heightmap);

    // setData runs here, light and spawn exist before trees and mobs
    calcLightDepths(level, 0, 0, level->width, level->height);
    Level_findSpawn(level);
    level->createTime = (long long)time(NULL) * 1000;
    snprintf(level->creator, sizeof(level->creator), "%s", Minecraft_getUserName());
    snprintf(level->name, sizeof(level->name), "A Nice World");

    plantTrees(&gen, heightmap);
    free(heightmap);

    Minecraft_levelLoadUpdate("Spawning..");
    int spawned = LevelGen_maybeSpawnMobs(level, gen.w * gen.h * gen.d / 800, NULL, true);
    printf("%d mobs\n", spawned);
}
