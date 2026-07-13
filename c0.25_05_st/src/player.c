// player.c: player base

#include "player.h"
#include <math.h>

void Player_init(Player* p, Level* level) {
    Entity_init(&p->e, level);
    p->e.heightOffset = 1.62f;
    // real source's own Entity constructor doesn't search for a free spawn
    // spot at all, it just sets pos to literal 0,0,0. Only Player's own
    // resetPos() override does, and only when explicitly invoked by the
    // level setup flow. This port used to bake that search into Entity_init
    // itself unconditionally for every entity type, which stayed harmless as
    // long as level->xSpawn/ySpawn/zSpawn were already valid by the time any
    // entity was created. That's no longer true once world gen starts
    // creating Creatures directly, so it now searches from an all zero, not
    // yet real spawn point. Called explicitly here instead, matching real
    // source and avoiding that wasted and unsafe search for every other
    // entity type
    Entity_resetPosition(&p->e);
    for (int i = 0; i < 5; ++i) p->keys[i] = false;
    p->jumping = false;
    p->userType = 0;
    p->score = 0;

    // c0.24_st_03: Player now extends Mob in the real source (health,
    // invulnerability, drowning/lava/fall damage, knockback)
    Mob_init(&p->e);
    p->e.onDeath = Player_die;

    Inventory_init(&p->inventory);
}

void Player_awardKillScore(Player* p, int points) {
    p->score += points;
}

void Player_die(Entity* self, Entity* attacker) {
    Entity_setSize(self, 0.2f, 0.2f);
    // Entity_setSize only stores the new width/height for the next
    // Entity_setPosition to apply; the live bounding box doesn't actually
    // shrink until that runs (matches how Particle_init sizes itself)
    Entity_setPosition(self, self->x, self->y, self->z);

    // Player.die() does not call the generic Mob.knockback(). It has its
    // own gentler, purely angle based keel over motion, driven by hurtDir,
    // the hurt flash wobble direction Mob_hurt sets, rotated into world
    // space by the player's own yaw, not the attacker's position
    self->motionY = 0.1f;
    if (attacker) {
        double rad = (self->hurtDir + self->yRotation) * M_PI / 180.0;
        self->motionX = -(float)cos(rad) * 0.1f;
        self->motionZ = -(float)sin(rad) * 0.1f;
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

// implemented in minecraft.c: scans items[] for anything overlapping the
// player's own bounding box grown 1 block horizontally, matches
// Player.aiStep()'s own level.findEntities(...).playerTouch(this) loop,
// narrowed to Item specifically since that's this port's only playerTouch
// consumer so far
extern void Minecraft_checkItemPickups(Entity* player);

void Player_onTick(Player* p) {
    Entity_onTick(&p->e);
    Mob_onTick(&p->e);
    Inventory_onTick(&p->inventory);

    float forward = 0.0f, strafe = 0.0f;

    bool inWater = Entity_isInWater(&p->e);
    bool inLava  = Entity_isInLava(&p->e);

    if (p->keys[PLAYER_KEY_UP])    forward -= 1.0f;
    if (p->keys[PLAYER_KEY_DOWN])  forward += 1.0f;
    if (p->keys[PLAYER_KEY_LEFT])  strafe  -= 1.0f;
    if (p->keys[PLAYER_KEY_RIGHT]) strafe  += 1.0f;

    // c0.0.14a_08: jump debounce, once triggered the key must be released
    // before another jump can fire, even if onGround becomes true again
    // while it's still held
    if (p->keys[PLAYER_KEY_JUMP]) {
        if (inWater || inLava) {
            p->e.motionY += 0.04f;
        } else if (p->e.onGround && !p->jumping) {
            p->e.motionY = 0.42f;
            p->jumping = true;
        }
    } else {
        p->jumping = false;
    }

    if (inWater) {
        float yo = p->e.y;
        Entity_moveRelative(&p->e, strafe, forward, 0.02f);
        Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);
        p->e.motionX *= 0.8f;
        p->e.motionY *= 0.8f;
        p->e.motionZ *= 0.8f;
        p->e.motionY -= 0.02f;
        if (p->e.horizontalCollision && Entity_isFree(&p->e, p->e.motionX, p->e.motionY + 0.6f - p->e.y + yo, p->e.motionZ)) {
            p->e.motionY = 0.3f;
        }
    } else if (inLava) {
        float yo = p->e.y;
        Entity_moveRelative(&p->e, strafe, forward, 0.02f);
        Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);
        p->e.motionX *= 0.5f;
        p->e.motionY *= 0.5f;
        p->e.motionZ *= 0.5f;
        p->e.motionY -= 0.02f;
        if (p->e.horizontalCollision && Entity_isFree(&p->e, p->e.motionX, p->e.motionY + 0.6f - p->e.y + yo, p->e.motionZ)) {
            p->e.motionY = 0.3f;
        }
    } else {
        Entity_moveRelative(&p->e, strafe, forward, p->e.onGround ? 0.1f : 0.02f);
        Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);
        p->e.motionX *= 0.91f;
        p->e.motionY *= 0.98f;
        p->e.motionZ *= 0.91f;
        p->e.motionY -= 0.08f;

        if (p->e.onGround) {
            p->e.motionX *= 0.6f;
            p->e.motionZ *= 0.6f;
        }
    }

    Minecraft_checkItemPickups(&p->e);
}