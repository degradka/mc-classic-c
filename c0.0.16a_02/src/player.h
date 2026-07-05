// player.h

#ifndef PLAYER_H
#define PLAYER_H

#include "common.h"
#include "entity.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

// the movement input from the keyboard
typedef struct {
    float xa, ya;
    bool jumping;
    bool jump;
    bool keys[10];
} Input;

typedef struct Player {
    Entity e;
    Input input;
} Player;

void Player_init(Player* player, Level* level);

void Player_onTick(Player* player);
void Player_turn(Player* player, float dx, float dy);

// takes a glfw key
void Player_setKey(Player* player, int key, bool state);
void Player_releaseAllKeys(Player* player);

#endif  // PLAYER_H
