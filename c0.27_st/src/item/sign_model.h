// item/sign_model.h: the sign's board and post

#ifndef SIGN_MODEL_H
#define SIGN_MODEL_H

#include "../character/cube.h"

typedef struct {
    Cube board;
    Cube post;
} SignModel;

void SignModel_init(SignModel* m);

#endif
