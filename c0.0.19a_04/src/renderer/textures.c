// renderer/textures.c

#include "textures.h"
#include <GL/glu.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// no wrap mode is set, so it stays GL_REPEAT
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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

    stbi_image_free(img);
    return (int)tex;
}