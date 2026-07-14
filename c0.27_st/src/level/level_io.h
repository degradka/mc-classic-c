// level/level_io.h: level.dat, version 1 or 2 in, version 2 out

#ifndef LEVEL_IO_H
#define LEVEL_IO_H

#include "level.h"
#include <stdbool.h>

bool LevelIO_load(Level* level, const char* path);
void LevelIO_save(const Level* level, const char* path);

#endif
