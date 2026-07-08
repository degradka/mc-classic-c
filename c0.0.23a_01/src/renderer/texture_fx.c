// renderer/texture_fx.c: animated water/lava textures, new in c0.0.19a_04.
// Matches com.mojang.minecraft.a.a.a (Lava) and .b (Water).

#include "texture_fx.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float randf(void) { return (float)rand() / (float)RAND_MAX; }

// wraps into 0..15, correct even for negative v since -1 & 15 == 15
static int wrap16(int v) { return v & 15; }

static void Lava_toRGBA(LavaTextureFX* fx) {
    for (int i = 0; i < TEXTURE_FX_SIZE * TEXTURE_FX_SIZE; ++i) {
        float t = fx->current[i];
        if (t > 1.0f) t = 1.0f;
        if (t < 0.0f) t = 0.0f;
        fx->base.pixels[i * 4 + 0] = (unsigned char)(int)(t * 200.0f + 55.0f);
        fx->base.pixels[i * 4 + 1] = (unsigned char)(int)(t * t * 255.0f);
        fx->base.pixels[i * 4 + 2] = (unsigned char)(int)(t * t * t * t * 128.0f);
        fx->base.pixels[i * 4 + 3] = 255;
    }
}

static void Lava_tick(TextureFX* base) {
    LavaTextureFX* fx = (LavaTextureFX*)base;

    for (int x = 0; x < TEXTURE_FX_SIZE; ++x) {
        for (int y = 0; y < TEXTURE_FX_SIZE; ++y) {
            float f = 0.0f;
            int i = (int)(sin(y * M_PI * 2.0 / TEXTURE_FX_SIZE) * 1.2000000476837158);
            int j = (int)(sin(x * M_PI * 2.0 / TEXTURE_FX_SIZE) * 1.2000000476837158);

            for (int k = x - 1; k <= x + 1; ++k) {
                for (int m = y - 1; m <= y + 1; ++m) {
                    int n = wrap16(k + i);
                    int i1 = wrap16(m + j);
                    f += fx->current[n + (i1 << 4)];
                }
            }

            // heat sample is the 2x2 block with (x,y) as its corner:
            // (x,y), (x+1,y), (x+1,y+1), (x,y+1)
            float heatAvg = (fx->heat[wrap16(x) + (wrap16(y) << 4)]
                            + fx->heat[wrap16(x + 1) + (wrap16(y) << 4)]
                            + fx->heat[wrap16(x + 1) + (wrap16(y + 1) << 4)]
                            + fx->heat[wrap16(x) + (wrap16(y + 1) << 4)]) / 4.0f;

            fx->next[x + (y << 4)] = f / 10.0f + heatAvg * 0.8f;
        }
    }

    for (int x = 0; x < TEXTURE_FX_SIZE; ++x) {
        for (int y = 0; y < TEXTURE_FX_SIZE; ++y) {
            int idx = x + (y << 4);
            // heatVelocity is usually negative between bubble resets, so
            // this pulls heat back down as often as it pushes it up
            fx->heat[idx] = fx->heat[idx] + fx->heatVelocity[idx] * 0.01f;
            if (fx->heat[idx] < 0.0f) fx->heat[idx] = 0.0f;
            fx->heatVelocity[idx] = fx->heatVelocity[idx] - 0.06f;
            if (randf() < 0.005f) fx->heatVelocity[idx] = 1.5f;
        }
    }

    float* swap = fx->next; fx->next = fx->current; fx->current = swap;
    Lava_toRGBA(fx);
}

static void Water_toRGBA(WaterTextureFX* fx) {
    for (int i = 0; i < TEXTURE_FX_SIZE * TEXTURE_FX_SIZE; ++i) {
        float t = fx->current[i];
        if (t > 1.0f) t = 1.0f;
        if (t < 0.0f) t = 0.0f;
        float t2 = t * t;
        fx->base.pixels[i * 4 + 0] = (unsigned char)(int)(32.0f + t2 * 32.0f);
        fx->base.pixels[i * 4 + 1] = (unsigned char)(int)(50.0f + t2 * 64.0f);
        fx->base.pixels[i * 4 + 2] = 255;
        fx->base.pixels[i * 4 + 3] = (unsigned char)(int)(146.0f + t2 * 50.0f);
    }
}

// horizontal-only 3-tap blur (x-1,x,x+1 at the same y), unlike lava's 3x3
static void Water_tick(TextureFX* base) {
    WaterTextureFX* fx = (WaterTextureFX*)base;

    for (int x = 0; x < TEXTURE_FX_SIZE; ++x) {
        for (int y = 0; y < TEXTURE_FX_SIZE; ++y) {
            float f = 0.0f;
            for (int i = x - 1; i <= x + 1; ++i) {
                int j = wrap16(i);
                int k = wrap16(y);
                f += fx->current[j + (k << 4)];
            }
            fx->next[x + (y << 4)] = f / 3.3f + fx->accum[x + (y << 4)] * 0.8f;
        }
    }

    for (int x = 0; x < TEXTURE_FX_SIZE; ++x) {
        for (int y = 0; y < TEXTURE_FX_SIZE; ++y) {
            int idx = x + (y << 4);
            fx->accum[idx] = fx->accum[idx] + fx->velocity[idx] * 0.05f;
            if (fx->accum[idx] < 0.0f) fx->accum[idx] = 0.0f;
            fx->velocity[idx] = fx->velocity[idx] - 0.1f;
            if (randf() < 0.05f) fx->velocity[idx] = 0.5f;
        }
    }

    float* swap = fx->next; fx->next = fx->current; fx->current = swap;
    Water_toRGBA(fx);
}

void LavaTextureFX_init(LavaTextureFX* fx, int tileId) {
    fx->current = fx->bufA;
    fx->next = fx->bufB;
    for (int i = 0; i < TEXTURE_FX_SIZE * TEXTURE_FX_SIZE; ++i) {
        fx->bufA[i] = 0.0f;
        fx->bufB[i] = 0.0f;
        fx->heat[i] = 0.0f;
        fx->heatVelocity[i] = 0.0f;
    }
    fx->base.tileId = tileId;
    fx->base.tick = Lava_tick;
    Lava_toRGBA(fx);
}

void WaterTextureFX_init(WaterTextureFX* fx, int tileId) {
    fx->current = fx->bufA;
    fx->next = fx->bufB;
    for (int i = 0; i < TEXTURE_FX_SIZE * TEXTURE_FX_SIZE; ++i) {
        fx->bufA[i] = 0.0f;
        fx->bufB[i] = 0.0f;
        fx->accum[i] = 0.0f;
        fx->velocity[i] = 0.0f;
    }
    fx->base.tileId = tileId;
    fx->base.tick = Water_tick;
    Water_toRGBA(fx);
}
