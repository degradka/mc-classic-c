// textures.h: texture loader

#ifndef TEXTURES_H
#define TEXTURES_H

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>

GLuint loadTexture(const char* path, int filterMode);
// mipmapped and meant for a single large repeating texture, such as the
// water or rock horizon. Do not use this for terrain.png or char.png,
// which are tile atlases where mipmapping bleeds colors between faces.
GLuint loadTextureTiled(const char* path, int filterMode);
void bind(int id);

#endif  // TEXTURES_H