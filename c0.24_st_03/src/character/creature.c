// character/creature.c: creature AI, behavior, and rendering

#include "creature.h"
#include "mob_zombie_model.h"
#include "mob_skeleton_model.h"
#include "mob_pig_model.h"
#include "mob_creeper_model.h"
#include "../renderer/textures.h"
#include "../mob.h"
#include "../player.h"
#include "../level/level.h"
#include "../level/tile/tile.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>
#include "../jrandom.h"
#include "humanoid_armor_model.h"

// spawns a leaf-textured particle used only by Creeper's death effect
extern void Minecraft_spawnExplosionParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz);

// after every attack attempt the creeper takes 4 from its target
static void Creeper_onAfterAttack(Ai* ai, Entity* mob, Entity* target) {
    (void)ai;
    Mob_hurt(mob, target, 4);
}

// Creeper$1.beforeRemove, closer particles fly out faster
static void Creeper_onDeathTimeout(Ai* ai, Entity* mob) {
    const float radius = 4.0f;
    Level_explode(mob->level, mob, mob->x, mob->y, mob->z, radius);
    for (int i = 0; i < 500; ++i) {
        float dx = (float)JRandom_nextGaussian(&ai->random) * radius / 4.0f;
        float dy = (float)JRandom_nextGaussian(&ai->random) * radius / 4.0f;
        float dz = (float)JRandom_nextGaussian(&ai->random) * radius / 4.0f;
        float mag = sqrtf(dx * dx + dy * dy + dz * dz);
        float mx = dx / mag / mag;
        float my = dy / mag / mag;
        float mz = dz / mag / mag;
        Minecraft_spawnExplosionParticle(mob->level, mob->x + dx, mob->y + dy, mob->z + dz, mx, my, mz);
    }
}

// the skeleton has no die of its own and scores like a zombie
static void Creature_onDeath(Entity* self, Entity* attacker) {
    Creature* c = (Creature*)self;

    if (attacker) {
        // arrows credit whoever shot them and only the player keeps score
        if (attacker->killCredit) attacker = attacker->killCredit;
        if (attacker->aiClassTag != AI_CLASS_PLAYER) attacker = NULL;
    }
    if (attacker) {
        Player* killer = (Player*)attacker;
        switch (c->kind) {
            case CREATURE_ZOMBIE:
            case CREATURE_SKELETON: Player_awardKillScore(killer, 100); break;
            case CREATURE_CREEPER:  Player_awardKillScore(killer, 250); break;
            case CREATURE_PIG:      Player_awardKillScore(killer, 10);  break;
        }
    }
}

void Creature_init(Creature* c, CreatureKind kind, Level* level, float x, float y, float z) {
    Entity_init(&c->e, level);
    Mob_init(&c->e);
    c->e.ai = &c->ai;
    c->e.onDeath = Creature_onDeath;
    // different creature kinds fight, creatures of the same kind do not
    c->e.aiClassTag = (int)kind;
    c->kind = kind;
    c->helmet = c->armor = false;
    if (kind == CREATURE_ZOMBIE || kind == CREATURE_SKELETON) {
        c->helmet = JRandom_mathRandom() < (double)0.2f;
        c->armor = JRandom_mathRandom() < (double)0.2f;
    }

    switch (kind) {
        case CREATURE_ZOMBIE:
            c->e.heightOffset = 1.62f;
            Ai_init(&c->ai, true, 30);
            break;
        case CREATURE_SKELETON:
            c->e.heightOffset = 1.62f;
            Ai_init(&c->ai, true, 30);
            break;
        case CREATURE_CREEPER:
            c->e.heightOffset = 1.62f;
            Ai_init(&c->ai, true, 45);
            c->ai.onAfterAttack = Creeper_onAfterAttack;
            c->ai.onDeathTimeout = Creeper_onDeathTimeout;
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

// renders armor using the same pose as the underlying model
static void renderArmor(const Creature* c, const Cube* head, const Cube* rightArm, const Cube* leftArm) {
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_CULL_FACE);
    if (!c->helmet && !c->armor) {
        glDisable(GL_ALPHA_TEST);
        return;
    }
    static int texArmor = 0;
    if (!texArmor) texArmor = loadTexture("resources/armor/plate.png", GL_NEAREST);
    static HumanoidArmorModel sArmor;
    static bool init = false;
    if (!init) { HumanoidArmorModel_init(&sArmor); init = true; }
    glBindTexture(GL_TEXTURE_2D, texArmor);
    glDisable(GL_CULL_FACE);
    HumanoidArmorModel_render(&sArmor, c->helmet, c->armor, head, rightArm, leftArm);
    glEnable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
}

static void renderModelForKind(const Creature* c, CreatureKind kind, float animStep, float run, float age,
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
                renderArmor(c, &sModel.head, &sModel.rightArm, &sModel.leftArm);
            }
            break;
        case CREATURE_SKELETON:
            if (!texSkeleton) texSkeleton = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texSkeleton);
            {
                static MobSkeletonModel sModel; static bool init = false;
                if (!init) { MobSkeletonModel_init(&sModel); init = true; }
                MobSkeletonModel_render(&sModel, animStep, run, age, headYaw, headPitch, attackSwing);
                renderArmor(c, &sModel.head, &sModel.rightArm, &sModel.leftArm);
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

    // body yaw, run and animStep, wrapped then lerped like the network players
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
    if (c->kind == CREATURE_CREEPER) {
        // Creeper.getBrightness, creepers pulse brighter as they get hurt
        float hurt = (float)(20 - e->health) / 20.0f;
        brightness *= (float)(sin((double)((float)e->tickCount + partialTicks)) * 0.5 + 0.5) * hurt * 0.5f + 0.25f + hurt * 0.25f;
    }
    glColor3f(brightness, brightness, brightness);

    glPushMatrix();
    glTranslatef(ix, iy - 1.62f, iz); // a fixed offset, not the entity's heightOffset

    // turned by 180 - bodyYaw
    float renderYaw = 180.0f - bodyYaw;

    // the hurt wobble, and once dead it tips over to 90 degrees in about a second
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
    // allowAlpha is true for every mob here, so culling is off for the model
    glDisable(GL_CULL_FACE);
    glScalef(-1.0f, 1.0f, 1.0f);

    glEnable(GL_TEXTURE_2D);
    renderModelForKind(c, c->kind, animStep, run, age, headYaw, headPitch, attackSwing);
    glDisable(GL_TEXTURE_2D);

    // for the last 10 invulnerable ticks after a hit it draws the pose again additively, a white flash
    if (e->invulnerableTime > MOB_INVULNERABLE_DURATION - 10) {
        glColor4f(1.0f, 1.0f, 1.0f, 0.75f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_TEXTURE_2D);
        renderModelForKind(c, c->kind, animStep, run, age, headYaw, headPitch, attackSwing);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
    }
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_CULL_FACE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glPopMatrix();
}
