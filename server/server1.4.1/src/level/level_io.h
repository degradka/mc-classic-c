// level/level_io.h: server_level.dat, gzip with a magic, a version byte and the level
// version 1 is the old raw format, version 2 a serialized Level, the server writes 2

#ifndef LEVEL_IO_H
#define LEVEL_IO_H

#include "level.h"
#include <stdint.h>

// NULL when it can't be loaded, the reason gets printed
Level* LevelIO_load(const uint8_t* data, int len);
void LevelIO_save(const Level* level, void* file);

#endif
