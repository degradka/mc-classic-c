// item/sign_model.h: the Sign's board+post model (c0.24_st_03), matches
// item/a.java. Two plain boxes, no animation

#ifndef SIGN_MODEL_H
#define SIGN_MODEL_H

#include "../character/cube.h"

typedef struct {
    Cube board;
    Cube post;
} SignModel;

void SignModel_init(SignModel* m);

#endif
