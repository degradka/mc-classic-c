// renderer/texture_fx.h: animated water/lava textures, new in c0.0.19a_04.
// Each registered effect owns a 16x16 RGBA patch that gets re-simulated once
// per game tick and re-uploaded directly into the live terrain.png texture
// at that tile's exact atlas cell. Matches com.mojang.minecraft.a.a.a
// (Lava) and .b (Water).

#ifndef RENDERER_TEXTURE_FX_H
#define RENDERER_TEXTURE_FX_H

#define TEXTURE_FX_SIZE 16

typedef struct TextureFX {
    unsigned char pixels[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE * 4]; // RGBA
    int tileId; // which terrain.png atlas cell this patches
    void (*tick)(struct TextureFX* self);
} TextureFX;

typedef struct {
    TextureFX base;
    // current/next point at bufA/bufB, swapped each tick rather than copied
    float bufA[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE];
    float bufB[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE];
    float* current;
    float* next;
    float heat[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE];
    float heatVelocity[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE];
} LavaTextureFX;

typedef struct {
    TextureFX base;
    float bufA[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE];
    float bufB[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE];
    float* current;
    float* next;
    float accum[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE];
    float velocity[TEXTURE_FX_SIZE * TEXTURE_FX_SIZE];
} WaterTextureFX;

void LavaTextureFX_init(LavaTextureFX* fx, int tileId);
void WaterTextureFX_init(WaterTextureFX* fx, int tileId);

#endif
