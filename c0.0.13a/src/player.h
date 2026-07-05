// player.h

#ifndef PLAYER_H
#define PLAYER_H

#include "common.h"
#include "entity.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

#define PLAYER_KEY_UP    0
#define PLAYER_KEY_DOWN  1
#define PLAYER_KEY_LEFT  2
#define PLAYER_KEY_RIGHT 3
#define PLAYER_KEY_JUMP  4

typedef struct Player {
    Entity e;
    bool keys[10];
} Player;

void Player_init(Player* player, Level* level);

void Player_onTick(Player* player);
void Player_turn(Player* player, float dx, float dy);

// takes a glfw key
void Player_setKey(Player* player, int key, bool state);
void Player_releaseAllKeys(Player* player);

#endif  // PLAYER_H
