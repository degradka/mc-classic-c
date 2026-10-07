// player.h

#ifndef PLAYER_H
#define PLAYER_H

#include "common.h"
#include "entity.h"
#include "options.h"
#include "mob.h"
#include "inventory.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

#define PLAYER_KEY_UP    0
#define PLAYER_KEY_DOWN  1
#define PLAYER_KEY_LEFT  2
#define PLAYER_KEY_RIGHT 3
#define PLAYER_KEY_JUMP  4

#define PLAYER_MAX_ARROWS 99

typedef struct Player {
    Entity e;
    bool keys[5];
    // from the server's Login reply, 100 or more is an operator and can break bedrock
    int userType;
    // shown on the Game over screen
    int score;
    // starts at 20, up to PLAYER_MAX_ARROWS
    int arrows;
    Inventory inventory;
    // Player.bob and Mob.tilt, the view bobbing
    float bob, oBob, tilt, oTilt;
} Player;

void Player_init(Player* player, Level* level);

// shrinks the box to 0.2, drops the eye height to 0.1
void Player_die(Entity* self, Entity* attacker);

void Player_awardKillScore(Player* player, int points);

void Player_onTick(Player* player);
void Player_turn(Player* player, GLFWwindow* window, float dx, float dy);

void Player_setKey(Player* player, int id, bool state);
void Player_releaseAllKeys(Player* player);
// reads the 5 movement keys from their bindings
void Player_pollKeys(Player* player, GLFWwindow* window, const Options* opts);

#endif  // PLAYER_H