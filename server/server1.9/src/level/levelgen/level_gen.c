// level/levelgen/level_gen.c

#include "level_gen.h"
#include "../tile/tile.h"
#include "synth/synth.h"
#include "synth/perlin_noise.h"
#include "synth/distort.h"
#include "../../log.h"
#include "../../jtime.h"
#include "../../math_helper.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define W 256
#define H 256
#define D 64
#define G 32

typedef struct {
    int8_t* blocks;
    JRandom random;
} Gen;

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

    int* heightmap = (int*)malloc((size_t)W * H * sizeof(int));
    const float scale = 1.3f;
    for (int x = 0; x < W; ++x) {
        for (int z = 0; z < H; ++z) {
            double lowH = low.synth.getValue(&low.synth, (double)((float)x * scale), (double)((float)z * scale)) / 6.0 + (double)-4;
            double highH = high.synth.getValue(&high.synth, (double)((float)x * scale), (double)((float)z * scale)) / 5.0 + 10.0 + (double)-4;
            double pick = selector.synth.getValue(&selector.synth, x, z) / 8.0;
            if (pick > 0.0) highH = lowH;
            double height = fmax(lowH, highH) / 2.0;
            if (height < 0.0) height *= 0.8;
            heightmap[x + z * W] = (int)height;
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
    for (int x = 0; x < W; ++x) {
        for (int z = 0; z < H; ++z) {
            double a = amount.synth.getValue(&amount.synth, x << 1, z << 1) / 8.0;
            int odd = parity.synth.getValue(&parity.synth, x << 1, z << 1) > 0.0 ? 1 : 0;
            if (!(a > 2.0)) continue;
            int hgt = heightmap[x + z * W];
            heightmap[x + z * W] = ((hgt - odd) / 2 << 1) + odd;
        }
    }
    PerlinNoise_destroy(&c1); PerlinNoise_destroy(&c2);
    PerlinNoise_destroy(&d1); PerlinNoise_destroy(&d2);
}

static void soil(Gen* gen, int* heightmap) {
    PerlinNoise rockNoise;
    PerlinNoise_init(&rockNoise, &gen->random, 8);
    for (int x = 0; x < W; ++x) {
        for (int z = 0; z < H; ++z) {
            int rockDepth = (int)(rockNoise.synth.getValue(&rockNoise.synth, x, z) / 24.0) - 4;
            int dirtTop = heightmap[x + z * W] + G;
            int rockTop = dirtTop + rockDepth;
            int top = dirtTop > rockTop ? dirtTop : rockTop;
            if (top > D - 2) top = D - 2;
            if (top < 1) top = 1;
            heightmap[x + z * W] = top;
            for (int y = 0; y < D; ++y) {
                int id = 0;
                if (y <= dirtTop) id = TILE_DIRT;
                if (y <= rockTop) id = TILE_ROCK;
                if (y == 0) id = TILE_LAVA;
                gen->blocks[(y * H + z) * W + x] = (int8_t)id;
            }
        }
    }
    PerlinNoise_destroy(&rockNoise);
}

static void carveCaves(Gen* gen) {
    JRandom* r = &gen->random;
    int count = W * H * D / 256 / 64 << 1;
    for (int i = 0; i < count; ++i) {
        float x = JRandom_nextFloat(r) * (float)W;
        float y = JRandom_nextFloat(r) * (float)D;
        float z = JRandom_nextFloat(r) * (float)H;
        int length = (int)((JRandom_nextFloat(r) + JRandom_nextFloat(r)) * 200.0f);
        float yaw = JRandom_nextFloat(r) * 3.1415927f * 2.0f;
        float yawSpeed = 0.0f;
        float pitch = JRandom_nextFloat(r) * 3.1415927f * 2.0f;
        float pitchSpeed = 0.0f;
        float radiusScale = JRandom_nextFloat(r) * JRandom_nextFloat(r);
        for (int step = 0; step < length; ++step) {
            x += MathHelper_sin(yaw) * MathHelper_cos(pitch);
            z += MathHelper_cos(yaw) * MathHelper_cos(pitch);
            y += MathHelper_sin(pitch);
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
            float radius = ((float)D - cy) / (float)D;
            radius = 1.2f + (radius * 3.5f + 1.0f) * radiusScale;
            radius = MathHelper_sin((float)step * 3.1415927f / (float)length) * radius;
            for (int bx = (int)(cx - radius); bx <= (int)(cx + radius); ++bx) {
                for (int by = (int)(cy - radius); by <= (int)(cy + radius); ++by) {
                    for (int bz = (int)(cz - radius); bz <= (int)(cz + radius); ++bz) {
                        float dx = (float)bx - cx;
                        float dy = (float)by - cy;
                        float dz = (float)bz - cz;
                        float dist = dx * dx + dy * dy * 2.0f + dz * dz;
                        if (!(dist < radius * radius) || bx < 1 || by < 1 || bz < 1 || bx >= W - 1 || by >= D - 1 || bz >= H - 1) continue;
                        int idx = (by * H + bz) * W + bx;
                        if (gen->blocks[idx] != TILE_ROCK) continue;
                        gen->blocks[idx] = 0;
                    }
                }
            }
        }
    }
}

// ore veins wandering through rock
static void placeOre(Gen* gen, int oreId, int percent) {
    JRandom* r = &gen->random;
    int count = W * H * D / 256 / 64 * percent / 100;
    for (int i = 0; i < count; ++i) {
        float x = JRandom_nextFloat(r) * (float)W;
        float y = JRandom_nextFloat(r) * (float)D;
        float z = JRandom_nextFloat(r) * (float)H;
        int length = (int)((JRandom_nextFloat(r) + JRandom_nextFloat(r)) * 75.0f * (float)percent / 100.0f);
        float yaw = JRandom_nextFloat(r) * 3.1415927f * 2.0f;
        float yawSpeed = 0.0f;
        float pitch = JRandom_nextFloat(r) * 3.1415927f * 2.0f;
        float pitchSpeed = 0.0f;
        for (int step = 0; step < length; ++step) {
            x += MathHelper_sin(yaw) * MathHelper_cos(pitch);
            z += MathHelper_cos(yaw) * MathHelper_cos(pitch);
            y += MathHelper_sin(pitch);
            yaw += yawSpeed * 0.2f;
            yawSpeed *= 0.9f;
            yawSpeed += nextFloatDiff(r);
            pitch += pitchSpeed * 0.5f;
            pitch *= 0.5f;
            pitchSpeed *= 0.9f;
            pitchSpeed += nextFloatDiff(r);
            float radius = MathHelper_sin((float)step * 3.1415927f / (float)length) * (float)percent / 100.0f + 1.0f;
            for (int bx = (int)(x - radius); bx <= (int)(x + radius); ++bx) {
                for (int by = (int)(y - radius); by <= (int)(y + radius); ++by) {
                    for (int bz = (int)(z - radius); bz <= (int)(z + radius); ++bz) {
                        float dx = (float)bx - x;
                        float dy = (float)by - y;
                        float dz = (float)bz - z;
                        float dist = dx * dx + dy * dy * 2.0f + dz * dz;
                        if (!(dist < radius * radius) || bx < 1 || by < 1 || bz < 1 || bx >= W - 1 || by >= D - 1 || bz >= H - 1) continue;
                        int idx = (by * H + bz) * W + bx;
                        if (gen->blocks[idx] != TILE_ROCK) continue;
                        gen->blocks[idx] = (int8_t)oreId;
                    }
                }
            }
        }
    }
}

// lava poured over water turns the water below it to rock
static void floodFillLiquid(Gen* gen, int x, int y, int z, int target) {
    const int upStep = W * H;
    int8_t* blocks = gen->blocks;
    int capacity = 0x100000;
    int* stack = (int*)malloc((size_t)capacity * sizeof(int));
    int sp = 0;
    stack[sp++] = (y * H + z) * W + x;
    while (sp > 0) {
        int cl = stack[--sp];
        int z0 = (cl / W) % H;
        int y0 = cl / upStep;
        int x0 = cl % W;
        while (x0 > 0 && blocks[cl - 1] == 0) { x0--; cl--; }
        int x1 = x0;
        while (x1 < W && blocks[cl + (x1 - x0)] == 0) x1++;
        int lastNorth = 0, lastSouth = 0, lastBelow = 0;
        for (int xx = x0; xx < x1; ++xx) {
            blocks[cl] = (int8_t)target;
            if (z0 > 0) {
                int north = blocks[cl - W] == 0;
                if (north && !lastNorth) {
                    if (sp == capacity) { capacity *= 2; stack = (int*)realloc(stack, (size_t)capacity * sizeof(int)); }
                    stack[sp++] = cl - W;
                }
                lastNorth = north;
            }
            if (z0 < H - 1) {
                int south = blocks[cl + W] == 0;
                if (south && !lastSouth) {
                    if (sp == capacity) { capacity *= 2; stack = (int*)realloc(stack, (size_t)capacity * sizeof(int)); }
                    stack[sp++] = cl + W;
                }
                lastSouth = south;
            }
            if (y0 > 0) {
                int belowId = blocks[cl - upStep];
                if ((target == TILE_LAVA || target == TILE_CALM_LAVA) && (belowId == TILE_WATER || belowId == TILE_CALM_WATER)) {
                    blocks[cl - upStep] = TILE_ROCK;
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
}

static void addWater(Gen* gen) {
    JRandom* r = &gen->random;
    for (int x = 0; x < W; ++x) {
        floodFillLiquid(gen, x, D / 2 - 1, 0, TILE_CALM_WATER);
        floodFillLiquid(gen, x, D / 2 - 1, H - 1, TILE_CALM_WATER);
    }
    for (int z = 0; z < H; ++z) {
        floodFillLiquid(gen, 0, D / 2 - 1, z, TILE_CALM_WATER);
        floodFillLiquid(gen, W - 1, D / 2 - 1, z, TILE_CALM_WATER);
    }
    int count = W * H / 8000;
    for (int i = 0; i < count; ++i) {
        int x = JRandom_nextIntN(r, W);
        int y = G - 1 - JRandom_nextIntN(r, 2);
        int z = JRandom_nextIntN(r, H);
        if (gen->blocks[(y * H + z) * W + x] != 0) continue;
        floodFillLiquid(gen, x, y, z, TILE_CALM_WATER);
    }
}

static void addLava(Gen* gen) {
    JRandom* r = &gen->random;
    int count = W * H * D / 20000;
    for (int i = 0; i < count; ++i) {
        int x = JRandom_nextIntN(r, W);
        int y = (int)(JRandom_nextFloat(r) * JRandom_nextFloat(r) * (float)(G - 3));
        int z = JRandom_nextIntN(r, H);
        if (gen->blocks[(y * H + z) * W + x] != 0) continue;
        floodFillLiquid(gen, x, y, z, TILE_CALM_LAVA);
    }
}

// gravel on lake and sea beds, grass on dry ground, sand on low dry ground
static void addBeaches(Gen* gen, const int* heightmap) {
    PerlinNoise sandNoise, gravelNoise;
    PerlinNoise_init(&sandNoise, &gen->random, 8);
    PerlinNoise_init(&gravelNoise, &gen->random, 8);
    for (int x = 0; x < W; ++x) {
        for (int z = 0; z < H; ++z) {
            bool sand = sandNoise.synth.getValue(&sandNoise.synth, x, z) > 8.0;
            bool gravel = gravelNoise.synth.getValue(&gravelNoise.synth, x, z) > 12.0;
            int top = heightmap[x + z * W];
            int idx = (top * H + z) * W + x;
            int above = gen->blocks[((top + 1) * H + z) * W + x] & 0xFF;
            if ((above == TILE_WATER || above == TILE_CALM_WATER) && top <= D / 2 - 1 && gravel) {
                gen->blocks[idx] = TILE_GRAVEL;
            }
            if (above != 0) continue;
            int id = TILE_GRASS;
            if (top <= D / 2 - 1 && sand) id = TILE_SAND;
            gen->blocks[idx] = (int8_t)id;
        }
    }
    PerlinNoise_destroy(&sandNoise);
    PerlinNoise_destroy(&gravelNoise);
}

static void plantFlowers(Gen* gen, const int* heightmap) {
    JRandom* r = &gen->random;
    int count = W * H / 3000;
    for (int i = 0; i < count; ++i) {
        int kind = JRandom_nextIntN(r, 2);
        int bx = JRandom_nextIntN(r, W);
        int bz = JRandom_nextIntN(r, H);
        for (int c = 0; c < 10; ++c) {
            int x = bx, z = bz;
            for (int s = 0; s < 5; ++s) {
                x += nextIntDiff(r, 6);
                z += nextIntDiff(r, 6);
                if (x < 0 || z < 0 || x >= W || z >= H) continue;
                int y = heightmap[x + z * W] + 1;
                if ((gen->blocks[(y * H + z) * W + x] & 0xFF) != 0) continue;
                if ((gen->blocks[((y - 1) * H + z) * W + x] & 0xFF) != TILE_GRASS) continue;
                gen->blocks[(y * H + z) * W + x] = (int8_t)(kind == 0 ? TILE_DANDELION : TILE_ROSE);
            }
        }
    }
}

static void plantMushrooms(Gen* gen, const int* heightmap) {
    JRandom* r = &gen->random;
    int count = W * H * D / 2000;
    for (int i = 0; i < count; ++i) {
        int kind = JRandom_nextIntN(r, 2);
        int bx = JRandom_nextIntN(r, W);
        int by = JRandom_nextIntN(r, D);
        int bz = JRandom_nextIntN(r, H);
        for (int c = 0; c < 20; ++c) {
            int x = bx, y = by, z = bz;
            for (int s = 0; s < 5; ++s) {
                x += nextIntDiff(r, 6);
                y += nextIntDiff(r, 2);
                z += nextIntDiff(r, 6);
                if (x < 0 || z < 0 || y < 1 || x >= W || z >= H || y >= heightmap[x + z * W] - 1) continue;
                if ((gen->blocks[(y * H + z) * W + x] & 0xFF) != 0) continue;
                if ((gen->blocks[((y - 1) * H + z) * W + x] & 0xFF) != TILE_ROCK) continue;
                gen->blocks[(y * H + z) * W + x] = (int8_t)(kind == 0 ? TILE_BROWN_MUSHROOM : TILE_RED_MUSHROOM);
            }
        }
    }
}

static void plantTrees(Gen* gen, Level* level, const int* heightmap) {
    JRandom* r = &gen->random;
    int count = W * H / 4000;
    for (int i = 0; i < count; ++i) {
        int bx = JRandom_nextIntN(r, W);
        int bz = JRandom_nextIntN(r, H);
        for (int c = 0; c < 20; ++c) {
            int x = bx, z = bz;
            for (int s = 0; s < 20; ++s) {
                x += nextIntDiff(r, 6);
                z += nextIntDiff(r, 6);
                if (x < 0 || z < 0 || x >= W || z >= H) continue;
                int y = heightmap[x + z * W] + 1;
                if (JRandom_nextIntN(r, 4) != 0) continue;
                Level_maybeGrowTree(level, x, y, z);
            }
        }
    }
}

static JStr* newJStr(const char* s) {
    JStr tmp;
    jstr_set(&tmp, s);
    JStr* out = (JStr*)malloc(sizeof *out);
    jstr_copy(out, &tmp);
    return out;
}

Level* LevelGen_generateLevel(const char* creator) {
    MathHelper_init();
    Gen gen;
    JRandom_initRandom(&gen.random);
    Log_info("Generating level");
    gen.blocks = (int8_t*)calloc((size_t)W * H * D + 1, 1);
    int* heightmap = raise(&gen);
    erode(&gen, heightmap);
    soil(&gen, heightmap);
    carveCaves(&gen);
    placeOre(&gen, TILE_COAL_ORE, 90);
    placeOre(&gen, TILE_IRON_ORE, 70);
    placeOre(&gen, TILE_GOLD_ORE, 50);
    addWater(&gen);
    addLava(&gen);
    addBeaches(&gen, heightmap);
    plantFlowers(&gen, heightmap);
    plantMushrooms(&gen, heightmap);

    Level* level = Level_create();
    level->waterLevel = G;
    Level_setData(level, W, D, H, gen.blocks);
    level->createTime = jtime_millis();
    level->creator = newJStr(creator);
    level->name = newJStr("A Nice World");
    plantTrees(&gen, level, heightmap);
    free(heightmap);
    return level;
}
