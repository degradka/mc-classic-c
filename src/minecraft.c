// minecraft.c — entry point, window+gl init, camera, picking, main loop

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>

#include <GL/glew.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include "common.h"

#include "level/level.h"
#include "level/level_renderer.h"
#include "renderer/frustum.h"
#include "level/tile/tile.h"
#include "renderer/textures.h"
#include "particle/particle_engine.h"
#include "player.h"
#include "character/zombie.h"
#include "timer.h"
#include "hitresult.h"
#include "gui/font.h"

#define MAX_MOBS 100

static GLFWwindow*    window;
static Level          level;
static LevelRenderer  levelRenderer;
static Player         player;
static Timer          timer;
static Zombie         mobs[MAX_MOBS];
static ParticleEngine particleEngine;

static int mobCount = 0;

static int prevLeft  = GLFW_RELEASE;
static int prevRight = GLFW_RELEASE;
static int prevEnter = GLFW_RELEASE;
static int prevNum1 = GLFW_RELEASE, prevNum2 = GLFW_RELEASE;
static int prevNum3 = GLFW_RELEASE, prevNum4 = GLFW_RELEASE;
static int prevNum6 = GLFW_RELEASE;
static int prevG    = GLFW_RELEASE;
static int prevY    = GLFW_RELEASE;

static int gEditMode = 0;              // 0=destroy, 1=place
static int gYMouseAxis = 1;            // toggled by Y key (1 or -1)

static int texTerrain = 0;

static Tessellator hudTess;
static Font gFont;                     // HUD font
static int selectedTileId = 1;         // 1=rock, 3=dirt, 4=stoneBrick, 5=wood
static int gFPS = 0;                   // last computed fps per second
static int gChunkUpdatesPerSec = 0;    // last computed chunk updates per second
static int gChunkUpdatesAccum = 0;     // accumulates during the current second

static int gWinWidth  = 854;
static int gWinHeight = 480;
static int gIsFullscreen = 0;

static GLfloat fogColorDaylight[4] = { 254.0f/255.0f, 251.0f/255.0f, 250.0f/255.0f, 1.0f };
static GLfloat fogColorShadow  [4] = {  14.0f/255.0f,  11.0f/255.0f,  10.0f/255.0f, 1.0f };

static int      isHitNull = 1;
static HitResult hitResult;

static void tick(Player* player, GLFWwindow* window);

/* --- input & GL state helpers ------------------------------------------------ */

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        if (glfwGetInputMode(w, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
            int ww, wh; glfwGetWindowSize(w, &ww, &wh);
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glfwSetCursorPos(w, ww * 0.5, wh * 0.5);
        }
    }
}

static void setupFog(int type) {
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP);

    if (type == 0) { // daylight
        glFogf(GL_FOG_DENSITY, 0.001f);
        glFogfv(GL_FOG_COLOR, fogColorDaylight);
        glDisable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);
    } else {         // shadow
        glFogf(GL_FOG_DENSITY, 0.01f);
        glFogfv(GL_FOG_COLOR, fogColorShadow);
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);

        GLfloat ambient[4] = { 0.6f, 0.6f, 0.6f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    }
}

/* --- boot/shutdown ----------------------------------------------------------- */

static int init(Level* lvl, LevelRenderer* lr, Player* p) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 0;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    if (gIsFullscreen) {
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.0.11a", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.0.11a", NULL, NULL);
    }

    if (!window) {
        glfwTerminate();
        fprintf(stderr, "Failed to create GLFW window\n");
        return 0;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // uncapped

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW init failed: %s\n", glewGetErrorString(err));
        return 0;
    }

    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_LIGHTING);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.5f, 0.8f, 1.0f, 0.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);

    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
    glfwSetKeyCallback(window, keyCallback);

    Tile_registerAll();

    texTerrain = loadTexture("resources/terrain.png", GL_NEAREST);
    Font_init(&gFont, "resources/default.gif");

    Level_init(lvl, 256, 256, 64);
    LevelRenderer_init(lr, lvl, texTerrain);
    calcLightDepths(lvl, 0, 0, lvl->width, lvl->height);

    Player_init(p, lvl);

    ParticleEngine_init(&particleEngine, lvl, (GLuint)texTerrain);

    mobCount = 0;
    for (int i = 0; i < 10 && i < MAX_MOBS; ++i) {
        Zombie_init(&mobs[mobCount], lvl, 128.0, 0.0, 129.0);
        Entity_resetPosition(&mobs[mobCount].base);
        mobCount++;
    }

    Timer_init(&timer, 20.0f);

    return 1;
}

static void destroy(Level* lvl) {
    Level_save(lvl);
    Level_destroy(lvl);
    glfwDestroyWindow(window);
    glfwTerminate();
}

/* --- camera ------------------------------------------------------------------ */

static void moveCameraToPlayer(Player* p, float t) {
    glTranslatef(0.0f, 0.0f, -0.3f); // eye offset

    glRotatef(p->e.xRotation, 1.0f, 0.0f, 0.0f);
    glRotatef(p->e.yRotation, 0.0f, 1.0f, 0.0f);

    double x = p->e.prevX + (p->e.x - p->e.prevX) * t;
    double y = p->e.prevY + (p->e.y - p->e.prevY) * t;
    double z = p->e.prevZ + (p->e.z - p->e.prevZ) * t;

    glTranslated(-x, -y, -z);
}

static void setupCamera(Player* p, float t) {
    int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0, (double)fbw / (double)fbh, 0.05, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    moveCameraToPlayer(p, t);
}

static void drawGui(float partialTicks) {
    (void)partialTicks;

    // Clear depth so HUD draws on top
    glClear(GL_DEPTH_BUFFER_BIT);

    // --- setup HUD camera to a fixed 240px logical height ---
    int fbw, fbh; 
    glfwGetFramebufferSize(window, &fbw, &fbh);

    int screenWidth  = fbw * 240 / fbh;  // integer scale
    int screenHeight = 240;              // fixed logical height

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, screenWidth, screenHeight, 0.0, 100.0, 300.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);

    // --- held block preview (top-right), 16x16 logical size ---
    glPushMatrix();
    glTranslated((double)screenWidth - 16.0, 16.0, 0.0);
    glScalef(16.0f, 16.0f, 16.0f);
    glRotatef(30.0f, 1, 0, 0);
    glRotatef(45.0f, 0, 1, 0);
    glTranslatef(-1.5f, 0.5f, -0.5f);
    glScalef(-1.0f, -1.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, texTerrain);
    glEnable(GL_TEXTURE_2D);

    Tessellator_init(&hudTess);
    const Tile* hand = gTiles[selectedTileId];
    if (hand && hand->render) {
        hand->render(hand, &hudTess, &level, 0, -2, 0, 0);
    }
    Tessellator_flush(&hudTess);

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();

    // Top-left: version + stats
    Font_drawShadow(&gFont, &hudTess, "0.0.11a", 2, 2, 0xFFFFFF);

    char stats[64];
    snprintf(stats, sizeof stats, "%d fps, %d chunk updates", gFPS, gChunkUpdatesPerSec);
    Font_drawShadow(&gFont, &hudTess, stats, 2, 12, 0xFFFFFF);

    int cx = screenWidth / 2;
    int cy = screenHeight / 2;

    glColor4f(1, 1, 1, 1);
    Tessellator_init(&hudTess);
    // vertical (height ~9)
    Tessellator_vertex(&hudTess, (float)(cx + 1), (float)(cy - 4), 0.0f);
    Tessellator_vertex(&hudTess, (float)(cx + 0), (float)(cy - 4), 0.0f);
    Tessellator_vertex(&hudTess, (float)(cx + 0), (float)(cy + 5), 0.0f);
    Tessellator_vertex(&hudTess, (float)(cx + 1), (float)(cy + 5), 0.0f);
    // horizontal (width ~9)
    Tessellator_vertex(&hudTess, (float)(cx + 5), (float)(cy + 0), 0.0f);
    Tessellator_vertex(&hudTess, (float)(cx - 4), (float)(cy + 0), 0.0f);
    Tessellator_vertex(&hudTess, (float)(cx - 4), (float)(cy + 1), 0.0f);
    Tessellator_vertex(&hudTess, (float)(cx + 5), (float)(cy + 1), 0.0f);
    Tessellator_flush(&hudTess);
}

/* --- picking ----------------------------------------------------------------- */

static void get_look_dir(const Player* p, double* dx, double* dy, double* dz) {
    const double yaw   = p->e.yRotation * M_PI / 180.0;
    const double pitch = p->e.xRotation * M_PI / 180.0;
    const double cp = cos(pitch), sp = sin(pitch);
    const double cy = cos(yaw),   sy = sin(yaw);
    *dx =  sy * cp;   // +X right
    *dy = -sp;        // +Y up
    *dz = -cy * cp;   // -Z forward
}

static int raycast_block(const Level* lvl,
                         double ox, double oy, double oz,
                         double dx, double dy, double dz,
                         double maxDist,
                         HitResult* out) {
    int x = (int)floor(ox);
    int y = (int)floor(oy);
    int z = (int)floor(oz);

    const int stepX = (dx > 0) - (dx < 0);
    const int stepY = (dy > 0) - (dy < 0);
    const int stepZ = (dz > 0) - (dz < 0);

    double tMaxX = (dx != 0.0) ? (((stepX > 0 ? (x + 1) : x) - ox) / dx) : DBL_MAX;
    double tMaxY = (dy != 0.0) ? (((stepY > 0 ? (y + 1) : y) - oy) / dy) : DBL_MAX;
    double tMaxZ = (dz != 0.0) ? (((stepZ > 0 ? (z + 1) : z) - oz) / dz) : DBL_MAX;

    double tDeltaX = (dx != 0.0) ? fabs(1.0 / dx) : DBL_MAX;
    double tDeltaY = (dy != 0.0) ? fabs(1.0 / dy) : DBL_MAX;
    double tDeltaZ = (dz != 0.0) ? fabs(1.0 / dz) : DBL_MAX;

    int face = -1;
    double t = 0.0;

    while (t <= maxDist) {
        if (x < 0 || y < 0 || z < 0 || x >= lvl->width || y >= lvl->depth || z >= lvl->height)
            return 0;

        // Any non-air tile is pickable (bushes, etc.), even if not solid.
        int id = Level_getTile(lvl, x, y, z);
        if (id != 0) {
            if (out) hitresult_create(out, x, y, z, 0, (face < 0 ? 0 : face));
            return 1;
        }

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                x += stepX; t = tMaxX; tMaxX += tDeltaX;
                face = (stepX > 0) ? 4 : 5;
            } else {
                z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ;
                face = (stepZ > 0) ? 2 : 3;
            }
        } else {
            if (tMaxY < tMaxZ) {
                y += stepY; t = tMaxY; tMaxY += tDeltaY;
                face = (stepY > 0) ? 0 : 1;
            } else {
                z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ;
                face = (stepZ > 0) ? 2 : 3;
            }
        }
    }
    return 0;
}

static void pick(float t) {
    double x = player.e.prevX + (player.e.x - player.e.prevX) * t;
    double y = player.e.prevY + (player.e.y - player.e.prevY) * t;
    double z = player.e.prevZ + (player.e.z - player.e.prevZ) * t;

    double dx, dy, dz;
    get_look_dir(&player, &dx, &dy, &dz);

    // nudge origin to match the 0.3 view-space translate
    x += dx * 0.3; y += dy * 0.3; z += dz * 0.3;

    const int reachBlocks = 3; // axis-aligned reach cube

    HitResult hr;
    if (raycast_block(&level, x, y, z, dx, dy, dz, 100.0, &hr)) {
        int px = (int)floor(player.e.x);
        int py = (int)floor(player.e.y);
        int pz = (int)floor(player.e.z);
        if (abs(hr.x - px) <= reachBlocks &&
            abs(hr.y - py) <= reachBlocks &&
            abs(hr.z - pz) <= reachBlocks) {
            hitResult = hr;
            isHitNull = 0;
            return;
        }
    }
    isHitNull = 1;
}

/* --- input actions ----------------------------------------------------------- */

static void handleGameplayKeys(GLFWwindow* w) {
    // Enter = save level
    int enter = glfwGetKey(w, GLFW_KEY_ENTER);
    if (enter == GLFW_PRESS && prevEnter == GLFW_RELEASE) {
        Level_save(&level);
    }
    prevEnter = enter;

    // 1..4 = select tile
    int n1 = glfwGetKey(w, GLFW_KEY_1);
    int n2 = glfwGetKey(w, GLFW_KEY_2);
    int n3 = glfwGetKey(w, GLFW_KEY_3);
    int n4 = glfwGetKey(w, GLFW_KEY_4);
    int n6 = glfwGetKey(w, GLFW_KEY_6);
    if (n1 == GLFW_PRESS && prevNum1 == GLFW_RELEASE) selectedTileId = 1;  // rock
    if (n2 == GLFW_PRESS && prevNum2 == GLFW_RELEASE) selectedTileId = 3;  // dirt
    if (n3 == GLFW_PRESS && prevNum3 == GLFW_RELEASE) selectedTileId = 4;  // stone brick
    if (n4 == GLFW_PRESS && prevNum4 == GLFW_RELEASE) selectedTileId = 5;  // wood
    if (n6 == GLFW_PRESS && prevNum6 == GLFW_RELEASE) selectedTileId = 6;  // bush
    prevNum1 = n1; prevNum2 = n2; prevNum3 = n3; prevNum4 = n4; prevNum6 = n6;

    // G = spawn zombie at player
    int g = glfwGetKey(w, GLFW_KEY_G);
    if (g == GLFW_PRESS && prevG == GLFW_RELEASE && mobCount < MAX_MOBS) {
        Zombie_init(&mobs[mobCount++], &level, player.e.x, player.e.y, player.e.z);
    }
    prevG = g;

    // Y = invert mouse Y axis
    int kY = glfwGetKey(w, GLFW_KEY_Y);
    if (kY == GLFW_PRESS && prevY == GLFW_RELEASE) {
        gYMouseAxis *= -1;
    }
    prevY = kY;
}

static void handleBlockClicks(GLFWwindow* w) {
    int left  = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT);
    int right = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT);

    if (glfwGetInputMode(w, GLFW_CURSOR) != GLFW_CURSOR_DISABLED &&
        (left == GLFW_PRESS || right == GLFW_PRESS)) {
        int ww, wh; glfwGetWindowSize(w, &ww, &wh);
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPos(w, ww * 0.5, wh * 0.5);

        // consume this edge so it doesn't also act this frame
        prevLeft = left;
        prevRight = right;
        return;
    }

    if (right == GLFW_PRESS && prevRight == GLFW_RELEASE) {
        gEditMode = (gEditMode + 1) % 2;
    }
    prevRight = right;

    // Left-click performs the current mode
    if (left == GLFW_PRESS && prevLeft == GLFW_RELEASE && !isHitNull) {
        if (gEditMode == 0) {
            // --- DESTROY
            int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
            const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
            bool changed = level_setTile(&level, hitResult.x, hitResult.y, hitResult.z, 0);
            if (t && changed) {
                Tile_onDestroy(t, &level, hitResult.x, hitResult.y, hitResult.z, &particleEngine);
            }
        } else {
            // --- PLACE on adjacent face
            int nx=0, ny=0, nz=0;
            switch (hitResult.f) {
                case 0: ny = -1; break; // bottom
                case 1: ny =  1; break; // top
                case 2: nz = -1; break; // -Z
                case 3: nz =  1; break; // +Z
                case 4: nx = -1; break; // -X
                case 5: nx =  1; break; // +X
            }
            int x = hitResult.x + nx;
            int y = hitResult.y + ny;
            int z = hitResult.z + nz;

            // AABB collision check: disallow placing inside player or mobs
            AABB aabb = Level_getTilePickAABB(&level, x, y, z);
            if (!AABB_intersects(&player.e.boundingBox, &aabb)) {
                bool blocked = false;
                for (int i = 0; i < mobCount; ++i) {
                    if (AABB_intersects(&mobs[i].base.boundingBox, &aabb)) { blocked = true; break; }
                }
                if (!blocked) {
                    level_setTile(&level, x, y, z, selectedTileId);
                }
            }
        }
    }
    prevLeft = left;
}

/* --- frame ------------------------------------------------------------------- */

static void render(Level* lvl, LevelRenderer* lr, Player* p, GLFWwindow* w, float t) {
    (void)w;
    (void)lvl;

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    int grabbed = (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED);
    if (grabbed) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        int ww, wh; glfwGetWindowSize(window, &ww, &wh);
        double cx = ww * 0.5, cy = wh * 0.5;

        float dx = (float)(mx - cx);
        float dy = (float)(my - cy);

        dy = -dy * (float)gYMouseAxis;

        Player_turn(p, window, dx, dy);

        // recenter so next frame's delta is from the middle
        glfwSetCursorPos(window, cx, cy);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setupCamera(p, t);

    setupFog(0);
    LevelRenderer_render(lr, 0);    // lit layer

    // Zombies in sunlight (lit)
    for (int i = 0; i < mobCount; ++i) {
        const Zombie* z = &mobs[i];
        if (Entity_isLit(&z->base) && frustum_isVisible(&z->base.boundingBox)) {
            Zombie_render(z, t);
        }
    }

    ParticleEngine_render(&particleEngine, p, t, 0);

    setupFog(1);
    LevelRenderer_render(lr, 1);    // shadow layer

    // Zombies in shadow (not lit)
    for (int i = 0; i < mobCount; ++i) {
        const Zombie* z = &mobs[i];
        if (!Entity_isLit(&z->base) && frustum_isVisible(&z->base.boundingBox)) {
            Zombie_render(z, t);
        }
    }

    ParticleEngine_render(&particleEngine, p, t, 1);

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    if (!isHitNull) {
        GLboolean wasAlpha = glIsEnabled(GL_ALPHA_TEST);
        if (wasAlpha) glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &hitResult, gEditMode, selectedTileId);
        if (wasAlpha) glEnable(GL_ALPHA_TEST);
    }

    drawGui(t);

    glfwSwapBuffers(window);
}

/* --- main loop --------------------------------------------------------------- */

static void run(Level* lvl, LevelRenderer* lr, Player* p) {
    if (!init(lvl, lr, p)) {
        fprintf(stderr, "Failed to initialize game\n");
        destroy(lvl);
        exit(EXIT_FAILURE);
    }

    int frames = 0;
    long long last = currentTimeMillis();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        Timer_advanceTime(&timer);
        for (int i = 0; i < timer.ticks; ++i) tick(p, window);

        pick(timer.partialTicks);
        handleBlockClicks(window);
        handleGameplayKeys(window);

        int built = LevelRenderer_updateDirtyChunks(&levelRenderer, &player);
        gChunkUpdatesAccum += built;

        render(lvl, lr, p, window, timer.partialTicks);

        frames++;
        while (currentTimeMillis() >= last + 1000LL) {
            gFPS = frames;
            gChunkUpdatesPerSec = gChunkUpdatesAccum;
        #ifndef NDEBUG
            printf("%d fps, %d chunk updates\n", gFPS, gChunkUpdatesPerSec);
        #endif
            last += 1000LL;
            frames = 0;
            gChunkUpdatesAccum = 0;
        }
    }

    destroy(lvl);
}

int main(void) {
    run(&level, &levelRenderer, &player);
    return EXIT_SUCCESS;
}

static void tick(Player* p, GLFWwindow* w) {
    (void)w;

    // random tile ticks (grass growth/decay lives here)
    Level_onTick(&level);

    ParticleEngine_onTick(&particleEngine);

    Player_onTick(p, window);
    for (int i = 0; i < mobCount; ) {
        Zombie_onTick(&mobs[i]);
        if (mobs[i].base.removed) {
            mobs[i] = mobs[mobCount - 1];
            mobCount--;
            continue;
        }
        i++;
    }
}