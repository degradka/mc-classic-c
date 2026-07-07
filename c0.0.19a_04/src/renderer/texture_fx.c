// renderer/texture_fx.c

#include "texture_fx.h"
#include "../jrandom.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void LavaFX_step(TextureFX* fx) {
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            float sum = 0.0f;
            int xo = (int)(sin((double)y * M_PI * 2.0 / 16.0) * (double)1.2f);
            int yo = (int)(sin((double)x * M_PI * 2.0 / 16.0) * (double)1.2f);
            for (int xx = x - 1; xx <= x + 1; ++xx) {
                for (int yy = y - 1; yy <= y + 1; ++yy) {
                    int a = (xx + xo) & 15;
                    int b = (yy + yo) & 15;
                    sum += fx->current[a + (b << 4)];
                }
            }
            fx->next[x + (y << 4)] = sum / 10.0f
                + (fx->heat[(x & 15) + ((y & 15) << 4)] + fx->heat[((x + 1) & 15) + ((y & 15) << 4)]
                   + fx->heat[((x + 1) & 15) + (((y + 1) & 15) << 4)] + fx->heat[(x & 15) + (((y + 1) & 15) << 4)]) / 4.0f * 0.8f;
            fx->heat[x + (y << 4)] += fx->heata[x + (y << 4)] * 0.01f;
            if (fx->heat[x + (y << 4)] < 0.0f) fx->heat[x + (y << 4)] = 0.0f;
            fx->heata[x + (y << 4)] -= 0.06f;
            if (JRandom_mathRandom() < 0.005) fx->heata[x + (y << 4)] = 1.5f;
        }
    }
    float tmp[256];
    memcpy(tmp, fx->next, sizeof tmp);
    memcpy(fx->next, fx->current, sizeof tmp);
    memcpy(fx->current, tmp, sizeof tmp);

    for (int i = 0; i < 256; ++i) {
        float f = fx->current[i];
        if (f > 1.0f) f = 1.0f;
        if (f < 0.0f) f = 0.0f;
        fx->pixels[i << 2]       = (unsigned char)(int)(f * 200.0f + 55.0f);
        fx->pixels[(i << 2) + 1] = (unsigned char)(int)(f * f * 255.0f);
        fx->pixels[(i << 2) + 2] = (unsigned char)(int)(f * f * f * f * 128.0f);
        fx->pixels[(i << 2) + 3] = 255;
    }
}

static void WaterFX_step(TextureFX* fx) {
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            float sum = 0.0f;
            for (int xx = x - 1; xx <= x + 1; ++xx) {
                sum += fx->current[(xx & 15) + ((y & 15) << 4)];
            }
            fx->next[x + (y << 4)] = sum / 3.3f + fx->heat[x + (y << 4)] * 0.8f;
        }
    }
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            fx->heat[x + (y << 4)] += fx->heata[x + (y << 4)] * 0.05f;
            if (fx->heat[x + (y << 4)] < 0.0f) fx->heat[x + (y << 4)] = 0.0f;
            fx->heata[x + (y << 4)] -= 0.1f;
            if (JRandom_mathRandom() < 0.05) fx->heata[x + (y << 4)] = 0.5f;
        }
    }
    float tmp[256];
    memcpy(tmp, fx->next, sizeof tmp);
    memcpy(fx->next, fx->current, sizeof tmp);
    memcpy(fx->current, tmp, sizeof tmp);

    for (int i = 0; i < 256; ++i) {
        float f = fx->current[i];
        if (f > 1.0f) f = 1.0f;
        if (f < 0.0f) f = 0.0f;
        float ff = f * f;
        fx->pixels[i << 2]       = (unsigned char)(int)(32.0f + ff * 32.0f);
        fx->pixels[(i << 2) + 1] = (unsigned char)(int)(50.0f + ff * 64.0f);
        fx->pixels[(i << 2) + 2] = 255;
        fx->pixels[(i << 2) + 3] = (unsigned char)(int)(146.0f + ff * 50.0f);
    }
}

static void init(TextureFX* fx, int tex, void (*step)(TextureFX*)) {
    memset(fx, 0, sizeof *fx);
    fx->tex = tex;
    fx->step = step;
}

void LavaFX_init(TextureFX* fx, int tex) {
    init(fx, tex, LavaFX_step);
}

void WaterFX_init(TextureFX* fx, int tex) {
    init(fx, tex, WaterFX_step);
}
