// player.h: player base

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

// c0.25_05_st: matches Player.MAX_ARROWS, the carry cap checked both by
// Arrow.playerTouch's own pickup gate and (once wired) the Tab fire key
#define PLAYER_MAX_ARROWS 99

typedef struct Player {
    Entity e;
    bool keys[5];
    // c0.0.14a_08: jump key debounce latch, can't retrigger a jump while the
    // key stays held across an onGround transition until it's released
    bool jumping;
    // c0.0.20a_02: from the Login reply's new trailing byte. 0 = normal,
    // >=100 = operator, gates whether this player can break Bedrock. Stays 0
    // in singleplayer, since no Login reply is ever received there
    int userType;
    // c0.24_st_03: shown on the Game over screen, reset to 0 on every new level
    int score;
    // c0.25_05_st: matches Player.arrows, starts at 20 on a fresh spawn,
    // decremented by the Tab fire key and incremented back by Arrow's own
    // pickup, capped at PLAYER_MAX_ARROWS
    int arrows;
    // c0.24_st_03: survival inventory, replaces the old Creative style fixed
    // gHotbar[]/gSelectedSlot pair. Starts empty, matching player/d.java's
    // own constructor. Filling it back up is what breaking tiles and
    // picking up the resulting Item entities is for
    Inventory inventory;
    // matches Player.bob/oBob and Mob.tilt/oTilt: drives the camera view-bob
    // effect (a small sideways sway plus a bounce timed to footsteps).
    // Computed every tick in Player_onTick from horizontal speed (bob) and
    // vertical speed (tilt), then applied as a camera transform in
    // minecraft.c, gated by the bobView option
    float bob, oBob, tilt, oTilt;
    // matches Entity.walkDist/walkDistO, used only for the view-bob phase.
    // This port's own Entity.walkDist (entity.h) is periodically wrapped to
    // space out footstep sounds, unlike real source's version, which grows
    // forever, so it can't double as the bob phase input here without
    // corrupting the sin/cos phase on every wrap. Kept as a separate,
    // never-wrapped accumulator instead, same formula, Player only
    float walkDistBob, walkDistBobO;
} Player;

void Player_init(Player* player, Level* level);

// c0.24_st_03: fired once via Entity's onDeath hook the tick health first
// reaches <=0. Shrinks the bounding box to 0.2x0.2, knocks back slightly
// away from the attacker (if any), and drops the eye height to 0.1 (a
// ragdoll-ish "fell over" camera pose, no actual ragdoll model exists)
void Player_die(Entity* self, Entity* attacker);

void Player_awardKillScore(Player* player, int points);

void Player_onTick(Player* player);
void Player_turn(Player* player, GLFWwindow* window, float dx, float dy);

void Player_setKey(Player* player, int id, bool state);
void Player_releaseAllKeys(Player* player);
// syncs the 5 movement/jump keys from live GLFW state into player->keys[],
// reading each key's remappable binding from Options rather than a hardcoded
// key. c0.0.23a_01: the real source checks only the single configured key
// per action, no fallback to a second hardcoded key, matching here too
void Player_pollKeys(Player* player, GLFWwindow* window, const Options* opts);

#endif  // PLAYER_H