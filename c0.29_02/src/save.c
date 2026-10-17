// save.c: level.dat like LevelIO, version 2 is an ObjectOutputStream of the whole Level

#include <GL/glew.h>
#include "save.h"
#include "level/javaser.h"
#include "mob.h"
#include "jrandom.h"
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEVEL_MAGIC 656127880

/* class descriptors, fields in the order they're written */

#define F(t, n) { t, n, NULL }
#define L(t, n, c) { t, n, c }

static const JField ARRAYLIST_F[] = { F('I', "size") };
static const JDesc ARRAYLIST = { "java.util.ArrayList", 8683452581122892189LL, 3, 1, ARRAYLIST_F, NULL };
static const JDesc ARRAYLIST_ARRAY = { "[Ljava.util.ArrayList;", 8686596894184378438LL, 2, 0, NULL, NULL };

static const JField RANDOM_F[] = { F('Z', "haveNextNextGaussian"), F('D', "nextNextGaussian"), F('J', "seed") };
static const JDesc RANDOM = { "java.util.Random", 3905348978240129619LL, 3, 3, RANDOM_F, NULL };

static const JField LEVEL_F[] = {
    F('I', "cloudColor"), F('J', "createTime"), F('Z', "creativeMode"), F('I', "depth"), F('I', "fogColor"), F('Z', "growTrees"),
    F('I', "height"), F('Z', "networkMode"), F('F', "rotSpawn"), F('I', "skyColor"), F('I', "tickCount"), F('I', "unprocessed"),
    F('I', "waterLevel"), F('I', "width"), F('I', "xSpawn"), F('I', "ySpawn"), F('I', "zSpawn"),
    L('L', "blockMap", "Lcom/mojang/minecraft/level/BlockMap;"), L('[', "blocks", "[B"),
    L('L', "creator", "Ljava/lang/String;"), L('L', "name", "Ljava/lang/String;"),
    L('L', "player", "Lcom/mojang/minecraft/Entity;")
};
static const JDesc LEVEL = { "com.mojang.minecraft.level.Level", 0, 2, 22, LEVEL_F, NULL };

static const JField BLOCKMAP_F[] = {
    F('I', "depth"), F('I', "height"), F('I', "width"), L('L', "all", "Ljava/util/List;"),
    L('[', "entityGrid", "[Ljava/util/List;"), L('L', "slot", "Lcom/mojang/minecraft/level/BlockMap$Slot;"),
    L('L', "slot2", "Lcom/mojang/minecraft/level/BlockMap$Slot;"), L('L', "tmp", "Ljava/util/List;")
};
static const JDesc BLOCKMAP = { "com.mojang.minecraft.level.BlockMap", 0, 2, 8, BLOCKMAP_F, NULL };

static const JField SLOT_F[] = {
    F('I', "xSlot"), F('I', "ySlot"), F('I', "zSlot"), L('L', "this$0", "Lcom/mojang/minecraft/level/BlockMap;")
};
static const JDesc SLOT = { "com.mojang.minecraft.level.BlockMap$Slot", 0, 2, 4, SLOT_F, NULL };

static const JField AABB_F[] = { F('F', "epsilon"), F('F', "x0"), F('F', "x1"), F('F', "y0"), F('F', "y1"), F('F', "z0"), F('F', "z1") };
static const JDesc AABB_D = { "com.mojang.minecraft.phys.AABB", 0, 2, 7, AABB_F, NULL };

static const JField ENTITY_F[] = {
    F('F', "bbHeight"), F('F', "bbWidth"), F('Z', "collision"), F('F', "fallDistance"), F('F', "footSize"),
    F('F', "heightOffset"), F('Z', "horizontalCollision"), F('Z', "hovered"), F('Z', "makeStepSound"), F('I', "nextStep"),
    F('Z', "noPhysics"), F('Z', "onGround"), F('F', "pushthrough"), F('Z', "removed"), F('Z', "slide"), F('I', "textureId"), F('F', "walkDist"),
    F('F', "walkDistO"), F('F', "x"), F('F', "xOld"), F('F', "xRot"), F('F', "xRotO"), F('F', "xd"), F('F', "xo"),
    F('F', "y"), F('F', "yOld"), F('F', "yRot"), F('F', "yRotO"), F('F', "ySlideOffset"), F('F', "yd"), F('F', "yo"),
    F('F', "z"), F('F', "zOld"), F('F', "zd"), F('F', "zo"),
    L('L', "bb", "Lcom/mojang/minecraft/phys/AABB;"), L('L', "blockMap", "Lcom/mojang/minecraft/level/BlockMap;"),
    L('L', "level", "Lcom/mojang/minecraft/level/Level;")
};
static const JDesc ENTITY = { "com.mojang.minecraft.Entity", 0, 2, 38, ENTITY_F, NULL };

static const JField MOB_F[] = {
    F('I', "airSupply"), F('Z', "allowAlpha"), F('F', "animStep"), F('F', "animStepO"), F('I', "attackTime"),
    F('F', "bobStrength"), F('Z', "dead"), F('I', "deathScore"), F('I', "deathTime"), F('Z', "hasHair"), F('I', "health"), F('F', "hurtDir"), F('I', "hurtDuration"),
    F('I', "hurtTime"), F('I', "invulnerableDuration"), F('I', "invulnerableTime"), F('I', "lastHealth"),
    F('F', "oRun"), F('F', "oTilt"), F('F', "renderOffset"), F('F', "rot"), F('F', "rotA"), F('F', "rotOffs"), F('F', "run"), F('F', "speed"),
    F('I', "tickCount"), F('F', "tilt"), F('F', "timeOffs"), F('F', "yBodyRot"), F('F', "yBodyRotO"),
    L('L', "ai", "Lcom/mojang/minecraft/mob/ai/AI;"), L('L', "modelName", "Ljava/lang/String;"),
    L('L', "textureName", "Ljava/lang/String;")
};
static const JDesc MOB = { "com.mojang.minecraft.mob.Mob", 0, 2, 33, MOB_F, &ENTITY };

static const JField PLAYER_F[] = {
    F('I', "arrows"), F('F', "bob"), F('F', "oBob"), F('I', "score"), F('B', "userType"),
    L('L', "inventory", "Lcom/mojang/minecraft/player/Inventory;")
};
static const JDesc PLAYER = { "com.mojang.minecraft.player.Player", 0, 2, 6, PLAYER_F, &MOB };

static const JField HUMANOID_F[] = { F('Z', "armor"), F('Z', "helmet") };
static const JDesc HUMANOID = { "com.mojang.minecraft.mob.HumanoidMob", 0, 2, 2, HUMANOID_F, &MOB };
static const JDesc ZOMBIE = { "com.mojang.minecraft.mob.Zombie", 0, 2, 0, NULL, &HUMANOID };
static const JDesc SKELETON = { "com.mojang.minecraft.mob.Skeleton", 0, 2, 0, NULL, &ZOMBIE };
static const JDesc CREEPER = { "com.mojang.minecraft.mob.Creeper", 0, 2, 0, NULL, &MOB };
static const JDesc QUADRUPED = { "com.mojang.minecraft.mob.QuadrupedMob", 0, 2, 0, NULL, &MOB };
static const JDesc PIG = { "com.mojang.minecraft.mob.Pig", 0, 2, 0, NULL, &QUADRUPED };
static const JDesc SPIDER = { "com.mojang.minecraft.mob.Spider", 0, 2, 0, NULL, &QUADRUPED };
static const JField SHEEP_F[] = { F('F', "graze"), F('F', "grazeO"), F('Z', "grazing"), F('I', "grazingTime"), F('Z', "hasFur") };
static const JDesc SHEEP = { "com.mojang.minecraft.mob.Sheep", 0, 2, 5, SHEEP_F, &QUADRUPED };

static const JField AI_F[] = { F('I', "defaultLookAngle") };
static const JDesc AI = { "com.mojang.minecraft.mob.ai.AI", 0, 2, 1, AI_F, NULL };
static const JField BASICAI_F[] = {
    F('I', "attackDelay"), F('Z', "jumping"), F('I', "noActionTime"), F('F', "runSpeed"), F('F', "xxa"),
    F('F', "yRotA"), F('F', "yya"), L('L', "attackTarget", "Lcom/mojang/minecraft/Entity;"),
    L('L', "level", "Lcom/mojang/minecraft/level/Level;"), L('L', "mob", "Lcom/mojang/minecraft/mob/Mob;"),
    L('L', "random", "Ljava/util/Random;")
};
static const JDesc BASICAI = { "com.mojang.minecraft.mob.ai.BasicAI", 0, 2, 11, BASICAI_F, &AI };
static const JField ATTACKAI_F[] = { F('I', "damage") };
static const JDesc ATTACKAI = { "com.mojang.minecraft.mob.ai.BasicAttackAI", 0, 2, 1, ATTACKAI_F, &BASICAI };
static const JField PLAYERAI_F[] = { L('L', "this$0", "Lcom/mojang/minecraft/player/Player;") };
static const JDesc PLAYERAI = { "com.mojang.minecraft.player.Player$1", 0, 2, 1, PLAYERAI_F, &BASICAI };
static const JField SKELETONAI_F[] = { L('L', "this$0", "Lcom/mojang/minecraft/mob/Skeleton;") };
static const JDesc SKELETONAI = { "com.mojang.minecraft.mob.Skeleton$1", 0, 2, 1, SKELETONAI_F, &ATTACKAI };
static const JField CREEPERAI_F[] = { L('L', "this$0", "Lcom/mojang/minecraft/mob/Creeper;") };
static const JDesc JUMPAI = { "com.mojang.minecraft.mob.ai.JumpAttackAI", 0, 2, 0, NULL, &ATTACKAI };
static const JDesc CREEPERAI = { "com.mojang.minecraft.mob.Creeper$1", 0, 2, 1, CREEPERAI_F, &ATTACKAI };
static const JField SHEEPAI_F[] = { L('L', "this$0", "Lcom/mojang/minecraft/mob/Sheep;") };
static const JDesc SHEEPAI = { "com.mojang.minecraft.mob.Sheep$1", 1, 2, 1, SHEEPAI_F, &BASICAI };

static const JField INVENTORY_F[] = { F('I', "selected"), L('[', "count", "[I"), L('[', "popTime", "[I"), L('[', "slots", "[I") };
static const JDesc INVENTORY = { "com.mojang.minecraft.player.Inventory", 0, 2, 4, INVENTORY_F, NULL };

static const JField ITEM_F[] = { F('I', "age"), F('I', "resource"), F('F', "rot"), F('I', "tickCount"), F('F', "xd"), F('F', "yd"), F('F', "zd") };
static const JDesc ITEM = { "com.mojang.minecraft.item.Item", 0, 2, 7, ITEM_F, &ENTITY };
static const JField ARROW_F[] = {
    F('I', "damage"), F('F', "gravity"), F('Z', "hasHit"), F('I', "stickTime"), F('I', "time"), F('I', "type"),
    F('F', "xRot"), F('F', "xRotO"), F('F', "xd"), F('F', "yRot"), F('F', "yRotO"), F('F', "yd"), F('F', "zd"),
    L('L', "owner", "Lcom/mojang/minecraft/Entity;")
};
static const JDesc ARROW = { "com.mojang.minecraft.item.Arrow", 0, 2, 14, ARROW_F, &ENTITY };

/* writing */

enum { K_NONE, K_PLAYER, K_MOB, K_ITEM, K_ARROW };

typedef struct {
    JWriter w;
    const World* world;
    char blockMap, slot, slot2, tmp, all, grid, playerAi, playerRandom;
    JRandom playerRng;
} Out;

static int kindOf(const World* wd, const Entity* e) {
    if (e == &wd->player->e) return K_PLAYER;
    if ((const Creature*)e >= wd->mobs && (const Creature*)e < wd->mobs + *wd->mobCount) return K_MOB;
    if ((const Item*)e >= wd->items && (const Item*)e < wd->items + *wd->itemCount) return ((const Item*)e)->pickedUp ? K_NONE : K_ITEM;
    if ((const Arrow*)e >= wd->arrows && (const Arrow*)e < wd->arrows + *wd->arrowCount) return ((const Arrow*)e)->pickedUp ? K_NONE : K_ARROW;
    return K_NONE;
}

static void writeEntity(Out* o, const Entity* e);

// a removed target isn't written, those copies aren't kept here
static void writeEntityRef(Out* o, const Entity* e) {
    if (!e) { JW_null(&o->w); return; }
    if (JW_ref(&o->w, e)) return;
    if (kindOf(o->world, e) == K_NONE) { JW_null(&o->w); return; }
    writeEntity(o, e);
}

static void writeRandom(Out* o, const JRandom* r, const void* id) {
    JW_object(&o->w, &RANDOM, id);
    JW_u1(&o->w, r->haveNextNextGaussian);
    JW_d8(&o->w, r->nextNextGaussian);
    JW_i8(&o->w, (int64_t)r->seed);
    JW_endBlock(&o->w);
}

static void entityFields(Out* o, const Entity* e, bool own) {
    JWriter* w = &o->w;
    // arrows, items and tnt keep motion and rotation in their own fields
    float xd = own ? 0.0f : e->motionX, yd = own ? 0.0f : e->motionY, zd = own ? 0.0f : e->motionZ;
    float xr = own ? 0.0f : e->xRotation, yr = own ? 0.0f : e->yRotation;
    float xro = own ? 0.0f : e->prevXRotation, yro = own ? 0.0f : e->prevYRotation;
    JW_f4(w, e->boundingBoxHeight);
    JW_f4(w, e->boundingBoxWidth);
    JW_u1(w, e->collision);
    JW_f4(w, e->fallDistance);
    JW_f4(w, e->footSize);
    JW_f4(w, e->heightOffset);
    JW_u1(w, e->horizontalCollision);
    // Entity.hovered, nothing ever sets it
    JW_u1(w, 0);
    JW_u1(w, e->makeStepSound);
    JW_i4(w, e->nextStep);
    JW_u1(w, e->noPhysics);
    JW_u1(w, e->onGround);
    JW_f4(w, e->pushthrough);
    JW_u1(w, e->removed);
    JW_u1(w, kindOf(o->world, e) != K_ARROW);
    JW_i4(w, 0); // a gl texture id, means nothing in another run
    JW_f4(w, e->walkDist);
    JW_f4(w, e->walkDistO);
    JW_f4(w, e->x); JW_f4(w, e->slotted ? e->xOld : e->x); JW_f4(w, xr); JW_f4(w, xro); JW_f4(w, xd); JW_f4(w, e->prevX);
    JW_f4(w, e->y); JW_f4(w, e->slotted ? e->yOld : e->y); JW_f4(w, yr); JW_f4(w, yro); JW_f4(w, e->ySlideOffset); JW_f4(w, yd); JW_f4(w, e->prevY);
    JW_f4(w, e->z); JW_f4(w, e->slotted ? e->zOld : e->z); JW_f4(w, zd); JW_f4(w, e->prevZ);
    JW_object(w, &AABB_D, &e->boundingBox);
    JW_f4(w, 0.0f);
    JW_f4(w, e->boundingBox.minX); JW_f4(w, e->boundingBox.maxX);
    JW_f4(w, e->boundingBox.minY); JW_f4(w, e->boundingBox.maxY);
    JW_f4(w, e->boundingBox.minZ); JW_f4(w, e->boundingBox.maxZ);
    JW_ref(w, &o->blockMap);
    JW_ref(w, o->world->level);
}

// Mob.deathScore, pig scores in its own die instead
static int deathScore(CreatureKind kind) {
    switch (kind) {
        case CREATURE_ZOMBIE:   return 80;
        case CREATURE_SKELETON: return 120;
        case CREATURE_CREEPER:  return 200;
        case CREATURE_SPIDER:   return 105;
        default:                return 0;
    }
}

static void mobFields(Out* o, const Entity* e, const char* model, const char* texture, bool player) {
    JWriter* w = &o->w;
    const Player* p = o->world->player;
    JW_i4(w, e->airSupply);
    JW_u1(w, 1);
    JW_f4(w, e->animStep);
    JW_f4(w, e->animStepO);
    JW_i4(w, e->attackTime);
    JW_f4(w, player ? 1.0f : ((const Creature*)e)->bobStrength);
    JW_u1(w, e->dead);
    JW_i4(w, player ? 0 : deathScore(((const Creature*)e)->kind));
    JW_i4(w, e->deathTime);
    JW_u1(w, 1);
    JW_i4(w, e->health);
    JW_f4(w, e->hurtDir);
    JW_i4(w, e->hurtDuration);
    JW_i4(w, e->hurtTime);
    JW_i4(w, MOB_INVULNERABLE_DURATION);
    JW_i4(w, e->invulnerableTime);
    JW_i4(w, e->lastHealth);
    JW_f4(w, e->oRun);
    JW_f4(w, player ? p->oTilt : 0.0f);
    JW_f4(w, e->renderOffset);
    JW_f4(w, e->mobRot);
    JW_f4(w, e->rotA);
    JW_f4(w, player ? 180.0f : 0.0f);
    JW_f4(w, e->run);
    JW_f4(w, 1.0f);
    JW_i4(w, e->tickCount);
    JW_f4(w, player ? p->tilt : 0.0f);
    JW_f4(w, e->timeOffs);
    JW_f4(w, e->yBodyRotation);
    JW_f4(w, e->prevYBodyRotation);

    if (player) {
        // the player's ai only turns input into movement, nothing of it is kept here
        JW_object(w, &PLAYERAI, &o->playerAi);
        JW_i4(w, 0);
        JW_i4(w, 0); JW_u1(w, 0); JW_i4(w, 0); JW_f4(w, 0.7f); JW_f4(w, 0.0f); JW_f4(w, 0.0f); JW_f4(w, 0.0f);
        JW_null(w);
        JW_ref(w, o->world->level);
        JW_ref(w, e);
        writeRandom(o, &o->playerRng, &o->playerRandom);
        JW_ref(w, e);
    } else {
        const Creature* c = (const Creature*)e;
        const Ai* ai = &c->ai;
        const JDesc* d = c->kind == CREATURE_ZOMBIE ? &ATTACKAI : c->kind == CREATURE_SKELETON ? &SKELETONAI
                       : c->kind == CREATURE_CREEPER ? &CREEPERAI : c->kind == CREATURE_SPIDER ? &JUMPAI
                       : c->kind == CREATURE_SHEEP ? &SHEEPAI : &BASICAI;
        JW_object(w, d, ai);
        JW_i4(w, ai->wanderPitch);
        JW_i4(w, ai->attackCooldown);
        JW_u1(w, ai->wantsJump);
        JW_i4(w, ai->noActionTime);
        JW_f4(w, ai->runSpeed);
        JW_f4(w, ai->speedX);
        JW_f4(w, ai->turnSpeed);
        JW_f4(w, ai->speedZ);
        writeEntityRef(o, ai->attackTarget);
        JW_ref(w, o->world->level);
        JW_ref(w, e);
        writeRandom(o, &ai->random, &ai->random);
        if (d != &BASICAI && d != &SHEEPAI) JW_i4(w, ai->damage);
        if (d == &SKELETONAI || d == &CREEPERAI || d == &SHEEPAI) JW_ref(w, e);
    }
    JW_string(w, model);
    JW_string(w, texture);
}

static void writeEntity(Out* o, const Entity* e) {
    JWriter* w = &o->w;
    int kind = kindOf(o->world, e);
    if (kind == K_PLAYER) {
        const Player* p = o->world->player;
        JW_object(w, &PLAYER, e);
        entityFields(o, e, false);
        mobFields(o, e, "humanoid", "/char.png", true);
        JW_i4(w, p->arrows);
        JW_f4(w, p->bob);
        JW_f4(w, p->oBob);
        JW_i4(w, p->score);
        JW_u1(w, p->userType);
        JW_object(w, &INVENTORY, &p->inventory);
        JW_i4(w, p->inventory.selected);
        JW_intArray(w, p->inventory.count, 9);
        JW_intArray(w, p->inventory.flash, 9);
        JW_intArray(w, p->inventory.id, 9);
    } else if (kind == K_MOB) {
        const Creature* c = (const Creature*)e;
        static const char* const MODEL[] = { "zombie", "skeleton", "creeper", "pig", "spider", "sheep" };
        static const char* const TEXTURE[] = { "/mob/zombie.png", "/mob/skeleton.png", "/mob/creeper.png", "/mob/pig.png", "/mob/spider.png", "/mob/sheep.png" };
        const JDesc* d = c->kind == CREATURE_ZOMBIE ? &ZOMBIE : c->kind == CREATURE_SKELETON ? &SKELETON
                       : c->kind == CREATURE_CREEPER ? &CREEPER : c->kind == CREATURE_SPIDER ? &SPIDER
                       : c->kind == CREATURE_SHEEP ? &SHEEP : &PIG;
        JW_object(w, d, e);
        entityFields(o, e, false);
        mobFields(o, e, MODEL[c->kind], TEXTURE[c->kind], false);
        if (c->kind == CREATURE_ZOMBIE || c->kind == CREATURE_SKELETON) {
            JW_u1(w, c->armor);
            JW_u1(w, c->helmet);
        }
        if (c->kind == CREATURE_SHEEP) {
            JW_f4(w, c->graze);
            JW_f4(w, c->grazeO);
            JW_u1(w, c->grazing);
            JW_i4(w, c->grazingTime);
            JW_u1(w, c->hasFur);
        }
    } else if (kind == K_ITEM) {
        const Item* it = (const Item*)e;
        JW_object(w, &ITEM, e);
        entityFields(o, e, true);
        JW_i4(w, it->age);
        JW_i4(w, it->resource);
        JW_f4(w, it->rot);
        JW_i4(w, it->tickCount);
        JW_f4(w, e->motionX); JW_f4(w, e->motionY); JW_f4(w, e->motionZ);
    } else if (kind == K_ARROW) {
        const Arrow* a = (const Arrow*)e;
        JW_object(w, &ARROW, e);
        entityFields(o, e, true);
        JW_i4(w, a->damage);
        JW_f4(w, a->gravity);
        JW_u1(w, a->hasHit);
        JW_i4(w, a->stickTime);
        JW_i4(w, a->time);
        JW_i4(w, a->type);
        JW_f4(w, e->xRotation); JW_f4(w, e->prevXRotation); JW_f4(w, e->motionX);
        JW_f4(w, e->yRotation); JW_f4(w, e->prevYRotation); JW_f4(w, e->motionY);
        JW_f4(w, e->motionZ);
        writeEntityRef(o, a->owner);
    } else {
        JW_null(w);
    }
}

static int collect(const World* wd, const Entity** out) {
    int n = 0;
    out[n++] = &wd->player->e;
    for (int i = 0; i < *wd->mobCount; ++i) out[n++] = &wd->mobs[i].e;
    for (int i = 0; i < *wd->itemCount; ++i) if (!wd->items[i].pickedUp) out[n++] = &wd->items[i].e;
    for (int i = 0; i < *wd->arrowCount; ++i) if (!wd->arrows[i].pickedUp) out[n++] = &wd->arrows[i].e;
    return n;
}

// the array length of a list that grew one add at a time from 10
static int capacityFor(int n) {
    int c = 10;
    while (c < n) c = c * 3 / 2 + 1;
    return c;
}

static void writeList(Out* o, const void* id, const Entity** list, int n) {
    JWriter* w = &o->w;
    JW_object(w, &ARRAYLIST, id);
    JW_i4(w, n);
    // the array length goes in the block, not the size
    int c = capacityFor(n);
    unsigned char b[4] = { (unsigned char)(c >> 24), (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c };
    JW_block(w, b, 4);
    for (int i = 0; i < n; ++i) writeEntityRef(o, list[i]);
    JW_endBlock(w);
}

static int cell(float v, int size) {
    int c = (int)(v / 16.0f);
    if (c < 0) c = 0;
    if (c >= size) c = size - 1;
    return c;
}

static void writeSlot(Out* o, const void* id) {
    JW_object(&o->w, &SLOT, id);
    JW_i4(&o->w, 0); JW_i4(&o->w, 0); JW_i4(&o->w, 0);
    JW_ref(&o->w, &o->blockMap);
}

bool Save_write(const World* wd, const char* path) {
    const Level* lv = wd->level;
    Out o;
    memset(&o, 0, sizeof o);
    o.world = wd;
    JRandom_initRandom(&o.playerRng);
    JWriter_init(&o.w);
    JWriter* w = &o.w;

    int total = 1 + *wd->mobCount + *wd->itemCount + *wd->arrowCount;
    const Entity** all = (const Entity**)malloc((size_t)total * sizeof(Entity*));
    const Entity** tmp = (const Entity**)malloc((size_t)total * sizeof(Entity*));
    if (!all || !tmp) { free(all); free(tmp); JWriter_free(w); return false; }
    int n = collect(wd, all);

    JW_object(w, &LEVEL, lv);
    JW_i4(w, lv->cloudColor);
    JW_i8(w, lv->createTime);
    JW_u1(w, lv->creativeMode);
    JW_i4(w, lv->depth);
    JW_i4(w, lv->fogColor);
    JW_u1(w, lv->growTrees);
    JW_i4(w, lv->height);
    JW_u1(w, 0);
    JW_f4(w, lv->rotSpawn);
    JW_i4(w, lv->skyColor);
    JW_i4(w, lv->tickCount);
    JW_i4(w, lv->unprocessed);
    JW_i4(w, lv->waterLevel);
    JW_i4(w, lv->width);
    JW_i4(w, lv->xSpawn);
    JW_i4(w, lv->ySpawn);
    JW_i4(w, lv->zSpawn);

    // BlockMap(width, depth, height) counts 16 block cells per axis
    int gw = lv->width / 16, gd = lv->depth / 16, gh = lv->height / 16;
    if (gw == 0) gw = 1;
    if (gd == 0) gd = 1;
    if (gh == 0) gh = 1;
    JW_object(w, &BLOCKMAP, &o.blockMap);
    JW_i4(w, gd);
    JW_i4(w, gh);
    JW_i4(w, gw);
    writeList(&o, &o.all, all, n);
    JW_array(w, &ARRAYLIST_ARRAY, &o.grid, gw * gd * gh);
    for (int z = 0; z < gh; ++z) {
        for (int y = 0; y < gd; ++y) {
            for (int x = 0; x < gw; ++x) {
                int m = 0;
                for (int i = 0; i < n; ++i) {
                    const Entity* e = all[i];
                    float ex = e->slotted ? e->xOld : e->x, ey = e->slotted ? e->yOld : e->y, ez = e->slotted ? e->zOld : e->z;
                    if (e->inGrid && cell(ex, gw) == x && cell(ey, gd) == y && cell(ez, gh) == z) tmp[m++] = e;
                }
                writeList(&o, NULL, tmp, m);
            }
        }
    }
    writeSlot(&o, &o.slot);
    writeSlot(&o, &o.slot2);
    writeList(&o, &o.tmp, NULL, 0);

    JW_byteArray(w, lv->blocks, lv->width * lv->height * lv->depth);
    JW_string(w, lv->creator);
    JW_string(w, lv->name);
    writeEntityRef(&o, &wd->player->e);
    free(all);
    free(tmp);

    bool ok = false;
    gzFile f = gzopen(path, "wb");
    if (f) {
        unsigned char head[5] = { (unsigned char)(LEVEL_MAGIC >> 24), (unsigned char)(LEVEL_MAGIC >> 16),
                                  (unsigned char)(LEVEL_MAGIC >> 8), (unsigned char)LEVEL_MAGIC, 2 };
        ok = gzwrite(f, head, 5) == 5 && gzwrite(f, w->buf, (unsigned)w->len) == (int)w->len;
        ok = gzclose(f) == Z_OK && ok;
    }
    JWriter_free(w);
    return ok;
}

/* reading */

static JStream* gStream = NULL;
static JNode* gRoot = NULL;
static unsigned char* gData = NULL;

static unsigned char* readAll(const char* path, size_t* len) {
    gzFile f = gzopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 1 << 20, n = 0;
    unsigned char* buf = (unsigned char*)malloc(cap);
    while (buf) {
        if (n == cap) {
            unsigned char* g = (unsigned char*)realloc(buf, cap * 2);
            if (!g) { free(buf); buf = NULL; break; }
            buf = g;
            cap *= 2;
        }
        int r = gzread(f, buf + n, (unsigned)(cap - n));
        if (r < 0) { free(buf); buf = NULL; break; }
        if (r == 0) break;
        n += (size_t)r;
    }
    gzclose(f);
    *len = n;
    return buf;
}

static int be(const unsigned char* p, int n) {
    int v = 0;
    for (int i = 0; i < n; ++i) v = (v << 8) | p[i];
    return v;
}

static void copyString(char* dst, size_t size, const unsigned char* src, size_t n) {
    if (n > size - 1) n = size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool loadV1(Level* level, const unsigned char* p, size_t len) {
    size_t pos = 0;
    char name[64], creator[64];
    if (pos + 2 > len) return false;
    size_t n = (size_t)be(p + pos, 2); pos += 2;
    if (pos + n > len) return false;
    copyString(name, sizeof name, p + pos, n); pos += n;
    if (pos + 2 > len) return false;
    n = (size_t)be(p + pos, 2); pos += 2;
    if (pos + n > len) return false;
    copyString(creator, sizeof creator, p + pos, n); pos += n;
    if (pos + 14 > len) return false;
    long long createTime = 0;
    for (int i = 0; i < 8; ++i) createTime = (createTime << 8) | p[pos + i];
    pos += 8;
    int w = be(p + pos, 2), h = be(p + pos + 2, 2), d = be(p + pos + 4, 2);
    pos += 6;
    size_t total = (size_t)w * h * d;
    if (total == 0 || pos + total > len) return false;
    byte* blocks = (byte*)malloc(total);
    if (!blocks) return false;
    memcpy(blocks, p + pos, total);
    Level_setBlocks(level, w, h, d, blocks);
    memcpy(level->name, name, sizeof name);
    memcpy(level->creator, creator, sizeof creator);
    level->createTime = createTime;
    return true;
}

#define LV "com.mojang.minecraft.level.Level"

static void nodeString(const JNode* n, char* dst, size_t size) {
    dst[0] = '\0';
    if (n && n->kind == JN_STRING && n->str) copyString(dst, size, (const unsigned char*)n->str, strlen(n->str));
}

// every class that can be read back
static const char* const KNOWN_CLASSES[] = {
    "[B", "[I", "[Ljava.util.ArrayList;", "java.util.ArrayList", "java.util.Random",
    "com.mojang.minecraft.Entity", "com.mojang.minecraft.item.Arrow", "com.mojang.minecraft.item.Item",
    "com.mojang.minecraft.level.BlockMap", "com.mojang.minecraft.level.BlockMap$Slot", "com.mojang.minecraft.level.Level",
    "com.mojang.minecraft.mob.Creeper", "com.mojang.minecraft.mob.Creeper$1", "com.mojang.minecraft.mob.HumanoidMob",
    "com.mojang.minecraft.mob.Mob", "com.mojang.minecraft.mob.Pig", "com.mojang.minecraft.mob.QuadrupedMob",
    "com.mojang.minecraft.mob.Sheep", "com.mojang.minecraft.mob.Sheep$1", "com.mojang.minecraft.mob.Skeleton",
    "com.mojang.minecraft.mob.Skeleton$1", "com.mojang.minecraft.mob.Spider", "com.mojang.minecraft.mob.Zombie",
    "com.mojang.minecraft.mob.ai.AI", "com.mojang.minecraft.mob.ai.BasicAI", "com.mojang.minecraft.mob.ai.BasicAttackAI",
    "com.mojang.minecraft.mob.ai.JumpAttackAI", "com.mojang.minecraft.phys.AABB", "com.mojang.minecraft.player.Inventory",
    "com.mojang.minecraft.player.Player", "com.mojang.minecraft.player.Player$1",
    "com.mojang.minecraft.item.TakeEntityAnim", "com.mojang.minecraft.net.NetworkPlayer",
    "com.mojang.minecraft.particle.Particle", "com.mojang.minecraft.particle.TerrainParticle", "com.mojang.minecraft.particle.WaterDropParticle",
};

// a class whose serialVersionUID changed is refused, except
// Player$1, Creeper$1 and Skeleton$1 which get through whatever they say
static const char* const CHECKED_NAMES[] = {
    "com.mojang.minecraft.Entity", "com.mojang.minecraft.item.Arrow", "com.mojang.minecraft.item.Item",
    "com.mojang.minecraft.item.TakeEntityAnim", "com.mojang.minecraft.level.BlockMap",
    "com.mojang.minecraft.level.BlockMap$Slot", "com.mojang.minecraft.level.Level", "com.mojang.minecraft.mob.Creeper",
    "com.mojang.minecraft.mob.HumanoidMob", "com.mojang.minecraft.mob.Mob", "com.mojang.minecraft.mob.Pig",
    "com.mojang.minecraft.mob.QuadrupedMob", "com.mojang.minecraft.mob.Sheep", "com.mojang.minecraft.mob.Sheep$1",
    "com.mojang.minecraft.mob.Skeleton", "com.mojang.minecraft.mob.Spider", "com.mojang.minecraft.mob.Zombie",
    "com.mojang.minecraft.mob.ai.AI", "com.mojang.minecraft.mob.ai.BasicAI", "com.mojang.minecraft.mob.ai.BasicAttackAI",
    "com.mojang.minecraft.mob.ai.JumpAttackAI", "com.mojang.minecraft.net.NetworkPlayer",
    "com.mojang.minecraft.particle.Particle", "com.mojang.minecraft.particle.TerrainParticle",
    "com.mojang.minecraft.particle.WaterDropParticle", "com.mojang.minecraft.phys.AABB",
    "com.mojang.minecraft.player.Inventory", "com.mojang.minecraft.player.Player",
};
static const int64_t CHECKED_SUIDS[] = {
    0, 0, 0,
    1, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 1,
    0, 0, 0,
    0, 0, 0,
    0, 77479605454997290LL,
    1, 1,
    1, 0,
    0, 0,
};

static bool loadV2(Level* level, const unsigned char* p, size_t len) {
    JNode* root = NULL;
    JStream* s = JStream_read(p, len, &root);
    if (!s) return false;
    if (!JNode_is(root, LV) || JStream_unknownClass(s, KNOWN_CLASSES, (int)(sizeof KNOWN_CLASSES / sizeof KNOWN_CLASSES[0])) ||
        JStream_suidMismatch(s, CHECKED_NAMES, CHECKED_SUIDS, (int)(sizeof CHECKED_NAMES / sizeof CHECKED_NAMES[0]))) {
        JStream_free(s);
        return false;
    }
    int w = (int)JNode_int(root, LV, "width"), h = (int)JNode_int(root, LV, "height"), d = (int)JNode_int(root, LV, "depth");
    JNode* b = JNode_obj(root, LV, "blocks");
    size_t total = (size_t)w * h * d;
    if (w <= 0 || h <= 0 || d <= 0 || !b || b->kind != JN_ARRAY || !b->bytes || (size_t)b->len != total) {
        JStream_free(s);
        return false;
    }
    byte* blocks = (byte*)malloc(total);
    if (!blocks) { JStream_free(s); return false; }
    memcpy(blocks, b->bytes, total);
    Level_setBlocks(level, w, h, d, blocks);
    nodeString(JNode_obj(root, LV, "name"), level->name, sizeof level->name);
    nodeString(JNode_obj(root, LV, "creator"), level->creator, sizeof level->creator);
    level->createTime = JNode_int(root, LV, "createTime");
    level->creativeMode = JNode_int(root, LV, "creativeMode") != 0;
    level->growTrees = JNode_int(root, LV, "growTrees") != 0;
    level->xSpawn = (int)JNode_int(root, LV, "xSpawn");
    level->ySpawn = (int)JNode_int(root, LV, "ySpawn");
    level->zSpawn = (int)JNode_int(root, LV, "zSpawn");
    level->rotSpawn = JNode_float(root, LV, "rotSpawn");
    level->tickCount = (int)JNode_int(root, LV, "tickCount");
    level->unprocessed = (int)JNode_int(root, LV, "unprocessed");
    // Level.initTransient
    level->waterLevel = (int)JNode_int(root, LV, "waterLevel");
    if (level->waterLevel == 0) level->waterLevel = d / 2;
    level->skyColor = (int)JNode_int(root, LV, "skyColor");
    if (level->skyColor == 0) level->skyColor = 0x99CCFF;
    level->fogColor = (int)JNode_int(root, LV, "fogColor");
    if (level->fogColor == 0) level->fogColor = 0xFFFFFF;
    level->cloudColor = (int)JNode_int(root, LV, "cloudColor");
    if (level->cloudColor == 0) level->cloudColor = 0xFFFFFF;
    gStream = s;
    gRoot = root;
    return true;
}

bool Save_loadLevel(Level* level, const char* path) {
    JStream_free(gStream);
    gStream = NULL;
    gRoot = NULL;
    free(gData);
    gData = NULL;

    size_t len = 0;
    unsigned char* data = readAll(path, &len);
    if (!data) return false;
    bool ok = false;
    if (len >= 5 && be(data, 4) == LEVEL_MAGIC) {
        if (data[4] <= 1) ok = loadV1(level, data + 5, len - 5);
        else if (data[4] == 2) ok = loadV2(level, data + 5, len - 5);
    }
    if (gStream) gData = data;
    else free(data);
    return ok;
}

/* entities of a version 2 file */

#define EN "com.mojang.minecraft.Entity"
#define MB "com.mojang.minecraft.mob.Mob"
#define BA "com.mojang.minecraft.mob.ai.BasicAI"

typedef struct { const JNode* node; Entity* e; } Pair;

static void readEntity(Entity* e, const JNode* n, bool own) {
    e->x = JNode_float(n, EN, "x");
    e->y = JNode_float(n, EN, "y");
    e->z = JNode_float(n, EN, "z");
    e->prevX = JNode_float(n, EN, "xo");
    e->prevY = JNode_float(n, EN, "yo");
    e->prevZ = JNode_float(n, EN, "zo");
    if (!own) {
        e->motionX = JNode_float(n, EN, "xd");
        e->motionY = JNode_float(n, EN, "yd");
        e->motionZ = JNode_float(n, EN, "zd");
        e->xRotation = JNode_float(n, EN, "xRot");
        e->yRotation = JNode_float(n, EN, "yRot");
        e->prevXRotation = JNode_float(n, EN, "xRotO");
        e->prevYRotation = JNode_float(n, EN, "yRotO");
    }
    e->boundingBoxWidth = JNode_float(n, EN, "bbWidth");
    e->boundingBoxHeight = JNode_float(n, EN, "bbHeight");
    e->heightOffset = JNode_float(n, EN, "heightOffset");
    e->footSize = JNode_float(n, EN, "footSize");
    e->ySlideOffset = JNode_float(n, EN, "ySlideOffset");
    e->noPhysics = JNode_int(n, EN, "noPhysics") != 0;
    e->pushthrough = JNode_float(n, EN, "pushthrough");
    e->onGround = JNode_int(n, EN, "onGround") != 0;
    e->horizontalCollision = JNode_int(n, EN, "horizontalCollision") != 0;
    e->collision = JNode_int(n, EN, "collision") != 0;
    e->removed = JNode_int(n, EN, "removed") != 0;
    e->walkDist = JNode_float(n, EN, "walkDist");
    e->walkDistO = JNode_float(n, EN, "walkDistO");
    e->nextStep = (int)JNode_int(n, EN, "nextStep");
    e->makeStepSound = JNode_int(n, EN, "makeStepSound") != 0;
    e->fallDistance = JNode_float(n, EN, "fallDistance");
    const JNode* bb = JNode_obj(n, EN, "bb");
    if (bb) {
        e->boundingBox = AABB_create(JNode_float(bb, NULL, "x0"), JNode_float(bb, NULL, "y0"), JNode_float(bb, NULL, "z0"),
                                     JNode_float(bb, NULL, "x1"), JNode_float(bb, NULL, "y1"), JNode_float(bb, NULL, "z1"));
    }
}

static void readMob(Entity* e, const JNode* n) {
    e->health = (int)JNode_int(n, MB, "health");
    e->lastHealth = (int)JNode_int(n, MB, "lastHealth");
    e->invulnerableTime = (int)JNode_int(n, MB, "invulnerableTime");
    e->hurtTime = (int)JNode_int(n, MB, "hurtTime");
    e->hurtDuration = (int)JNode_int(n, MB, "hurtDuration");
    e->hurtDir = JNode_float(n, MB, "hurtDir");
    e->deathTime = (int)JNode_int(n, MB, "deathTime");
    e->attackTime = (int)JNode_int(n, MB, "attackTime");
    e->airSupply = (int)JNode_int(n, MB, "airSupply");
    e->yBodyRotation = JNode_float(n, MB, "yBodyRot");
    e->prevYBodyRotation = JNode_float(n, MB, "yBodyRotO");
    e->dead = JNode_int(n, MB, "dead") != 0;
    e->tickCount = (int)JNode_int(n, MB, "tickCount");
    e->run = JNode_float(n, MB, "run");
    e->oRun = JNode_float(n, MB, "oRun");
    e->animStep = JNode_float(n, MB, "animStep");
    e->animStepO = JNode_float(n, MB, "animStepO");
    e->timeOffs = JNode_float(n, MB, "timeOffs");
    e->mobRot = JNode_float(n, MB, "rot");
    e->rotA = JNode_float(n, MB, "rotA");
    e->renderOffset = JNode_float(n, MB, "renderOffset");
}

static void readRandom(JRandom* r, const JNode* n) {
    if (!n) return;
    r->seed = (uint64_t)JNode_int(n, NULL, "seed") & ((1ULL << 48) - 1);
    r->haveNextNextGaussian = JNode_int(n, NULL, "haveNextNextGaussian") != 0;
    r->nextNextGaussian = JNode_double(n, NULL, "nextNextGaussian");
}

static void readAi(Ai* ai, const JNode* n) {
    if (!n) return;
    ai->wanderPitch = (int)JNode_int(n, "com.mojang.minecraft.mob.ai.AI", "defaultLookAngle");
    ai->attackCooldown = (int)JNode_int(n, BA, "attackDelay");
    ai->wantsJump = JNode_int(n, BA, "jumping") != 0;
    ai->noActionTime = (int)JNode_int(n, BA, "noActionTime");
    ai->runSpeed = JNode_float(n, BA, "runSpeed");
    ai->speedX = JNode_float(n, BA, "xxa");
    ai->speedZ = JNode_float(n, BA, "yya");
    ai->turnSpeed = JNode_float(n, BA, "yRotA");
    if (JNode_field(n, "com.mojang.minecraft.mob.ai.BasicAttackAI", "damage")) {
        ai->damage = (int)JNode_int(n, "com.mojang.minecraft.mob.ai.BasicAttackAI", "damage");
    }
    readRandom(&ai->random, JNode_obj(n, BA, "random"));
}

static Entity* find(const Pair* pairs, int n, const JNode* node) {
    for (int i = 0; i < n; ++i) if (pairs[i].node == node) return pairs[i].e;
    return NULL;
}

bool Save_loadEntities(World* w) {
    if (!gStream) return false;
    const JNode* bm = JNode_obj(gRoot, LV, "blockMap");
    const JNode* all = bm ? JNode_obj(bm, NULL, "all") : NULL;
    const JNode* playerNode = JNode_obj(gRoot, LV, "player");
    int count = all ? all->nextra : 0;
    Pair* pairs = (Pair*)calloc((size_t)count + 1, sizeof(Pair));
    int np = 0;
    bool havePlayer = false;

    for (int i = 0; pairs && i < count; ++i) {
        const JNode* n = all->extra[i];
        if (!n || n->kind != JN_OBJECT) continue;
        const char* cls = n->cls->name;
        Entity* e = NULL;
        if (strcmp(cls, "com.mojang.minecraft.player.Player") == 0 && (n == playerNode || !havePlayer)) {
            Player* p = w->player;
            e = &p->e;
            readEntity(e, n, false);
            readMob(e, n);
            p->arrows = (int)JNode_int(n, NULL, "arrows");
            p->bob = JNode_float(n, NULL, "bob");
            p->oBob = JNode_float(n, NULL, "oBob");
            p->score = (int)JNode_int(n, NULL, "score");
            p->userType = (int)JNode_int(n, NULL, "userType");
            p->tilt = JNode_float(n, MB, "tilt");
            p->oTilt = JNode_float(n, MB, "oTilt");
            const JNode* inv = JNode_obj(n, NULL, "inventory");
            if (inv) {
                p->inventory.selected = (int)JNode_int(inv, NULL, "selected");
                const JNode* a[3] = { JNode_obj(inv, NULL, "slots"), JNode_obj(inv, NULL, "count"), JNode_obj(inv, NULL, "popTime") };
                int* dst[3] = { p->inventory.id, p->inventory.count, p->inventory.flash };
                for (int k = 0; k < 3; ++k) {
                    for (int j = 0; a[k] && a[k]->prims && j < 9 && j < a[k]->len; ++j) dst[k][j] = (int)a[k]->prims[j];
                }
            }
            havePlayer = true;
        } else if (strcmp(cls, "com.mojang.minecraft.mob.Zombie") == 0 || strcmp(cls, "com.mojang.minecraft.mob.Skeleton") == 0 ||
                   strcmp(cls, "com.mojang.minecraft.mob.Creeper") == 0 || strcmp(cls, "com.mojang.minecraft.mob.Pig") == 0 ||
                   strcmp(cls, "com.mojang.minecraft.mob.Spider") == 0 || strcmp(cls, "com.mojang.minecraft.mob.Sheep") == 0) {
            if (*w->mobCount >= w->maxMobs) continue;
            CreatureKind kind = cls[25] == 'Z' ? CREATURE_ZOMBIE : strcmp(cls + 25, "Skeleton") == 0 ? CREATURE_SKELETON
                              : cls[25] == 'C' ? CREATURE_CREEPER : strcmp(cls + 25, "Spider") == 0 ? CREATURE_SPIDER
                              : strcmp(cls + 25, "Sheep") == 0 ? CREATURE_SHEEP : CREATURE_PIG;
            Creature* c = &w->mobs[(*w->mobCount)++];
            Creature_init(c, kind, w->level, JNode_float(n, EN, "x"), JNode_float(n, EN, "y"), JNode_float(n, EN, "z"));
            e = &c->e;
            readEntity(e, n, false);
            readMob(e, n);
            readAi(&c->ai, JNode_obj(n, MB, "ai"));
            if (kind == CREATURE_ZOMBIE || kind == CREATURE_SKELETON) {
                c->helmet = JNode_int(n, "com.mojang.minecraft.mob.HumanoidMob", "helmet") != 0;
                c->armor = JNode_int(n, "com.mojang.minecraft.mob.HumanoidMob", "armor") != 0;
            }
            if (kind == CREATURE_SHEEP) {
                const char* SH = "com.mojang.minecraft.mob.Sheep";
                c->graze = JNode_float(n, SH, "graze");
                c->grazeO = JNode_float(n, SH, "grazeO");
                c->grazing = JNode_int(n, SH, "grazing") != 0;
                c->grazingTime = (int)JNode_int(n, SH, "grazingTime");
                c->hasFur = JNode_int(n, SH, "hasFur") != 0;
            }
        } else if (strcmp(cls, "com.mojang.minecraft.item.Item") == 0) {
            if (*w->itemCount >= w->maxItems) continue;
            Item* it = &w->items[(*w->itemCount)++];
            Item_init(it, w->level, JNode_float(n, EN, "x"), JNode_float(n, EN, "y"), JNode_float(n, EN, "z"),
                      (int)JNode_int(n, NULL, "resource"));
            e = &it->e;
            readEntity(e, n, true);
            it->age = (int)JNode_int(n, NULL, "age");
            it->rot = JNode_float(n, NULL, "rot");
            it->tickCount = (int)JNode_int(n, NULL, "tickCount");
            e->motionX = JNode_float(n, NULL, "xd");
            e->motionY = JNode_float(n, NULL, "yd");
            e->motionZ = JNode_float(n, NULL, "zd");
        } else if (strcmp(cls, "com.mojang.minecraft.item.Arrow") == 0) {
            if (*w->arrowCount >= w->maxArrows) continue;
            Arrow* a = &w->arrows[(*w->arrowCount)++];
            Arrow_init(a, w->level, NULL, JNode_float(n, EN, "x"), JNode_float(n, EN, "y"), JNode_float(n, EN, "z"), 0.0f, 0.0f, 1.0f);
            e = &a->e;
            readEntity(e, n, true);
            a->damage = (int)JNode_int(n, NULL, "damage");
            a->gravity = JNode_float(n, NULL, "gravity");
            a->hasHit = JNode_int(n, NULL, "hasHit") != 0;
            a->stickTime = (int)JNode_int(n, NULL, "stickTime");
            a->time = (int)JNode_int(n, NULL, "time");
            a->type = (int)JNode_int(n, NULL, "type");
            const char* AR = "com.mojang.minecraft.item.Arrow";
            e->xRotation = JNode_float(n, AR, "xRot");
            e->prevXRotation = JNode_float(n, AR, "xRotO");
            e->yRotation = JNode_float(n, AR, "yRot");
            e->prevYRotation = JNode_float(n, AR, "yRotO");
            e->motionX = JNode_float(n, AR, "xd");
            e->motionY = JNode_float(n, AR, "yd");
            e->motionZ = JNode_float(n, AR, "zd");
        }
        if (e) {
            pairs[np].node = n;
            pairs[np].e = e;
            np++;
        }
    }

    // the file keeps where each entity was slotted and which grid cell holds it
    const JNode* grid = bm ? JNode_obj(bm, NULL, "entityGrid") : NULL;
    for (int i = 0; pairs && i < np; ++i) {
        const JNode* n = pairs[i].node;
        Entity* e = pairs[i].e;
        e->slotted = true;
        e->xOld = JNode_float(n, EN, "xOld");
        e->yOld = JNode_float(n, EN, "yOld");
        e->zOld = JNode_float(n, EN, "zOld");
        e->inGrid = false;
        for (int c = 0; grid && grid->elems && c < grid->len && !e->inGrid; ++c) {
            const JNode* list = grid->elems[c];
            for (int k = 0; list && k < list->nextra && !e->inGrid; ++k) e->inGrid = list->extra[k] == n;
        }
    }

    // references, a target that isn't in the world anymore is dropped
    for (int i = 0; pairs && i < np; ++i) {
        const JNode* n = pairs[i].node;
        Entity* e = pairs[i].e;
        if (JNode_is(n, "com.mojang.minecraft.item.Arrow")) {
            Arrow* a = (Arrow*)e;
            a->owner = find(pairs, np, JNode_obj(n, NULL, "owner"));
            e->killCredit = a->owner;
        } else if (e != &w->player->e && e->ai) {
            const JNode* ai = JNode_obj(n, MB, "ai");
            e->ai->attackTarget = ai ? find(pairs, np, JNode_obj(ai, BA, "attackTarget")) : NULL;
        }
    }

    free(pairs);
    JStream_free(gStream);
    gStream = NULL;
    gRoot = NULL;
    free(gData);
    gData = NULL;
    return havePlayer;
}
