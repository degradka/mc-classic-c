// player.c: player base

#include "player.h"
#include <math.h>

void Player_init(Player* p, Level* level) {
    Entity_init(&p->e, level);
    p->e.heightOffset = 1.62f;
    for (int i = 0; i < 5; ++i) p->keys[i] = false;
    p->jumping = false;
    p->userType = 0;
}

void Player_setKey(Player* p, int id, bool state) {
    if (id >= 0 && id < 5) p->keys[id] = state;
}

void Player_releaseAllKeys(Player* p) {
    for (int i = 0; i < 5; ++i) p->keys[i] = false;
}

void Player_pollKeys(Player* p, GLFWwindow* w) {
    Player_setKey(p, PLAYER_KEY_UP,    glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_UP)    == GLFW_PRESS);
    Player_setKey(p, PLAYER_KEY_DOWN,  glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_DOWN)  == GLFW_PRESS);
    Player_setKey(p, PLAYER_KEY_LEFT,  glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_LEFT)  == GLFW_PRESS);
    Player_setKey(p, PLAYER_KEY_RIGHT, glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS);
    Player_setKey(p, PLAYER_KEY_JUMP,  glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS);
}

void Player_turn(Player* p, GLFWwindow* window, float dx, float dy) {
    Entity_turn(&p->e, dx, dy);
    // consume mouse deltas by resetting cursor to the origin each frame
    glfwSetCursorPos(window, 0, 0);
}

void Player_onTick(Player* p) {
    Entity_onTick(&p->e);

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
}