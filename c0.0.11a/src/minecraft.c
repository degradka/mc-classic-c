// minecraft.c: window, main loop, input, camera and picking

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

static int editMode = 0;
static int yMouseAxis = 1;
static int paintTexture = 1;

static int texTerrain = 0;

static Tessellator hudTess;
static Font gFont;
static char fpsString[64] = "";

static int gWinWidth  = 854;
static int gWinHeight = 480;
static int gIsFullscreen = 0;

static GLfloat fogColor0[4] = { 254.0f/255.0f, 251.0f/255.0f, 250.0f/255.0f, 1.0f };
static GLfloat fogColor1[4] = {  14.0f/255.0f,  11.0f/255.0f,  10.0f/255.0f, 1.0f };

static int       isHitNull = 1;
static HitResult hitResult;

static GLuint selectBuffer[2000];

static bool mouseGrabbed = false;

// glfw callbacks queue the events and tick() drains them
#define MAX_EVENTS 256
typedef struct { int key; } InputEvent;
static InputEvent mouseEvents[MAX_EVENTS];
static int        mouseEventCount = 0;
static InputEvent keyEvents[MAX_EVENTS];
static int        keyEventCount = 0;

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)w; (void)scancode; (void)mods;
    if (action == GLFW_PRESS && keyEventCount < MAX_EVENTS) keyEvents[keyEventCount++].key = key;
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
    (void)w; (void)mods;
    if (action == GLFW_PRESS && mouseEventCount < MAX_EVENTS) mouseEvents[mouseEventCount++].key = button;
}

static void grabMouse(void) {
    if (mouseGrabbed) return;
    mouseGrabbed = true;
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
}

static void releaseMouse(void) {
    if (!mouseGrabbed) return;
    mouseGrabbed = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

static void setupFog(int i) {
    if (i == 0) {
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 0.001f);
        glFogfv(GL_FOG_COLOR, fogColor0);
        glDisable(GL_LIGHTING);
    } else if (i == 1) {
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 0.01f);
        glFogfv(GL_FOG_COLOR, fogColor1);
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
        float br = 0.6f;
        GLfloat ambient[4] = { br, br, br, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    }
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
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    Tile_registerAll();

    texTerrain = loadTexture("resources/terrain.png", GL_NEAREST);

    Level_init(&level, 256, 256, 64);
    LevelRenderer_init(&levelRenderer, &level, texTerrain);
    Player_init(&player, &level);
    ParticleEngine_init(&particleEngine, &level, (GLuint)texTerrain);
    Font_init(&gFont, "resources/default.gif");

    mobCount = 0;
    for (int i = 0; i < 10; ++i) {
        Zombie_init(&mobs[mobCount], &level, 128.0f, 0.0f, 128.0f);
        Entity_resetPosition(&mobs[mobCount].base);
        mobCount++;
    }

    grabMouse();

    Timer_init(&timer, 20.0f);

    return 1;
}

static void destroy(void) {
    Level_save(&level);
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
    gluPerspective(70.0f, (float)width / (float)height, 0.05f, 1000.0f);
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
    gluPerspective(70.0f, (float)width / (float)height, 0.05f, 1000.0f);
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

    // the depth is read as a signed int
    int p = 0;
    long long closest = 0;
    int names[10];
    int hitNameCount = 0;
    for (int i = 0; i < hits; i++) {
        int nameCount = (int)selectBuffer[p++];
        long long minZ = (int)selectBuffer[p++];
        p++;
        long long dist = minZ;
        if (dist < closest || i == 0) {
            closest = dist;
            hitNameCount = nameCount;
            for (int j = 0; j < nameCount; j++) names[j] = (int)selectBuffer[p++];
        } else {
            p += nameCount;
        }
    }

    if (hitNameCount > 0) {
        hitresult_create(&hitResult, names[1], names[2], names[3], names[0], names[4]);
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

static void tick(void) {
    for (int e = 0; e < mouseEventCount; e++) {
        if (!mouseGrabbed) {
            grabMouse();
            continue;
        }
        if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_LEFT) handleMouseClick();
        if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_RIGHT) editMode = (editMode + 1) % 2;
    }
    mouseEventCount = 0;

    for (int e = 0; e < keyEventCount; e++) {
        int key = keyEvents[e].key;
        if (key == GLFW_KEY_ESCAPE && !gIsFullscreen) releaseMouse();
        if (key == GLFW_KEY_ENTER) Level_save(&level);
        if (key == GLFW_KEY_1) paintTexture = 1;
        if (key == GLFW_KEY_2) paintTexture = 3;
        if (key == GLFW_KEY_3) paintTexture = 4;
        if (key == GLFW_KEY_4) paintTexture = 5;
        if (key == GLFW_KEY_6) paintTexture = 6;
        if (key == GLFW_KEY_Y) yMouseAxis *= -1;
        if (key == GLFW_KEY_G && mobCount < MAX_MOBS) {
            Zombie_init(&mobs[mobCount++], &level, player.e.x, player.e.y, player.e.z);
        }
    }
    keyEventCount = 0;

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

    Player_onTick(&player, window);
}

/* frame */

static void drawGui(int width, int height) {
    int screenWidth  = width * 240 / height;
    int screenHeight = height * 240 / height;

    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, screenWidth, screenHeight, 0.0, 100.0, 300.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);

    glPushMatrix();
    glTranslatef((float)(screenWidth - 16), 16.0f, 0.0f);
    glScalef(16.0f, 16.0f, 16.0f);
    glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-1.5f, 0.5f, -0.5f);
    glScalef(-1.0f, -1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, texTerrain);
    glEnable(GL_TEXTURE_2D);
    Tessellator_init(&hudTess);
    gTiles[paintTexture]->render(gTiles[paintTexture], &hudTess, &level, 0, -2, 0, 0);
    Tessellator_flush(&hudTess);
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();

    Font_drawShadow(&gFont, &hudTess, "0.0.11a", 2, 2, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, fpsString, 2, 12, 0xFFFFFF);

    int wc = screenWidth / 2;
    int hc = screenHeight / 2;
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    Tessellator_init(&hudTess);
    Tessellator_vertex(&hudTess, (float)(wc + 1), (float)(hc - 4), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc - 0), (float)(hc - 4), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc - 0), (float)(hc + 5), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc + 1), (float)(hc + 5), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc + 5), (float)(hc - 0), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc - 4), (float)(hc - 0), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc - 4), (float)(hc + 1), 0.0f);
    Tessellator_vertex(&hudTess, (float)(wc + 5), (float)(hc + 1), 0.0f);
    Tessellator_flush(&hudTess);
}

static void render(float a) {
    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) releaseMouse();

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    if (mouseGrabbed) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        int ww, wh; glfwGetWindowSize(window, &ww, &wh);
        double cx = ww * 0.5, cy = wh * 0.5;

        // mouse y counts up from the bottom
        float xo = (float)(mx - cx);
        float yo = (float)(cy - my);
        Player_turn(&player, window, xo, yo * (float)yMouseAxis);
        glfwSetCursorPos(window, cx, cy);
    }

    pick(a, width, height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupCamera(a, width, height);
    glEnable(GL_CULL_FACE);
    Frustum frustum;
    frustum_calculate(&frustum);
    LevelRenderer_updateDirtyChunks(&levelRenderer, &player);

    setupFog(0);
    glEnable(GL_FOG);
    LevelRenderer_render(&levelRenderer, 0);
    for (int i = 0; i < mobCount; i++) {
        const Zombie* z = &mobs[i];
        if (Entity_isLit(&z->base) && frustum_isVisible(&frustum, &z->base.boundingBox)) Zombie_render(z, a);
    }
    ParticleEngine_render(&particleEngine, &player, a, 0);

    setupFog(1);
    LevelRenderer_render(&levelRenderer, 1);
    for (int i = 0; i < mobCount; i++) {
        const Zombie* z = &mobs[i];
        if (!Entity_isLit(&z->base) && frustum_isVisible(&frustum, &z->base.boundingBox)) Zombie_render(z, a);
    }
    ParticleEngine_render(&particleEngine, &player, a, 1);

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    if (!isHitNull) {
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &hitResult, editMode, paintTexture);
        glEnable(GL_ALPHA_TEST);
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
