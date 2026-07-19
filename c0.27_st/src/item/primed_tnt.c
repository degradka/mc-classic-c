// item/primed_tnt.c: see primed_tnt.h

#include "primed_tnt.h"
#include "../level/level.h"
#include "../level/tile/tile.h"
#include "../renderer/tessellator.h"
#include "../renderer/textures.h"
#include <GL/glew.h>
#include <math.h>
#include <stdlib.h>

static Tessellator sTess;

static inline float frand01(void) { return (float)rand() / (float)RAND_MAX; }

// matches Java's Random.nextGaussian() (Box-Muller), same approach already
// used by Creeper's own death-explosion particle scatter
static float gaussianRand(void) {
    float u1 = frand01();
    if (u1 < 1e-7f) u1 = 1e-7f;
    float u2 = frand01();
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

// implemented in minecraft.c: spawns into particleEngine, shared with
// Creeper's own death-explosion scatter
extern void Minecraft_spawnExplosionParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz, const Tile* tile);
// implemented in minecraft.c: spawns a SmokeParticle into particleEngine
extern void Minecraft_spawnSmokeParticle(Level* lvl, float x, float y, float z);

void PrimedTnt_init(PrimedTnt* t, Level* level, float x, float y, float z) {
    Entity* e = &t->e;
    Entity_init(e, level);
    Entity_setSize(e, 0.98f, 0.98f);
    e->heightOffset = 0.49f; // bbHeight/2
    Entity_setPosition(e, x, y, z);

    // matches PrimedTnt's own ctor exactly, including its harmless real
    // source bug: the random angle is already in radians (0..2*PI from
    // Math.random()*PI*2), then gets treated as degrees and converted to
    // radians again via *PI/180, leaving only a tiny (~0.11 rad max)
    // horizontal hop, basically imperceptible, but a genuine quirk of the
    // original, not a rounding error introduced here
    float angle = (float)((double)rand() / RAND_MAX * M_PI * 2.0);
    e->motionX = -sinf(angle * (float)M_PI / 180.0f) * 0.02f;
    e->motionY = 0.2f;
    e->motionZ = -cosf(angle * (float)M_PI / 180.0f) * 0.02f;

    e->makeStepSound = false;
    t->fuse = 40; // matches PrimedTnt's own 40 tick fuse
}

// implemented in minecraft.c: does the actual world-hole/particle/entity
// damage work already used by Creeper's own death explosion
extern void Level_explode(Level* level, Entity* source, float x, float y, float z, float radius);

void PrimedTnt_onTick(PrimedTnt* t) {
    Entity* e = &t->e;
    e->prevX = e->x; e->prevY = e->y; e->prevZ = e->z;

    e->motionY -= 0.04f;
    Entity_move(e, e->motionX, e->motionY, e->motionZ);
    e->motionX *= 0.98f;
    e->motionY *= 0.98f;
    e->motionZ *= 0.98f;
    if (e->onGround) {
        e->motionX *= 0.7f;
        e->motionZ *= 0.7f;
        e->motionY *= -0.5f;
    }

    // matches PrimedTnt's own `this.life-- <= 0` exactly: the CURRENT fuse
    // value is what's compared (one more tick of falling/flashing happens
    // at fuse==0 before the explosion actually fires), the decrement itself
    // is just bookkeeping afterward. A pre-decrement here would explode one
    // tick early
    if (t->fuse-- <= 0) {
        Entity_remove(e);
        // matches PrimedTnt's own null-source, radius 4 explosion; no
        // entity is credited/immune, so the player and every mob in range
        // (including whoever lit the fuse) can take damage from their own TNT
        const float radius = 4.0f;
        Level_explode(e->level, NULL, e->x, e->y, e->z, radius);

        // matches PrimedTnt's own 100 particle debris burst exactly: a real
        // Gaussian offset from the blast center, textured as TNT itself (not
        // Leaves like Creeper's own scatter), each particle's own outward
        // velocity scaled by the inverse square of its own distance from
        // center; was entirely missing, TNT explosions had no dedicated
        // particle burst at all beyond whatever Level_explode's own 30%
        // per-block drop chance incidentally showed
        for (int i = 0; i < 100; ++i) {
            float dx = gaussianRand() * radius / 4.0f;
            float dy = gaussianRand() * radius / 4.0f;
            float dz = gaussianRand() * radius / 4.0f;
            float mag = sqrtf(dx * dx + dy * dy + dz * dz);
            if (mag < 0.0001f) mag = 0.0001f;
            float mx = dx / mag / mag;
            float my = dy / mag / mag;
            float mz = dz / mag / mag;
            Minecraft_spawnExplosionParticle(e->level, e->x + dx, e->y + dy, e->z + dz, mx, my, mz, &TILE_TNT);
        }
    } else {
        // matches PrimedTnt's own tick() tail exactly: every tick that
        // doesn't explode instead emits one SmokeParticle just above the
        // block's own center, a lit-fuse cue that was entirely missing
        // before particles.png was available to render it
        Minecraft_spawnSmokeParticle(e->level, e->x, e->y + 0.6f, e->z);
    }
}

void PrimedTnt_render(const PrimedTnt* t, float partialTicks) {
    const Entity* e = &t->e;
    float brightness = Level_getBrightness(e->level, (int)e->x, (int)e->y, (int)e->z);
    int life = t->fuse;

    float ix = e->prevX + (e->x - e->prevX) * partialTicks;
    float iy = e->prevY + (e->y - e->prevY) * partialTicks;
    float iz = e->prevZ + (e->z - e->prevZ) * partialTicks;

    static int tex = 0;
    if (!tex) tex = loadTexture("resources/terrain.png", GL_NEAREST);

    glPushMatrix();
    glColor4f(brightness, brightness, brightness, 1.0f);
    glTranslatef(ix - 0.5f, iy - 0.5f, iz - 0.5f);

    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    // this port's renderItem shades entirely via vertex color (its own
    // renderFace emits no normals for GL_LIGHTING to react to), so lighting
    // needs to be off for both passes here, same reasoning as the first
    // person hand's own held-block render
    glDisable(GL_LIGHTING);
    // normally shaded first pass, exactly like a placed block
    TILE_TNT.renderItem(&TILE_TNT, &sTess, brightness);

    // CORRECTION: real source disables texturing before the flash pass, so
    // it reads as a flat white silhouette, not a still-visible TNT texture
    // pattern tinted white
    glDisable(GL_TEXTURE_2D);

    // matches PrimedTnt's own flashing white overlay exactly: normally an
    // every-4-tick blink at 0.4 alpha, then an every-tick blink at 0.6 alpha
    // once life<=16, then a steady 0.9 once life<=2 (about to go off).
    // Lighting off + additive blend so this reads as a glow layered on top
    // of the first pass, not a color swap. CORRECTION: this alpha was
    // silently never taking effect, since Tessellator_color only stores RGB, and
    // the tessellator emits color via a 3 component glColorPointer, which
    // per GL spec forces alpha to 1.0 for every vertex regardless of the
    // glColor4f set here, so the overlay always drew fully opaque instead
    // of blinking (read as "just gets brighter", never fading back down).
    // Fixed by having renderItem ignore its own internal per-face color
    // calls for this second pass, so the raw glColor4f below (alpha
    // included) is what actually reaches the vertices
    float alpha = (float)((life / 4 + 1) % 2) * 0.4f;
    if (life <= 16) alpha = (float)((life + 1) % 2) * 0.6f;
    if (life <= 2)  alpha = 0.9f;

    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // ignoreColor keeps renderItem's own internal Tessellator_color calls
    // (which would both override this flat white AND silently drop alpha
    // back to 1.0) from running, so the glColor4f above, alpha included,
    // is what actually reaches every vertex of this pass
    Tessellator_setIgnoreColor(&sTess, 1);
    TILE_TNT.renderItem(&TILE_TNT, &sTess, 1.0f);
    Tessellator_setIgnoreColor(&sTess, 0);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    // CORRECTION: real source explicitly re-enables texturing here rather
    // than leaving it off; this was the opposite call, leaving global GL
    // texture state disabled on exit instead of restored
    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
    glPopMatrix();
}
