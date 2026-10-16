// user.h: the local player's name

#ifndef USER_H
#define USER_H

typedef struct User {
    char name[64];
} User;

void User_init(User* u, const char* name);

// User.creativeTiles, the block select screen order
#define USER_CREATIVE_TILE_COUNT 42
extern const int USER_CREATIVE_TILES[USER_CREATIVE_TILE_COUNT];

#endif // USER_H
