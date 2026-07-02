// level/perlin.h: simple Perlin like height field

#ifndef PERLIN_H
#define PERLIN_H

// octave 0 or 1, out holds width*height values from 0 to 255
void Perlin_read(int width, int height, int octave, int* out);

#endif