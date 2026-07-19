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

// c0.0.19a_04: the synthetic halfway waypoint's rotation used to be a naive
// (from+to)/2 average, which spun the wrong way around the 0/360 wrap. Now
// takes the shortest path from `from` to `to` and steps halfway along it
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

    // matches NetworkPlayer.tick(): always drains at least one queued
    // waypoint, then up to 5 more only while the backlog is still over 10,
    // a catch up mechanism for lag spikes rather than a normal per tick rate
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
    np->runO = np->run;
    float targetRun = 0.0f;
    if (dist != 0.0f) {
        targetRun = 1.0f;
        animDelta = dist * 3.0f;
        targetBodyYaw = -(atan2f(dz, dx) * 180.0f / (float)M_PI + 90.0f);
    }
    // c0.0.19a_04: run eases toward 0/1 instead of animStep snapping to 0 on
    // stopping, matching the "fixed the default player stance" changelog item
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

    float run = np->runO + (np->run - np->runO) * partialTicks;

    // c0.0.19a_04: explicit wraparound normalization on the prev/current
    // angle pairs right before lerping, on top of the wraparound already
    // done in onTick, which didn't exist before this version
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
    // head model rotation is relative to the body, which is rotated
    // separately below. Matches Mob.java's own "f7 -= f5" exactly, plain
    // yRot minus bodyYaw, no negation. An extra negation here previously
    // worked around the missing body flip below, and would now double
    // compensate in the wrong direction now that the actual flip is
    // restored
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
    // c0.0.19a_04: leg bob switched from a constant amplitude sine to a
    // run speed scaled cosine, matching "fixed the default player stance"
    float offY = -(fabsf(cosf(animStep * 0.6662f)) * 5.0f * run) - 23.0f;
    glTranslatef(0.0f, offY, 0.0f);
    // real source rotates by 180 minus bodyYaw plus rotOffs, not bodyYaw
    // directly, Mob.java:238,243,247, inherited by NetworkPlayer via
    // HumanoidMob, rotOffs is 0 here
    glRotatef(180.0f - bodyYaw, 0.0f, 1.0f, 0.0f);
    // real source's Mob.render() only disables alpha test for allowAlpha
    // false mobs, disabling GL_CULL_FACE instead when true, the default,
    // which is a no op in this project since GL_CULL_FACE is never actually
    // enabled anywhere, see minecraft.c. Alpha test must stay enabled, the
    // ambient state from init(), so any future alpha cutout skin regions
    // render as transparent instead of black
    glScalef(-1.0f, 1.0f, 1.0f); // c0.0.19a_04: fixed mirroring
    ZombieModel_render(&sModel, animStep, run, (float)np->tickCount + partialTicks, headYaw, headPitch);
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

    // c0.0.20a_02: the new per face entity lighting darkens/tints the primary
    // colored text if left enabled here, so it's switched off for that draw
    // and back on for the shadow copy behind it
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

    glDisable(GL_TEXTURE_2D);
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
    // CORRECTION: Move has a position, no rotation: hasPos=true, hasRot=false
    // (was backwards, matching this file's own doc comment but not its code)
    pushMoveEntry(np, (np->xp + dx / 2.0f) / 32.0f, (np->yp + dy / 2.0f) / 32.0f, (np->zp + dz / 2.0f) / 32.0f,
                  0.0f, 0.0f, true, false);
    np->xp += dx; np->yp += dy; np->zp += dz;
    pushMoveEntry(np, np->xp / 32.0f, np->yp / 32.0f, np->zp / 32.0f, 0.0f, 0.0f, true, false);
}

void NetworkPlayer_queueLook(NetworkPlayer* np, float yaw, float pitch) {
    // CORRECTION: Look has a rotation, no position: hasPos=false, hasRot=true
    pushMoveEntry(np, 0.0f, 0.0f, 0.0f, midpointAngle(np->base.yRotation, yaw), midpointAngle(np->base.xRotation, pitch), false, true);
    pushMoveEntry(np, 0.0f, 0.0f, 0.0f, yaw, pitch, false, true);
}
