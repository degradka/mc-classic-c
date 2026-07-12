// net/network_player.c

#include "network_player.h"
#include "../character/zombie_model.h"
#include "../renderer/textures.h"
#include "../renderer/tessellator.h"
#include <GL/glew.h>
#include <math.h>
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

static void pushMoveEntry(NetworkPlayer* np, float x, float y, float z, float yaw, float pitch, bool hasPos, bool hasRot) {
    if (np->queueCount >= NETPLAYER_QUEUE_CAP) return; // shouldn't happen, catch up drain keeps this bounded
    int tail = (np->queueHead + np->queueCount) % NETPLAYER_QUEUE_CAP;
    NetPlayerMoveEntry* e = &np->moveQueue[tail];
    e->x = x; e->y = y; e->z = z;
    e->yaw = yaw; e->pitch = pitch;
    e->hasPos = hasPos; e->hasRot = hasRot;
    np->queueCount++;
}

void NetworkPlayer_init(NetworkPlayer* np, Level* level, int id, const char* name,
                         int xRaw, int yRaw, int zRaw, float yaw, float pitch) {
    Entity_init(&np->base, level);
    Entity_setPosition(&np->base, xRaw / 32.0f, yRaw / 32.0f, zRaw / 32.0f);
    np->base.yRotation = yaw;
    np->base.xRotation = pitch;
    np->base.heightOffset = 1.62f;

    np->used = true;
    np->id = id;
    snprintf(np->name, sizeof np->name, "%s", name);
    np->animStep = np->animStepO = 0.0f;
    np->yBodyRot = np->yBodyRotO = 0.0f;
    np->xp = xRaw; np->yp = yRaw; np->zp = zRaw;
    np->queueHead = np->queueCount = 0;

    if (!sModelInit) { ZombieModel_init(&sModel); sModelInit = 1; }
    if (!texChar) texChar = loadTexture("resources/char.png", GL_NEAREST);
}

void NetworkPlayer_onTick(NetworkPlayer* np) {
    Entity_onTick(&np->base);
    np->animStepO = np->animStep;

    // matches NetworkPlayer.tick(): always drains at least one queued
    // waypoint, then up to 5 more only while the backlog is still over 10,
    // a catch up mechanism for lag spikes rather than a normal per-tick rate
    int budget = 5;
    do {
        if (np->queueCount > 0) {
            NetPlayerMoveEntry e = np->moveQueue[np->queueHead];
            np->queueHead = (np->queueHead + 1) % NETPLAYER_QUEUE_CAP;
            np->queueCount--;
            if (e.hasPos) Entity_setPosition(&np->base, e.x, e.y, e.z);
            if (e.hasRot) { np->base.yRotation = e.yaw; np->base.xRotation = e.pitch; }
        }
    } while (budget-- > 0 && np->queueCount > 10);

    // body yaw turns to face movement direction, decoupled from head yaw
    // (which can look up to 75 degrees away from the body before the body
    // catches up), matching NetworkPlayer.tick()'s exact formula
    float dx = np->base.x - np->base.prevX;
    float dz = np->base.z - np->base.prevZ;
    np->yBodyRotO = np->yBodyRot;
    float dist = sqrtf(dx * dx + dz * dz);
    float targetBodyYaw = np->yBodyRot;
    float animDelta = 0.0f;
    if (dist == 0.0f) {
        np->animStep = 0.0f; // standing still resets the walk cycle, doesn't just pause it
    } else {
        animDelta = dist * 3.0f;
        targetBodyYaw = -(atan2f(dz, dx) * 180.0f / (float)M_PI + 90.0f);
    }

    float diff = targetBodyYaw - np->yBodyRot;
    while (diff < -180.0f) diff += 360.0f;
    while (diff >= 180.0f) diff -= 360.0f;
    np->yBodyRot += diff * 0.1f;

    float headDiff = np->base.yRotation - np->yBodyRot;
    while (headDiff < -180.0f) headDiff += 360.0f;
    while (headDiff >= 180.0f) headDiff -= 360.0f;
    bool facingBackward = (headDiff < -90.0f || headDiff >= 90.0f);
    if (headDiff < -75.0f) headDiff = -75.0f;
    if (headDiff >= 75.0f) headDiff = 75.0f;
    np->yBodyRot = np->base.yRotation - headDiff;
    if (facingBackward) animDelta = -animDelta;

    // keep the interpolated angle pairs within 180 degrees of each other so
    // render()'s linear lerp doesn't spin the long way around
    while (np->base.yRotation - np->base.prevYRotation < -180.0f) np->base.prevYRotation -= 360.0f;
    while (np->base.yRotation - np->base.prevYRotation >= 180.0f) np->base.prevYRotation += 360.0f;
    while (np->yBodyRot - np->yBodyRotO < -180.0f) np->yBodyRotO -= 360.0f;
    while (np->yBodyRot - np->yBodyRotO >= 180.0f) np->yBodyRotO += 360.0f;
    while (np->base.xRotation - np->base.prevXRotation < -180.0f) np->base.prevXRotation -= 360.0f;
    while (np->base.xRotation - np->base.prevXRotation >= 180.0f) np->base.prevXRotation += 360.0f;

    np->animStep += animDelta;
}

void NetworkPlayer_render(const NetworkPlayer* np, float partialTicks, float localPlayerYaw, Font* font) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texChar);

    float bodyYaw   = np->yBodyRotO + (np->yBodyRot - np->yBodyRotO) * partialTicks;
    float headYaw   = np->base.prevYRotation + (np->base.yRotation - np->base.prevYRotation) * partialTicks;
    float headPitch = np->base.prevXRotation + (np->base.xRotation - np->base.prevXRotation) * partialTicks;
    headYaw -= bodyYaw; // head model rotation is relative to the body, which is rotated separately below
    float animStep  = np->animStepO + (np->animStep - np->animStepO) * partialTicks;

    float b = Entity_getBrightness(&np->base);
    glColor3f(b, b, b);

    float ix = np->base.prevX + (np->base.x - np->base.prevX) * partialTicks;
    float iyRaw = np->base.prevY + (np->base.y - np->base.prevY) * partialTicks;
    float iz = np->base.prevZ + (np->base.z - np->base.prevZ) * partialTicks;

    glPushMatrix();
    glTranslatef(ix, iyRaw - np->base.heightOffset, iz);
    glScalef(1.0f, -1.0f, 1.0f);
    const float size = 0.0625f;
    glScalef(size, size, size);
    float offY = -(fabsf(sinf(animStep * 0.6662f)) * 5.0f) - 23.0f;
    glTranslatef(0.0f, offY, 0.0f);
    // real source rotates by (180-bodyYaw+rotOffs), not bodyYaw directly
    // (Mob.render, inherited by NetworkPlayer via HumanoidMob; rotOffs is 0
    // here) - was just bodyYaw, making other players visually face the
    // opposite direction from their actual movement/facing
    glRotatef(180.0f - bodyYaw, 0.0f, 1.0f, 0.0f);
    ZombieModel_render(&sModel, animStep, headYaw, headPitch);
    glPopMatrix();

    // name tag: floats above the head, billboarded to face the camera
    // (rotated by the local player's own yaw, not the remote player's), a
    // fixed dark gray shadow copy behind it regardless of the main color
    glPushMatrix();
    glTranslatef(ix, iyRaw + 0.8f, iz);
    glRotatef(-localPlayerYaw, 0.0f, 1.0f, 0.0f);
    glScalef(0.05f, -0.05f, 0.05f);
    float textWidth = (float)Font_width(font, np->name);
    glTranslatef(-textWidth / 2.0f, 0.0f, 0.0f);

    bool isNotch = equalsIgnoreCase(np->name, "Notch");
    Font_draw(font, &sNameTess, np->name, 0, 0, isNotch ? 0xFFFF00 : 0xFFFFFF);
    glTranslatef(1.0f, 1.0f, -0.05f);
    Font_draw(font, &sNameTess, np->name, 0, 0, 0x505050);
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
}

void NetworkPlayer_teleport(NetworkPlayer* np, int xRaw, int yRaw, int zRaw, float yaw, float pitch) {
    pushMoveEntry(np, (np->xp + xRaw) / 64.0f, (np->yp + yRaw) / 64.0f, (np->zp + zRaw) / 64.0f,
                  (np->base.yRotation + yaw) / 2.0f, (np->base.xRotation + pitch) / 2.0f, true, true);
    np->xp = xRaw; np->yp = yRaw; np->zp = zRaw;
    pushMoveEntry(np, np->xp / 32.0f, np->yp / 32.0f, np->zp / 32.0f, yaw, pitch, true, true);
}

void NetworkPlayer_queueMoveLook(NetworkPlayer* np, int dx, int dy, int dz, float yaw, float pitch) {
    pushMoveEntry(np, (np->xp + dx / 2.0f) / 32.0f, (np->yp + dy / 2.0f) / 32.0f, (np->zp + dz / 2.0f) / 32.0f,
                  (np->base.yRotation + yaw) / 2.0f, (np->base.xRotation + pitch) / 2.0f, true, true);
    np->xp += dx; np->yp += dy; np->zp += dz;
    pushMoveEntry(np, np->xp / 32.0f, np->yp / 32.0f, np->zp / 32.0f, yaw, pitch, true, true);
}

void NetworkPlayer_queueMove(NetworkPlayer* np, int dx, int dy, int dz) {
    pushMoveEntry(np, (np->xp + dx / 2.0f) / 32.0f, (np->yp + dy / 2.0f) / 32.0f, (np->zp + dz / 2.0f) / 32.0f,
                  0.0f, 0.0f, false, true);
    np->xp += dx; np->yp += dy; np->zp += dz;
    pushMoveEntry(np, np->xp / 32.0f, np->yp / 32.0f, np->zp / 32.0f, 0.0f, 0.0f, false, true);
}

void NetworkPlayer_queueLook(NetworkPlayer* np, float yaw, float pitch) {
    pushMoveEntry(np, 0.0f, 0.0f, 0.0f, (np->base.yRotation + yaw) / 2.0f, (np->base.xRotation + pitch) / 2.0f, true, false);
    pushMoveEntry(np, 0.0f, 0.0f, 0.0f, yaw, pitch, true, false);
}
