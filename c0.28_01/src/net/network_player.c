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

// the halfway rotation takes the short way round
static float midpointAngle(float from, float to) {
    float diff = to - from;
    while (diff >= 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return from + diff * 0.5f;
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
    np->run = np->runO = 0.0f;
    np->tickCount = 0;
    np->xp = xRaw; np->yp = yRaw; np->zp = zRaw;
    np->queueHead = np->queueCount = 0;

    if (!sModelInit) { ZombieModel_init(&sModel); sModelInit = 1; }
    if (!texChar) texChar = loadTexture("resources/char.png", GL_NEAREST);
}

void NetworkPlayer_onTick(NetworkPlayer* np) {
    Entity_onTick(&np->base);
    np->animStepO = np->animStep;
    np->tickCount++;

    // one move a tick, up to 5 more while more than 10 are waiting
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

    // the body turns toward the movement, the head can look 75 degrees away before it follows
    float dx = np->base.x - np->base.prevX;
    float dz = np->base.z - np->base.prevZ;
    np->yBodyRotO = np->yBodyRot;
    float dist = sqrtf(dx * dx + dz * dz);
    float targetBodyYaw = np->yBodyRot;
    float animDelta = 0.0f;
    np->runO = np->run;
    float targetRun = 0.0f;
    if (dist != 0.0f) {
        targetRun = 1.0f;
        animDelta = dist * 3.0f;
        targetBodyYaw = -(atan2f(dz, dx) * 180.0f / (float)M_PI + 90.0f);
    }
    // run eases toward 0 or 1 instead of snapping
    np->run += (targetRun - np->run) * 0.3f;

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
    np->yBodyRot += headDiff * 0.1f;
    if (facingBackward) animDelta = -animDelta;

    // keep the angle pairs within 180 degrees so the lerp takes the short way
    while (np->base.yRotation - np->base.prevYRotation < -180.0f) np->base.prevYRotation -= 360.0f;
    while (np->base.yRotation - np->base.prevYRotation >= 180.0f) np->base.prevYRotation += 360.0f;
    while (np->yBodyRot - np->yBodyRotO < -180.0f) np->yBodyRotO -= 360.0f;
    while (np->yBodyRot - np->yBodyRotO >= 180.0f) np->yBodyRotO += 360.0f;
    while (np->base.xRotation - np->base.prevXRotation < -180.0f) np->base.prevXRotation -= 360.0f;
    while (np->base.xRotation - np->base.prevXRotation >= 180.0f) np->base.prevXRotation += 360.0f;

    np->animStep += animDelta;
}

void NetworkPlayer_render(const NetworkPlayer* np, float partialTicks, float localPlayerYaw, Font* font) {
    glBindTexture(GL_TEXTURE_2D, texChar);

    float run = np->runO + (np->run - np->runO) * partialTicks;

    // wrap the angle pairs before the lerp
    float bodyYawO = np->yBodyRotO;
    while (bodyYawO - np->yBodyRot < -180.0f) bodyYawO += 360.0f;
    while (bodyYawO - np->yBodyRot >= 180.0f) bodyYawO -= 360.0f;
    float bodyYaw = bodyYawO + (np->yBodyRot - bodyYawO) * partialTicks;

    float headPitchO = np->base.prevXRotation;
    while (headPitchO - np->base.xRotation < -180.0f) headPitchO += 360.0f;
    while (headPitchO - np->base.xRotation >= 180.0f) headPitchO -= 360.0f;

    float headYawO = np->base.prevYRotation;
    while (headYawO - np->base.yRotation < -180.0f) headYawO += 360.0f;
    while (headYawO - np->base.yRotation >= 180.0f) headYawO -= 360.0f;

    float headYaw   = headYawO + (np->base.yRotation - headYawO) * partialTicks;
    float headPitch = headPitchO + (np->base.xRotation - headPitchO) * partialTicks;
    // the head rotation is relative to the body
    headYaw -= bodyYaw;
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
    // the leg swing is a cosine scaled by run
    float offY = -(fabsf(cosf(animStep * 0.6662f)) * 5.0f * run) - 23.0f;
    glTranslatef(0.0f, offY, 0.0f);
    // turned by 180 - bodyYaw
    glRotatef(180.0f - bodyYaw, 0.0f, 1.0f, 0.0f);
    // allowAlpha is false for network players, so the alpha test goes off for the model
    glDisable(GL_ALPHA_TEST);
    glScalef(-1.0f, 1.0f, 1.0f);
    ZombieModel_render(&sModel, animStep, run, (float)np->tickCount + partialTicks, headYaw, headPitch);
    glEnable(GL_ALPHA_TEST);
    glPopMatrix();

    // the name tag faces the local player's yaw, with a dark gray shadow behind it
    glPushMatrix();
    glTranslatef(ix, iyRaw + 0.8f, iz);
    glRotatef(-localPlayerYaw, 0.0f, 1.0f, 0.0f);
    glScalef(0.05f, -0.05f, 0.05f);
    float textWidth = (float)Font_width(font, np->name);
    glTranslatef(-textWidth / 2.0f, 0.0f, 0.0f);

    // the entity lighting would tint the text, off for it and back on for the shadow
    glNormal3f(1.0f, -1.0f, 1.0f);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    bool isNotch = equalsIgnoreCase(np->name, "Notch");
    Font_draw(font, &sNameTess, np->name, 0, 0, isNotch ? 0xFFFF00 : 0xFFFFFF);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glTranslatef(1.0f, 1.0f, -0.05f);
    Font_draw(font, &sNameTess, np->name, 0, 0, 0x505050);
    glPopMatrix();
}

void NetworkPlayer_teleport(NetworkPlayer* np, int xRaw, int yRaw, int zRaw, float yaw, float pitch) {
    pushMoveEntry(np, (np->xp + xRaw) / 64.0f, (np->yp + yRaw) / 64.0f, (np->zp + zRaw) / 64.0f,
                  midpointAngle(np->base.yRotation, yaw), midpointAngle(np->base.xRotation, pitch), true, true);
    np->xp = xRaw; np->yp = yRaw; np->zp = zRaw;
    pushMoveEntry(np, np->xp / 32.0f, np->yp / 32.0f, np->zp / 32.0f, yaw, pitch, true, true);
}

void NetworkPlayer_queueMoveLook(NetworkPlayer* np, int dx, int dy, int dz, float yaw, float pitch) {
    pushMoveEntry(np, (np->xp + dx / 2.0f) / 32.0f, (np->yp + dy / 2.0f) / 32.0f, (np->zp + dz / 2.0f) / 32.0f,
                  midpointAngle(np->base.yRotation, yaw), midpointAngle(np->base.xRotation, pitch), true, true);
    np->xp += dx; np->yp += dy; np->zp += dz;
    pushMoveEntry(np, np->xp / 32.0f, np->yp / 32.0f, np->zp / 32.0f, yaw, pitch, true, true);
}

void NetworkPlayer_queueMove(NetworkPlayer* np, int dx, int dy, int dz) {
    pushMoveEntry(np, (np->xp + dx / 2.0f) / 32.0f, (np->yp + dy / 2.0f) / 32.0f, (np->zp + dz / 2.0f) / 32.0f,
                  0.0f, 0.0f, true, false);
    np->xp += dx; np->yp += dy; np->zp += dz;
    pushMoveEntry(np, np->xp / 32.0f, np->yp / 32.0f, np->zp / 32.0f, 0.0f, 0.0f, true, false);
}

void NetworkPlayer_queueLook(NetworkPlayer* np, float yaw, float pitch) {
    pushMoveEntry(np, 0.0f, 0.0f, 0.0f, midpointAngle(np->base.yRotation, yaw), midpointAngle(np->base.xRotation, pitch), false, true);
    pushMoveEntry(np, 0.0f, 0.0f, 0.0f, yaw, pitch, false, true);
}
