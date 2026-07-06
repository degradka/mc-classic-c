// net/network_player.c

#include "network_player.h"
#include "../character/zombie_model.h"
#include "../renderer/textures.h"
#include "../renderer/tessellator.h"
#include "../gui/font.h"
#include <GL/glew.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

extern Font* Minecraft_getFont(void);
extern float Minecraft_getPlayerYRot(void);

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

static void push(NetworkPlayer* np, PositionUpdate u) {
    if (np->queueCount == np->queueCapacity) {
        np->queueCapacity = np->queueCapacity ? np->queueCapacity * 2 : 16;
        np->moveQueue = (PositionUpdate*)realloc(np->moveQueue, (size_t)np->queueCapacity * sizeof(PositionUpdate));
    }
    np->moveQueue[np->queueCount++] = u;
}

static PositionUpdate posRot(float x, float y, float z, float yRot, float xRot) {
    PositionUpdate u = { x, y, z, yRot, xRot, true, true };
    return u;
}

static PositionUpdate posOnly(float x, float y, float z) {
    PositionUpdate u = { x, y, z, 0.0f, 0.0f, false, true };
    return u;
}

static PositionUpdate rotOnly(float yRot, float xRot) {
    PositionUpdate u = { 0.0f, 0.0f, 0.0f, yRot, xRot, true, false };
    return u;
}

void NetworkPlayer_init(NetworkPlayer* np, Level* level, int id, const char* name,
                        int x, int y, int z, float yRot, float xRot) {
    memset(np, 0, sizeof *np);
    Entity_init(&np->base, level);
    np->id = id;
    snprintf(np->name, sizeof np->name, "%s", name);
    np->xp = x;
    np->yp = y;
    np->zp = z;
    Entity_setPosition(&np->base, (float)x / 32.0f, (float)y / 32.0f, (float)z / 32.0f);
    np->base.xRotation = xRot;
    np->base.yRotation = yRot;
    np->base.heightOffset = 1.62f;

    if (!sModelInit) { ZombieModel_init(&sModel); sModelInit = 1; }
    if (!texChar) texChar = loadTexture("resources/char.png", GL_NEAREST);
}

void NetworkPlayer_free(NetworkPlayer* np) {
    free(np->moveQueue);
    np->moveQueue = NULL;
    np->queueCount = np->queueCapacity = 0;
}

void NetworkPlayer_onTick(NetworkPlayer* np) {
    Entity_onTick(&np->base);
    np->animStepO = np->animStep;

    // one move a tick, up to 5 more while more than 10 are waiting
    int n = 5;
    do {
        if (np->queueCount > 0) {
            PositionUpdate u = np->moveQueue[0];
            memmove(np->moveQueue, np->moveQueue + 1, (size_t)(np->queueCount - 1) * sizeof(PositionUpdate));
            np->queueCount--;
            Entity_setPosUpdate(&np->base, &u);
        }
    } while (n-- > 0 && np->queueCount > 10);

    float xd = np->base.x - np->base.prevX;
    float zd = np->base.z - np->base.prevZ;
    np->yBodyRotO = np->yBodyRot;
    float dist = (float)sqrt(xd * xd + zd * zd);
    float target = np->yBodyRot;
    float step = 0.0f;
    if (dist == 0.0f) {
        np->animStep = 0.0f;
    } else {
        step = dist * 3.0f;
        target = -((float)atan2(zd, xd) * 180.0f / (float)M_PI + 90.0f);
    }

    float d = target - np->yBodyRot;
    while (d < -180.0f) d += 360.0f;
    while (d >= 180.0f) d -= 360.0f;
    np->yBodyRot += d * 0.1f;

    d = np->base.yRotation - np->yBodyRot;
    while (d < -180.0f) d += 360.0f;
    while (d >= 180.0f) d -= 360.0f;
    bool backwards = d < -90.0f || d >= 90.0f;
    if (d < -75.0f) d = -75.0f;
    if (d >= 75.0f) d = 75.0f;
    np->yBodyRot = np->base.yRotation - d;
    if (backwards) step = -step;

    while (np->base.yRotation - np->base.yRotO < -180.0f) np->base.yRotO -= 360.0f;
    while (np->base.yRotation - np->base.yRotO >= 180.0f) np->base.yRotO += 360.0f;
    while (np->yBodyRot - np->yBodyRotO < -180.0f) np->yBodyRotO -= 360.0f;
    while (np->yBodyRot - np->yBodyRotO >= 180.0f) np->yBodyRotO += 360.0f;
    while (np->base.xRotation - np->base.xRotO < -180.0f) np->base.xRotO -= 360.0f;
    while (np->base.xRotation - np->base.xRotO >= 180.0f) np->base.xRotO += 360.0f;

    np->animStep += step;
}

void NetworkPlayer_render(const NetworkPlayer* np, float a) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texChar);

    float bodyYRot = np->yBodyRotO + (np->yBodyRot - np->yBodyRotO) * a;
    float headYRot = np->base.yRotO + (np->base.yRotation - np->base.yRotO) * a;
    float headXRot = np->base.xRotO + (np->base.xRotation - np->base.xRotO) * a;
    headYRot -= bodyYRot;

    glPushMatrix();
    float anim = np->animStepO + (np->animStep - np->animStepO) * a;
    float br = Entity_getBrightness(&np->base);
    glColor3f(br, br, br);
    float size = 0.0625f;
    float yy = (float)(-fabs(sin((double)anim * 0.6662)) * 5.0 - 23.0);
    float x = np->base.prevX + (np->base.x - np->base.prevX) * a;
    float y = np->base.prevY + (np->base.y - np->base.prevY) * a;
    float z = np->base.prevZ + (np->base.z - np->base.prevZ) * a;
    glTranslatef(x, y - np->base.heightOffset, z);
    glScalef(1.0f, -1.0f, 1.0f);
    glScalef(size, size, size);
    glTranslatef(0.0f, yy, 0.0f);
    glRotatef(bodyYRot, 0.0f, 1.0f, 0.0f);
    ZombieModel_render(&sModel, anim, headYRot, headXRot);
    glPopMatrix();

    // the name faces the local player, with a dark copy behind it
    Font* font = Minecraft_getFont();
    glPushMatrix();
    glTranslatef(x, y + 0.8f, z);
    glRotatef(-Minecraft_getPlayerYRot(), 0.0f, 1.0f, 0.0f);
    float s = 0.05f;
    glScalef(0.05f, -s, s);
    glTranslatef((float)(-Font_width(font, np->name)) / 2.0f, 0.0f, 0.0f);
    if (equalsIgnoreCase(np->name, "Notch")) {
        Font_draw(font, &sNameTess, np->name, 0, 0, 0xFFFF00);
    } else {
        Font_draw(font, &sNameTess, np->name, 0, 0, 0xFFFFFF);
    }
    glTranslatef(1.0f, 1.0f, -0.05f);
    Font_draw(font, &sNameTess, np->name, 0, 0, 0x505050);
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
}

void NetworkPlayer_teleport(NetworkPlayer* np, int x, int y, int z, float yRot, float xRot) {
    push(np, posRot((float)(np->xp + x) / 64.0f, (float)(np->yp + y) / 64.0f, (float)(np->zp + z) / 64.0f,
                    (np->base.yRotation + yRot) / 2.0f, (np->base.xRotation + xRot) / 2.0f));
    np->xp = x;
    np->yp = y;
    np->zp = z;
    push(np, posRot((float)np->xp / 32.0f, (float)np->yp / 32.0f, (float)np->zp / 32.0f, yRot, xRot));
}

void NetworkPlayer_queueMoveLook(NetworkPlayer* np, int dx, int dy, int dz, float yRot, float xRot) {
    push(np, posRot(((float)np->xp + (float)dx / 2.0f) / 32.0f, ((float)np->yp + (float)dy / 2.0f) / 32.0f,
                    ((float)np->zp + (float)dz / 2.0f) / 32.0f,
                    (np->base.yRotation + yRot) / 2.0f, (np->base.xRotation + xRot) / 2.0f));
    np->xp += dx;
    np->yp += dy;
    np->zp += dz;
    push(np, posRot((float)np->xp / 32.0f, (float)np->yp / 32.0f, (float)np->zp / 32.0f, yRot, xRot));
}

void NetworkPlayer_queueMove(NetworkPlayer* np, int dx, int dy, int dz) {
    push(np, posOnly(((float)np->xp + (float)dx / 2.0f) / 32.0f, ((float)np->yp + (float)dy / 2.0f) / 32.0f,
                     ((float)np->zp + (float)dz / 2.0f) / 32.0f));
    np->xp += dx;
    np->yp += dy;
    np->zp += dz;
    push(np, posOnly((float)np->xp / 32.0f, (float)np->yp / 32.0f, (float)np->zp / 32.0f));
}

void NetworkPlayer_queueLook(NetworkPlayer* np, float yRot, float xRot) {
    push(np, rotOnly((np->base.yRotation + yRot) / 2.0f, (np->base.xRotation + xRot) / 2.0f));
    push(np, rotOnly(yRot, xRot));
}
