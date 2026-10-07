// gui/font.c

#include "font.h"
#include "../renderer/tessellator.h"
#include "../renderer/textures.h"
#include <string.h>
#include <stdlib.h>

#include "stb_image.h"

static inline void rgb_from_int(int c, float* r, float* g, float* b) {
    *r = ((c >> 16) & 0xFF) / 255.0f;
    *g = ((c >>  8) & 0xFF) / 255.0f;
    *b = ( c        & 0xFF) / 255.0f;
}

static inline int color_from_code(int code) {
    int br = (code & 8) << 3;
    int b = (code & 1) * 191 + br;
    int g = ((code & 2) >> 1) * 191 + br;
    int r = ((code & 4) >> 2) * 191 + br;
    return (r << 16) | (g << 8) | b;
}

// anything that isn't 0-9a-f falls back to white
static int code_index(unsigned char c) {
    const char* p = c ? strchr("0123456789abcdef", c) : NULL;
    return p ? (int)(p - "0123456789abcdef") : 15;
}

void Font_init(Font* f, const char* path) {
    memset(f->charWidth, 0, sizeof(f->charWidth));
    f->texture = 0;

    int w, h, ch;
    unsigned char* img = stbi_load(path, &w, &h, &ch, STBI_rgb_alpha);
    if (!img) {
        fprintf(stderr, "Font_init: failed to load %s\n", path);
        return;
    }

    // only the first 128 are measured, the rest stay 0
    for (int i = 0; i < 128; ++i) {
        int xt = i % 16;
        int yt = i / 16;
        int x = 0;
        int emptyColumn = 0;

        for (; x < 8 && !emptyColumn; ++x) {
            int xPixel = xt * 8 + x;
            emptyColumn = 1;
            for (int y = 0; y < 8 && emptyColumn; ++y) {
                int yPixel = yt * 8 + y;
                int idx = (yPixel * w + xPixel) * 4; // RGBA
                // ink is the blue channel over 128, not alpha
                unsigned char blue = img[idx + 2];
                if (blue > 128) emptyColumn = 0;
            }
        }
        if (i == 32) x = 4; // space = half width
        f->charWidth[i] = x;
    }

    f->texture = loadTexture(path, GL_NEAREST);

    stbi_image_free(img);
}

static void Font_draw_internal(Font* f, Tessellator* t, const char* s, int x, int y, int color, int darken) {
    if (!s || !*s || f->texture == 0) return;

    if (darken) color = (color & 0xFCFCFC) >> 2;
    float r,g,b; rgb_from_int(color, &r,&g,&b);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, (int)f->texture);

    Tessellator_begin(t);
    Tessellator_color(t, r, g, b);

    int xo = 0;
    const unsigned char* str = (const unsigned char*)s;
    size_t n = strlen(s);

    for (size_t i = 0; i < n; ++i) {
        unsigned char c = str[i];

        if (c == '&' && i + 1 < n) {
            color = color_from_code(code_index(str[i + 1]));
            if (darken) color = (color & 0xFCFCFC) >> 2;
            rgb_from_int(color, &r,&g,&b);
            Tessellator_color(t, r,g,b);
            // the char right after the code is drawn in the same step and it throws when there is none
            i += 2;
            if (i >= n) break;
            c = str[i];
        }

        int ix = (c % 16) * 8;
        int iy = (c / 16) * 8;
        const float s = 7.99f;

        float u0 = ix / 128.0f, u1 = (ix + s) / 128.0f;
        float v0 = iy / 128.0f, v1 = (iy + s) / 128.0f;

        Tessellator_vertexUV(t, (float)(x + xo),     (float)y + s, 0.0f, u0, v1);
        Tessellator_vertexUV(t, (float)(x + xo) + s, (float)y + s, 0.0f, u1, v1);
        Tessellator_vertexUV(t, (float)(x + xo) + s, (float)y,     0.0f, u1, v0);
        Tessellator_vertexUV(t, (float)(x + xo),     (float)y,     0.0f, u0, v0);

        xo += f->charWidth[c];
    }

    Tessellator_end(t);
    glDisable(GL_TEXTURE_2D);
}

void Font_draw(Font* f, Tessellator* t, const char* s, int x, int y, int color) {
    Font_draw_internal(f, t, s, x, y, color, 0);
}

void Font_drawShadow(Font* f, Tessellator* t, const char* s, int x, int y, int color) {
    Font_draw_internal(f, t, s, x + 1, y + 1, color, 1);
    Font_draw_internal(f, t, s, x,     y,     color, 0);
}

int Font_width(Font* f, const char* s) {
    if (!s) return 0;
    const unsigned char* str = (const unsigned char*)s;
    size_t n = strlen(s);
    int len = 0;

    for (size_t i = 0; i < n; ++i) {
        unsigned char c = str[i];
        if (c == '&') { i++; continue; }
        len += f->charWidth[c];
    }
    return len;
}