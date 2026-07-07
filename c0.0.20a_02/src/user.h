// user.h: the local player's multiplayer identity, unused in this version

#ifndef USER_H
#define USER_H

typedef struct User {
    char name[64];
} User;

void User_init(User* u, const char* name);

#endif // USER_H
