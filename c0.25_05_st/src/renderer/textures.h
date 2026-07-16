// textures.h: texture loader

#ifndef TEXTURES_H
#define TEXTURES_H

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

GLuint loadTexture(const char* path, int filterMode);
// mipmapped and meant for a single large repeating texture, such as the
// water or rock horizon. Do not use this for terrain.png or char.png,
// which are tile atlases where mipmapping bleeds colors between faces.
GLuint loadTextureTiled(const char* path, int filterMode);

// c0.25_05_st: matches Options.g (anaglyph3d), read by loadTexture/
// loadTextureTiled to decide whether to apply Textures.a(BufferedImage,int)'s
// own luminance remap to newly loaded pixels before upload. Call once at
// boot right after Options_load, and again whenever the option is toggled
// mid session, though only textures loaded/reloaded after the call pick up
// the new setting (see options.c's own toggle handler comment for why this
// port doesn't re-upload every already-loaded texture live like real source)
void Textures_setAnaglyph(bool enabled);

#endif  // TEXTURES_H