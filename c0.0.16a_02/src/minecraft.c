// minecraft.c: window, main loop, input, camera, picking, chat, the level loading screen and the server glue

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "net/connection.h"
#include "net/network_player.h"

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
#include "gui/chat_screen.h"
#include "gui/error_screen.h"

static GLFWwindow*    window;
// the level can be gone while a server level downloads
static Level          level;
static bool           hasLevel = false;
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
static ChatScreen     chatScreen;
static ErrorScreen    errorScreen;
static Screen*        screen = NULL;

// set when the frame throws, the rest of it is skipped
static bool broke = false;

// the command line [host] [port] [user]
static char connectHost[256] = "";
static int  connectPort = 25565;
static char userName[64] = "";

static NetConnection connection;
static bool networked = false;
// true while the server level downloads, nothing is rendered then
static bool downloading = false;
// id to NetworkPlayer map, the players stay alive while the map or the level holds them
static NetworkPlayer* netPlayers[128];

#define MAX_CHAT_LINES 8
typedef struct { char text[72]; int age; } ChatLine;
static ChatLine chatLines[MAX_CHAT_LINES + 1];
static int chatLineCount = 0;

// glfw callbacks queue the events and the tick drains them
// a typed character comes as its own key event with key 0
#define MAX_EVENTS 256
typedef struct { int key; char c; bool state; double x, y; int wheel; } InputEvent;
static InputEvent mouseEvents[MAX_EVENTS];
static int        mouseEventCount = 0;
static InputEvent keyEvents[MAX_EVENTS];
static int        keyEventCount = 0;
static bool       keyRepeat = false;

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)w; (void)scancode; (void)mods;
    if ((action == GLFW_REPEAT && !keyRepeat) || keyEventCount >= MAX_EVENTS) return;
    keyEvents[keyEventCount].key = key;
    keyEvents[keyEventCount].c = '\0';
    keyEvents[keyEventCount].state = action != GLFW_RELEASE;
    keyEventCount++;
}

static void charCallback(GLFWwindow* w, unsigned int codepoint) {
    (void)w;
    if (keyEventCount >= MAX_EVENTS) return;
    keyEvents[keyEventCount].key = 0;
    keyEvents[keyEventCount].c = codepoint < 128 ? (char)codepoint : '\0';
    keyEvents[keyEventCount].state = true;
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

void Minecraft_setKeyRepeat(bool on) {
    keyRepeat = on;
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

Font* Minecraft_getFont(void) {
    return &gFont;
}

float Minecraft_getPlayerYRot(void) {
    return player.e.yRotation;
}

/* screens */

static bool errorShown(void) {
    return screen == (Screen*)&errorScreen;
}

void Minecraft_grabMouse(void);

// nothing replaces an error screen
void Minecraft_setScreen(Screen* s) {
    if (errorShown()) return;
    if (screen && screen->removed) screen->removed(screen);
    screen = s;
    if (s) {
        if (mouseGrabbed) {
            Player_releaseAllKeys(&player);
            mouseGrabbed = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        if (s->opened) s->opened(s);
        return;
    }
    Minecraft_grabMouse();
}

void Minecraft_showPauseScreen(void) {
    if (errorShown()) return;
    int sw, sh;
    guiSize(&sw, &sh);
    PauseScreen_init(&pauseScreen, &gFont, sw, sh);
    Minecraft_setScreen((Screen*)&pauseScreen);
}

void Minecraft_showNewLevelScreen(void) {
    if (errorShown()) return;
    int sw, sh;
    guiSize(&sw, &sh);
    NewLevelScreen_init(&newLevelScreen, &gFont, sw, sh);
    Minecraft_setScreen((Screen*)&newLevelScreen);
}

void Minecraft_showErrorScreen(const char* t, const char* message) {
    if (errorShown()) return;
    int sw, sh;
    guiSize(&sw, &sh);
    ErrorScreen_init(&errorScreen, &gFont, sw, sh, t, message);
    Minecraft_setScreen((Screen*)&errorScreen);
}

static void showChatScreen(void) {
    if (errorShown()) return;
    int sw, sh;
    guiSize(&sw, &sh);
    ChatScreen_init(&chatScreen, &gFont, sw, sh);
    Minecraft_setScreen((Screen*)&chatScreen);
}

// the frame's catch, every throw that can happen here is a NullPointerException
static void clientError(void) {
    Minecraft_showErrorScreen("Client error", "The game broke! [java.lang.NullPointerException]");
    broke = true;
}

void Minecraft_grabMouse(void) {
    if (mouseGrabbed) return;
    mouseGrabbed = true;
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
    Minecraft_setScreen(NULL);
    lastClickTick = ticksRan + 10000;
}

/* level loading screen */

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

/* levels */

static bool inLevel(const NetworkPlayer* p) {
    if (!hasLevel) return false;
    for (int i = 0; i < level.playerCount; i++) {
        if (level.players[i] == p) return true;
    }
    return false;
}

static bool inMap(const NetworkPlayer* p) {
    for (int i = 0; i < 128; i++) {
        if (netPlayers[i] == p) return true;
    }
    return false;
}

static void freePlayer(NetworkPlayer* p) {
    NetworkPlayer_free(p);
    free(p);
}

// frees the players only the old level held, then the level itself
static void dropLevel(void) {
    if (!hasLevel) return;
    for (int i = 0; i < level.playerCount; i++) {
        if (!inMap(level.players[i])) freePlayer(level.players[i]);
    }
    Level_destroy(&level);
    hasLevel = false;
}

// the level is already loaded or generated into place, or gone
static void setLevel(bool present) {
    hasLevel = present;
    if (levelRenderer.terrainTex) LevelRenderer_setLevel(&levelRenderer, present ? &level : NULL);
    particleEngine.count = 0;
    player.e.level = present ? &level : NULL;
    Entity_resetPosition(&player.e);
}

// size 0, 1 or 2 is 128, 256 or 512 wide, each one is a new level
void Minecraft_generateNewLevel(int size) {
    dropLevel();
    Level_init(&level);
    LevelGen_generateLevel(&level, userName[0] ? userName : "anonymous", 128 << size, 128 << size, 64);
    setLevel(true);
}

static bool fileExists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void Minecraft_clearLevel(void) {
    dropLevel();
    setLevel(false);
}

void Minecraft_installNetworkLevel(int w, int d, int h, unsigned char* blocks) {
    dropLevel();
    Level_init(&level);
    Level_setData(&level, w, d, h, blocks);
    setLevel(true);
    downloading = false;
}

void Minecraft_networkSetBlock(int x, int y, int z, int type) {
    if (hasLevel) level_setTile(&level, x, y, z, type);
}

void Minecraft_networkMoveSelf(float x, float y, float z, float a, float b) {
    Entity_moveTo(&player.e, x, y, z, a, b);
}

// false where it throws, the spawn goes in the map before the missing level is touched
bool Minecraft_spawnNetworkPlayer(int id, const char* name, int x, int y, int z, float yRot, float xRot) {
    NetworkPlayer* p = (NetworkPlayer*)malloc(sizeof(NetworkPlayer));
    NetworkPlayer_init(p, hasLevel ? &level : NULL, id, name, x, y, z, yRot, xRot);
    NetworkPlayer* old = netPlayers[id];
    netPlayers[id] = p;
    if (old && !inLevel(old)) freePlayer(old);
    if (!hasLevel) return false;
    Level_addPlayer(&level, p);
    return true;
}

void Minecraft_teleportNetworkPlayer(int id, int x, int y, int z, float yRot, float xRot) {
    if (netPlayers[id]) NetworkPlayer_teleport(netPlayers[id], x, y, z, yRot, xRot);
}

void Minecraft_queueNetworkPlayerMoveLook(int id, int dx, int dy, int dz, float yRot, float xRot) {
    if (netPlayers[id]) NetworkPlayer_queueMoveLook(netPlayers[id], dx, dy, dz, yRot, xRot);
}

void Minecraft_queueNetworkPlayerMove(int id, int dx, int dy, int dz) {
    if (netPlayers[id]) NetworkPlayer_queueMove(netPlayers[id], dx, dy, dz);
}

void Minecraft_queueNetworkPlayerLook(int id, float yRot, float xRot) {
    if (netPlayers[id]) NetworkPlayer_queueLook(netPlayers[id], yRot, xRot);
}

bool Minecraft_despawnNetworkPlayer(int id) {
    NetworkPlayer* p = netPlayers[id];
    if (!p) return true;
    netPlayers[id] = NULL;
    if (!hasLevel) {
        freePlayer(p);
        return false;
    }
    Level_removePlayer(&level, p);
    freePlayer(p);
    return true;
}

void Minecraft_addChatLine(const char* text) {
    snprintf(chatLines[chatLineCount].text, sizeof chatLines[chatLineCount].text, "%s", text);
    chatLines[chatLineCount].age = 0;
    chatLineCount++;
    while (chatLineCount > MAX_CHAT_LINES) {
        memmove(&chatLines[0], &chatLines[1], sizeof(ChatLine) * (size_t)(chatLineCount - 1));
        chatLineCount--;
    }
}

// the chat screen sends through the connection even when there is none
void Minecraft_sendChat(const char* text) {
    if (!networked) {
        clientError();
        return;
    }
    NetConnection_sendMessage(&connection, text);
}

/* fog */

static void setupFog(void) {
    GLfloat c[4] = { fogRed, fogGreen, fogBlue, 1.0f };
    glFogfv(GL_FOG_COLOR, c);
    const Tile* currentTile = gTiles[Level_getTile(&level, (int)player.e.x, (int)(player.e.y + 0.12f), (int)player.e.z)];
    if (currentTile && currentTile->liquidType != LIQUID_NONE) {
        glFogi(GL_FOG_MODE, GL_EXP);
        if (currentTile->liquidType == LIQUID_WATER) {
            GLfloat a[4] = { 0.4f, 0.4f, 0.9f, 1.0f };
            glFogf(GL_FOG_DENSITY, 0.1f);
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
        } else if (currentTile->liquidType == LIQUID_LAVA) {
            GLfloat a[4] = { 0.4f, 0.3f, 0.3f, 1.0f };
            glFogf(GL_FOG_DENSITY, 2.0f);
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
        }
    } else {
        GLfloat a[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, fogEnd);
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
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.0.16a_02", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.0.16a_02", NULL, NULL);
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
    glfwSetCharCallback(window, charCallback);
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

    Timer_init(&timer, 20.0f);

    if (connectHost[0]) {
        if (NetConnection_open(&connection, connectHost, connectPort, userName[0] ? userName : "guest")) {
            networked = true;
            Minecraft_beginLevelLoading("Connecting..");
            downloading = true;
        } else {
            Minecraft_showErrorScreen("Failed to connect", "You failed to connect to the server. It's probably down!");
        }
    } else {
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
        if (success) {
            hasLevel = true;
        } else {
            Level_destroy(&level);
            Level_init(&level);
            LevelGen_generateLevel(&level, userName[0] ? userName : "anonymous", 128 << 1, 128 << 1, 64);
            hasLevel = true;
        }
    }

    LevelRenderer_init(&levelRenderer, texTerrain);
    ParticleEngine_init(&particleEngine, &level, (GLuint)texTerrain);
    Player_init(&player, hasLevel ? &level : NULL);
    Entity_resetPosition(&player.e);
    if (hasLevel) LevelRenderer_setLevel(&levelRenderer, &level);

    return 1;
}

static void destroy(void) {
    // whatever level is here gets saved on the way out, a server's level too
    if (hasLevel) LevelIO_save(&level, "level.dat");
    else Level_saveNull("level.dat");
    if (networked) NetConnection_close(&connection);
    LevelRenderer_destroy(&levelRenderer);
    dropLevel();
    for (int i = 0; i < 128; i++) {
        if (netPlayers[i]) freePlayer(netPlayers[i]);
        netPlayers[i] = NULL;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
}

/* camera */

static void moveCameraToPlayer(float a) {
    glTranslatef(0.0f, 0.0f, -0.3f);
    glRotatef(player.e.xRotO + (player.e.xRotation - player.e.xRotO) * a, 1.0f, 0.0f, 0.0f);
    glRotatef(player.e.yRotO + (player.e.yRotation - player.e.yRotO) * a, 0.0f, 1.0f, 0.0f);
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

// only air and liquids can be built into, the server hears about it first
static void handleMouseClick(void) {
    if (isHitNull) return;
    // the hit can outlive the level it came from
    if (!hasLevel) {
        clientError();
        return;
    }
    int x = hitResult.x;
    int y = hitResult.y;
    int z = hitResult.z;
    if (editMode != 0) {
        if (hitResult.f == 0) y--;
        if (hitResult.f == 1) y++;
        if (hitResult.f == 2) z--;
        if (hitResult.f == 3) z++;
        if (hitResult.f == 4) x--;
        if (hitResult.f == 5) x++;
    }
    const Tile* oldTile = gTiles[Level_getTile(&level, x, y, z)];
    if (editMode == 0) {
        bool changed = level_setTile(&level, x, y, z, 0);
        if (oldTile && changed) {
            if (networked) NetConnection_sendSetBlock(&connection, x, y, z, editMode, paintTexture);
            Tile_onDestroy(oldTile, &level, x, y, z, &particleEngine);
        }
        return;
    }
    const Tile* there = gTiles[Level_getTile(&level, x, y, z)];
    if (there && there != &TILE_WATER && there != &TILE_CALM_WATER && there != &TILE_LAVA && there != &TILE_CALM_LAVA) return;
    const Tile* tile = gTiles[paintTexture];
    AABB aabb;
    if (tile->getAABB(tile, x, y, z, &aabb)) {
        if (AABB_intersects(&player.e.boundingBox, &aabb) || !Level_isFree(&level, &aabb)) return;
    }
    if (networked) NetConnection_sendSetBlock(&connection, x, y, z, editMode, paintTexture);
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
    int e = 0;
    while (e < keyEventCount && !broke) {
        InputEvent ev = keyEvents[e++];
        if (ev.state && s->keyPressed) s->keyPressed(s, ev.c, ev.key);
    }
    // what's left after a throw waits for the next tick, which the error screen drains
    memmove(keyEvents, keyEvents + e, sizeof(InputEvent) * (size_t)(keyEventCount - e));
    keyEventCount -= e;
}

static void tick(void) {
    for (int i = 0; i < chatLineCount; i++) {
        if ((float)(chatLines[i].age++) >= timer.ticksPerSecond * 10.0f) {
            memmove(&chatLines[i], &chatLines[i + 1], sizeof(ChatLine) * (size_t)(chatLineCount - i - 1));
            chatLineCount--;
            i--;
        }
    }

    if (networked) {
        if (connection.connected && !NetConnection_tick(&connection)) {
            Minecraft_showErrorScreen("Disconnected!", "You've lost connection to the server");
            downloading = false;
            NetConnection_close(&connection);
            networked = false;
            // the position goes through the connection that was just dropped
            clientError();
            return;
        }
        NetConnection_sendPosition(&connection, player.e.x, player.e.y, player.e.z, player.e.yRotation, player.e.xRotation);
    }

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
                if (broke) {
                    memmove(mouseEvents, mouseEvents + e + 1, sizeof(InputEvent) * (size_t)(mouseEventCount - e - 1));
                    mouseEventCount -= e + 1;
                    return;
                }
                lastClickTick = ticksRan;
            }
            if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_RIGHT && mouseEvents[e].state) editMode = (editMode + 1) % 2;
            if (mouseEvents[e].key == GLFW_MOUSE_BUTTON_MIDDLE && mouseEvents[e].state && !isHitNull) {
                if (!hasLevel) {
                    clientError();
                    memmove(mouseEvents, mouseEvents + e + 1, sizeof(InputEvent) * (size_t)(mouseEventCount - e - 1));
                    mouseEventCount -= e + 1;
                    return;
                }
                int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
                if (id == TILE_GRASS.id) id = TILE_DIRT.id;
                for (int k = 0; k < 9; ++k) {
                    if (id == palette[k]) paintTexture = palette[k];
                }
            }
        }
        mouseEventCount = 0;

        // a key can open a screen, the rest of the queue still goes here
        for (int e = 0; e < keyEventCount; e++) {
            int key = keyEvents[e].key;
            if (key == 0) continue;
            Player_setKey(&player, key, keyEvents[e].state);
            if (!keyEvents[e].state) continue;
            if (key == GLFW_KEY_ESCAPE) Minecraft_showPauseScreen();
            if (key == GLFW_KEY_R) Entity_resetPosition(&player.e);
            if (key == GLFW_KEY_ENTER) {
                if (!hasLevel) {
                    clientError();
                    memmove(keyEvents, keyEvents + e + 1, sizeof(InputEvent) * (size_t)(keyEventCount - e - 1));
                    keyEventCount -= e + 1;
                    return;
                }
                Level_setSpawnPos(&level, (int)player.e.x, (int)player.e.y, (int)player.e.z, player.e.yRotation);
                Entity_resetPosition(&player.e);
            }
            for (int k = 0; k < 9; ++k) {
                if (key == GLFW_KEY_1 + k) paintTexture = palette[k];
            }
            if (key == GLFW_KEY_Y) yMouseAxis = -yMouseAxis;
            if (key == GLFW_KEY_G && !networked) {
                if (!hasLevel) {
                    clientError();
                    memmove(keyEvents, keyEvents + e + 1, sizeof(InputEvent) * (size_t)(keyEventCount - e - 1));
                    keyEventCount -= e + 1;
                    return;
                }
                if (level.entityCount + level.playerCount < 256) {
                    Zombie z;
                    Zombie_init(&z, &level, player.e.x, player.e.y, player.e.z);
                    Level_addEntity(&level, &z);
                }
            }
            if (key == GLFW_KEY_F) levelRenderer.drawDistance = (levelRenderer.drawDistance + 1) % 4;
            if (key == GLFW_KEY_T) {
                Player_releaseAllKeys(&player);
                showChatScreen();
            }
        }
        keyEventCount = 0;

        // holding the left button repeats every timer.ticksPerSecond / 4 ticks
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
            (float)(ticksRan - lastClickTick) >= timer.ticksPerSecond / 4.0f && mouseGrabbed) {
            handleMouseClick();
            if (broke) return;
            lastClickTick = ticksRan;
        }
    } else {
        lastClickTick = ticksRan + 10000;
    }

    if (screen) {
        screenUpdateEvents(screen);
        if (broke) return;
        if (screen && screen->tick) screen->tick(screen);
    }

    if (hasLevel) {
        levelRenderer.cloudTickCounter++;
        Level_tickEntities(&level);
        if (!networked) Level_onTick(&level);
        ParticleEngine_onTick(&particleEngine);
        Player_onTick(&player);
    }
}

/* frame */

static void setupGuiCamera(void) {
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

static void drawGui(void) {
    int sw, sh;
    guiSize(&sw, &sh);
    setupGuiCamera();

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

    Font_drawShadow(&gFont, &hudTess, "0.0.16a_02", 2, 2, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, fpsString, 2, 12, 0xFFFFFF);
    for (int i = 0; i < chatLineCount; i++) {
        Font_drawShadow(&gFont, &hudTess, chatLines[i].text, 2, sh - 4 - (chatLineCount << 3) + (i << 3) - 16, 0xFFFFFF);
    }

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
}

static void renderLevel(float a, int width, int height) {
    glViewport(0, 0, width, height);

    // the sky fades to white as the view distance shrinks
    float f = 1.0f / (float)(4 - levelRenderer.drawDistance);
    f = (float)pow(f, 0.25);
    fogRed   = 0.6f * (1.0f - f) + f;
    fogGreen = 0.8f * (1.0f - f) + f;
    fogBlue  = 1.0f * (1.0f - f) + f;
    const Tile* headTile = gTiles[Level_getTile(&level, (int)player.e.x, (int)(player.e.y + 0.12f), (int)player.e.z)];
    if (headTile && headTile->liquidType != LIQUID_NONE) {
        if (headTile->liquidType == LIQUID_WATER) {
            fogRed = 0.02f;
            fogGreen = 0.02f;
            fogBlue = 0.2f;
        } else if (headTile->liquidType == LIQUID_LAVA) {
            fogRed = 0.6f;
            fogGreen = 0.1f;
            fogBlue = 0.0f;
        }
    }
    glClearColor(fogRed, fogGreen, fogBlue, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pick(a, width, height);

    fogEnd = (float)(512 >> (levelRenderer.drawDistance << 1));
    setupCamera(a, width, height);
    glEnable(GL_CULL_FACE);
    Frustum frustum;
    frustum_calculate(&frustum);
    LevelRenderer_cull(&levelRenderer, &frustum);
    LevelRenderer_updateDirtyChunks(&levelRenderer, &player);

    bool inside = Level_isSolid(&level, player.e.x, player.e.y, player.e.z, 0.1f);
    setupFog();
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
    // setupFog turns the lighting back on, so the clouds are lit
    setupFog();
    LevelRenderer_renderClouds(&levelRenderer, a);
    setupFog();
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
    setupFog();
    LevelRenderer_renderSurroundingWater(&levelRenderer);
    glEnable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    int liquidLists = LevelRenderer_render(&levelRenderer, &player, 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (liquidLists > 0) LevelRenderer_renderAgain(&levelRenderer);
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

    drawGui();
}

// false where it throws, the frame isn't shown then
static bool render(float a) {
    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) Minecraft_showPauseScreen();

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

    // nothing is drawn while a server level downloads, the loading screen draws itself
    if (downloading) return true;

    int width, height;
    getSize(&width, &height);
    if (hasLevel) {
        renderLevel(a, width, height);
    } else {
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        setupGuiCamera();
    }

    if (screen) {
        int sw, sh;
        guiSize(&sw, &sh);
        double cx, cy;
        glfwGetCursorPos(window, &cx, &cy);
        int mx, my;
        lwjglMouse(cx, cy, &mx, &my);
        screen->render(screen, mx * sw / width, sh - my * sh / height - 1);
    }

    if (Font_threw) return false;
    glfwSwapBuffers(window);
    // pumps the window messages
    glfwPollEvents();
    return true;
}

/* main loop */

int main(int argc, char** argv) {
    if (argc >= 2) {
        snprintf(connectHost, sizeof connectHost, "%s", argv[1]);
        if (argc >= 3) connectPort = atoi(argv[2]);
        if (argc >= 4) snprintf(userName, sizeof userName, "%s", argv[3]);
    }

    if (!init()) {
        fprintf(stderr, "Failed to initialize game\n");
        exit(EXIT_FAILURE);
    }

    long long lastTime = currentTimeMillis();
    int frames = 0;

    while (!glfwWindowShouldClose(window)) {
        broke = false;
        Font_threw = false;
        Timer_advanceTime(&timer);
        for (int i = 0; i < timer.ticks && !broke; ++i) {
            ticksRan++;
            tick();
        }

        if (!broke && render(timer.partialTicks)) frames++;

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
