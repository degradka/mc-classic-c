// renderer/texture_fx.h: the animated lava and water tiles, 16x16 rgba written into terrain.png every tick

#ifndef TEXTURE_FX_H
#define TEXTURE_FX_H

typedef struct TextureFX {
    unsigned char pixels[16 * 16 * 4];
    int tex;
    float current[256], next[256], heat[256], heata[256];
    void (*step)(struct TextureFX* fx);
} TextureFX;

void LavaFX_init(TextureFX* fx, int tex);
void WaterFX_init(TextureFX* fx, int tex);

#endif
