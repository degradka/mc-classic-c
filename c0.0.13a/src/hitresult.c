// hitresult.c — block raycast result

#include "hitresult.h"
#include "player.h"

void hitresult_create(HitResult* h, int x, int y, int z, int o, int f) {
    h->x = x; h->y = y; h->z = z;
    h->o = o; h->f = f;
}

static double distanceTo(const HitResult* h, const Player* player, int editMode) {
    int xx = h->x, yy = h->y, zz = h->z;
    if (editMode == 1) {
        if (h->f == 0) yy--;
        if (h->f == 1) yy++;
        if (h->f == 2) zz--;
        if (h->f == 3) zz++;
        if (h->f == 4) xx--;
        if (h->f == 5) xx++;
    }
    double xd = xx - player->e.x;
    double yd = yy - player->e.y;
    double zd = zz - player->e.z;
    return xd * xd + yd * yd + zd * zd;
}

bool HitResult_isCloserThan(const HitResult* h, const Player* player, const HitResult* o, int editMode) {
    double dist  = distanceTo(h, player, 0);
    double dist2 = distanceTo(o, player, 0);
    if (dist < dist2) return true;

    dist  = distanceTo(h, player, editMode);
    dist2 = distanceTo(o, player, editMode);
    return dist < dist2;
}