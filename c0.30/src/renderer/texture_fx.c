// renderer/texture_fx.c: the animated water and lava

#include "texture_fx.h"
#include "../math_helper.h"
#include "../jrandom.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// wraps into 0..15, correct even for negative v since -1 & 15 == 15
static int wrap16(int v) { return v & 15; }

static void Lava_toRGBA(LavaTextureFX* fx) {
    for (int i = 0; i < TEXTURE_FX_SIZE * TEXTURE_FX_SIZE; ++i) {
        float t = fx->current[i] * 2.0f;
        if (t > 1.0f) t = 1.0f;
        if (t < 0.0f) t = 0.0f;
        int r = (int)(t * 100.0f + 155.0f);
        int g = (int)(t * t * 255.0f);
        int b = (int)(t * t * t * t * 128.0f);
        if (fx->base.anaglyph) {
            int nr = (r * 30 + g * 59 + b * 11) / 100;
            int ng = (r * 30 + g * 70) / 100;
            int nb = (r * 30 + b * 70) / 100;
            r = nr; g = ng; b = nb;
        }
        fx->base.pixels[i * 4 + 0] = (unsigned char)r;
        fx->base.pixels[i * 4 + 1] = (unsigned char)g;
        fx->base.pixels[i * 4 + 2] = (unsigned char)b;
        fx->base.pixels[i * 4 + 3] = 255;
    }
}

static void Lava_tick(TextureFX* base) {
    LavaTextureFX* fx = (LavaTextureFX*)base;

    for (int x = 0; x < TEXTURE_FX_SIZE; ++x) {
        for (int y = 0; y < TEXTURE_FX_SIZE; ++y) {
            float f = 0.0f;
            int i = (int)(MathHelper_sin((float)y * 3.1415927f * 2.0f / 16.0f) * 1.2f);
            int j = (int)(MathHelper_sin((float)x * 3.1415927f * 2.0f / 16.0f) * 1.2f);

            for (int k = x - 1; k <= x + 1; ++k) {
                for (int m = y - 1; m <= y + 1; ++m) {
                    int n = wrap16(k + i);
                    int i1 = wrap16(m + j);
                    f += fx->current[n + (i1 << 4)];
                }
            }

            // the 2x2 block with (x,y) as its corner
            float heatAvg = (fx->heat[wrap16(x) + (wrap16(y) << 4)]
                            + fx->heat[wrap16(x + 1) + (wrap16(y) << 4)]
                            + fx->heat[wrap16(x + 1) + (wrap16(y + 1) << 4)]
                            + fx->heat[wrap16(x) + (wrap16(y + 1) << 4)]) / 4.0f;

            fx->next[x + (y << 4)] = f / 10.0f + heatAvg * 0.8f;

            int idx = x + (y << 4);
            fx->heat[idx] = fx->heat[idx] + fx->heatVelocity[idx] * 0.01f;
            if (fx->heat[idx] < 0.0f) fx->heat[idx] = 0.0f;
            fx->heatVelocity[idx] = fx->heatVelocity[idx] - 0.06f;
            if (JRandom_mathRandom() < 0.005) fx->heatVelocity[idx] = 1.5f;
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
        int r = (int)(32.0f + t2 * 32.0f);
        int g = (int)(50.0f + t2 * 64.0f);
        int b = 255;
        if (fx->base.anaglyph) {
            b = (r * 30 + g * 59 + 2805) / 100;
            int ng = (r * 30 + g * 70) / 100;
            int nb = (r * 30 + 17850) / 100;
            r = b; g = ng; b = nb;
        }
        fx->base.pixels[i * 4 + 0] = (unsigned char)r;
        fx->base.pixels[i * 4 + 1] = (unsigned char)g;
        fx->base.pixels[i * 4 + 2] = (unsigned char)b;
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
            if (JRandom_mathRandom() < 0.05) fx->velocity[idx] = 0.5f;
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
    fx->base.anaglyph = false;
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
    fx->base.anaglyph = false;
    fx->base.tick = Water_tick;
    Water_toRGBA(fx);
}
