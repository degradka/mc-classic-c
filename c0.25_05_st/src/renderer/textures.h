// renderer/textures.h: texture loader

#ifndef TEXTURES_H
#define TEXTURES_H

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

GLuint loadTexture(const char* path, int filterMode);

// re-uploads every loaded texture when the value changes
void Textures_setAnaglyph(bool enabled);

#endif  // TEXTURES_H