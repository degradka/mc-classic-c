// net/network_player.c

#include "network_player.h"
#include "../character/zombie_model.h"
#include "../renderer/textures.h"
#include "../renderer/tessellator.h"
#include "../mob.h"
#include "../jrandom.h"
#include <GL/glew.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static ZombieModel sModel;
static int sModelInit = 0;
static int texChar = 0;
static Tessellator sNameTess;

static bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++; b++;
    }
    return *a == *b;
}

// Font.removeColorCodes
static void removeColorCodes(const char* s, char* out, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; s[i]; ++i) {
        if (s[i] == '&') {
            if (!s[++i]) break;
            continue;
        }
        if (n + 1 < cap) out[n++] = s[i];
    }
    out[n] = '\0';
}

static void push(NetworkPlayer* np, PlayerMove m) {
    if (np->moveHead + np->moveCount == np->moveCap) {
        if (np->moveHead > 0) {
            memmove(np->moves, np->moves + np->moveHead, (size_t)np->moveCount * sizeof(PlayerMove));
            np->moveHead = 0;
        } else {
            np->moveCap = np->moveCap ? np->moveCap * 2 : 64;
            np->moves = (PlayerMove*)realloc(np->moves, (size_t)np->moveCap * sizeof(PlayerMove));
        }
    }
    np->moves[np->moveHead + np->moveCount++] = m;
}

static PlayerMove moveRot(float x, float y, float z, float yRot, float xRot) {
    PlayerMove m = { x, y, z, yRot, xRot, true, true };
    return m;
}

static PlayerMove moveOnly(float x, float y, float z) {
    PlayerMove m = { x, y, z, 0.0f, 0.0f, false, true };
    return m;
}

static PlayerMove rotOnly(float yRot, float xRot) {
    PlayerMove m = { 0.0f, 0.0f, 0.0f, yRot, xRot, true, false };
    return m;
}

// Entity.setPos(PlayerMove)
static void applyMove(Entity* e, const PlayerMove* m) {
    if (m->moving) Entity_setPosition(e, m->x, m->y, m->z);
    else Entity_setPosition(e, e->x, e->y, e->z);
    if (m->rotating) {
        e->yRotation = m->yRot;
        e->xRotation = m->xRot;
    }
}

// NetworkPlayer.aiStep, one move a tick, up to 5 more while more than 10 are waiting
static void aiStep(Entity* e) {
    NetworkPlayer* np = (NetworkPlayer*)e;
    int n = 5;
    do {
        if (np->moveCount <= 0) continue;
        PlayerMove m = np->moves[np->moveHead++];
        if (--np->moveCount == 0) np->moveHead = 0;
        applyMove(e, &m);
    } while (n-- > 0 && np->moveCount > 10);
    e->onGround = true;
}

void NetworkPlayer_init(NetworkPlayer* np, Level* level, int id, const char* displayName,
                        int xRaw, int yRaw, int zRaw, float yaw, float pitch) {
    memset(np, 0, sizeof *np);
    Entity_init(&np->e, level);
    Mob_init(&np->e);
    // HumanoidMob rolls helmet and armor, NetworkPlayer turns both off
    JRandom_mathRandom();
    JRandom_mathRandom();
    Entity_setPosition(&np->e, (float)xRaw, (float)yRaw, (float)zRaw);
    np->id = id;
    snprintf(np->displayName, sizeof np->displayName, "%s", displayName);
    removeColorCodes(displayName, np->name, sizeof np->name);
    np->xp = xRaw; np->yp = yRaw; np->zp = zRaw;
    np->e.heightOffset = 0.0f;
    np->e.pushthrough = 0.8f;
    Entity_setPosition(&np->e, (float)xRaw / 32.0f, (float)yRaw / 32.0f, (float)zRaw / 32.0f);
    np->e.xRotation = pitch;
    np->e.yRotation = yaw;
    np->e.afterAiStep = aiStep;

    if (!sModelInit) { ZombieModel_init(&sModel); sModelInit = 1; }
    if (!texChar) texChar = loadTexture("resources/char.png", GL_NEAREST);
}

void NetworkPlayer_free(NetworkPlayer* np) {
    free(np->moves);
    np->moves = NULL;
    np->moveHead = np->moveCount = np->moveCap = 0;
}

void NetworkPlayer_onTick(NetworkPlayer* np) {
    Entity_onTick(&np->e);
    Mob_onTick(&np->e);
}

// HumanoidMob.renderModel with the humanoid model and char.png
static void networkPlayerModel(const void* self, float animStep, float run, float age,
                               float headYaw, float headPitch, float attackSwing, float partialTicks) {
    (void)self; (void)attackSwing; (void)partialTicks;
    glBindTexture(GL_TEXTURE_2D, texChar);
    ZombieModel_render(&sModel, animStep, run, age, headYaw, headPitch);
    glEnable(GL_ALPHA_TEST);
    // the hair pass, char.png's hat area is fully transparent so only its cull toggle is left
    glDisable(GL_CULL_FACE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
}

void NetworkPlayer_render(const NetworkPlayer* np, float partialTicks, float localPlayerYaw, Font* font) {
    const Entity* e = &np->e;
    Mob_render(e, partialTicks, Entity_getBrightness(e), 1.0f, false, networkPlayerModel, np);

    glPushMatrix();
    glTranslatef(e->prevX + (e->x - e->prevX) * partialTicks, e->prevY + (e->y - e->prevY) * partialTicks + 0.8f,
                 e->prevZ + (e->z - e->prevZ) * partialTicks);
    glRotatef(-localPlayerYaw, 0.0f, 1.0f, 0.0f);
    glScalef(0.05f, -0.05f, 0.05f);
    glTranslatef((float)(-Font_width(font, np->displayName)) / 2.0f, 0.0f, 0.0f);
    glNormal3f(1.0f, -1.0f, 1.0f);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    Font_draw(font, &sNameTess, np->displayName, 0, 0, equalsIgnoreCase(np->name, "Notch") ? 0xFFFF00 : 0xFFFFFF);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glTranslatef(1.0f, 1.0f, -0.05f);
    Font_draw(font, &sNameTess, np->name, 0, 0, 0x505050);
    glPopMatrix();
}

// the halfway step turns the short way round
static float halfway(float from, float to) {
    float d = to - from;
    while (d >= 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return from + d * 0.5f;
}

void NetworkPlayer_teleport(NetworkPlayer* np, int xRaw, int yRaw, int zRaw, float yaw, float pitch) {
    float yh = halfway(np->e.yRotation, yaw), xh = halfway(np->e.xRotation, pitch);
    push(np, moveRot((float)(np->xp + xRaw) / 64.0f, (float)(np->yp + yRaw) / 64.0f, (float)(np->zp + zRaw) / 64.0f, yh, xh));
    np->xp = xRaw; np->yp = yRaw; np->zp = zRaw;
    push(np, moveRot((float)np->xp / 32.0f, (float)np->yp / 32.0f, (float)np->zp / 32.0f, yaw, pitch));
}

void NetworkPlayer_queueMoveLook(NetworkPlayer* np, int dx, int dy, int dz, float yaw, float pitch) {
    float yh = halfway(np->e.yRotation, yaw), xh = halfway(np->e.xRotation, pitch);
    push(np, moveRot(((float)np->xp + (float)dx / 2.0f) / 32.0f, ((float)np->yp + (float)dy / 2.0f) / 32.0f,
                     ((float)np->zp + (float)dz / 2.0f) / 32.0f, yh, xh));
    np->xp += dx; np->yp += dy; np->zp += dz;
    push(np, moveRot((float)np->xp / 32.0f, (float)np->yp / 32.0f, (float)np->zp / 32.0f, yaw, pitch));
}

void NetworkPlayer_queueMove(NetworkPlayer* np, int dx, int dy, int dz) {
    push(np, moveOnly(((float)np->xp + (float)dx / 2.0f) / 32.0f, ((float)np->yp + (float)dy / 2.0f) / 32.0f,
                      ((float)np->zp + (float)dz / 2.0f) / 32.0f));
    np->xp += dx; np->yp += dy; np->zp += dz;
    push(np, moveOnly((float)np->xp / 32.0f, (float)np->yp / 32.0f, (float)np->zp / 32.0f));
}

void NetworkPlayer_queueLook(NetworkPlayer* np, float yaw, float pitch) {
    push(np, rotOnly(halfway(np->e.yRotation, yaw), halfway(np->e.xRotation, pitch)));
    push(np, rotOnly(yaw, pitch));
}
