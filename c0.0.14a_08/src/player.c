// player.c

#include "player.h"
#include <math.h>
#include <string.h>

void Player_init(Player* p, Level* level) {
    Entity_init(&p->e, level);
    memset(&p->input, 0, sizeof(p->input));
    p->e.heightOffset = 1.62f;
}

void Player_setKey(Player* p, int key, bool state) {
    int id = -1;
    if (key == GLFW_KEY_UP || key == GLFW_KEY_W) id = 0;
    if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S) id = 1;
    if (key == GLFW_KEY_LEFT || key == GLFW_KEY_A) id = 2;
    if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_D) id = 3;
    if (key == GLFW_KEY_SPACE || key == GLFW_KEY_LEFT_SUPER) id = 4;
    if (id >= 0) p->input.keys[id] = state;
}

void Player_releaseAllKeys(Player* p) {
    for (int i = 0; i < 10; ++i) p->input.keys[i] = false;
}

void Player_turn(Player* p, float dx, float dy) {
    Entity_turn(&p->e, dx, dy);
}

static void Input_update(Input* in) {
    in->xa = 0.0f;
    in->ya = 0.0f;
    if (in->keys[0]) in->ya -= 1.0f;
    if (in->keys[1]) in->ya += 1.0f;
    if (in->keys[2]) in->xa -= 1.0f;
    if (in->keys[3]) in->xa += 1.0f;
    in->jump = in->keys[4];
}

void Player_onTick(Player* p) {
    Entity_onTick(&p->e);

    bool inWater = Entity_isInWater(&p->e);
    bool inLava  = Entity_isInLava(&p->e);
    Input_update(&p->input);

    // holding jump only jumps once until it's let go
    if (p->input.jump) {
        if (inWater) {
            p->e.motionY += 0.04f;
        } else if (inLava) {
            p->e.motionY += 0.04f;
        } else if (p->e.onGround && !p->input.jumping) {
            p->e.motionY = 0.42f;
            p->input.jumping = true;
        }
    } else {
        p->input.jumping = false;
    }

    if (inWater) {
        float yo = p->e.y;
        Entity_moveRelative(&p->e, p->input.xa, p->input.ya, 0.02f);
        Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);
        p->e.motionX *= 0.8f;
        p->e.motionY *= 0.8f;
        p->e.motionZ *= 0.8f;
        p->e.motionY = (float)(p->e.motionY - 0.02);
        if (p->e.horizontalCollision && Entity_isFree(&p->e, p->e.motionX, p->e.motionY + 0.6f - p->e.y + yo, p->e.motionZ)) {
            p->e.motionY = 0.3f;
        }
    } else if (inLava) {
        float yo = p->e.y;
        Entity_moveRelative(&p->e, p->input.xa, p->input.ya, 0.02f);
        Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);
        p->e.motionX *= 0.5f;
        p->e.motionY *= 0.5f;
        p->e.motionZ *= 0.5f;
        p->e.motionY = (float)(p->e.motionY - 0.02);
        if (p->e.horizontalCollision && Entity_isFree(&p->e, p->e.motionX, p->e.motionY + 0.6f - p->e.y + yo, p->e.motionZ)) {
            p->e.motionY = 0.3f;
        }
    } else {
        Entity_moveRelative(&p->e, p->input.xa, p->input.ya, p->e.onGround ? 0.1f : 0.02f);
        Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);
        p->e.motionX *= 0.91f;
        p->e.motionY *= 0.98f;
        p->e.motionZ *= 0.91f;
        p->e.motionY = (float)(p->e.motionY - 0.08);
        if (p->e.onGround) {
            p->e.motionX *= 0.6f;
            p->e.motionZ *= 0.6f;
        }
    }
}
