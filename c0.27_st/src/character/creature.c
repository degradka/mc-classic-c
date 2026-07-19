// creature.c: a concrete spawnable AI driven mob, see creature.h

#include "creature.h"
#include "mob_zombie_model.h"
#include "mob_skeleton_model.h"
#include "mob_pig_model.h"
#include "mob_creeper_model.h"
#include "mob_spider_model.h"
#include "humanoid_armor_model.h"
#include "../renderer/textures.h"
#include "../mob.h"
#include "../player.h"
#include "../level/level.h"
#include "../level/tile/tile.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

// Box-Muller transform: an actual standard normal (mean 0, stddev 1)
// distribution, not just a uniform substitute, since Creeper's own death
// particle scatter relies on the real Gaussian's own clustered-toward-center
// shape (matches Random.nextGaussian() closely enough; exact bit-for-bit
// sequences were never a goal in this port, but the distribution's actual
// shape visibly matters here, unlike the flat rolls used for AI ticks)
static float gaussianRand(void) {
    float u1 = frand01();
    if (u1 < 1e-7f) u1 = 1e-7f;
    float u2 = frand01();
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

// implemented in minecraft.c: spawns into arrows[]
extern bool Minecraft_spawnArrow(Level* lvl, Entity* owner, float x, float y, float z, float yaw, float pitch, float speed);
// implemented in minecraft.c: spawns into items[]
extern void Minecraft_spawnItem(Level* lvl, float x, float y, float z, int resource);
// implemented in minecraft.c: spawns a Leaves textured particle (real
// source's own a.y constant), used only by Creeper's own death scatter
extern void Minecraft_spawnExplosionParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz, const Tile* tile);

// matches Skeleton$1's own tick() override: while alive and a target is
// locked, a 1 in 30 chance per tick to fire, on top of everything
// BasicAttackAI's own melee already does
static void Skeleton_onAfterUpdate(Ai* ai, Entity* mob) {
    if (mob->health > 0 && ai->attackTarget && rand() % 30 == 0) {
        // matches Skeleton.shootArrow(Level): along the skeleton's own
        // current facing, flipped 180 degrees plus up to +-22.5 degrees of
        // horizontal jitter, and a smaller vertical jitter, speed 1.0
        float yaw = mob->yRotation + 180.0f + (frand01() * 45.0f - 22.5f);
        float pitch = mob->xRotation - (frand01() * 45.0f - 10.0f);
        Minecraft_spawnArrow(mob->level, mob, mob->x, mob->y, mob->z, yaw, pitch, 1.0f);
    }
}

// matches Skeleton.access$000, fired from Skeleton$1's own beforeRemove()
// override, itself Ai_onDeathTimeout, called once from Mob_onTick right
// before a dead Skeleton actually gets removed: scatters 4 to 9 arrows (two
// averaged random rolls times 3 plus 4) in fully random directions around
// the corpse, credited to the player rather than the dead Skeleton itself so
// they can actually be picked back up
static void Skeleton_onDeathTimeout(Ai* ai, Entity* mob) {
    (void)ai;
    int count = (int)((frand01() + frand01()) * 3.0f + 4.0f);
    Entity* player = Level_getPlayer(mob->level);
    for (int i = 0; i < count; ++i) {
        float yaw = frand01() * 360.0f;
        float pitch = -(frand01() * 60.0f);
        Minecraft_spawnArrow(mob->level, player, mob->x, mob->y - 0.2f, mob->z, yaw, pitch, 0.4f);
    }
}

// matches Creeper$1's own attack() override calling super.attack() first,
// then, only once that landed a real hit, this.mob.hurt(entity, 6): the
// Creeper damages itself 6, using the target purely as the "attacker"
// argument for hurtDir/knockback bookkeeping. The player takes no extra
// damage from this at all, the Creeper takes 6 backfire damage per landed
// melee hit
static void Creeper_onAfterAttack(Ai* ai, Entity* mob, Entity* target) {
    (void)ai;
    Mob_hurt(mob, target, 6);
}

// matches Creeper$1's own beforeRemove() override, itself Ai_onDeathTimeout,
// called once from Mob_onTick right before a dead Creeper actually gets
// removed: a radius 4 explosion (crater plus nearby entity damage, see
// Level_explode), then 500 particles scattered via a real Gaussian offset,
// each one's own outward velocity scaled by the inverse square of its own
// distance from center (closer particles fly out faster), matching the
// real source's own f3/f6/f6 formula exactly, an actual bytecode confirmed
// detail, not a typo to "fix" into a plain unit vector
static void Creeper_onDeathTimeout(Ai* ai, Entity* mob) {
    (void)ai;
    const float radius = 4.0f;
    Level_explode(mob->level, mob, mob->x, mob->y, mob->z, radius);
    for (int i = 0; i < 500; ++i) {
        float dx = gaussianRand() * radius / 4.0f;
        float dy = gaussianRand() * radius / 4.0f;
        float dz = gaussianRand() * radius / 4.0f;
        float mag = sqrtf(dx * dx + dy * dy + dz * dz);
        if (mag < 0.0001f) mag = 0.0001f;
        float mx = dx / mag / mag;
        float my = dy / mag / mag;
        float mz = dz / mag / mag;
        Minecraft_spawnExplosionParticle(mob->level, mob->x + dx, mob->y + dy, mob->z + dz, mx, my, mz, &TILE_LEAVES);
    }
}

// matches JumpAttackAI's own jumpFromGround() override: with no locked
// target, an ordinary jump (the same 0.42f hop every other mob gets, since
// this hook fully replaces the default rather than layering on it). With a
// target, zero all horizontal drift and lunge forward along the current
// facing instead, at a higher arc than a normal jump, a pounce rather than
// continuous movement
static void Spider_onJumpFromGround(Ai* ai, Entity* mob) {
    if (!ai->attackTarget) {
        mob->motionY = 0.42f;
        return;
    }
    mob->motionX = 0.0f;
    mob->motionZ = 0.0f;
    Entity_moveRelative(mob, 0.0f, 1.0f, 0.6f);
    mob->motionY = 0.5f;
}

// score awarded per kind on a credited kill, Mob_hurt's attacker argument,
// c0.27_st: Creeper/Zombie's own die() overrides are gone in real source,
// replaced by a deathScore field set in each species' constructor instead
// (Mob's own base die() reads it generically). Skeleton's constructor now
// sets its own deathScore too, running after Zombie's inherited default and
// overwriting it, so it's no longer silently tied to Zombie's value the way
// it was every earlier version (100, via the missing override this comment
// used to describe as "a genuine original quirk, preserved faithfully",
// but that framing is now stale, since this really did change here)
static void Creature_onDeath(Entity* self, Entity* attacker) {
    Creature* c = (Creature*)self;

    if (attacker) {
        // an Arrow is itself the attacker, so knockback and hurtDir resolve
        // relative to where it struck, but redirects credit to whoever shot
        // it via killCredit
        if (attacker->killCredit) attacker = attacker->killCredit;
        // CORRECTION: matches real source's own awardKillScore dispatch, where
        // Entity's own base implementation is an empty no-op, only Player
        // overrides it to actually add score. A mob can retaliate-lock onto
        // and kill another mob (Ai_onHurt's own cross-species aggro, a
        // genuine intentional feature), so attacker here is not always the
        // Player, but it used to get unconditionally cast to Player* and
        // written through regardless, corrupting whatever memory actually
        // sits at that offset inside the real (non-Player) attacker
        if (attacker->aiClassTag == AI_CLASS_PLAYER) {
            Player* killer = (Player*)attacker;
            switch (c->kind) {
                case CREATURE_ZOMBIE:   Player_awardKillScore(killer, 80);  break;
                case CREATURE_SKELETON: Player_awardKillScore(killer, 120); break;
                case CREATURE_CREEPER:  Player_awardKillScore(killer, 200); break;
                case CREATURE_PIG:      Player_awardKillScore(killer, 10);  break;
                case CREATURE_SPIDER:   Player_awardKillScore(killer, 105); break;
            }
        }
    }

    // matches Pig.die(): drops 1 to 2 brown mushrooms unconditionally,
    // regardless of whether this was a credited kill or environmental
    // damage (fall, drown, lava), unlike the score award above
    if (c->kind == CREATURE_PIG) {
        int count = (int)(frand01() + frand01() + 1.0f);
        for (int i = 0; i < count; ++i) {
            Minecraft_spawnItem(self->level, self->x, self->y, self->z, TILE_MUSHROOM_BROWN.id);
        }
    }
}

void Creature_init(Creature* c, CreatureKind kind, Level* level, float x, float y, float z) {
    Entity_init(&c->e, level);
    Mob_init(&c->e);
    c->e.ai = &c->ai;
    c->e.onDeath = Creature_onDeath;
    // matches Java's getClass() equality check inside BasicAttackAI.hurt():
    // each CreatureKind counts as its own distinct kind of thing, so a
    // zombie hitting a skeleton (or vice versa) makes them fight, but a
    // zombie hitting another zombie does not
    c->e.aiClassTag = (int)kind;
    c->kind = kind;

    // c0.27_st: matches HumanoidMob's own per-instance field initializers
    // exactly, a 20% independent chance each, rolled once at construction.
    // Real source only has these fields on HumanoidMob (Zombie/Skeleton's
    // own base class), so every other kind leaves them false
    c->hasHelmet = (kind == CREATURE_ZOMBIE || kind == CREATURE_SKELETON) && frand01() < 0.2f;
    c->hasArmor  = (kind == CREATURE_ZOMBIE || kind == CREATURE_SKELETON) && frand01() < 0.2f;

    switch (kind) {
        case CREATURE_ZOMBIE:
            c->e.heightOffset = 1.62f;
            Ai_init(&c->ai, true, 30, 1.0f, 6);
            break;
        case CREATURE_SKELETON:
            c->e.heightOffset = 1.62f;
            // CORRECTION: wander pitch 0, not 30. Skeleton's real ctor chains
            // to Zombie's own ctor (which sets 30 on a throwaway BasicAttackAI),
            // then immediately builds a brand new Skeleton$1 instance and
            // reassigns this.ai to it, discarding that throwaway object
            // entirely. Skeleton$1 never touches defaultLookAngle, so it
            // keeps AI's own field-initializer default of 0, bytecode
            // confirmed via javap (no defaultLookAngle write reaches the
            // object that ends up as Skeleton's real ai)
            Ai_init(&c->ai, true, 0, 0.3f, 8);
            c->ai.onAfterUpdate = Skeleton_onAfterUpdate;
            c->ai.onDeathTimeout = Skeleton_onDeathTimeout;
            break;
        case CREATURE_CREEPER:
            c->e.heightOffset = 1.62f;
            Ai_init(&c->ai, true, 45, 0.7f, 6);
            c->ai.onAfterAttack = Creeper_onAfterAttack;
            c->ai.onDeathTimeout = Creeper_onDeathTimeout;
            break;
        case CREATURE_PIG:
            c->e.heightOffset = 1.72f;
            Entity_setSize(&c->e, 1.4f, 1.2f);
            Ai_init(&c->ai, false, 0, 0.7f, 6);
            break;
        case CREATURE_SPIDER:
            // matches Spider's own ctor: heightOffset 0.72, size 1.4x0.9
            // (shorter than Pig's 1.2), JumpAttackAI (chase=true,
            // defaultLookAngle 0, runSpeed 0.7*0.8=0.56, damage unchanged
            // at BasicAttackAI's own default of 6)
            c->e.heightOffset = 0.72f;
            Entity_setSize(&c->e, 1.4f, 0.9f);
            Ai_init(&c->ai, true, 0, 0.56f, 6);
            c->ai.onJumpFromGround = Spider_onJumpFromGround;
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
        case CREATURE_SPIDER:   return "resources/mob/spider.png";
    }
    return "resources/mob/zombie.png";
}

// c0.27_st: matches Mob's new bobStrength field (default 1.0), multiplied
// into the walk head-bob calc in Creature_render. Only Spider overrides it,
// to 0, so it doesn't head-bob while walking
static float bobStrengthForKind(CreatureKind kind) {
    return (kind == CREATURE_SPIDER) ? 0.0f : 1.0f;
}

// c0.27_st: matches HumanoidMob's own armor overlay pass exactly, binding
// /armor/plate.png, copying the just-posed base model's own head/rightArm/
// leftArm rotation onto the shared armor model (body never rotates), and
// rendering whichever parts showHelmet/showArmor enable. GL_ALPHA_TEST is
// left alone here (not toggled at all): this codebase's own standing
// invariant is that alpha test stays enabled globally for the whole
// program, set up once at init, so the parts of plate.png not covered by
// armor (transparent) already draw correctly without touching it. GL_CULL_FACE
// toggling is intentionally omitted too, per the standing project invariant
// that it is never enabled anywhere in this port; real source's own
// enable/disable pairs around this pass would be a no-op reproduction of
// those same invariants anyway
static void renderArmorOverlay(bool showHelmet, bool showArmor,
                               float headXRot, float headYRot,
                               float rightArmXRot, float rightArmZRot,
                               float leftArmXRot, float leftArmZRot) {
    if (!showHelmet && !showArmor) return;

    static int texArmor = 0;
    if (!texArmor) texArmor = loadTexture("resources/armor/plate.png", GL_NEAREST);
    static HumanoidArmorModel sArmor; static bool armorInit = false;
    if (!armorInit) { HumanoidArmorModel_init(&sArmor); armorInit = true; }

    glBindTexture(GL_TEXTURE_2D, texArmor);
    HumanoidArmorModel_render(&sArmor, showHelmet, showArmor,
                              headXRot, headYRot,
                              rightArmXRot, rightArmZRot,
                              leftArmXRot, leftArmZRot);
}

// binds this kind's own texture and renders its model, factored out of
// Creature_render so the invulnerability flash overlay, a second additive
// blended rerender of the exact same pose, doesn't have to duplicate the
// whole switch
static void renderModelForKind(CreatureKind kind, bool hasHelmet, bool hasArmor,
                                float animStep, float run, float age,
                                float headYaw, float headPitch, float attackSwing) {
    static int texZombie = 0, texSkeleton = 0, texCreeper = 0, texPig = 0, texSpider = 0;
    switch (kind) {
        case CREATURE_ZOMBIE:
            if (!texZombie) texZombie = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texZombie);
            {
                static MobZombieModel sModel; static bool init = false;
                if (!init) { MobZombieModel_init(&sModel); init = true; }
                MobZombieModel_render(&sModel, animStep, run, age, headYaw, headPitch, attackSwing);
                renderArmorOverlay(hasHelmet, hasArmor,
                                   sModel.head.xRot, sModel.head.yRot,
                                   sModel.rightArm.xRot, sModel.rightArm.zRot,
                                   sModel.leftArm.xRot, sModel.leftArm.zRot);
            }
            break;
        case CREATURE_SKELETON:
            if (!texSkeleton) texSkeleton = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texSkeleton);
            {
                static MobSkeletonModel sModel; static bool init = false;
                if (!init) { MobSkeletonModel_init(&sModel); init = true; }
                MobSkeletonModel_render(&sModel, animStep, run, age, headYaw, headPitch, attackSwing);
                renderArmorOverlay(hasHelmet, hasArmor,
                                   sModel.head.xRot, sModel.head.yRot,
                                   sModel.rightArm.xRot, sModel.rightArm.zRot,
                                   sModel.leftArm.xRot, sModel.leftArm.zRot);
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
        case CREATURE_SPIDER:
            if (!texSpider) texSpider = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texSpider);
            {
                static MobSpiderModel sModel; static bool init = false;
                if (!init) { MobSpiderModel_init(&sModel); init = true; }
                MobSpiderModel_render(&sModel, animStep, run, age, headYaw, headPitch);
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
    float bobY = -(fabsf(cosf(animStep * 0.6662f)) * 5.0f * run * bobStrengthForKind(c->kind)) - 23.0f;
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
    renderModelForKind(c->kind, c->hasHelmet, c->hasArmor, animStep, run, age, headYaw, headPitch, attackSwing);
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
        renderModelForKind(c->kind, c->hasHelmet, c->hasArmor, animStep, run, age, headYaw, headPitch, attackSwing);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // matches Mob.java:271, restored before any other entity/HUD draw sees it
    }

    glPopMatrix();
}
