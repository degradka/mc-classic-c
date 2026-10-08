// character/creature.c: creature AI, behavior, and rendering

#include "creature.h"
#include "mob_zombie_model.h"
#include "mob_skeleton_model.h"
#include "quadruped_model.h"
#include "mob_spider_model.h"
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
#include "../math_helper.h"

extern bool Minecraft_spawnArrow(Level* lvl, Entity* owner, float x, float y, float z, float yaw, float pitch, float speed);
extern void Minecraft_spawnItem(Level* lvl, float x, float y, float z, int resource);
// spawns a leaf-textured particle used only by Creeper's death effect
extern void Minecraft_spawnExplosionParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz);

// a 1 in 30 chance per tick to fire, on top of everything
// BasicAttackAI's own melee already does
static void Skeleton_onAfterUpdate(Ai* ai, Entity* mob) {
    if (mob->health > 0 && JRandom_nextIntN(&ai->random, 30) == 0 && ai->attackTarget) {
        // along the skeleton's own current facing,
        // flipped 180 degrees plus randomized pitch
        float yaw = mob->yRotation + 180.0f + (float)(JRandom_mathRandom() * 45.0 - 22.5);
        float pitch = mob->xRotation - (float)(JRandom_mathRandom() * 45.0 - 10.0);
        Minecraft_spawnArrow(mob->level, mob, mob->x, mob->y, mob->z, yaw, pitch, 1.0f);
    }
}

// before removal, scatter 4-9 collectible arrows around the corpse,
// crediting the player so the arrows can be picked up
static void Skeleton_onDeathTimeout(Ai* ai, Entity* mob) {
    (void)ai;
    int count = (int)((JRandom_mathRandom() + JRandom_mathRandom()) * 3.0 + 4.0);
    Entity* player = Level_getPlayer(mob->level);
    for (int i = 0; i < count; ++i) {
        float yaw = (float)JRandom_mathRandom() * 360.0f;
        float pitch = -((float)JRandom_mathRandom()) * 60.0f;
        Minecraft_spawnArrow(mob->level, player, mob->x, mob->y - 0.2f, mob->z, yaw, pitch, 0.4f);
    }
}

// a succesful melee hit also deals 6 self-damage to the creeper
// the target is passed as the attacker so it gets knockback
static void Creeper_onAfterAttack(Ai* ai, Entity* mob, Entity* target) {
    (void)ai;
    Mob_hurt(mob, target, 6);
}

// on death explode with radius 4 and throw 500 particles
// particle velocity uses source's inverse-square scaling, not a unit vector
static void Creeper_onDeathTimeout(Ai* ai, Entity* mob) {
    const float radius = 4.0f;
    Level_explode(mob->level, mob, mob->x, mob->y, mob->z, radius);
    for (int i = 0; i < 500; ++i) {
        float dx = (float)JRandom_nextGaussian(&ai->random) * radius / 4.0f;
        float dy = (float)JRandom_nextGaussian(&ai->random) * radius / 4.0f;
        float dz = (float)JRandom_nextGaussian(&ai->random) * radius / 4.0f;
        float mag = MathHelper_sqrt(dx * dx + dy * dy + dz * dz);
        float mx = dx / mag / mag;
        float my = dy / mag / mag;
        float mz = dz / mag / mag;
        Minecraft_spawnExplosionParticle(mob->level, mob->x + dx, mob->y + dy, mob->z + dz, mx, my, mz);
    }
}

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
            case CREATURE_ZOMBIE:   Player_awardKillScore(killer, 80);  break;
            case CREATURE_SKELETON: Player_awardKillScore(killer, 120); break;
            case CREATURE_CREEPER:  Player_awardKillScore(killer, 200); break;
            case CREATURE_PIG:      Player_awardKillScore(killer, 10);  break;
            case CREATURE_SPIDER:   Player_awardKillScore(killer, 105); break;
            case CREATURE_SHEEP:    Player_awardKillScore(killer, 10);  break;
        }
    }

    // pigs and sheep drop 1-2 brown mushrooms no matter what killed them
    if (c->kind == CREATURE_PIG || c->kind == CREATURE_SHEEP) {
        int count = (int)(JRandom_mathRandom() + JRandom_mathRandom() + 1.0);
        for (int i = 0; i < count; ++i) {
            Minecraft_spawnItem(self->level, self->x, self->y, self->z, TILE_MUSHROOM_BROWN.id);
        }
    }
}

// eats the grass in front of it to grow its wool
static void Sheep_update(Ai* ai, Entity* mob) {
    Creature* c = (Creature*)mob;
    float rad = mob->yRotation * 3.1415927f / 180.0f;
    float fx = -0.7f * MathHelper_sin(rad);
    float fz = 0.7f * MathHelper_cos(rad);
    int x = (int)(mob->x + fx);
    int y = (int)(mob->y - 2.0f);
    int z = (int)(mob->z + fz);
    if (c->grazing) {
        if (Level_getTile(mob->level, x, y, z) != TILE_GRASS.id) {
            c->grazing = false;
            return;
        }
        if (++c->grazingTime == 60) {
            level_setTile(mob->level, x, y, z, TILE_DIRT.id);
            if (JRandom_nextIntN(&ai->random, 5) == 0) c->hasFur = true;
        }
        ai->speedX = 0.0f;
        ai->speedZ = 0.0f;
        mob->xRotation = (float)(40 + c->grazingTime / 2 % 2 * 10);
        return;
    }
    if (Level_getTile(mob->level, x, y, z) == TILE_GRASS.id) {
        c->grazing = true;
        c->grazingTime = 0;
    }
    Ai_update(ai, mob);
}

static void Sheep_afterAiStep(Entity* e) {
    Creature* c = (Creature*)e;
    c->grazeO = c->graze;
    c->graze = c->grazing ? (c->graze + 0.2f) : (c->graze - 0.2f);
    if (c->graze < 0.0f) c->graze = 0.0f;
    if (c->graze > 1.0f) c->graze = 1.0f;
}

static bool Sheep_hurtOverride(Entity* e, Entity* attacker) {
    return Creature_shear((Creature*)e, attacker);
}

bool Creature_shear(Creature* c, Entity* attacker) {
    if (c->kind != CREATURE_SHEEP || !c->hasFur || !attacker || attacker->aiClassTag != AI_CLASS_PLAYER) return false;
    c->hasFur = false;
    int count = (int)(JRandom_mathRandom() * 3.0 + 1.0);
    for (int i = 0; i < count; ++i) Minecraft_spawnItem(c->e.level, c->e.x, c->e.y, c->e.z, TILE_CLOTH[15].id);
    return true;
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
    c->bobStrength = 1.0f;
    c->hasFur = true;
    c->grazing = false;
    c->grazingTime = 0;
    c->graze = c->grazeO = 0.0f;
    if (kind == CREATURE_ZOMBIE || kind == CREATURE_SKELETON) {
        c->helmet = JRandom_mathRandom() < (double)0.2f;
        c->armor = JRandom_mathRandom() < (double)0.2f;
    }

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
            c->bobStrength = 0.0f;
            Ai_init(&c->ai, true, 0, 0.7f * 0.8f, 6);
            c->ai.jumpAttack = true;
            break;
        case CREATURE_SHEEP:
            c->e.heightOffset = 1.72f;
            Entity_setSize(&c->e, 1.4f, 1.72f);
            Ai_init(&c->ai, false, 0, 0.7f, 6);
            c->ai.onUpdate = Sheep_update;
            c->e.afterAiStep = Sheep_afterAiStep;
            c->e.hurtOverride = Sheep_hurtOverride;
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
        case CREATURE_SPIDER:   return "resources/mob/spider.png";
        case CREATURE_SHEEP:    return "resources/mob/sheep.png";
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
                                float headYaw, float headPitch, float attackSwing, float partialTicks) {
    static int texZombie = 0, texSkeleton = 0, texCreeper = 0, texPig = 0, texSpider = 0, texSheep = 0, texFur = 0;
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
                static QuadrupedModel sModel; static bool init = false;
                if (!init) { QuadrupedModel_init(&sModel, 6); init = true; }
                QuadrupedModel_render(&sModel, animStep, run, headYaw, headPitch);
            }
            break;
        case CREATURE_SHEEP:
            if (!texSheep) texSheep = loadTexture(textureForKind(kind), GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, texSheep);
            {
                static QuadrupedModel sModel, sFur; static bool init = false;
                if (!init) { SheepModel_init(&sModel); SheepFurModel_init(&sFur); init = true; }
                float headY = sModel.head.y, headZ = sModel.head.z;
                sModel.head.y += (c->grazeO + (c->graze - c->grazeO) * partialTicks) * 8.0f;
                sModel.head.z -= c->grazeO + (c->graze - c->grazeO) * partialTicks;
                QuadrupedModel_render(&sModel, animStep, run, headYaw, headPitch);
                if (c->hasFur) {
                    if (!texFur) texFur = loadTexture("resources/mob/sheep_fur.png", GL_NEAREST);
                    glBindTexture(GL_TEXTURE_2D, texFur);
                    glDisable(GL_CULL_FACE);
                    // the head copies x not z, the fur head stays put while it grazes
                    sFur.head.yRot = sModel.head.yRot;
                    sFur.head.xRot = sModel.head.xRot;
                    sFur.head.y = sModel.head.y;
                    sFur.head.x = sModel.head.x;
                    sFur.body.yRot = sModel.body.yRot;
                    sFur.body.xRot = sModel.body.xRot;
                    sFur.legBackLeft.xRot = sModel.legBackLeft.xRot;
                    sFur.legBackRight.xRot = sModel.legBackRight.xRot;
                    sFur.legFrontLeft.xRot = sModel.legFrontLeft.xRot;
                    sFur.legFrontRight.xRot = sModel.legFrontRight.xRot;
                    Cube_render(&sFur.head);
                    Cube_render(&sFur.body);
                    Cube_render(&sFur.legBackLeft);
                    Cube_render(&sFur.legBackRight);
                    Cube_render(&sFur.legFrontLeft);
                    Cube_render(&sFur.legFrontRight);
                }
                sModel.head.y = headY;
                sModel.head.z = headZ;
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

static void creatureModel(const void* self, float animStep, float run, float age,
                          float headYaw, float headPitch, float attackSwing, float partialTicks) {
    const Creature* c = (const Creature*)self;
    renderModelForKind(c, c->kind, animStep, run, age, headYaw, headPitch, attackSwing, partialTicks);
}

void Creature_render(const Creature* c, float partialTicks) {
    const Entity* e = &c->e;
    float brightness = Entity_getBrightness(e);
    if (c->kind == CREATURE_CREEPER) {
        // Creeper.getBrightness, creepers pulse brighter as they get hurt
        float hurt = (float)(20 - e->health) / 20.0f;
        brightness *= (MathHelper_sin((float)e->tickCount + partialTicks) * 0.5f + 0.5f) * hurt * 0.5f + 0.25f + hurt * 0.25f;
    }
    Mob_render(e, partialTicks, brightness, c->bobStrength, true, creatureModel, c);
}
