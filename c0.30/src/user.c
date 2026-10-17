// user.c

#include "user.h"
#include <stdio.h>

void User_init(User* u, const char* name) {
    snprintf(u->name, sizeof(u->name), "%s", name);
}

const int USER_CREATIVE_TILES[USER_CREATIVE_TILE_COUNT] = {
    1, 4, 45, 3, 5, 17, 18, 20, 44, 48, 6, 37, 38, 39, 40, 12, 13, 19,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    16, 15, 14, 42, 41, 47, 46, 49
};
