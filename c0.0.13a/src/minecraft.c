// minecraft.c: entry point, window and GL init, camera, picking, main loop

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
#include "user.h"
#include "character/zombie.h"
#include "timer.h"
#include "hitresult.h"
#include "gui/font.h"
#include "gui/screen.h"
#include "gui/pause_screen.h"

#define MAX_MOBS 100

static GLFWwindow*    window;
static Level          level;
static LevelRenderer  levelRenderer;
static Player         player;
static Timer          timer;
static Zombie         mobs[MAX_MOBS];
static ParticleEngine particleEngine;
static User           user;

static int mobCount = 0;

static int prevLeft  = GLFW_RELEASE;
static int prevRight = GLFW_RELEASE;
static int prevEnter = GLFW_RELEASE;
static int prevNum1 = GLFW_RELEASE, prevNum2 = GLFW_RELEASE;
static int prevNum3 = GLFW_RELEASE, prevNum4 = GLFW_RELEASE;
static int prevNum6 = GLFW_RELEASE;
static int prevG    = GLFW_RELEASE;
static int prevY    = GLFW_RELEASE;
static int prevF    = GLFW_RELEASE;
static int prevR    = GLFW_RELEASE;

static int gEditMode = 0;              // 0=destroy, 1=place
static int gYMouseAxis = 1;            // toggled by Y key, 1 or negative 1

static int texTerrain = 0;
static int texDirt = 0;
static char gLoadTitle[64];

static Tessellator hudTess;
static Font gFont;                     // HUD font
static int selectedTileId = 1;         // 1=rock, 3=dirt, 4=stoneBrick, 5=wood
static int gFPS = 0;                   // last computed fps per second
static int gChunkUpdatesPerSec = 0;    // last computed chunk updates per second
static int gChunkUpdatesAccum = 0;     // accumulates during the current second

static int gWinWidth  = 854;
static int gWinHeight = 480;
static int gIsFullscreen = 0;

static GLfloat fogColorDaylight[4] = { 0.5f, 0.8f, 1.0f, 1.0f };
static GLfloat fogColorShadow  [4] = {  14.0f/255.0f,  11.0f/255.0f,  10.0f/255.0f, 1.0f };

static int      isHitNull = 1;
static HitResult hitResult;

static PauseScreen gPauseScreen;
static Screen*     activeScreen = NULL; // not null means paused, showing a menu
static int prevScreenClick = GLFW_RELEASE;

static void tick(Player* player, GLFWwindow* window);
static void computeGuiMouse(int* xm, int* ym);

/* input and GL state helpers */

// c0.0.13a: ESC, losing focus, or any other release mouse trigger, zeroes
// player input and opens the pause menu, matching Minecraft.releaseMouse().
static void releaseMouseAndOpenPause(void) {
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return; // already released

    Player_releaseAllKeys(&player);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;
    PauseScreen_init(&gPauseScreen, &gFont, screenWidth, screenHeight);
    activeScreen = (Screen*)&gPauseScreen;
}

// matches Minecraft.grabMouse(): grabs cursor again and closes any open screen
void Minecraft_closeScreenAndGrabMouse(void) {
    activeScreen = NULL;
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
}

void Minecraft_generateNewLevel(void) {
    // matches Minecraft.generateNewLevel(), which regenerates at a
    // different size than the boot map, not just a fresh copy of it
    LevelRenderer_destroy(&levelRenderer);
    Level_resize(&level, 32, 512, 64);
    LevelRenderer_init(&levelRenderer, &level, texTerrain);

    Entity_resetPosition(&player.e);
    mobCount = 0;
}

// matches Minecraft.beginLevelLoading(): sets up the 2D ortho projection
// once and remembers the title, which levelLoadUpdate keeps redrawing
void Minecraft_beginLevelLoading(const char* title) {
    int i = 0;
    for (; title[i] != '\0' && i < 63; ++i) gLoadTitle[i] = title[i];
    gLoadTitle[i] = '\0';

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, screenWidth, screenHeight, 0.0, 100.0, 300.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);
}

// matches Minecraft.levelLoadUpdate(): draws a checkered dirt background
// with the title and status text, shows it, and blocks briefly so the
// player can actually read it between world generation phases
void Minecraft_levelLoadUpdate(const char* status) {
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDirt);

    Tessellator_begin(&hudTess);
    Tessellator_color(&hudTess, 0.5019608f, 0.5019608f, 0.5019608f); // 0x808080
    float s = 32.0f;
    Tessellator_vertexUV(&hudTess, 0.0f, (float)screenHeight, 0.0f, 0.0f, screenHeight / s);
    Tessellator_vertexUV(&hudTess, (float)screenWidth, (float)screenHeight, 0.0f, screenWidth / s, screenHeight / s);
    Tessellator_vertexUV(&hudTess, (float)screenWidth, 0.0f, 0.0f, screenWidth / s, 0.0f);
    Tessellator_vertexUV(&hudTess, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    Tessellator_end(&hudTess);

    int titleWidth  = Font_width(&gFont, gLoadTitle);
    int statusWidth = Font_width(&gFont, status);
    Font_drawShadow(&gFont, &hudTess, gLoadTitle, (screenWidth - titleWidth) / 2, screenHeight / 2 - 4 - 8, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, status, (screenWidth - statusWidth) / 2, screenHeight / 2 - 4 + 4, 0xFFFFFF);

    glfwSwapBuffers(window);
    // Java relies on AWT's own message pump staying alive during the sleep;
    // GLFW has none, so poll here to avoid the OS flagging the window as hung
    glfwPollEvents();
    sleepMillis(200);
}

const char* Minecraft_getUserName(void) {
    return user.name;
}

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)w; (void)scancode; (void)mods;
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        releaseMouseAndOpenPause();
    }
}

static void setupFog(int type) {
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP);

    int px = (int)floor(player.e.x);
    int py = (int)floor(player.e.y + 0.12);
    int pz = (int)floor(player.e.z);
    const Tile* currentTile = gTiles[Level_getTile(&level, px, py, pz)];

    GLfloat ambient[4];

    // whichever liquid the player's head is inside overrides the daylight
    // and shadow fog entirely, regardless of which layer is being drawn
    if (currentTile && currentTile->liquidType == LIQUID_WATER) {
        GLfloat waterFog[4] = { 0.02f, 0.02f, 0.2f, 1.0f };
        glFogf(GL_FOG_DENSITY, 0.1f);
        glFogfv(GL_FOG_COLOR, waterFog);
        ambient[0] = 0.3f; ambient[1] = 0.3f; ambient[2] = 0.7f; ambient[3] = 1.0f;
    } else if (currentTile && currentTile->liquidType == LIQUID_LAVA) {
        GLfloat lavaFog[4] = { 0.6f, 0.1f, 0.0f, 1.0f };
        glFogf(GL_FOG_DENSITY, 2.0f);
        glFogfv(GL_FOG_COLOR, lavaFog);
        ambient[0] = 0.4f; ambient[1] = 0.3f; ambient[2] = 0.3f; ambient[3] = 1.0f;
    } else if (type == 0) { // daylight
        glFogf(GL_FOG_DENSITY, 0.001f);
        glFogfv(GL_FOG_COLOR, fogColorDaylight);
        ambient[0] = 1.0f; ambient[1] = 1.0f; ambient[2] = 1.0f; ambient[3] = 1.0f;
    } else {                 // shadow
        glFogf(GL_FOG_DENSITY, 0.01f);
        glFogfv(GL_FOG_COLOR, fogColorShadow);
        ambient[0] = 0.6f; ambient[1] = 0.6f; ambient[2] = 0.6f; ambient[3] = 1.0f;
    }

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    glEnable(GL_NORMALIZE);
    glColorMaterial(GL_FRONT, GL_AMBIENT);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
}

/* boot and shutdown */

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
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.0.13a", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.0.13a", NULL, NULL);
    }

    if (!window) {
        glfwTerminate();
        fprintf(stderr, "Failed to create GLFW window\n");
        return 0;
    }
    glfwMakeContextCurrent(window);
    glEnable(GL_MULTISAMPLE);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
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

    User_init(&user, "noname");

    texTerrain = loadTexture("resources/terrain.png", GL_NEAREST);
    texDirt    = loadTexture("resources/dirt.png", GL_NEAREST);
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

/* camera */

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

    // fog is already disabled by render() before this call, kept here too
    // since drawGui is also reachable from the boot loading screen
    glDisable(GL_FOG);

    // set up the HUD camera at a fixed 240px logical height
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

    // held block preview in the top right corner, 16x16 logical size
    glPushMatrix();
    glTranslated((double)screenWidth - 16.0, 16.0, 0.0);
    glScalef(16.0f, 16.0f, 16.0f);
    glRotatef(30.0f, 1, 0, 0);
    glRotatef(45.0f, 0, 1, 0);
    glTranslatef(-1.5f, 0.5f, -0.5f);
    glScalef(-1.0f, -1.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, texTerrain);
    glEnable(GL_TEXTURE_2D);

    Tessellator_begin(&hudTess);
    const Tile* hand = gTiles[selectedTileId];
    if (hand && hand->render) {
        hand->render(hand, &hudTess, &level, 0, -2, 0, 0);
    }
    Tessellator_end(&hudTess);

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();

    // top left corner: version and stats
    Font_drawShadow(&gFont, &hudTess, "0.0.13a", 2, 2, 0xFFFFFF);

    char stats[64];
    snprintf(stats, sizeof stats, "%d fps, %d chunk updates", gFPS, gChunkUpdatesPerSec);
    Font_drawShadow(&gFont, &hudTess, stats, 2, 12, 0xFFFFFF);

    int cx = screenWidth / 2;
    int cy = screenHeight / 2;

    glColor4f(1, 1, 1, 1);
    Tessellator_begin(&hudTess);
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
    Tessellator_end(&hudTess);

    if (activeScreen) {
        int xm, ym;
        computeGuiMouse(&xm, &ym);
        activeScreen->render(activeScreen, xm, ym);
    }
}

/* picking */

static void get_look_dir(const Player* p, double* dx, double* dy, double* dz) {
    const double yaw   = p->e.yRotation * M_PI / 180.0;
    const double pitch = p->e.xRotation * M_PI / 180.0;
    const double cp = cos(pitch), sp = sin(pitch);
    const double cy = cos(yaw),   sy = sin(yaw);
    *dx =  sy * cp;   // positive X is right
    *dy = -sp;        // positive Y is up
    *dz = -cy * cp;   // negative Z is forward
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

        // any non air tile is pickable, including bushes, even if not solid,
        // except liquids, which the ray passes straight through
        int id = Level_getTile(lvl, x, y, z);
        if (id != 0) {
            const Tile* tile = gTiles[id];
            if (!tile || tile->mayPick(tile)) {
                if (out) hitresult_create(out, x, y, z, 0, (face < 0 ? 0 : face));
                return 1;
            }
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

    // nudge origin to match the 0.3 view space translate
    x += dx * 0.3; y += dy * 0.3; z += dz * 0.3;

    const int reachBlocks = 3; // axis aligned reach cube

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

/* input actions */

static void handleGameplayKeys(GLFWwindow* w) {
    // R = reset position
    int r = glfwGetKey(w, GLFW_KEY_R);
    if (r == GLFW_PRESS && prevR == GLFW_RELEASE) {
        Entity_resetPosition(&player.e);
    }
    prevR = r;

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

    // F = cycle draw distance, Java key 33 is F, not I as I first assumed
    int kF = glfwGetKey(w, GLFW_KEY_F);
    if (kF == GLFW_PRESS && prevF == GLFW_RELEASE) {
        LevelRenderer_toggleDrawDistance(&levelRenderer);
    }
    prevF = kF;
}

static void handleBlockClicks(GLFWwindow* w) {
    int left  = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT);
    int right = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT);

    // boot time idle state, no screen open yet, cursor just has not been grabbed
    if (glfwGetInputMode(w, GLFW_CURSOR) != GLFW_CURSOR_DISABLED &&
        (left == GLFW_PRESS || right == GLFW_PRESS)) {
        Minecraft_closeScreenAndGrabMouse();

        // consume this edge so it doesn't also act this frame
        prevLeft = left;
        prevRight = right;
        return;
    }

    if (right == GLFW_PRESS && prevRight == GLFW_RELEASE) {
        gEditMode = (gEditMode + 1) % 2;
    }
    prevRight = right;

    // left click performs the current mode
    if (left == GLFW_PRESS && prevLeft == GLFW_RELEASE && !isHitNull) {
        if (gEditMode == 0) {
            // destroy
            int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
            const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
            bool changed = level_setTile(&level, hitResult.x, hitResult.y, hitResult.z, 0);
            if (t && changed) {
                Tile_onDestroy(t, &level, hitResult.x, hitResult.y, hitResult.z, &particleEngine);
            }
        } else {
            // place on adjacent face
            int nx=0, ny=0, nz=0;
            switch (hitResult.f) {
                case 0: ny = -1; break; // bottom
                case 1: ny =  1; break; // top
                case 2: nz = -1; break; // negative Z
                case 3: nz =  1; break; // positive Z
                case 4: nx = -1; break; // negative X
                case 5: nx =  1; break; // positive X
            }
            int x = hitResult.x + nx;
            int y = hitResult.y + ny;
            int z = hitResult.z + nz;

            // AABB collision check, disallow placing inside player or mobs
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

// maps the current cursor position into the same fixed 240 tall logical GUI
// space that drawGui() renders in and that Screen layouts are sized for.
static void computeGuiMouse(int* xm, int* ym) {
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;

    double curX, curY;
    glfwGetCursorPos(window, &curX, &curY);
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);

    // GLFW's cursor Y is already top down, unlike LWJGL's Mouse.getY(), which
    // is bottom up and is why the Java source flips it. No flip needed here.
    *xm = (int)(curX * screenWidth / ww);
    *ym = (int)(curY * screenHeight / wh);
}

static void handleScreenClicks(GLFWwindow* w) {
    int left = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT);
    if (left == GLFW_PRESS && prevScreenClick == GLFW_RELEASE && activeScreen && activeScreen->mouseClicked) {
        int xm, ym;
        computeGuiMouse(&xm, &ym);
        activeScreen->mouseClicked(activeScreen, xm, ym, 0);
    }
    prevScreenClick = left;

    // handleBlockClicks() doesn't run while paused, so keep its edge trackers
    // in sync too, otherwise the same physical click that closes the screen,
    // such as clicking Back to game, looks like a brand new press once
    // gameplay resumes, and immediately fires a destroy or place action.
    prevLeft  = left;
    prevRight = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT);
}

/* frame */

static void render(Level* lvl, LevelRenderer* lr, Player* p, GLFWwindow* w, float t) {
    (void)w;
    (void)lvl;

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        releaseMouseAndOpenPause();
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

    glEnable(GL_MULTISAMPLE);

    setupCamera(p, t);

    Frustum frustum;
    frustum_calculate(&frustum);
    LevelRenderer_cull(lr, &frustum);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    setupFog(0);
    LevelRenderer_render(lr, p, 0);    // lit layer

    // Zombies in sunlight (lit)
    for (int i = 0; i < mobCount; ++i) {
        const Zombie* z = &mobs[i];
        if (Entity_isLit(&z->base) && frustum_isVisible(&frustum, &z->base.boundingBox)) {
            Zombie_render(z, t);
        }
    }

    ParticleEngine_render(&particleEngine, p, t, 0);

    setupFog(1);
    LevelRenderer_render(lr, p, 1);    // shadow layer

    // Zombies in shadow (not lit)
    for (int i = 0; i < mobCount; ++i) {
        const Zombie* z = &mobs[i];
        if (!Entity_isLit(&z->base) && frustum_isVisible(&frustum, &z->base.boundingBox)) {
            Zombie_render(z, t);
        }
    }

    ParticleEngine_render(&particleEngine, p, t, 1);

    LevelRenderer_renderSurroundingGround(lr);

    if (!isHitNull) {
        glDisable(GL_LIGHTING);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &hitResult, gEditMode, selectedTileId);
        LevelRenderer_renderHitOutline(&hitResult, gEditMode);
        glEnable(GL_ALPHA_TEST);
        glEnable(GL_LIGHTING);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setupFog(0);
    LevelRenderer_renderSurroundingWater(lr);

    // liquid layer, drawn twice: once depth only so the translucent surface
    // does not overdraw itself, then for real with color writes back on
    glEnable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    LevelRenderer_render(lr, p, 2);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    LevelRenderer_render(lr, p, 2);
    glDisable(GL_BLEND);

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    if (!isHitNull) {
        glDepthFunc(GL_LESS);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &hitResult, gEditMode, selectedTileId);
        LevelRenderer_renderHitOutline(&hitResult, gEditMode);
        glEnable(GL_ALPHA_TEST);
        glDepthFunc(GL_LEQUAL);
    }

    drawGui(t);

    glfwSwapBuffers(window);
}

/* main loop */

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

        if (!activeScreen) {
            handleBlockClicks(window);
            handleGameplayKeys(window);
            Player_pollKeys(p, window);
        } else {
            handleScreenClicks(window);
            // handleScreenClicks's button callback can close the screen,
            // setting activeScreen to null mid frame, so recheck before use.
            if (activeScreen && activeScreen->tick) activeScreen->tick(activeScreen);
        }

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

    Player_onTick(p);
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