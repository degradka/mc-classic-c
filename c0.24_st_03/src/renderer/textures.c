// renderer/textures.c

#include "textures.h"
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static bool gAnaglyph = false;

#define MAX_TEXTURES 64
static struct { char path[128]; GLuint id; } gTextures[MAX_TEXTURES];
static int gTextureCount = 0;

// Textures.setupTexture
static bool upload(const char* path, GLuint tex) {
    int w, h, ch;
    unsigned char* img = stbi_load(path, &w, &h, &ch, STBI_rgb_alpha);
    if (!img) { fprintf(stderr, "Failed to load texture %s\n", path); return false; }
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (gAnaglyph) {
        for (int i = 0; i < w * h; ++i) {
            unsigned char* px = img + i * 4;
            int r = px[0], g = px[1], b = px[2];
            px[0] = (unsigned char)((r * 30 + g * 59 + b * 11) / 100);
            px[1] = (unsigned char)((r * 30 + g * 70) / 100);
            px[2] = (unsigned char)((r * 30 + b * 70) / 100);
        }
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    stbi_image_free(img);
    return true;
}

void Textures_setAnaglyph(bool enabled) {
    if (gAnaglyph == enabled) return;
    gAnaglyph = enabled;
    for (int i = 0; i < gTextureCount; ++i) upload(gTextures[i].path, gTextures[i].id);
}

GLuint loadTexture(const char* path, int filterMode) {
    (void)filterMode;
    for (int i = 0; i < gTextureCount; ++i) {
        if (strcmp(gTextures[i].path, path) == 0) return gTextures[i].id;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) { fprintf(stderr, "Failed to generate texture id\n"); return 0; }
    if (!upload(path, tex)) return 0;
    if (gTextureCount < MAX_TEXTURES) {
        snprintf(gTextures[gTextureCount].path, sizeof(gTextures[0].path), "%s", path);
        gTextures[gTextureCount].id = tex;
        gTextureCount++;
    }
    return tex;
}
