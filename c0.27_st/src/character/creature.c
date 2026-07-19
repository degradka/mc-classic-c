// character/creature.c: creature AI, behavior, and rendering

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

// Box-Muller, a real normal distribution, the creeper particles need its shape
static float gaussianRand(void) {
    float u1 = frand01();
    if (u1 < 1e-7f) u1 = 1e-7f;
    float u2 = frand01();
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

extern bool Minecraft_spawnArrow(Level* lvl, Entity* owner, float x, float y, float z, float yaw, float pitch, float speed);
extern void Minecraft_spawnItem(Level* lvl, float x, float y, float z, int resource);
// spawns a leaf-textured particle used only by Creeper's death effect
extern void Minecraft_spawnExplosionParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz, const Tile* tile);

// a 1 in 30 chance per tick to fire, on top of everything
// BasicAttackAI's own melee already does
static void Skeleton_onAfterUpdate(Ai* ai, Entity* mob) {
    if (mob->health > 0 && ai->attackTarget && rand() % 30 == 0) {
        // along the skeleton's own current facing,
        // flipped 180 degrees plus randomized pitch
        float yaw = mob->yRotation + 180.0f + (frand01() * 45.0f - 22.5f);
        float pitch = mob->xRotation - (frand01() * 45.0f - 10.0f);
        Minecraft_spawnArrow(mob->level, mob, mob->x, mob->y, mob->z, yaw, pitch, 1.0f);
    }
}

// before removal, scatter 4-9 collectible arrows around the corpse,
// crediting the player so the arrows can be picked up
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

// a succesful melee hit also deals 6 self-damage to the creeper
// the target is passed as the attacker so it gets knockback
static void Creeper_onAfterAttack(Ai* ai, Entity* mob, Entity* target) {
    (void)ai;
    Mob_hurt(mob, target, 6);
}

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

// JumpAttackAI: a normal jump without a target, with one it stops and pounces forward
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

static void Creature_onDeath(Entity* self, Entity* attacker) {
    Creature* c = (Creature*)self;

    if (attacker) {
        // arrows credit whoever shot them and only the player keeps score
        if (attacker->killCredit) attacker = attacker->killCredit;
        // only the player keeps score, the attacker can be another mob
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

    // pigs and sheep drop 1-2 brown mushrooms no matter what killed them
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
    c->e.aiClassTag = (int)kind;
    c->kind = kind;

    // helmet and armor, 1 in 5 each
    c->hasHelmet = (kind == CREATURE_ZOMBIE || kind == CREATURE_SKELETON) && frand01() < 0.2f;
    c->hasArmor  = (kind == CREATURE_ZOMBIE || kind == CREATURE_SKELETON) && frand01() < 0.2f;

    switch (kind) {
        case CREATURE_ZOMBIE:
            c->e.heightOffset = 1.62f;
            Ai_init(&c->ai, true, 30, 1.0f, 6);
            break;
        case CREATURE_SKELETON:
            c->e.heightOffset = 1.62f;
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

    // removed far below the level like the old zombie
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

// bobStrength, the spider has none
static float bobStrengthForKind(CreatureKind kind) {
    return (kind == CREATURE_SPIDER) ? 0.0f : 1.0f;
}

// the armor layer copies the head and arm rotation from the posed model, plate.png
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

// binds the texture and draws the model, the hurt flash draws it a second time
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
    glColor3f(brightness, brightness, brightness);

    glPushMatrix();
    glTranslatef(ix, iy - 1.62f, iz);

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
    float bobY = -(fabsf(cosf(animStep * 0.6662f)) * 5.0f * run * bobStrengthForKind(c->kind)) - 23.0f;
    glTranslatef(0.0f, bobY, 0.0f);
    glRotatef(renderYaw, 0.0f, 1.0f, 0.0f);
    glScalef(-1.0f, 1.0f, 1.0f);

    glEnable(GL_TEXTURE_2D);
    renderModelForKind(c->kind, c->hasHelmet, c->hasArmor, animStep, run, age, headYaw, headPitch, attackSwing);
    glDisable(GL_TEXTURE_2D);

    // for the last 10 invulnerable ticks after a hit it draws the pose again additively, a white flash
    if (e->invulnerableTime > MOB_INVULNERABLE_DURATION - 10) {
        glColor4f(1.0f, 1.0f, 1.0f, 0.75f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_TEXTURE_2D);
        renderModelForKind(c->kind, c->hasHelmet, c->hasArmor, animStep, run, age, headYaw, headPitch, attackSwing);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    glPopMatrix();
}
