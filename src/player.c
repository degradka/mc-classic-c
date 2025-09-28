// player.c — player base

#include "player.h"
#include <math.h>

static int s_jumpLatch = 0;

static inline int isInWater(const Player* p) {
    AABB box = AABB_grow(&p->e.boundingBox, 0.f, -0.4f, 0.f);
    return Level_containsLiquid(p->e.level, &box, 1);  // NOTE the &
}
static inline int isInLava(const Player* p) {
    return Level_containsLiquid(p->e.level, &p->e.boundingBox, 2); // NOTE the &
}

void Player_init(Player* p, Level* level) {
    Entity_init(&p->e, level);
    p->e.heightOffset = 1.62f;
}

void Player_turn(Player* p, GLFWwindow* window, float dx, float dy) {
    Entity_turn(&p->e, dx, dy);
    // consume mouse deltas by resetting cursor to the origin each frame
    glfwSetCursorPos(window, 0, 0);
}

void Player_onTick(Player* p, GLFWwindow* window) {
    Entity_onTick(&p->e);

    float forward = 0.0f, strafe = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) forward -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) forward += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) strafe  -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) strafe  += 1.0f;

    const int water = isInWater(p);
    const int lava  = isInLava(p);
    const int space = glfwGetKey(window, GLFW_KEY_SPACE);

    // swim rise: identical to Java
    if (space == GLFW_PRESS && water)       p->e.motionY += 0.04f;
    else if (space == GLFW_PRESS && lava)   p->e.motionY += 0.04f;
    // ground jump: single impulse until SPACE is released once
    else if (space == GLFW_PRESS && p->e.onGround && !s_jumpLatch) {
        p->e.motionY = 0.42f;
        s_jumpLatch = 1; // consume jump (prevents autojump)
    }
    if (space == GLFW_RELEASE) s_jumpLatch = 0;

    if (water) {
        double oldY = p->e.y;
        Entity_moveRelative(&p->e, strafe, forward, 0.02f);
        Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);

        p->e.motionX *= 0.8f;
        p->e.motionY *= 0.8f;
        p->e.motionZ *= 0.8f;
        p->e.motionY -= 0.02f;

        if (p->e.horizontalCollision && Entity_isFree(&p->e, p->e.motionX, (float)(p->e.motionY + 0.6f - p->e.y + oldY), p->e.motionZ)) {
            p->e.motionY = 0.3f;
        }
        return;
    }

    if (lava) {
        double oldY = p->e.y;
        Entity_moveRelative(&p->e, strafe, forward, 0.02f);
        Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);

        p->e.motionX *= 0.5f;
        p->e.motionY *= 0.5f;
        p->e.motionZ *= 0.5f;
        p->e.motionY -= 0.02f;

        if (p->e.horizontalCollision && Entity_isFree(&p->e, p->e.motionX, (float)(p->e.motionY + 0.6f - p->e.y + oldY), p->e.motionZ)) {
            p->e.motionY = 0.3f;
        }
        return;
    }

    // normal
    Entity_moveRelative(&p->e, strafe, forward, p->e.onGround ? 0.1f : 0.02f);
    Entity_move(&p->e, p->e.motionX, p->e.motionY, p->e.motionZ);
    
    p->e.motionX *= 0.91f;
    p->e.motionY *= 0.98f;
    p->e.motionZ *= 0.91f;

    // Then apply gravity
    p->e.motionY -= 0.08f;

    // Finally, add extra ground friction
    if (p->e.onGround) {
        p->e.motionX *= 0.6f;   // 0.0.13a uses 0.6
        p->e.motionZ *= 0.6f;
    }
}