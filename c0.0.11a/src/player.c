// player.c

#include "player.h"
#include <math.h>

void Player_init(Player* p, Level* level) {
    Entity_init(&p->e, level);
    p->e.heightOffset = 1.62f;
}

void Player_turn(Player* p, GLFWwindow* window, float dx, float dy) {
    (void)window;
    Entity_turn(&p->e, dx, dy);
}

static bool down(GLFWwindow* w, int key) {
    return glfwGetKey(w, key) == GLFW_PRESS;
}

void Player_onTick(Player* p, GLFWwindow* window) {
    Entity_onTick(&p->e);

    float xa = 0.0f, ya = 0.0f;

    if (down(window, GLFW_KEY_R)) Entity_resetPosition(&p->e);
    if (down(window, GLFW_KEY_UP) || down(window, GLFW_KEY_W)) ya--;
    if (down(window, GLFW_KEY_DOWN) || down(window, GLFW_KEY_S)) ya++;
    if (down(window, GLFW_KEY_LEFT) || down(window, GLFW_KEY_A)) xa--;
    if (down(window, GLFW_KEY_RIGHT) || down(window, GLFW_KEY_D)) xa++;
    if ((down(window, GLFW_KEY_SPACE) || down(window, GLFW_KEY_LEFT_SUPER)) && p->e.onGround) {
        p->e.motionY = 0.5f;
    }

    Entity_moveRelative(&p->e, xa, ya, p->e.onGround ? 0.1f : 0.02f);
    p->e.motionY = (float)(p->e.motionY - 0.08);
    Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);

    p->e.motionX *= 0.91f;
    p->e.motionY *= 0.98f;
    p->e.motionZ *= 0.91f;

    if (p->e.onGround) {
        p->e.motionX *= 0.7f;
        p->e.motionZ *= 0.7f;
    }
}
