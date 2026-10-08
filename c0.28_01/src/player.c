// player.c

#include "player.h"
#include "math_helper.h"
#include <math.h>

void Player_init(Player* p, Level* level) {
    Entity_init(&p->e, level);
    p->e.heightOffset = 1.62f;
    p->e.aiClassTag = AI_CLASS_PLAYER;
    Entity_resetPosition(&p->e);
    for (int i = 0; i < 5; ++i) p->keys[i] = false;
    p->userType = 0;
    p->score = 0;
    p->arrows = 20;
    p->bob = p->oBob = p->tilt = p->oTilt = 0.0f;

    Mob_init(&p->e);
    p->e.onDeath = Player_die;

    Inventory_init(&p->inventory);
}

// Player.resetPos, back to the spawn point with full health
void Player_resetPos(Player* p) {
    p->e.heightOffset = 1.62f;
    Entity_setSize(&p->e, 0.6f, 1.8f);
    Entity_resetPosition(&p->e);
    p->e.health = 20;
    p->e.deathTime = 0;
}

void Player_awardKillScore(Player* p, int points) {
    p->score += points;
}

void Player_die(Entity* self, Entity* attacker) {
    Entity_setSize(self, 0.2f, 0.2f);
    // the box only shrinks on the next Entity_setPosition
    Entity_setPosition(self, self->x, self->y, self->z);

    // Player.die doesn't use Mob.knockback, it tips over along hurtDir
    self->motionY = 0.1f;
    if (attacker) {
        self->motionX = -MathHelper_cos((self->hurtDir + self->yRotation) * 3.1415927f / 180.0f) * 0.1f;
        self->motionZ = -MathHelper_sin((self->hurtDir + self->yRotation) * 3.1415927f / 180.0f) * 0.1f;
    } else {
        self->motionX = 0.0f;
        self->motionZ = 0.0f;
    }
    self->heightOffset = 0.1f;
}

void Player_setKey(Player* p, int id, bool state) {
    if (id >= 0 && id < 5) p->keys[id] = state;
}

void Player_releaseAllKeys(Player* p) {
    for (int i = 0; i < 5; ++i) p->keys[i] = false;
}

void Player_pollKeys(Player* p, GLFWwindow* w, const Options* opts) {
    Player_setKey(p, PLAYER_KEY_UP,    glfwGetKey(w, opts->keys[OPT_KEY_FORWARD].glfwKey) == GLFW_PRESS);
    Player_setKey(p, PLAYER_KEY_DOWN,  glfwGetKey(w, opts->keys[OPT_KEY_BACK].glfwKey)    == GLFW_PRESS);
    Player_setKey(p, PLAYER_KEY_LEFT,  glfwGetKey(w, opts->keys[OPT_KEY_LEFT].glfwKey)    == GLFW_PRESS);
    Player_setKey(p, PLAYER_KEY_RIGHT, glfwGetKey(w, opts->keys[OPT_KEY_RIGHT].glfwKey)   == GLFW_PRESS);
    Player_setKey(p, PLAYER_KEY_JUMP,  glfwGetKey(w, opts->keys[OPT_KEY_JUMP].glfwKey)    == GLFW_PRESS);
}

void Player_turn(Player* p, GLFWwindow* window, float dx, float dy) {
    Entity_turn(&p->e, dx, dy);
    // consume mouse deltas by resetting cursor to the origin each frame
    glfwSetCursorPos(window, 0, 0);
}

extern void Minecraft_checkItemPickups(Entity* player);
extern void Minecraft_checkArrowPickups(Entity* player);
extern void Minecraft_pushNearby(Entity* mob);


void Player_onTick(Player* p) {
    Entity_onTick(&p->e);
    Mob_onTick(&p->e);
    Inventory_onTick(&p->inventory);
    // oTilt and oBob before this tick's move
    p->oTilt = p->tilt;
    p->oBob = p->bob;

    float forward = 0.0f, strafe = 0.0f;
    bool jump = false;
    if (p->e.health > 0) {
        if (p->keys[PLAYER_KEY_UP])    forward -= 1.0f;
        if (p->keys[PLAYER_KEY_DOWN])  forward += 1.0f;
        if (p->keys[PLAYER_KEY_LEFT])  strafe  -= 1.0f;
        if (p->keys[PLAYER_KEY_RIGHT]) strafe  += 1.0f;
        jump = p->keys[PLAYER_KEY_JUMP];
    }

    bool inWater = Entity_isInWater(&p->e);
    bool inLava  = Entity_isInLava(&p->e);
    if (jump) {
        if (inWater)              p->e.motionY += 0.04f;
        else if (inLava)          p->e.motionY += 0.04f;
        else if (p->e.onGround)   p->e.motionY = 0.42f;
    }

    strafe *= 0.98f;
    forward *= 0.98f;
    Mob_travel(&p->e, strafe, forward);
    Minecraft_pushNearby(&p->e);

    // a dead player doesn't pick anything up
    if (p->e.health > 0) {
        Minecraft_checkItemPickups(&p->e);
        Minecraft_checkArrowPickups(&p->e);
    }

    // bob and tilt from the speed after this tick's move
    float speed = MathHelper_sqrt(p->e.motionX * p->e.motionX + p->e.motionZ * p->e.motionZ);
    float tiltTarget = atanf(-p->e.motionY * 0.2f) * 15.0f;
    if (speed > 0.1f) speed = 0.1f;
    if (!p->e.onGround || p->e.health <= 0) speed = 0.0f;
    if (p->e.onGround || p->e.health <= 0) tiltTarget = 0.0f;
    p->bob += (speed - p->bob) * 0.4f;
    p->tilt += (tiltTarget - p->tilt) * 0.8f;
}