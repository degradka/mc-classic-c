// mob_zombie_model.h: the real AI-driven Zombie's own model (c0.24_st_03),
// distinct from the shared humanoid box model in zombie_model.c/h (which is
// actually the base HumanoidModel used by NetworkPlayer's own rendering, and
// which this project keeps under the "Zombie" name for historical reasons
// going back to the old debug Zombie entity, do not confuse the two). Same
// box geometry as that shared model, but a different animation: arms locked
// forward instead of swinging while walking, only moving for a punch swing
// (driven by attackTime) and a small idle sway, matching the real source's
// mob/d/e.java overriding mob/d/h.java's animation but not its geometry

#ifndef MOB_ZOMBIE_MODEL_H
#define MOB_ZOMBIE_MODEL_H

#include "cube.h"

typedef struct {
    Cube head;
    Cube body;
    Cube rightArm, leftArm;
    Cube rightLeg, leftLeg;
    // c0.27_st: matches HumanoidMob's own always-on "hair" second head layer
    // (real source's hasHair defaults true, never overridden), a slightly
    // enlarged (+0.5 expand) copy of the head box, same texture, offset to
    // atlas position (32,0), that always follows the head's own rotation
    Cube hair;
} MobZombieModel;

void MobZombieModel_init(MobZombieModel* m);
// animStep/run/age/headYaw/headPitch match the shared ZombieModel_render's
// own parameters; attackSwing is new here, 0..1 over the attack animation
// (matches the real source's model.a = max(0,attackTime-partialTick)/5)
void MobZombieModel_render(MobZombieModel* m, float animStep, float run, float age,
                           float headYaw, float headPitch, float attackSwing);

#endif
