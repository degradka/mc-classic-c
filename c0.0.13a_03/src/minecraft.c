// minecraft.c: window, main loop, input, camera, picking and the level loading screen

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <GL/glew.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include "common.h"

#include "level/level.h"
#include "level/level_renderer.h"
#include "level/chunk.h"
#include "level/levelgen/level_gen.h"
#include "renderer/frustum.h"
#include "level/tile/tile.h"
#include "renderer/textures.h"
#include "particle/particle_engine.h"
#include "player.h"
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

static int mobCount = 0;

static int editMode = 0;
static int yMouseAxis = 1;
static int paintTexture = 1;

static int texTerrain = 0;
static int texDirt = 0;

static Tessellator hudTess;
static Font gFont;
static char fpsString[64] = "";
static char title[64] = "";
static char loadStatus[64] = "";

static int gWinWidth  = 854;
static int gWinHeight = 480;
static int gIsFullscreen = 0;

static GLfloat fogColor0[4] = { 0.5f, 0.8f, 1.0f, 1.0f };
static GLfloat fogColor1[4] = { 14.0f/255.0f, 11.0f/255.0f, 10.0f/255.0f, 1.0f };

static int       isHitNull = 1;
static HitResult hitResult;

static GLuint selectBuffer[2000];

static bool mouseGrabbed = false;

static PauseScreen pauseScreen;
static Screen*     screen = NULL;

// glfw callbacks queue the events and the tick drains them
#define MAX_EVENTS 256
typedef struct { int key; bool state; double x, y; } InputEvent;
static InputEvent mouseEvents[MAX_EVENTS];
static int        mouseEventCount = 0;
static InputEvent keyEvents[MAX_EVENTS];
static int        keyEventCount = 0;

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)w; (void)scancode; (void)mods;
    if (action == GLFW_REPEAT || keyEventCount >= MAX_EVENTS) return;
    keyEvents[keyEventCount].key = key;
    keyEvents[keyEventCount].state = action == GLFW_PRESS;
    keyEventCount++;
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
    (void)mods;
    if (mouseEventCount >= MAX_EVENTS) return;
    mouseEvents[mouseEventCount].key = button;
    mouseEvents[mouseEventCount].state = action == GLFW_PRESS;
    glfwGetCursorPos(w, &mouseEvents[mouseEventCount].x, &mouseEvents[mouseEventCount].y);
    mouseEventCount++;
}

static void getSize(int* width, int* height) {
    glfwGetFramebufferSize(window, width, height);
}

// mouse y counts up from the bottom
static void lwjglMouse(double cx, double cy, int* x, int* y) {
    int width, height;
    getSize(&width, &height);
    *x = (int)cx;
    *y = height - 1 - (int)cy;
}

// a screen passed in here has already been set up with the 240 tall gui size
void Minecraft_setScreen(Screen* s) {
    if (screen && screen->removed) screen->removed(screen);
    screen = s;
}

static void openPauseScreen(void) {
    int width, height;
    getSize(&width, &height);
    int screenWidth  = width * 240 / height;
    int screenHeight = height * 240 / height;
    if (screen && screen->removed) screen->removed(screen);
    PauseScreen_init(&pauseScreen, &gFont, screenWidth, screenHeight);
    screen = (Screen*)&pauseScreen;
}

void Minecraft_grabMouse(void) {
    if (mouseGrabbed) return;
    mouseGrabbed = true;
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
    Minecraft_setScreen(NULL);
}

static void releaseMouse(void) {
    if (!mouseGrabbed) return;
    Player_releaseAllKeys(&player);
    mouseGrabbed = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    openPauseScreen();
}

void Minecraft_beginLevelLoading(const char* t) {
    snprintf(title, sizeof title, "%s", t);
    int width, height;
    getSize(&width, &height);
    int screenWidth  = width * 240 / height;
    int screenHeight = height * 240 / height;
    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, screenWidth, screenHeight, 0.0, 100.0, 300.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);
}

// the dirt background, title and status, and the bar when percent >= 0
void Minecraft_levelLoadProgress(int percent) {
    int width, height;
    getSize(&width, &height);
    int screenWidth  = width * 240 / height;
    int screenHeight = height * 240 / height;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDirt);
    Tessellator_begin(&hudTess);
    Tessellator_color(&hudTess, 64.0f / 255.0f, 64.0f / 255.0f, 64.0f / 255.0f);
    float s = 32.0f;
    Tessellator_vertexUV(&hudTess, 0.0f, (float)screenHeight, 0.0f, 0.0f, (float)screenHeight / s);
    Tessellator_vertexUV(&hudTess, (float)screenWidth, (float)screenHeight, 0.0f, (float)screenWidth / s, (float)screenHeight / s);
    Tessellator_vertexUV(&hudTess, (float)screenWidth, 0.0f, 0.0f, (float)screenWidth / s, 0.0f);
    Tessellator_vertexUV(&hudTess, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    Tessellator_end(&hudTess);
    if (percent >= 0) {
        int x = screenWidth / 2 - 50;
        int y = screenHeight / 2 + 16;
        glDisable(GL_TEXTURE_2D);
        Tessellator_begin(&hudTess);
        Tessellator_color(&hudTess, 128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);
        Tessellator_vertex(&hudTess, (float)x, (float)y, 0.0f);
        Tessellator_vertex(&hudTess, (float)x, (float)(y + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(x + 100), (float)(y + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(x + 100), (float)y, 0.0f);
        Tessellator_color(&hudTess, 128.0f / 255.0f, 1.0f, 128.0f / 255.0f);
        Tessellator_vertex(&hudTess, (float)x, (float)y, 0.0f);
        Tessellator_vertex(&hudTess, (float)x, (float)(y + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(x + percent), (float)(y + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(x + percent), (float)y, 0.0f);
        Tessellator_end(&hudTess);
        glEnable(GL_TEXTURE_2D);
    }
    Font_drawShadow(&gFont, &hudTess, title, (screenWidth - Font_width(&gFont, title)) / 2, screenHeight / 2 - 4 - 16, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, loadStatus, (screenWidth - Font_width(&gFont, loadStatus)) / 2, screenHeight / 2 - 4 + 8, 0xFFFFFF);
    glfwSwapBuffers(window);
    // pumps the window messages
    glfwPollEvents();
}

void Minecraft_levelLoadUpdate(const char* status) {
    snprintf(loadStatus, sizeof loadStatus, "%s", status);
    Minecraft_levelLoadProgress(-1);
}

void Minecraft_generateNewLevel(void) {
    LevelGen_generateLevel(&level, "anonymous", 256, 256, 64);
    Entity_resetPosition(&player.e);
    mobCount = 0;
}

static void attemptSaveLevel(void) {
    Level_save(&level, "level.dat");
}

static bool fileExists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void setupFog(int i) {
    const Tile* currentTile = gTiles[Level_getTile(&level, (int)player.e.x, (int)(player.e.y + 0.12f), (int)player.e.z)];
    if (currentTile && currentTile->liquidType == LIQUID_WATER) {
        GLfloat c[4] = { 0.02f, 0.02f, 0.2f, 1.0f };
        GLfloat a[4] = { 0.3f, 0.3f, 0.7f, 1.0f };
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 0.1f);
        glFogfv(GL_FOG_COLOR, c);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
    } else if (currentTile && currentTile->liquidType == LIQUID_LAVA) {
        GLfloat c[4] = { 0.6f, 0.1f, 0.0f, 1.0f };
        GLfloat a[4] = { 0.4f, 0.3f, 0.3f, 1.0f };
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 2.0f);
        glFogfv(GL_FOG_COLOR, c);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
    } else if (i == 0) {
        GLfloat a[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 0.001f);
        glFogfv(GL_FOG_COLOR, fogColor0);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
    } else if (i == 1) {
        float br = 0.6f;
        GLfloat a[4] = { br, br, br, 1.0f };
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 0.01f);
        glFogfv(GL_FOG_COLOR, fogColor1);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
    }
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT);
    glEnable(GL_LIGHTING);
}

/* boot and shutdown */

static int init(void) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 0;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if (gIsFullscreen) {
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.0.13a_03", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.0.13a_03", NULL, NULL);
    }

    if (!window) {
        glfwTerminate();
        fprintf(stderr, "Failed to create GLFW window\n");
        return 0;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW init failed: %s\n", glewGetErrorString(err));
        return 0;
    }

    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.5f, 0.8f, 1.0f, 0.0f);
    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
    glCullFace(GL_BACK);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    Tile_registerAll();

    texTerrain = loadTexture("resources/terrain.png", GL_NEAREST);
    texDirt    = loadTexture("resources/dirt.png", GL_NEAREST);
    Font_init(&gFont, "resources/default.gif");

    int width, height;
    getSize(&width, &height);
    glViewport(0, 0, width, height);

    Level_init(&level);
    bool success = false;
    // a missing file shows no loading screen
    if (fileExists("level.dat")) {
        success = Level_load(&level, "level.dat");
        if (!success && fileExists("level.dat")) success = Level_loadLegacy(&level, "level.dat");
    }
    // there's no logged in user, so the creator is "anonymous"
    if (!success) LevelGen_generateLevel(&level, "anonymous", 256, 256, 64);

    LevelRenderer_init(&levelRenderer, &level, texTerrain);
    Player_init(&player, &level);
    ParticleEngine_init(&particleEngine, &level, (GLuint)texTerrain);

    mobCount = 0;

    Timer_init(&timer, 20.0f);

    return 1;
}

static void destroy(void) {
    attemptSaveLevel();
    LevelRenderer_destroy(&levelRenderer);
    Level_destroy(&level);
    glfwDestroyWindow(window);
    glfwTerminate();
}

/* camera */

static void moveCameraToPlayer(float a) {
    glTranslatef(0.0f, 0.0f, -0.3f);
    glRotatef(player.e.xRotation, 1.0f, 0.0f, 0.0f);
    glRotatef(player.e.yRotation, 0.0f, 1.0f, 0.0f);
    float x = player.e.prevX + (player.e.x - player.e.prevX) * a;
    float y = player.e.prevY + (player.e.y - player.e.prevY) * a;
    float z = player.e.prevZ + (player.e.z - player.e.prevZ) * a;
    glTranslatef(-x, -y, -z);
}

static void setupCamera(float a, int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0f, (float)width / (float)height, 0.05f, 1024.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    moveCameraToPlayer(a);
}

static void setupPickCamera(float a, int x, int y, int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    gluPickMatrix((float)x, (float)y, 5.0f, 5.0f, viewport);
    gluPerspective(70.0f, (float)width / (float)height, 0.05f, 1024.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    moveCameraToPlayer(a);
}

/* picking */

static void pick(float a, int width, int height) {
    glSelectBuffer(2000, selectBuffer);
    glRenderMode(GL_SELECT);
    setupPickCamera(a, width / 2, height / 2, width, height);
    Frustum frustum;
    frustum_calculate(&frustum);
    LevelRenderer_pick(&levelRenderer, &player, &frustum);
    int hits = glRenderMode(GL_RENDER);

    int p = 0;
    int names[10];
    HitResult current;
    HitResult best;
    bool haveBest = false;
    for (int i = 0; i < hits; i++) {
        int nameCount = (int)selectBuffer[p++];
        p += 2;
        for (int j = 0; j < nameCount; j++) names[j] = (int)selectBuffer[p++];
        hitresult_create(&current, names[1], names[2], names[3], names[0], names[4]);
        if (!haveBest || HitResult_isCloserThan(&current, &player, &best, editMode)) {
            best = current;
            haveBest = true;
        }
    }

    if (haveBest) {
        hitResult = best;
        isHitNull = 0;
    } else {
        isHitNull = 1;
    }
}

/* input actions */

static bool isFree(const AABB* aabb) {
    if (AABB_intersects(&player.e.boundingBox, aabb)) return false;
    for (int i = 0; i < mobCount; i++) {
        if (AABB_intersects(&mobs[i].base.boundingBox, aabb)) return false;
    }
    return true;
}

static void handleMouseClick(void) {
    if (editMode == 0) {
        if (!isHitNull) {
            const Tile* oldTile = gTiles[Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z)];
            bool changed = level_setTile(&level, hitResult.x, hitResult.y, hitResult.z, 0);
            if (oldTile && changed) Tile_onDestroy(oldTile, &level, hitResult.x, hitResult.y, hitResult.z, &particleEngine);
        }
    } else if (!isHitNull) {
        int x = hitResult.x;
        int y = hitResult.y;
        int z = hitResult.z;
        if (hitResult.f == 0) y--;
        if (hitResult.f == 1) y++;
        if (hitResult.f == 2) z--;
        if (hitResult.f == 3) z++;
        if (hitResult.f == 4) x--;
        if (hitResult.f == 5) x++;
        const Tile* tile = gTiles[paintTexture];
        AABB aabb;
        if (!tile->getAABB(tile, x, y, z, &aabb) || isFree(&aabb)) level_setTile(&level, x, y, z, paintTexture);
    }
}

// Screen.updateEvents, the screen keeps draining even if a click closes it
static void screenUpdateEvents(Screen* s) {
    int width, height;
    getSize(&width, &height);
    for (int e = 0; e < mouseEventCount; e++) {
        if (mouseEvents[e].state) {
            int ex, ey;
            lwjglMouse(mouseEvents[e].x, mouseEvents[e].y, &ex, &ey);
            int xm = ex * s->width / width;
            int ym = s->height - ey * s->height / height - 1;
            if (mouseEvents[e].key != GLFW_MOUSE_BUTTON_LEFT) continue;
            for (int i = 0; i < s->buttonCount; i++) {
                Button* b = &s->buttons[i];
                if (xm < b->x || ym < b->y || xm >= b->x + b->w || ym >= b->y + b->h) continue;
                if (s->buttonClicked) s->buttonClicked(s, b);
            }
        }
    }
    mouseEventCount = 0;
    for (int e = 0; e < keyEventCount; e++) {
        if (keyEvents[e].state && s->keyPressed) s->keyPressed(s, 0, keyEvents[e].key);
    }
    keyEventCount = 0;
}

static void tick(void) {
    if (!screen) {
        for (int e = 0; e < mouseEventCount; e++) {
            if (!mouseGrabbed && mouseEvents[e].state) {
                Minecraft_grabMouse();
                continue;
            }
            if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_LEFT && mouseEvents[e].state) handleMouseClick();
            if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_RIGHT && mouseEvents[e].state) editMode = (editMode + 1) % 2;
        }
        mouseEventCount = 0;

        // a key can open the pause screen, the rest of the queue still goes here
        for (int e = 0; e < keyEventCount; e++) {
            int key = keyEvents[e].key;
            Player_setKey(&player, key, keyEvents[e].state);
            if (keyEvents[e].state) {
                if (key == GLFW_KEY_ESCAPE) releaseMouse();
                if (key == GLFW_KEY_ENTER) attemptSaveLevel();
                if (key == GLFW_KEY_R) Entity_resetPosition(&player.e);
                if (key == GLFW_KEY_1) paintTexture = 1;
                if (key == GLFW_KEY_2) paintTexture = 3;
                if (key == GLFW_KEY_3) paintTexture = 4;
                if (key == GLFW_KEY_4) paintTexture = 5;
                if (key == GLFW_KEY_6) paintTexture = 6;
                if (key == GLFW_KEY_Y) yMouseAxis *= -1;
                if (key == GLFW_KEY_G && mobCount < MAX_MOBS) {
                    Zombie_init(&mobs[mobCount++], &level, player.e.x, player.e.y, player.e.z);
                }
                if (key == GLFW_KEY_F) LevelRenderer_toggleDrawDistance(&levelRenderer);
            }
        }
        keyEventCount = 0;
    }

    if (screen) {
        screenUpdateEvents(screen);
        if (screen && screen->tick) screen->tick(screen);
    }

    Level_onTick(&level);
    ParticleEngine_onTick(&particleEngine);

    for (int i = 0; i < mobCount; i++) {
        Zombie_onTick(&mobs[i]);
        if (mobs[i].base.removed) {
            memmove(&mobs[i], &mobs[i + 1], (size_t)(mobCount - i - 1) * sizeof(Zombie));
            mobCount--;
            i--;
        }
    }

    Player_onTick(&player);
}

/* frame */

static void drawGui(int width, int height) {
    int screenWidth  = width * 240 / height;
    int screenHeight = height * 240 / height;
    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);
    int mx, my;
    lwjglMouse(cx, cy, &mx, &my);
    int xMouse = mx * screenWidth / width;
    int yMouse = screenHeight - my * screenHeight / height - 1;

    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, screenWidth, screenHeight, 0.0, 100.0, 300.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);

    glPushMatrix();
    glTranslatef((float)(screenWidth - 16), 16.0f, -50.0f);
    glScalef(16.0f, 16.0f, 16.0f);
    glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-1.5f, 0.5f, 0.5f);
    glScalef(-1.0f, -1.0f, -1.0f);
    glBindTexture(GL_TEXTURE_2D, texTerrain);
    glEnable(GL_TEXTURE_2D);
    Tessellator_begin(&hudTess);
    gTiles[paintTexture]->render(gTiles[paintTexture], &hudTess, &level, 0, -2, 0, 0);
    Tessellator_end(&hudTess);
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();

    Font_drawShadow(&gFont, &hudTess, "0.0.13a_03", 2, 2, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, fpsString, 2, 12, 0xFFFFFF);

    int wc = screenWidth / 2;
    int hc = screenHeight / 2;
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    Tessellator_begin(&hudTess);
    Tessellator_vertex(&hudTess, (float)(wc + 1), (float)(hc - 4), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc - 0), (float)(hc - 4), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc - 0), (float)(hc + 5), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc + 1), (float)(hc + 5), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc + 5), (float)(hc - 0), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc - 4), (float)(hc - 0), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc - 4), (float)(hc + 1), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc + 5), (float)(hc + 1), 0.0f);
    Tessellator_end(&hudTess);

    if (screen) screen->render(screen, xMouse, yMouse);
}

static void render(float a) {
    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) releaseMouse();

    int width, height;
    getSize(&width, &height);
    glViewport(0, 0, width, height);

    if (mouseGrabbed) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        int ww, wh; glfwGetWindowSize(window, &ww, &wh);
        double cx = ww * 0.5, cy = wh * 0.5;

        // mouse y counts up from the bottom
        float xo = (float)(mx - cx);
        float yo = (float)(cy - my);
        Player_turn(&player, xo, yo * (float)yMouseAxis);
        glfwSetCursorPos(window, cx, cy);
    }

    pick(a, width, height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupCamera(a, width, height);
    glEnable(GL_CULL_FACE);
    Frustum frustum;
    frustum_calculate(&frustum);
    LevelRenderer_cull(&levelRenderer, &frustum);
    LevelRenderer_updateDirtyChunks(&levelRenderer, &player);

    setupFog(0);
    glEnable(GL_FOG);
    LevelRenderer_render(&levelRenderer, &player, 0);
    for (int i = 0; i < mobCount; i++) {
        const Zombie* z = &mobs[i];
        if (Entity_isLit(&z->base) && frustum_isVisible(&frustum, &z->base.boundingBox)) Zombie_render(z, a);
    }
    ParticleEngine_render(&particleEngine, &player, a, 0);

    setupFog(1);
    LevelRenderer_render(&levelRenderer, &player, 1);
    for (int i = 0; i < mobCount; i++) {
        const Zombie* z = &mobs[i];
        if (!Entity_isLit(&z->base) && frustum_isVisible(&frustum, &z->base.boundingBox)) Zombie_render(z, a);
    }
    ParticleEngine_render(&particleEngine, &player, a, 1);

    LevelRenderer_renderSurroundingGround(&levelRenderer);

    if (!isHitNull) {
        glDisable(GL_LIGHTING);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &player, &hitResult, editMode, paintTexture);
        LevelRenderer_renderHitOutline(&hitResult, editMode);
        glEnable(GL_ALPHA_TEST);
        glEnable(GL_LIGHTING);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setupFog(0);
    LevelRenderer_renderSurroundingWater(&levelRenderer);
    glEnable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    LevelRenderer_render(&levelRenderer, &player, 2);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    LevelRenderer_render(&levelRenderer, &player, 2);
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    if (!isHitNull) {
        glDepthFunc(GL_LESS);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &player, &hitResult, editMode, paintTexture);
        LevelRenderer_renderHitOutline(&hitResult, editMode);
        glEnable(GL_ALPHA_TEST);
        glDepthFunc(GL_LEQUAL);
    }

    drawGui(width, height);

    glfwSwapBuffers(window);
}

/* main loop */

int main(void) {
    if (!init()) {
        fprintf(stderr, "Failed to initialize game\n");
        exit(EXIT_FAILURE);
    }

    long long lastTime = currentTimeMillis();
    int frames = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        Timer_advanceTime(&timer);
        for (int i = 0; i < timer.ticks; ++i) tick();

        render(timer.partialTicks);
        frames++;

        while (currentTimeMillis() >= lastTime + 1000LL) {
            snprintf(fpsString, sizeof fpsString, "%d fps, %d chunk updates", frames, Chunk_updates);
            Chunk_updates = 0;
            lastTime += 1000LL;
            frames = 0;
        }
    }

    destroy();
    return EXIT_SUCCESS;
}
