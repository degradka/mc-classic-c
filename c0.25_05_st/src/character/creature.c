// creature.c: a concrete spawnable AI driven mob, see creature.h

#include "creature.h"
#include "mob_zombie_model.h"
#include "mob_skeleton_model.h"
#include "mob_pig_model.h"
#include "mob_creeper_model.h"
#include "../renderer/textures.h"
#include "../mob.h"
#include "../player.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>

// score awarded per kind on a credited kill, Mob_hurt's attacker argument,
// matches each species' own die(Entity) override. Skeleton has none of its
// own, mob/Skeleton.java doesn't override die(), so killing one silently
// credits Zombie's own value instead. This is a genuine original quirk,
// preserved faithfully rather than corrected
static void Creature_onDeath(Entity* self, Entity* attacker) {
    if (!attacker) return; // only a credited kill awards score
    Creature* c = (Creature*)self;
    // an Arrow is itself the attacker, so knockback and hurtDir resolve
    // relative to where it struck, but redirects credit to whoever shot it
    // via killCredit. Anything else attacking is the local Player directly
    if (attacker->killCredit) attacker = attacker->killCredit;
    Player* killer = (Player*)attacker;
    switch (c->kind) {
        case CREATURE_ZOMBIE:
        case CREATURE_SKELETON: Player_awardKillScore(killer, 100); break;
        case CREATURE_CREEPER:  Player_awardKillScore(killer, 250); break;
        case CREATURE_PIG:      Player_awardKillScore(killer, 10);  break;
    }
}

void Creature_init(Creature* c, CreatureKind kind, Level* level, float x, float y, float z) {
    Entity_init(&c->e, level);
    Mob_init(&c->e);
    c->e.ai = &c->ai;
    c->e.onDeath = Creature_onDeath;
    c->kind = kind;

    switch (kind) {
        case CREATURE_ZOMBIE:
        case CREATURE_SKELETON:
            c->e.heightOffset = 1.62f;
            Ai_init(&c->ai, true, 30);
            break;
        case CREATURE_CREEPER:
            c->e.heightOffset = 1.62f;
            Ai_init(&c->ai, true, 45);
            break;
        case CREATURE_PIG:
            c->e.heightOffset = 1.72f;
            Entity_setSize(&c->e, 1.4f, 1.2f);
            Ai_init(&c->ai, false, 0);
            break;
    }

    Entity_setPosition(&c->e, x, y, z);
}

void Creature_onTick(Creature* c) {
    Entity_onTick(&c->e);
    Mob_onTick(&c->e);

    // safety net matching the old debug Zombie's own out of bounds cleanup:
    // nothing should ever legitimately fall this far below a 64 deep world
    if (c->e.y < -100.0f) Entity_remove(&c->e);
}

static const char* textureForKind(CreatureKind kind) {
    switch (kind) {
        case CREATURE_ZOMBIE:   return "resources/mob/zombie.png";
        case CREATURE_SKELETON: return "resources/mob/skeleton.png";
        case CREATURE_CREEPER:  return "resources/mob/creeper.png";
        case CREATURE_PIG:      return "resources/mob/pig.png";
    }
    return "resources/mob/zombie.png";
}

// binds this kind's own texture and renders its model, factored out of
// Creature_render so the invulnerability flash overlay, a second additive
// blended rerender of the exact same pose, doesn't have to duplicate the
// whole switch
static void renderModelForKind(CreatureKind kind, float animStep, float run, float age,
                                float headYaw, float headPitch, float attackSwing) {
    static int texZombie = 0, texSkeleton = 0, texCreeper = 0, texPig = 0;
    switch (kind) {
        case CREATURE_ZOMBIE:
            if (!texZombie) texZombie = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texZombie);
            {
                static MobZombieModel sModel; static bool init = false;
                if (!init) { MobZombieModel_init(&sModel); init = true; }
                MobZombieModel_render(&sModel, animStep, run, age, headYaw, headPitch, attackSwing);
            }
            break;
        case CREATURE_SKELETON:
            if (!texSkeleton) texSkeleton = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texSkeleton);
            {
                static MobSkeletonModel sModel; static bool init = false;
                if (!init) { MobSkeletonModel_init(&sModel); init = true; }
                MobSkeletonModel_render(&sModel, animStep, run, age, headYaw, headPitch, attackSwing);
            }
            break;
        case CREATURE_CREEPER:
            if (!texCreeper) texCreeper = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texCreeper);
            {
                static MobCreeperModel sModel; static bool init = false;
                if (!init) { MobCreeperModel_init(&sModel); init = true; }
                MobCreeperModel_render(&sModel, animStep, run, age, headYaw, headPitch);
            }
            break;
        case CREATURE_PIG:
            if (!texPig) texPig = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texPig);
            {
                static MobPigModel sModel; static bool init = false;
                if (!init) { MobPigModel_init(&sModel); init = true; }
                MobPigModel_render(&sModel, animStep, run, age, headYaw, headPitch);
            }
            break;
    }
}

void Creature_render(const Creature* c, float partialTicks) {
    const Entity* e = &c->e;

    // interpolated body yaw, run, and animStep, matches the exact same
    // wraparound then lerp pattern already proven correct by NetworkPlayer's
    // own rendering, net/network_player.c
    float bodyYawO = e->prevYBodyRotation;
    while (bodyYawO - e->yBodyRotation < -180.0f) bodyYawO += 360.0f;
    while (bodyYawO - e->yBodyRotation >= 180.0f) bodyYawO -= 360.0f;
    float bodyYaw = bodyYawO + (e->yBodyRotation - bodyYawO) * partialTicks;

    float headYawO = e->prevYRotation;
    while (headYawO - e->yRotation < -180.0f) headYawO += 360.0f;
    while (headYawO - e->yRotation >= 180.0f) headYawO -= 360.0f;
    float headPitchO = e->prevXRotation;
    while (headPitchO - e->xRotation < -180.0f) headPitchO += 360.0f;
    while (headPitchO - e->xRotation >= 180.0f) headPitchO -= 360.0f;

    float headYaw   = headYawO + (e->yRotation - headYawO) * partialTicks;
    float headPitch = headPitchO + (e->xRotation - headPitchO) * partialTicks;
    headYaw -= bodyYaw;

    float run = e->oRun + (e->run - e->oRun) * partialTicks;
    float animStep = e->animStepO + (e->animStep - e->animStepO) * partialTicks;
    float age = (float)e->tickCount + partialTicks;
    float attackSwing = (float)e->attackTime - partialTicks;
    if (attackSwing < 0.0f) attackSwing = 0.0f;
    attackSwing /= (float)MOB_ATTACK_DURATION;

    float ix = e->prevX + (e->x - e->prevX) * partialTicks;
    float iy = e->prevY + (e->y - e->prevY) * partialTicks;
    float iz = e->prevZ + (e->z - e->prevZ) * partialTicks;

    float brightness = Entity_getBrightness(e);
    glColor3f(brightness, brightness, brightness);

    glPushMatrix();
    glTranslatef(ix, iy - 1.62f, iz); // real source's own fixed constant, not this Entity's own heightOffset

    // real source rotates the whole pose by 180 minus bodyYaw plus rotOffs,
    // not bodyYaw directly, Mob.java:238,243,247. rotOffs defaults to 0 and
    // no mob in this version overrides it, so this is just 180 minus
    // bodyYaw here. The same formula applies in network_player.c, which
    // inherits the exact same Mob.render() in real source via HumanoidMob
    float renderYaw = 180.0f - bodyYaw;

    // hurt flash wobble, a brief sideways tilt pulse, and once dead, a tips
    // over effect that grows to a full 90 degree fall over about a second,
    // matching Mob.render()'s own formula, bytecode verified
    float hurtPulse = (float)e->hurtTime - partialTicks;
    if (hurtPulse > 0.0f || e->health <= 0) {
        if (hurtPulse < 0.0f) {
            hurtPulse = 0.0f;
        } else {
            hurtPulse /= (float)e->hurtDuration;
            hurtPulse = (float)sin(hurtPulse * hurtPulse * hurtPulse * hurtPulse * M_PI) * 14.0f;
        }
        if (e->health <= 0) {
            float deathT = ((float)e->deathTime + partialTicks) / 20.0f;
            hurtPulse += deathT * deathT * 800.0f;
            if (hurtPulse > 90.0f) hurtPulse = 90.0f;
        }
        glRotatef(renderYaw, 0.0f, 1.0f, 0.0f);
        glRotatef(-e->hurtDir, 0.0f, 1.0f, 0.0f);
        glRotatef(-hurtPulse, 0.0f, 0.0f, 1.0f);
        glRotatef(e->hurtDir, 0.0f, 1.0f, 0.0f);
        glRotatef(-renderYaw, 0.0f, 1.0f, 0.0f);
    }

    glScalef(1.0f, -1.0f, 1.0f);
    const float scale = 0.0625f;
    glScalef(scale, scale, scale); // pixel-unit model boxes -> world block units
    float bobY = -(fabsf(cosf(animStep * 0.6662f)) * 5.0f * run) - 23.0f;
    glTranslatef(0.0f, bobY, 0.0f);
    glRotatef(renderYaw, 0.0f, 1.0f, 0.0f);
    // real source's Mob.render() branches here on allowAlpha, default true,
    // no mob in this version overrides it false. The true case disables
    // GL_CULL_FACE, not GL_ALPHA_TEST, and since this project's own client
    // never actually enables GL_CULL_FACE anywhere, see minecraft.c's own
    // glCullFace and glFrontFace comment, that branch is a no op here.
    // Alpha test must stay enabled, the ambient state set up once in
    // init(), so alpha zero texture regions actually get discarded instead
    // of drawing as solid black, which matters for skeleton.png's ribcage
    // gaps that should be transparent
    glScalef(-1.0f, 1.0f, 1.0f); // c0.0.19a_04 style mirroring fix, same as NetworkPlayer and Zombie

    glEnable(GL_TEXTURE_2D);
    renderModelForKind(c->kind, animStep, run, age, headYaw, headPitch, attackSwing);
    glDisable(GL_TEXTURE_2D);

    // real source's own render() also does a second, additive blended
    // rerender of the exact same pose while invulnerableTime exceeds
    // invulnerableDuration minus 10, the last 10 ticks of a fresh hit's
    // iframes, right after being hit. A brief white glow flash overlay,
    // matches Mob.java:258-265 exactly
    if (e->invulnerableTime > MOB_INVULNERABLE_DURATION - 10) {
        glColor4f(1.0f, 1.0f, 1.0f, 0.75f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_TEXTURE_2D);
        renderModelForKind(c->kind, animStep, run, age, headYaw, headPitch, attackSwing);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // matches Mob.java:271, restored before any other entity/HUD draw sees it
    }

    glPopMatrix();
}
