// level/levelgen/level_gen.c

#include "level_gen.h"
#include "../tile/tile.h"
#include "synth/synth.h"
#include "synth/perlin_noise.h"
#include "synth/distort.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// implemented in minecraft.c, matching Minecraft implementing LevelLoaderListener
extern void Minecraft_beginLevelLoading(const char* title);
extern void Minecraft_levelLoadUpdate(const char* status);
extern void Minecraft_levelLoadProgress(int percent);
extern const char* Minecraft_getUserName(void);

static inline float randf(void) {
    return (float)rand() / ((float)RAND_MAX + 1.0f);
}

// two distorted Perlin fields blended through a third noise field as a
// selector, same formula as c0.0.14a_08.
static int* raiseHeightmap(int width, int height) {
    int* heightmap = (int*)malloc((size_t)width * height * sizeof(int));

    PerlinNoise a1, a2, b1, b2, plain;
    PerlinNoise_init(&a1, 8);
    PerlinNoise_init(&a2, 8);
    PerlinNoise_init(&b1, 8);
    PerlinNoise_init(&b2, 8);
    PerlinNoise_init(&plain, 8);

    Distort distortA, distortB;
    Distort_init(&distortA, &a1.synth, &a2.synth);
    Distort_init(&distortB, &b1.synth, &b2.synth);

    int total = width * height;
    int done = 0;
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            int i = x + y * width;
            if (done % 256 == 0) Minecraft_levelLoadProgress(done * 100 / (total > 1 ? total - 1 : 1));
            done++;

            double d14 = distortA.synth.getValue(&distortA.synth, x * 1.3, y * 1.3) / 8.0 - 8.0;
            double d16 = distortB.synth.getValue(&distortB.synth, x * 1.3, y * 1.3) / 6.0 + 6.0;
            double d18 = plain.synth.getValue(&plain.synth, x, y) / 8.0;
            if (d18 > 0.0) d16 = d14;
            double d20 = (d14 > d16 ? d14 : d16) / 2.0;
            if (d20 < 0.0) d20 = d20 / 2.0;
            heightmap[i] = (int)d20;
        }
    }

    PerlinNoise_destroy(&a1); PerlinNoise_destroy(&a2);
    PerlinNoise_destroy(&b1); PerlinNoise_destroy(&b2);
    PerlinNoise_destroy(&plain);

    return heightmap;
}

// terraces patches of the heightmap to an even parity, matching c0.0.13a_03's
// separate "Eroding.." pass over the freshly raised heightmap
static void erodeHeightmap(int width, int height, int* heightmap) {
    PerlinNoise c1, c2, d1, d2;
    PerlinNoise_init(&c1, 8);
    PerlinNoise_init(&c2, 8);
    PerlinNoise_init(&d1, 8);
    PerlinNoise_init(&d2, 8);

    Distort distortC, distortD;
    Distort_init(&distortC, &c1.synth, &c2.synth);
    Distort_init(&distortD, &d1.synth, &d2.synth);

    int total = width * height;
    int done = 0;
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            int i = x + y * width;
            if (done % 256 == 0) Minecraft_levelLoadProgress(done * 100 / (total > 1 ? total - 1 : 1));
            done++;

            double d13 = distortC.synth.getValue(&distortC.synth, x * 2, y * 2) / 8.0;
            int erodeFlag = (distortD.synth.getValue(&distortD.synth, x * 2, y * 2) > 0.0) ? 1 : 0;
            if (d13 > 2.0) {
                int v = heightmap[i];
                heightmap[i] = (((v - erodeFlag) / 2) << 1) + erodeFlag;
            }
        }
    }

    PerlinNoise_destroy(&c1); PerlinNoise_destroy(&c2);
    PerlinNoise_destroy(&d1); PerlinNoise_destroy(&d2);
}

// fills dirt/rock per column from the heightmap (grass itself gets placed
// later, by the beach pass overwriting whatever ends up exposed at the
// surface). Rock depth is noise-driven per column, same as c0.0.14a_08, and
// the result gets written back into the heightmap for the beach/tree passes.
static void buildBlocks(Level* level, int* heightmap) {
    const int w = level->width, h = level->height, d = level->depth;

    PerlinNoise rockNoise;
    PerlinNoise_init(&rockNoise, 8);

    for (int x = 0; x < w; ++x) {
        for (int z = 0; z < h; ++z) {
            int i = x + z * w;
            int rockDepth = (int)(rockNoise.synth.getValue(&rockNoise.synth, x, z) / 24.0) - 4;
            int surfaceY = heightmap[i] + d / 2;
            int rockY = surfaceY + rockDepth;
            heightmap[i] = surfaceY > rockY ? surfaceY : rockY;

            for (int y = 0; y < d; ++y) {
                int idx = (y * h + z) * w + x;
                int id = 0;
                if (y <= surfaceY) id = TILE_DIRT.id;
                if (y <= rockY) id = TILE_ROCK.id;
                level->blocks[idx] = (byte)id;
            }
        }
    }

    PerlinNoise_destroy(&rockNoise);
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
        if (i % 100 == 0) Minecraft_levelLoadProgress(i * 100 / (count > 1 ? count - 1 : 1));
    }
}

// New in c0.0.14a_08: ore veins, reusing the same tunnel walk algorithm as
// carveTunnels but replacing Rock with ore instead of clearing to air. Count
// and vein size both scale with `percent` (a relative rarity weight, not a
// depth restriction despite how it reads at a glance): Coal is the most
// common and has the biggest veins, Gold the rarest and smallest.
static void placeOreVein(Level* level, int oreId, int percent) {
    const int w = level->width, h = level->height, d = level->depth;
    int count = w * h * d / 256 / 64 * percent / 100;

    for (int i = 0; i < count; ++i) {
        float x = randf() * w;
        float y = randf() * d;
        float z = randf() * h;
        int length = (int)((randf() + randf()) * 75.0f * percent / 100.0f);
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

            float size = (float)(sin(l * M_PI / length) * percent / 100.0 + 1.0);

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
                                level->blocks[ii] = (byte)oreId;
                            }
                        }
                    }
                }
            }
        }
        if (i % 100 == 0) Minecraft_levelLoadProgress(i * 100 / (count > 1 ? count - 1 : 1));
    }
}

// New in c0.0.14a_08: replaces the grass top tile with Sand or Gravel at or
// below water level, based on two independent 8 octave Perlin fields.
static void growBeaches(Level* level, const int* heightmap) {
    PerlinNoise sandNoise, gravelNoise;
    PerlinNoise_init(&sandNoise, 8);
    PerlinNoise_init(&gravelNoise, 8);

    const int w = level->width, h = level->height, d = level->depth;
    for (int x = 0; x < w; ++x) {
        Minecraft_levelLoadProgress(x * 100 / (w > 1 ? w - 1 : 1));
        for (int z = 0; z < h; ++z) {
            int isSand   = sandNoise.synth.getValue(&sandNoise.synth, x, z) > 8.0;
            int isGravel = gravelNoise.synth.getValue(&gravelNoise.synth, x, z) > 12.0;

            // heightmap already holds an absolute Y here, buildBlocks wrote
            // it back with the depth/2 offset baked in
            int surfaceY = heightmap[x + z * w];
            int aboveIdx = ((surfaceY + 1) * h + z) * w + x;
            if (level->blocks[aboveIdx] != 0) continue; // covered, leave alone

            int id = TILE_GRASS.id;
            if (surfaceY <= d / 2 - 1 && isGravel) id = TILE_GRAVEL.id;
            if (surfaceY <= d / 2 - 1 && isSand)   id = TILE_SAND.id;
            level->blocks[(surfaceY * h + z) * w + x] = (byte)id;
        }
    }

    PerlinNoise_destroy(&sandNoise);
    PerlinNoise_destroy(&gravelNoise);
}

// New in c0.0.14a_08: real tree generation, replacing the previously
// treeless world gen entirely. Random walk site search, 4-5 block trunks,
// a 3x3 leaf canopy that drops its 4 diagonal corners only on the very top
// layer for a "+" shaped cap.
static void plantTrees(Level* level, const int* heightmap) {
    const int w = level->width, h = level->height, d = level->depth;
    int attempts = w * h / 4000;

    for (int a = 0; a < attempts; ++a) {
        if (a % 20 == 0) Minecraft_levelLoadProgress(a * 100 / (attempts > 1 ? attempts - 1 : 1));

        int cx = rand() % w;
        int cz = rand() % h;
        for (int outer = 0; outer < 20; ++outer) {
            int x = cx, z = cz;
            for (int inner = 0; inner < 20; ++inner) {
                x += (rand() % 6) - (rand() % 6);
                z += (rand() % 6) - (rand() % 6);
                if (x < 0 || z < 0 || x >= w || z >= h) continue;

                int baseY = heightmap[x + z * w] + 1; // heightmap already absolute here
                int trunkHeight = rand() % 2 + 4;

                int canPlace = 1;
                for (int ly = baseY; ly <= baseY + 1 + trunkHeight && canPlace; ++ly) {
                    int radius = (ly >= baseY + 1 + trunkHeight - 2) ? 2 : 1;
                    for (int lx = x - radius; lx <= x + radius && canPlace; ++lx) {
                        for (int lz = z - radius; lz <= z + radius && canPlace; ++lz) {
                            if (lx < 0 || ly < 0 || lz < 0 || lx >= w || ly >= d || lz >= h) { canPlace = 0; break; }
                            if (level->blocks[(ly * h + lz) * w + lx] != 0) { canPlace = 0; break; }
                        }
                    }
                }

                if (canPlace &&
                    level->blocks[((baseY - 1) * h + z) * w + x] == TILE_GRASS.id &&
                    baseY < d - trunkHeight - 1) {
                    level->blocks[((baseY - 1) * h + z) * w + x] = TILE_DIRT.id;

                    int topY = baseY + trunkHeight;
                    for (int ly = topY - 2; ly <= topY; ++ly) {
                        int fromTop = topY - ly; // 0 only on the very top canopy layer
                        for (int lx = x - 1; lx <= x + 1; ++lx) {
                            int dx = lx - x;
                            for (int lz = z - 1; lz <= z + 1; ++lz) {
                                int dz = lz - z;
                                if (fromTop != 0 || abs(dx) != 1 || abs(dz) != 1) {
                                    level->blocks[(ly * h + lz) * w + lx] = TILE_LEAVES.id;
                                }
                            }
                        }
                    }
                    for (int ly = 0; ly < trunkHeight; ++ly) {
                        level->blocks[((baseY + ly) * h + z) * w + x] = TILE_LOG.id;
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
    // c0.0.13a_03 seeds interior lakes far more often: divisor dropped from
    // 5000 to 200, about 25x more random seed attempts on a 256x256 map
    int count = level->width * level->height / 200;
    for (int i = 0; i < count; ++i) {
        if (i % 20 == 0) Minecraft_levelLoadProgress(i * 100 / (count > 1 ? count - 1 : 1));
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
        if (i % 100 == 0) Minecraft_levelLoadProgress(i * 100 / (total > 1 ? total - 1 : 1));
        int x = rand() % level->width;
        // c0.0.13a_03 keeps lava 4 blocks further from the water table than
        // before, capping its spawn depth at depth/2 - 4 instead of depth/2
        int y = rand() % (level->depth / 2 - 4);
        int z = rand() % level->height;
        if (level->blocks[(y * level->height + z) * level->width + x] == 0) {
            lavaCount++;
            floodFillLiquid(level, x, y, z, 0, TILE_CALM_LAVA.id);
        }
    }
    printf("LavaCount: %d\n", lavaCount);
}

void LevelGen_generateMap(Level* level) {
    Minecraft_beginLevelLoading("Generating level");

    Minecraft_levelLoadUpdate("Raising..");
    int* heightmap = raiseHeightmap(level->width, level->height);

    Minecraft_levelLoadUpdate("Eroding..");
    erodeHeightmap(level->width, level->height, heightmap);

    Minecraft_levelLoadUpdate("Soiling..");
    buildBlocks(level, heightmap);

    Minecraft_levelLoadUpdate("Carving..");
    carveTunnels(level);
    placeOreVein(level, TILE_COAL_ORE.id, 90);
    placeOreVein(level, TILE_IRON_ORE.id, 70);
    placeOreVein(level, TILE_GOLD_ORE.id, 50);

    Minecraft_levelLoadUpdate("Watering..");
    addWater(level);

    Minecraft_levelLoadUpdate("Melting..");
    addLava(level);

    Minecraft_levelLoadUpdate("Growing..");
    growBeaches(level, heightmap);

    Minecraft_levelLoadUpdate("Planting..");
    plantTrees(level, heightmap);
    free(heightmap);

    level->createTime = (long long)time(NULL) * 1000;
    snprintf(level->creator, sizeof(level->creator), "%s", Minecraft_getUserName());
    snprintf(level->name, sizeof(level->name), "A Nice World");
}
