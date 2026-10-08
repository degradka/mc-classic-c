// character/quadruped_model.h: QuadrupedModel and the pig and sheep models built on it

#ifndef QUADRUPED_MODEL_H
#define QUADRUPED_MODEL_H

#include "cube.h"

typedef struct {
    Cube head, body;
    Cube legBackLeft, legBackRight, legFrontLeft, legFrontRight;
} QuadrupedModel;

// legHeight is 6 for the pig, 12 for the sheep
void QuadrupedModel_init(QuadrupedModel* m, int legHeight);
void SheepModel_init(QuadrupedModel* m);
void SheepFurModel_init(QuadrupedModel* m);
void QuadrupedModel_render(QuadrupedModel* m, float animStep, float run, float headYaw, float headPitch);

#endif
