// item/sign_model.c: the Sign's board+post model, see sign_model.h

#include "sign_model.h"

void SignModel_init(SignModel* m) {
    Cube_init(&m->board, 0, 0);
    Cube_addBox(&m->board, -12, -14, -1, 24, 12, 2);

    Cube_init(&m->post, 0, 14);
    Cube_addBox(&m->post, -1, -2, -1, 2, 14, 2);
}
