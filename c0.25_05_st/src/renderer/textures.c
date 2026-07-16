// textures.c: texture loader

#include "textures.h"
#include <GL/glu.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static bool gAnaglyph = false;

void Textures_setAnaglyph(bool enabled) {
    gAnaglyph = enabled;
}

// matches Textures.a(BufferedImage,int)'s own per-pixel remap exactly,
// including using truncating integer division like the real Java int math.
// Applied to every loaded texture when anaglyph3d is on: the game only ever
// draws through a single (post-processing-free) red or cyan channel mask
// per eye in that mode, so full color textures would otherwise look wrong
// (this is a real, source-verified step of "3D anaglyph" here, not just the
// stereo render itself)
static void applyAnaglyphTint(unsigned char* img, int w, int h) {
    for (int i = 0; i < w * h; ++i) {
        unsigned char* px = img + i * 4;
        int r = px[0], g = px[1], b = px[2];
        int newR = (r * 30 + g * 59 + b * 11) / 100;
        int newG = (r * 30 + g * 70) / 100;
        int newB = (r * 30 + b * 70) / 100;
        px[0] = (unsigned char)newR;
        px[1] = (unsigned char)newG;
        px[2] = (unsigned char)newB;
    }
}

// Java's Textures.loadTexture never sets a wrap mode, so it runs on
// OpenGL's spec default, which is GL_REPEAT, not GL_CLAMP_TO_EDGE.
GLuint loadTexture(const char* path, int filterMode) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) { fprintf(stderr, "Failed to generate texture id\n"); return 0; }

    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterMode);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int w,h,ch;
    unsigned char* img = stbi_load(path, &w, &h, &ch, STBI_rgb_alpha);
    if (!img) { fprintf(stderr, "Failed to load texture %s\n", path); return 0; }
    if (gAnaglyph) applyAnaglyphTint(img, w, h);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img);

    stbi_image_free(img);
    return (int)tex;
}

GLuint loadTextureTiled(const char* path, int filterMode) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) { fprintf(stderr, "Failed to generate texture id\n"); return 0; }

    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLint minFilter = (filterMode == GL_NEAREST) ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterMode);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int w,h,ch;
    unsigned char* img = stbi_load(path, &w, &h, &ch, STBI_rgb_alpha);
    if (!img) { fprintf(stderr, "Failed to load texture %s\n", path); return 0; }
    if (gAnaglyph) applyAnaglyphTint(img, w, h);

    // gluBuild2DMipmaps builds the mip chain on the CPU, matching what Java
    // does, instead of glGenerateMipmap, which needs GL 3.0 or an FBO
    // extension that may not be present under the GL 2.0 context we request.
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img);

    stbi_image_free(img);
    return (int)tex;
}