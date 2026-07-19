// character/humanoid_armor_model.h: matches real source's "humanoid.armor"
// cached model, the same generic humanoid geometry as the base model, but
// built with a uniform +1.0 expand (see d/h.java's own single "h(float
// expand)" constructor, base="humanoid"=h(0), armor="humanoid.armor"=h(1)),
// textured from /armor/plate.png, shared (one instance) across every kind
// that can wear it. Legs are permanently invisible for armor (real source's
// own HumanoidMob.renderModel always sets both leg parts' visibility false)
// so this port skips building them entirely rather than keep dead geometry

#ifndef HUMANOID_ARMOR_MODEL_H
#define HUMANOID_ARMOR_MODEL_H

#include <stdbool.h>
#include "cube.h"

typedef struct {
    Cube head;
    Cube body;
    Cube rightArm, leftArm;
} HumanoidArmorModel;

void HumanoidArmorModel_init(HumanoidArmorModel* m);

// showHelmet/showArmor toggle head vs body+arms independently, matching
// real source's own this.helmet/this.armor booleans. The x/y/zRot pose
// values are copied in from whichever base model (Zombie's or Skeleton's)
// just posed itself this frame, matching real source's own per-field copy
// (head copies xRot+yRot; each arm copies xRot+zRot; body never rotates)
void HumanoidArmorModel_render(HumanoidArmorModel* m, bool showHelmet, bool showArmor,
                               float headXRot, float headYRot,
                               float rightArmXRot, float rightArmZRot,
                               float leftArmXRot, float leftArmZRot);

#endif
