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
    // Java mapping: lower 3 bits = RGB, high bit = brightness boost
    int br = (code & 0x8) ? 0x80 : 0x00;  // same pattern as Java ( *8 ) but fold into step below
    int b = ((code & 0x1) ? 191 : 0) + br;
    int g = ((code & 0x2) ? 191 : 0) + br;
    int r = ((code & 0x4) ? 191 : 0) + br;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (r << 16) | (g << 8) | b;
}

static int is_color_code_char(char c) {
    const char* hex = "0123456789abcdef";
    return (strchr(hex, c) != NULL);
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

    // compute per-char widths for first 128 chars
    // each cell is 8x8, laid out in a 16x16 grid on a 128x128 image
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
                // checks the blue channel (>128) to decide if a column has ink
                unsigned char b = img[idx + 2];
                if (b > 128) emptyColumn = 0;
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
    bind((int)f->texture);

    Tessellator_begin(t);
    Tessellator_color(t, r, g, b);

    int xo = 0;
    const unsigned char* str = (const unsigned char*)s;
    size_t n = strlen(s);

    for (size_t i = 0; i < n; ++i) {
        unsigned char c = str[i];

        if (c == '&' && i + 1 < n && is_color_code_char((char)str[i+1])) {
            // handle color code
            int code = (int)(strchr("0123456789abcdef", (char)str[i+1]) - "0123456789abcdef");
            color = color_from_code(code);
            if (darken) color = (color & 0xFCFCFC) >> 2;
            rgb_from_int(color, &r,&g,&b);
            Tessellator_color(t, r,g,b);
            i++; // skip code char
            continue;
        }

        int ix = (c % 16) * 8;
        int iy = (c / 16) * 8;

        float u0 = ix / 128.0f, u1 = (ix + 8) / 128.0f;
        float v0 = iy / 128.0f, v1 = (iy + 8) / 128.0f;

        // quad in screen space (z=0)
        Tessellator_vertexUV(t, (float)(x + xo),     (float)(y + 8), 0.0f, u0, v1);
        Tessellator_vertexUV(t, (float)(x + xo + 8), (float)(y + 8), 0.0f, u1, v1);
        Tessellator_vertexUV(t, (float)(x + xo + 8), (float) y,      0.0f, u1, v0);
        Tessellator_vertexUV(t, (float)(x + xo),     (float) y,      0.0f, u0, v0);

        xo += f->charWidth[c];
    }

    Tessellator_end(t);
    glDisable(GL_TEXTURE_2D);
}

void Font_draw(Font* f, Tessellator* t, const char* s, int x, int y, int color) {
    Font_draw_internal(f, t, s, x, y, color, 0);
}

void Font_drawShadow(Font* f, Tessellator* t, const char* s, int x, int y, int color) {
    // drop shadow
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
        if (c == '&' && i + 1 < n && is_color_code_char((char)str[i+1])) { i++; continue; }
        len += f->charWidth[c];
    }
    return len;
}