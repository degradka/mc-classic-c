// level/levelgen/level_gen.c

#include "level_gen.h"
#include "../tile/tile.h"
#include "../../character/creature.h" // CreatureKind constants
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
// implemented in minecraft.c, spawns into mobs[] for the world gen mob
// population below. Returns false with nothing added if the mob cap is
// reached or the spot turned out not to actually be free
extern bool Minecraft_spawnMob(Level* lvl, int kind, float x, float y, float z);

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

            // c0.24_st_03: both height-bias constants changed from
            // c0.0.23a_01's own /8.0-8.0 and /6.0+6.0 (confirmed via direct
            // source comparison, not an estimate). This shifts the overall
            // terrain height distribution
            double d14 = distortA.synth.getValue(&distortA.synth, x * 1.3, y * 1.3) / 6.0 - 4.0;
            double d16 = distortB.synth.getValue(&distortB.synth, x * 1.3, y * 1.3) / 5.0 + 10.0 - 4.0;
            double d18 = plain.synth.getValue(&plain.synth, x, y) / 8.0;
            if (d18 > 0.0) d16 = d14;
            double d20 = (d14 > d16 ? d14 : d16) / 2.0;
            // c0.0.19a_04: was d20/2.0, now a steeper falloff below sea
            // level, producing a smaller/steeper shoreline (client-only,
            // server's own copy of this generator is byte-identical)
            if (d20 < 0.0) d20 = d20 * 0.8;
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
                // c0.24_st_03: the absolute bottom layer of the map is lava
                // instead of solid rock/bedrock. This is confirmed genuinely
                // new this version (zero bedrock placement anywhere in this
                // jar's own world-gen at all, and c0.0.23a_01's equivalent
                // stage has no such override)
                if (y == 0) id = TILE_LAVA.id;
                level->blocks[idx] = (byte)id;
            }
        }
    }

    PerlinNoise_destroy(&rockNoise);
}

static void carveTunnels(Level* level) {
    const int w = level->width, h = level->height, d = level->depth;
    // c0.24_st_03: cave tunnel count doubled from c0.0.23a_01's own real
    // source (that version's carve count has no such factor at all;
    // confirmed via direct comparison, not an estimate)
    const int count = w * h * d / 256 / 64 * 2;

    for (int i = 0; i < count; ++i) {
        float x = randf() * w;
        float y = randf() * d;
        float z = randf() * h;
        int length = (int)(randf() + randf() * 150.0f);
        float dir1 = randf() * (float)M_PI * 2.0f;
        float dira1 = 0.0f;
        float dir2 = randf() * (float)M_PI * 2.0f;
        float dira2 = 0.0f;
        // c0.24_st_03: one squared-random size scale per tunnel (skews
        // small, occasionally large), feeding into the new per-point size
        // formula below, confirmed genuinely new versus c0.0.23a_01's own
        // fixed 2.5/1.0 constants
        float radiusScale = randf() * randf();

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

            // c0.0.19a_04: skips this path node entirely some of the time,
            // and when not skipped, carves a sphere centered up to 2 blocks
            // off the walker's exact position (each axis independently)
            // instead of dead on it. Together these make caves sparser and
            // more broken up rather than smooth continuous tunnels.
            // c0.24_st_03: the skip chance itself loosened from 30% to 25%
            // (confirmed via direct source comparison against c0.0.23a_01,
            // not an estimate), letting marginally more carve points through
            if (randf() < 0.25f) continue;

            float cx = x + randf() * 4.0f - 2.0f;
            float cy = y + randf() * 4.0f - 2.0f;
            float cz = z + randf() * 4.0f - 2.0f;

            // c0.24_st_03: size now also grows toward the bottom of the map
            // (depthFrac approaches 1 as cy approaches 0) and is scaled by
            // this tunnel's own radiusScale, then enveloped by the same
            // sin(l*PI/length) taper as before. Unlike the old fixed
            // 2.5+1.0 formula, the taper now multiplies the WHOLE size
            // (including the former "+1.0" floor), so tunnels taper all the
            // way to a genuine point at both ends instead of a small stub
            float depthFrac = ((float)d - cy) / (float)d;
            float sizeBase = 1.2f + (depthFrac * 3.5f + 1.0f) * radiusScale;
            float size = (float)sin(l * M_PI / length) * sizeBase;

            for (int xx = (int)(cx - size); xx <= (int)(cx + size); ++xx) {
                for (int yy = (int)(cy - size); yy <= (int)(cy + size); ++yy) {
                    for (int zz = (int)(cz - size); zz <= (int)(cz + size); ++zz) {
                        float xd = xx - cx;
                        float yd = yy - cy;
                        float zd = zz - cz;
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
// treeless world gen entirely. Random walk site search.
// c0.24_st_03: this stage's own inline trunk/canopy construction (4-5 block
// trunk, fixed corner-dropping rule) is confirmed replaced by a scatter
// search that calls the new shared Level_maybeGrowTree for each candidate
// spot instead. This is the same function task #63 added for live sapling
// growth, confirmed via direct source comparison against c0.0.23a_01 (whose
// equivalent stage still has the old construction inlined directly here).
// Attempt count (w*h/4000) and the 20x20 site search itself are unchanged
static void plantTrees(Level* level, const int* heightmap) {
    const int w = level->width, h = level->height;
    int attempts = w * h / 4000;

    for (int a = 0; a < attempts; ++a) {
        if (a % 20 == 0) Minecraft_levelLoadProgress(a * 50 / (attempts > 1 ? attempts - 1 : 1));

        int baseX = rand() % w;
        int baseZ = rand() % h;
        for (int outer = 0; outer < 20; ++outer) {
            int x = baseX, z = baseZ;
            for (int inner = 0; inner < 20; ++inner) {
                x += (rand() % 6) - (rand() % 6);
                z += (rand() % 6) - (rand() % 6);
                if (x < 0 || z < 0 || x >= w || z >= h) continue;

                int y = heightmap[x + z * w] + 1; // heightmap already absolute here
                if (rand() % 4 != 0) continue;
                Level_maybeGrowTree(level, x, y, z);
            }
        }
    }
}

// mushroom placement, confirmed genuinely new this version, with zero
// mushroom references anywhere in c0.0.23a_01's own world gen. Random walk
// site search similar to plantTrees, but placing single brown or red
// mushroom tiles on top of exposed rock instead of a tree shape. The
// heightmap check below matches level/a/a.java's actual mushroom method
// exactly, `n10 >= nArray[n9+n11*n2]-1`, where nArray is the heightmap
// passed into that method as a parameter. This check is not just semantic,
// it is the only upper bound on y at all: y random walks by up to 1 either
// way per step over 5 steps and has nothing else stopping it from drifting
// past the world's own depth, so removing it causes an out of bounds
// level->blocks read
static void plantMushrooms(Level* level, const int* heightmap) {
    const int w = level->width, h = level->height, d = level->depth;
    int attempts = w * h * d / 2000;
    int placed = 0;

    for (int a = 0; a < attempts; ++a) {
        if (a % 50 == 0) Minecraft_levelLoadProgress(a * 50 / (attempts > 1 ? attempts - 1 : 1) + 50);

        int kind = rand() % 2; // 0 = brown, 1 = red
        int bx = rand() % w;
        int by = rand() % d;
        int bz = rand() % h;

        for (int outer = 0; outer < 20; ++outer) {
            int x = bx, y = by, z = bz;
            for (int inner = 0; inner < 5; ++inner) {
                x += (rand() % 6) - (rand() % 6);
                z += (rand() % 6) - (rand() % 6);
                y += (rand() % 2) - (rand() % 2);
                if (x < 0 || z < 0 || y < 1 || x >= w || z >= h) continue;
                if (y >= heightmap[x + z * w] - 1) continue;
                if (level->blocks[(y * h + z) * w + x] != 0) continue;

                int below = level->blocks[((y - 1) * h + z) * w + x];
                if (below != TILE_ROCK.id) continue;

                level->blocks[(y * h + z) * w + x] = (byte)(kind == 0 ? TILE_MUSHROOM_BROWN.id : TILE_MUSHROOM_RED.id);
                placed++;
            }
        }
    }
    printf("Added %d mushrooms\n", placed);
}

// world gen mob population, matches level/a/a.java's own private a(Level)
// exactly, the final "Spawning.." stage. Bytecode confirms this version has
// no manual spawn debug key counterpart at all; real source populates the
// world automatically instead. One random mob type is picked per site, a
// random point anywhere in the level, with y biased toward the bottom via
// min(randf,randf), and skipped unless it is open, not solid, not inside a
// liquid, and unlit 80% of the time, mirroring vanilla's dark cave spawn
// bias. Each surviving site then tries 3 separate short random walks of 3
// steps each, looking for a spot with solid ground directly below and two
// clear tiles above for body and head, spawning the mob there if its own
// bounding box also turns out to be clear
static void spawnMobs(Level* level) {
    const int w = level->width, h = level->height, d = level->depth;
    // real source's n4 0/1/2/3 is Zombie, Skeleton, Pig, Creeper, a
    // different order than this port's own CreatureKind enum of Zombie,
    // Skeleton, Creeper, Pig, mapped explicitly rather than reordering the
    // enum everywhere
    static const int kindForRoll[4] = { CREATURE_ZOMBIE, CREATURE_SKELETON, CREATURE_PIG, CREATURE_CREEPER };

    int attempts = w * h * d / 800;
    int placed = 0;

    for (int a = 0; a < attempts; ++a) {
        if (a % 50 == 0) Minecraft_levelLoadProgress(a * 100 / (attempts > 1 ? attempts - 1 : 1));

        int kind = kindForRoll[rand() % 4];
        int sx = rand() % w;
        int sy = (int)(fminf(randf(), randf()) * (float)d);
        int sz = rand() % h;

        if (Level_isSolidTile(level, sx, sy, sz)) continue;
        int siteId = Level_getTile(level, sx, sy, sz);
        const Tile* siteTile = (siteId > 0 && siteId < 256) ? gTiles[siteId] : NULL;
        if (siteTile && siteTile->liquidType != LIQUID_NONE) continue;
        if (Level_isLit(level, sx, sy, sz) && rand() % 5 != 0) continue;

        for (int cluster = 0; cluster < 3; ++cluster) {
            int x = sx, y = sy, z = sz;
            for (int step = 0; step < 3; ++step) {
                x += (rand() % 6) - (rand() % 6);
                z += (rand() % 6) - (rand() % 6);
                y += (rand() % 2) - (rand() % 2);
                if (x < 0 || z < 1 || y < 0 || y >= d - 2 || x >= w || z >= h) continue;
                if (!Level_isSolidTile(level, x, y - 1, z)) continue;
                if (Level_isSolidTile(level, x, y, z) || Level_isSolidTile(level, x, y + 1, z)) continue;

                if (Minecraft_spawnMob(level, kind, (float)x + 0.5f, (float)y + 1.0f, (float)z + 0.5f)) {
                    placed++;
                }
            }
        }
    }
    printf("%d mobs\n", placed);
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
    // c0.24_st_03: pass count halved (fewer pockets)
    int total = level->width * level->height * level->depth / 20000;
    for (int i = 0; i < total; ++i) {
        if (i % 100 == 0) Minecraft_levelLoadProgress(i * 100 / (total > 1 ? total - 1 : 1));
        int x = rand() % level->width;
        // c0.24_st_03: Y is now squared-random (skews toward 0, i.e. the
        // very bottom of the map) up to waterLevel-3, replacing the old
        // uniform nextInt(depth/2-4) spread. Each pocket lands much closer
        // to the bottom of the world on average, genuinely matching the
        // wiki's "lava layer above bedrock" claim (confirmed via direct
        // source comparison against c0.0.23a_01, not an estimate)
        int y = (int)(randf() * randf() * (Level_getWaterLevel(level) - 3.0f));
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
    plantMushrooms(level, heightmap);
    free(heightmap);

    Minecraft_levelLoadUpdate("Spawning..");
    spawnMobs(level);

    level->createTime = (long long)time(NULL) * 1000;
    snprintf(level->creator, sizeof(level->creator), "%s", Minecraft_getUserName());
    snprintf(level->name, sizeof(level->name), "A Nice World");
}
