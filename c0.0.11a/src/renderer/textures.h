// textures.h: texture loader

#ifndef TEXTURES_H
#define TEXTURES_H

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>

GLuint loadTexture(const char* path, int filterMode);
void bind(int id);

#endif  // TEXTURES_H