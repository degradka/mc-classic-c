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
#include "level/level_io.h"
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
#include "gui/new_level_screen.h"

static GLFWwindow*    window;
static Level          level;
static LevelRenderer  levelRenderer;
static Player         player;
static Timer          timer;
static ParticleEngine particleEngine;

static int editMode = 0;
static int yMouseAxis = 1;
static int paintTexture = 1;
// the blocks the number keys, the wheel and the middle button pick from
static int palette[9];

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

// the clear and fog color, set again every frame
static float fogRed = 0.5f, fogGreen = 0.8f, fogBlue = 1.0f;
static GLfloat fogColor1[4] = { 14.0f/255.0f, 11.0f/255.0f, 10.0f/255.0f, 1.0f };
static float fogEnd = 0.0f;

static int       isHitNull = 1;
static HitResult hitResult;

static GLuint selectBuffer[2000];

static bool mouseGrabbed = false;
// ticks since boot, and the tick of the last click for holding the button
static int ticksRan = 0;
static int lastClickTick = 0;

static PauseScreen    pauseScreen;
static NewLevelScreen newLevelScreen;
static Screen*        screen = NULL;

// glfw callbacks queue the events and the tick drains them
#define MAX_EVENTS 256
typedef struct { int key; bool state; double x, y; int wheel; } InputEvent;
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
    InputEvent* e = &mouseEvents[mouseEventCount++];
    e->key = button;
    e->state = action == GLFW_PRESS;
    e->wheel = 0;
    glfwGetCursorPos(w, &e->x, &e->y);
}

// a wheel event has no button and isn't a press
static void scrollCallback(GLFWwindow* w, double xoff, double yoff) {
    (void)xoff;
    if (mouseEventCount >= MAX_EVENTS || yoff == 0.0) return;
    InputEvent* e = &mouseEvents[mouseEventCount++];
    e->key = -1;
    e->state = false;
    e->wheel = yoff > 0.0 ? 1 : -1;
    glfwGetCursorPos(w, &e->x, &e->y);
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

static void guiSize(int* screenWidth, int* screenHeight) {
    int width, height;
    getSize(&width, &height);
    *screenWidth  = width * 240 / height;
    *screenHeight = height * 240 / height;
}

// a screen passed in here has already been set up with the 240 tall gui size
void Minecraft_setScreen(Screen* s) {
    if (screen && screen->removed) screen->removed(screen);
    screen = s;
}

void Minecraft_showPauseScreen(void) {
    int sw, sh;
    guiSize(&sw, &sh);
    if (screen && screen->removed) screen->removed(screen);
    PauseScreen_init(&pauseScreen, &gFont, sw, sh);
    screen = (Screen*)&pauseScreen;
}

void Minecraft_showNewLevelScreen(void) {
    int sw, sh;
    guiSize(&sw, &sh);
    if (screen && screen->removed) screen->removed(screen);
    NewLevelScreen_init(&newLevelScreen, &gFont, sw, sh);
    screen = (Screen*)&newLevelScreen;
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
    Minecraft_showPauseScreen();
}

void Minecraft_beginLevelLoading(const char* t) {
    snprintf(title, sizeof title, "%s", t);
    int sw, sh;
    guiSize(&sw, &sh);
    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, sw, sh, 0.0, 100.0, 300.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);
}

// the dirt background, title and status, and the bar when percent >= 0
void Minecraft_levelLoadProgress(int percent) {
    int sw, sh;
    guiSize(&sw, &sh);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDirt);
    Tessellator_begin(&hudTess);
    Tessellator_color(&hudTess, 64.0f / 255.0f, 64.0f / 255.0f, 64.0f / 255.0f);
    float s = 32.0f;
    Tessellator_vertexUV(&hudTess, 0.0f, (float)sh, 0.0f, 0.0f, (float)sh / s);
    Tessellator_vertexUV(&hudTess, (float)sw, (float)sh, 0.0f, (float)sw / s, (float)sh / s);
    Tessellator_vertexUV(&hudTess, (float)sw, 0.0f, 0.0f, (float)sw / s, 0.0f);
    Tessellator_vertexUV(&hudTess, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    Tessellator_end(&hudTess);
    if (percent >= 0) {
        int x = sw / 2 - 50;
        int y = sh / 2 + 16;
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
    Font_drawShadow(&gFont, &hudTess, title, (sw - Font_width(&gFont, title)) / 2, sh / 2 - 4 - 16, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, loadStatus, (sw - Font_width(&gFont, loadStatus)) / 2, sh / 2 - 4 + 8, 0xFFFFFF);
    glfwSwapBuffers(window);
    // pumps the window messages
    glfwPollEvents();
}

void Minecraft_levelLoadUpdate(const char* status) {
    snprintf(loadStatus, sizeof loadStatus, "%s", status);
    Minecraft_levelLoadProgress(-1);
}

// called once the level has been loaded or generated into place
static void setLevel(void) {
    if (levelRenderer.terrainTex) LevelRenderer_setLevel(&levelRenderer, &level);
    particleEngine.count = 0;
    player.e.level = &level;
    Entity_resetPosition(&player.e);
}

// size 0, 1 or 2 is 128, 256 or 512 wide, each one is a new level
void Minecraft_generateNewLevel(int size) {
    Level_destroy(&level);
    Level_init(&level);
    // there's no logged in user, so the creator is "anonymous"
    LevelGen_generateLevel(&level, "anonymous", 128 << size, 128 << size, 64);
    setLevel();
}

static void attemptSaveLevel(void) {
    LevelIO_save(&level, "level.dat");
}

static bool fileExists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void setupFog(int i) {
    if (i == -1) {
        GLfloat c[4] = { fogRed, fogGreen, fogBlue, 1.0f };
        GLfloat a[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, fogEnd);
        glFogfv(GL_FOG_COLOR, c);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
        return;
    }
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
        GLfloat c[4] = { fogRed, fogGreen, fogBlue, 1.0f };
        GLfloat a[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, fogEnd);
        glFogfv(GL_FOG_COLOR, c);
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
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.0.14a_08", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.0.14a_08", NULL, NULL);
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
    glfwSetScrollCallback(window, scrollCallback);

    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
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
    int p[9] = { TILE_ROCK.id, TILE_DIRT.id, TILE_STONEBRICK.id, TILE_WOOD.id, TILE_BUSH.id,
                 TILE_LOG.id, TILE_LEAVES.id, TILE_SAND.id, TILE_GRAVEL.id };
    memcpy(palette, p, sizeof(palette));

    texTerrain = loadTexture("resources/terrain.png", GL_NEAREST);
    texDirt    = loadTexture("resources/dirt.png", GL_NEAREST);
    Font_init(&gFont, "resources/default.gif");

    int width, height;
    getSize(&width, &height);
    glViewport(0, 0, width, height);

    // a missing file shows no loading screen
    Level_init(&level);
    bool success = false;
    if (fileExists("level.dat")) {
        success = LevelIO_load(&level, "level.dat");
        if (!success) {
            Level_destroy(&level);
            Level_init(&level);
            if (fileExists("level.dat")) success = Level_loadLegacy(&level, "level.dat");
        }
    }
    if (!success) {
        Level_destroy(&level);
        Level_init(&level);
        LevelGen_generateLevel(&level, "anonymous", 128 << 1, 128 << 1, 64);
    }

    LevelRenderer_init(&levelRenderer, texTerrain);
    ParticleEngine_init(&particleEngine, &level, (GLuint)texTerrain);
    Player_init(&player, &level);
    Entity_resetPosition(&player.e);
    LevelRenderer_setLevel(&levelRenderer, &level);

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
    glRotatef(player.e.xRotation - player.e.xRotI * (1.0f - a), 1.0f, 0.0f, 0.0f);
    glRotatef(player.e.yRotation - player.e.yRotI * (1.0f - a), 0.0f, 1.0f, 0.0f);
    float x = player.e.prevX + (player.e.x - player.e.prevX) * a;
    float y = player.e.prevY + (player.e.y - player.e.prevY) * a;
    float z = player.e.prevZ + (player.e.z - player.e.prevZ) * a;
    glTranslatef(-x, -y, -z);
}

static void setupCamera(float a, int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0f, (float)width / (float)height, 0.05f, fogEnd);
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

static float hitDistanceTo(const HitResult* h, int mode) {
    int x = h->x, y = h->y, z = h->z;
    if (mode == 1) {
        if (h->f == 0) y--;
        if (h->f == 1) y++;
        if (h->f == 2) z--;
        if (h->f == 3) z++;
        if (h->f == 4) x--;
        if (h->f == 5) x++;
    }
    float xd = (float)x - player.e.x;
    float yd = (float)y - player.e.y;
    float zd = (float)z - player.e.z;
    return xd * xd + yd * yd + zd * zd;
}

// only faces whose front isn't covered by a solid tile can be picked
static void pick(float a, int width, int height) {
    glSelectBuffer(2000, selectBuffer);
    glRenderMode(GL_SELECT);
    setupPickCamera(a, width / 2, height / 2, width, height);
    Frustum frustum;
    frustum_calculate(&frustum);

    float rr = 2.5f;
    AABB box = AABB_grow(&player.e.boundingBox, rr, rr, rr);
    int x0 = (int)box.minX;
    int x1 = (int)(box.maxX + 1.0f);
    int y0 = (int)box.minY;
    int y1 = (int)(box.maxY + 1.0f);
    int z0 = (int)box.minZ;
    int z1 = (int)(box.maxZ + 1.0f);
    glInitNames();
    glPushName(0);
    glPushName(0);
    for (int x = x0; x < x1; x++) {
        glLoadName(x);
        glPushName(0);
        for (int y = y0; y < y1; y++) {
            glLoadName(y);
            glPushName(0);
            for (int z = z0; z < z1; z++) {
                const Tile* tile = gTiles[Level_getTile(&level, x, y, z)];
                AABB tileBox = AABB_create((float)x, (float)y, (float)z, (float)(x + 1), (float)(y + 1), (float)(z + 1));
                if (!tile || !tile->mayPick(tile) || !frustum_isVisible(&frustum, &tileBox)) continue;
                glLoadName(z);
                glPushName(0);
                for (int i = 0; i < 6; i++) {
                    if (!Tile_isFaceVisible(&level, x, y, z, i)) continue;
                    glLoadName(i);
                    Tessellator_begin(&hudTess);
                    Face_render(&hudTess, x, y, z, i, player.e.x, player.e.y, player.e.z);
                    Tessellator_end(&hudTess);
                }
                glPopName();
            }
            glPopName();
        }
        glPopName();
    }
    glPopName();
    glPopName();
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
        if (!haveBest || hitDistanceTo(&current, editMode) < hitDistanceTo(&best, editMode)) {
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

// only air and liquids can be built into
static void handleMouseClick(void) {
    if (isHitNull) return;
    const Tile* oldTile = gTiles[Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z)];
    if (editMode == 0) {
        bool changed = level_setTile(&level, hitResult.x, hitResult.y, hitResult.z, 0);
        if (oldTile && changed) Tile_onDestroy(oldTile, &level, hitResult.x, hitResult.y, hitResult.z, &particleEngine);
        return;
    }
    int x = hitResult.x;
    int y = hitResult.y;
    int z = hitResult.z;
    if (hitResult.f == 0) y--;
    if (hitResult.f == 1) y++;
    if (hitResult.f == 2) z--;
    if (hitResult.f == 3) z++;
    if (hitResult.f == 4) x--;
    if (hitResult.f == 5) x++;
    const Tile* there = gTiles[Level_getTile(&level, x, y, z)];
    if (there && there != &TILE_WATER && there != &TILE_CALM_WATER && there != &TILE_LAVA && there != &TILE_CALM_LAVA) return;
    const Tile* tile = gTiles[paintTexture];
    AABB aabb;
    if (tile->getAABB(tile, x, y, z, &aabb)) {
        if (AABB_intersects(&player.e.boundingBox, &aabb) || !Level_isFree(&level, &aabb)) return;
    }
    level_setTile(&level, x, y, z, paintTexture);
    tile->onPlace(tile, &level, x, y, z);
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
            int wheel = mouseEvents[e].wheel;
            if (wheel != 0) {
                int i = 0;
                for (int k = 0; k < 9; ++k) {
                    if (palette[k] == paintTexture) i = k;
                }
                i += wheel;
                while (i < 0) i += 9;
                while (i >= 9) i -= 9;
                paintTexture = palette[i];
            }
            if (!mouseGrabbed && mouseEvents[e].state) {
                Minecraft_grabMouse();
                continue;
            }
            if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_LEFT && mouseEvents[e].state) {
                handleMouseClick();
                lastClickTick = ticksRan;
            }
            if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_RIGHT && mouseEvents[e].state) editMode = (editMode + 1) % 2;
            if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_MIDDLE && mouseEvents[e].state && !isHitNull) {
                int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
                if (id == TILE_GRASS.id) id = TILE_DIRT.id;
                for (int k = 0; k < 9; ++k) {
                    if (id == palette[k]) paintTexture = palette[k];
                }
            }
        }
        mouseEventCount = 0;

        // a key can open the pause screen, the rest of the queue still goes here
        for (int e = 0; e < keyEventCount; e++) {
            int key = keyEvents[e].key;
            Player_setKey(&player, key, keyEvents[e].state);
            if (!keyEvents[e].state) continue;
            if (key == GLFW_KEY_ESCAPE) releaseMouse();
            if (key == GLFW_KEY_R) Entity_resetPosition(&player.e);
            if (key == GLFW_KEY_ENTER) {
                Level_setSpawnPos(&level, (int)player.e.x, (int)player.e.y, (int)player.e.z, player.e.yRotation);
                Entity_resetPosition(&player.e);
            }
            for (int k = 0; k < 9; ++k) {
                if (key == GLFW_KEY_1 + k) paintTexture = palette[k];
            }
            if (key == GLFW_KEY_Y) yMouseAxis = -yMouseAxis;
            if (key == GLFW_KEY_G && level.entityCount < 256) {
                Zombie z;
                Zombie_init(&z, &level, player.e.x, player.e.y, player.e.z);
                Level_addEntity(&level, &z);
            }
            if (key == GLFW_KEY_F) levelRenderer.drawDistance = (levelRenderer.drawDistance + 1) % 4;
        }
        keyEventCount = 0;

        // holding the left button repeats every timer.ticksPerSecond / 4 ticks
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
            (float)(ticksRan - lastClickTick) >= timer.ticksPerSecond / 4.0f && mouseGrabbed) {
            handleMouseClick();
            lastClickTick = ticksRan;
        }
    } else {
        lastClickTick = ticksRan + 10000;
    }

    if (screen) {
        screenUpdateEvents(screen);
        if (screen && screen->tick) screen->tick(screen);
    }

    levelRenderer.cloudTickCounter++;
    Level_onTick(&level);
    ParticleEngine_onTick(&particleEngine);
    Player_onTick(&player);
}

/* frame */

static void drawGui(int width, int height) {
    int sw, sh;
    guiSize(&sw, &sh);
    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);
    int mx, my;
    lwjglMouse(cx, cy, &mx, &my);
    int xMouse = mx * sw / width;
    int yMouse = sh - my * sh / height - 1;

    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, sw, sh, 0.0, 100.0, 300.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);

    glPushMatrix();
    glTranslatef((float)(sw - 16), 16.0f, -50.0f);
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

    Font_drawShadow(&gFont, &hudTess, "0.0.14a_08", 2, 2, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, fpsString, 2, 12, 0xFFFFFF);

    int wc = sw / 2;
    int hc = sh / 2;
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

    if (mouseGrabbed) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        int ww, wh; glfwGetWindowSize(window, &ww, &wh);
        double cx = ww * 0.5, cy = wh * 0.5;

        // mouse deltas are whole pixels and y goes up
        int dx = (int)(mx - cx);
        int dy = (int)(cy - my);
        Player_turn(&player, (float)dx, (float)(dy * yMouseAxis));
        glfwSetCursorPos(window, cx, cy);
    }

    int width, height;
    getSize(&width, &height);
    glViewport(0, 0, width, height);

    pick(a, width, height);

    fogRed = 0.92f;
    fogGreen = 0.98f;
    fogBlue = 1.0f;
    glClearColor(fogRed, fogGreen, fogBlue, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    fogEnd = (float)(1024 >> (levelRenderer.drawDistance << 1));
    setupCamera(a, width, height);
    glEnable(GL_CULL_FACE);
    Frustum frustum;
    frustum_calculate(&frustum);
    LevelRenderer_cull(&levelRenderer, &frustum);
    LevelRenderer_updateDirtyChunks(&levelRenderer, &player);

    bool inside = Level_isSolid(&level, player.e.x, player.e.y, player.e.z, 0.1f);
    setupFog(0);
    glEnable(GL_FOG);
    LevelRenderer_render(&levelRenderer, &player, 0);
    if (inside) {
        int px = (int)player.e.x;
        int py = (int)player.e.y;
        int pz = (int)player.e.z;
        for (int x = px - 1; x <= px + 1; x++)
        for (int y = py - 1; y <= py + 1; y++)
        for (int z = pz - 1; z <= pz + 1; z++) {
            LevelRenderer_renderInside(&levelRenderer, x, y, z);
        }
    }
    LevelRenderer_renderEntities(&levelRenderer, &frustum, a);
    ParticleEngine_render(&particleEngine, &player, a);
    LevelRenderer_renderSurroundingGround(&levelRenderer);
    glDisable(GL_LIGHTING);
    setupFog(-1);
    LevelRenderer_renderClouds(&levelRenderer, a);
    setupFog(1);
    glEnable(GL_LIGHTING);

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
    LevelRenderer_render(&levelRenderer, &player, 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    LevelRenderer_render(&levelRenderer, &player, 1);
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);

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
        for (int i = 0; i < timer.ticks; ++i) {
            ticksRan++;
            tick();
        }

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
