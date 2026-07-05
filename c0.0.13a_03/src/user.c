// user.c

#include "user.h"
#include <stdio.h>

void User_init(User* u, const char* name) {
    snprintf(u->name, sizeof(u->name), "%s", name);
}
