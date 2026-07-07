// player.h: player base

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
    bool keys[5];
    // c0.0.14a_08: jump key debounce latch, can't retrigger a jump while the
    // key stays held across an onGround transition until it's released
    bool jumping;
} Player;

void Player_init(Player* player, Level* level);

void Player_onTick(Player* player);
void Player_turn(Player* player, GLFWwindow* window, float dx, float dy);

void Player_setKey(Player* player, int id, bool state);
void Player_releaseAllKeys(Player* player);
// syncs the 5 movement/jump keys from live GLFW state into player->keys[]
void Player_pollKeys(Player* player, GLFWwindow* window);

#endif  // PLAYER_H