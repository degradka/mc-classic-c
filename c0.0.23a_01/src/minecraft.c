// minecraft.c: window, main loop, input, camera, picking, hud, the level loading screen and the server glue

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
#include "java_hashmap.h"
#include "renderer/texture_fx.h"
#include "options.h"
#include "keys.h"
#include "audio/sound.h"
#include "gui/font.h"
#include "gui/screen.h"
#include "gui/pause_screen.h"
#include "gui/chat_screen.h"
#include "gui/error_screen.h"
#include "gui/inventory_screen.h"

static GLFWwindow*    window;
// the level can be gone while a server level downloads
static Level          level;
static bool           hasLevel = false;
static LevelRenderer  levelRenderer;
static Player         player;
static Timer          timer;
static ParticleEngine particleEngine;
static Options        options;

static int editMode = 0;
// what the build key offers, the inventory starts with the first 9
const int Minecraft_creativeTiles[] = { 1, 4, 3, 5, 17, 18, 6, 37, 38, 39, 40, 12, 13, 20, 19, 41,
                                        21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36 };
const int Minecraft_creativeTileCount = 32;
// the player's Inventory
static int slots[9];
static int selectedSlot = 0;
// from the server's login, 100 and up may break bedrock
static int userType = 0;

static int texTerrain = 0;
static int texGui = 0;

// lava then water, each steps once when it registers
static TextureFX lavaFX, waterFX;
static TextureFX* textureFX[2] = { &lavaFX, &waterFX };
static int texDirt = 0;

static Tessellator hudTess;
static Font gFont;
static char fpsString[64] = "";
static char title[64] = "";
static char loadStatus[64] = "";
// the tab list name under the mouse, the chat screen adds it on a click
static char hoveredUsername[65];
static bool hasHoveredUsername = false;

static int gWinWidth  = 854;
static int gWinHeight = 480;
static int gIsFullscreen = 0;

// the clear and fog color, set again every frame
static float fogRed = 0.5f, fogGreen = 0.8f, fogBlue = 1.0f;
static float renderDistance = 0.0f;
// pauses when the window loses focus, not while it's out of focus
static bool displayActive = false;

static bool      hasHit = false;
static HitResult hitResult;

static bool mouseGrabbed = false;
// ticks since boot, and the tick of the last click for holding the button
static int ticksRan = 0;
static int prevFrameTime = 0;

static Screen* screen = NULL;
// nothing is drawn while connecting and while the server level downloads
static bool hideGui = false;

// set when the frame throws, the rest of it is skipped
static bool broke = false;

// the command line [host] [port] [user] [mppass]
static char connectHost[256] = "";
static int  connectPort = 25565;
static char userName[64] = "";
static char mppass[64] = "--";

static NetConnection connection;
static bool networked = false;
// our position only goes out once the level is here
static bool levelReceived = false;
// id to NetworkPlayer map, the players stay alive while the map or the level holds them
static JHashMap netPlayers;

// newest first
#define MAX_CHAT_LINES 50
typedef struct { char text[72]; int counter; } ChatLine;
static ChatLine chatLines[MAX_CHAT_LINES + 1];
static int chatLineCount = 0;

// glfw callbacks queue the events and the tick drains them
// keys are the game's codes, a typed character joins the key event it came from
#define MAX_EVENTS 256
typedef struct { int key; char c; bool state; double x, y; int wheel; } InputEvent;
static InputEvent mouseEvents[MAX_EVENTS];
static int        mouseEventCount = 0;
static InputEvent keyEvents[MAX_EVENTS];
static int        keyEventCount = 0;
static bool       keyRepeat = false;
static bool       lastKeyDropped = false;
static bool       lastKeyTakesChar = false;

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)w; (void)scancode; (void)mods;
    lastKeyDropped = (action == GLFW_REPEAT && !keyRepeat) || keyEventCount >= MAX_EVENTS;
    lastKeyTakesChar = false;
    if (lastKeyDropped) return;
    keyEvents[keyEventCount].key = Keys_fromGlfw(key);
    keyEvents[keyEventCount].c = '\0';
    keyEvents[keyEventCount].state = action != GLFW_RELEASE;
    keyEventCount++;
    lastKeyTakesChar = action != GLFW_RELEASE;
}

static void charCallback(GLFWwindow* w, unsigned int codepoint) {
    (void)w;
    char c = codepoint < 128 ? (char)codepoint : '\0';
    if (lastKeyDropped) return;
    if (lastKeyTakesChar) {
        keyEvents[keyEventCount - 1].c = c;
        lastKeyTakesChar = false;
        return;
    }
    if (keyEventCount >= MAX_EVENTS) return;
    keyEvents[keyEventCount].key = 0;
    keyEvents[keyEventCount].c = c;
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

// Keyboard.isKeyDown
static bool isKeyDown(int key) {
    int g = Keys_toGlfw(key);
    return g >= 0 && glfwGetKey(window, g) == GLFW_PRESS;
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

const char* Minecraft_getUserName(void) {
    return userName;
}

bool Minecraft_hasUser(void) {
    return userName[0] != '\0';
}

bool Minecraft_isNetworked(void) {
    return networked;
}

Options* Minecraft_getOptions(void) {
    return &options;
}

Screen* Minecraft_getScreen(void) {
    return screen;
}

int Minecraft_getGuiTexture(void) {
    return texGui;
}

const char* Minecraft_hoveredUsername(void) {
    return hasHoveredUsername ? hoveredUsername : NULL;
}

/* gl errors */

static void checkGlError(const char* where) {
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) return;
    printf("########## GL ERROR ##########\n");
    printf("@ %s\n", where);
    printf("%d: %s\n", (int)err, (const char*)gluErrorString(err));
    exit(0);
}

/* screens */

void Minecraft_grabMouse(void);

// nothing replaces an error screen
void Minecraft_setScreen(Screen* s) {
    if (ErrorScreen_isOpen(screen)) return;
    if (screen && screen->closeScreen) screen->closeScreen(screen);
    screen = s;
    if (s) {
        if (mouseGrabbed) {
            Player_releaseAllKeys(&player);
            mouseGrabbed = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        int sw, sh;
        guiSize(&sw, &sh);
        Screen_setup(s, &gFont, sw, sh);
        hideGui = false;
        return;
    }
    Minecraft_grabMouse();
}

void Minecraft_showErrorScreen(const char* t, const char* message) {
    Minecraft_setScreen(ErrorScreen_new(t, message));
}

// leaves an open pause screen alone
static void pauseGame(void) {
    if (PauseScreen_isOpen(screen)) return;
    Minecraft_setScreen(PauseScreen_new());
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
    prevFrameTime = ticksRan + 10000;
}

/* inventory */

static int containsTileAt(int id) {
    for (int i = 0; i < 9; i++) {
        if (slots[i] == id) return i;
    }
    return -1;
}

// a block already in another slot swaps places with the selected one
void Minecraft_inventorySetTile(int id) {
    int i = containsTileAt(id);
    if (i >= 0) slots[i] = slots[selectedSlot];
    slots[selectedSlot] = id;
}

static void scrollHotbar(int n) {
    if (n > 0) n = 1;
    if (n < 0) n = -1;
    selectedSlot -= n;
    while (selectedSlot < 0) selectedSlot += 9;
    while (selectedSlot >= 9) selectedSlot -= 9;
}

static bool isCreativeTile(int id) {
    for (int k = 0; k < Minecraft_creativeTileCount; ++k) {
        if (Minecraft_creativeTiles[k] == id) return true;
    }
    return false;
}

const Level* Minecraft_getLevel(void) {
    return &level;
}

int Minecraft_getTerrainTexture(void) {
    return texTerrain;
}

void Minecraft_setUserType(int type) {
    userType = type;
}

/* sounds */

void Minecraft_playSoundEntity(const char* name, Entity* e, float volume, float pitch) {
    Sound_playEntity(name, e, &player.e, volume, pitch);
}

void Minecraft_playSoundAt(const char* name, float x, float y, float z, float volume, float pitch) {
    Sound_playAt(name, x, y, z, &player.e, volume, pitch);
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
    void* values[256];
    int n = JHashMap_values(&netPlayers, values, 256);
    for (int i = 0; i < n; i++) {
        if (values[i] == p) return true;
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
    level.networkMode = true;
    setLevel(true);
    hideGui = false;
    levelReceived = true;
}

void Minecraft_networkSetBlock(int x, int y, int z, int type) {
    if (hasLevel) Level_netSetTile(&level, x, y, z, type);
}

// the spawn rotation gets 320 where the move gets 360
bool Minecraft_networkSelfSpawn(int x, int y, int z, int yaw, int pitch) {
    if (!hasLevel) return false;
    Level_setSpawnPos(&level, x / 32, y / 32, z / 32, (float)(yaw * 320 / 256));
    Entity_moveTo(&player.e, (float)x / 32.0f, (float)y / 32.0f, (float)z / 32.0f,
                  (float)(yaw * 360) / 256.0f, (float)(pitch * 360) / 256.0f);
    return true;
}

void Minecraft_networkSelfTeleport(int x, int y, int z, int yaw, int pitch) {
    Entity_moveTo(&player.e, (float)x / 32.0f, (float)y / 32.0f, (float)z / 32.0f,
                  (float)(yaw * 360) / 256.0f, (float)(pitch * 360) / 256.0f);
}

// false where it throws, the spawn goes in the map before the missing level is touched
bool Minecraft_spawnNetworkPlayer(int id, const char* name, int x, int y, int z, float yRot, float xRot) {
    NetworkPlayer* p = (NetworkPlayer*)malloc(sizeof(NetworkPlayer));
    NetworkPlayer_init(p, hasLevel ? &level : NULL, id, name, x, y, z, yRot, xRot);
    NetworkPlayer* old = (NetworkPlayer*)JHashMap_put(&netPlayers, id, p);
    if (old && !inLevel(old)) freePlayer(old);
    if (!hasLevel) return false;
    Level_addPlayer(&level, p);
    return true;
}

void Minecraft_teleportNetworkPlayer(int id, int x, int y, int z, float yRot, float xRot) {
    NetworkPlayer* p = (NetworkPlayer*)JHashMap_get(&netPlayers, id);
    if (p) NetworkPlayer_teleport(p, x, y, z, yRot, xRot);
}

void Minecraft_queueNetworkPlayerMoveLook(int id, int dx, int dy, int dz, float yRot, float xRot) {
    NetworkPlayer* p = (NetworkPlayer*)JHashMap_get(&netPlayers, id);
    if (p) NetworkPlayer_queueMoveLook(p, dx, dy, dz, yRot, xRot);
}

void Minecraft_queueNetworkPlayerMove(int id, int dx, int dy, int dz) {
    NetworkPlayer* p = (NetworkPlayer*)JHashMap_get(&netPlayers, id);
    if (p) NetworkPlayer_queueMove(p, dx, dy, dz);
}

void Minecraft_queueNetworkPlayerLook(int id, float yRot, float xRot) {
    NetworkPlayer* p = (NetworkPlayer*)JHashMap_get(&netPlayers, id);
    if (p) NetworkPlayer_queueLook(p, yRot, xRot);
}

bool Minecraft_despawnNetworkPlayer(int id) {
    NetworkPlayer* p = (NetworkPlayer*)JHashMap_remove(&netPlayers, id);
    if (!p) return true;
    if (!hasLevel) {
        freePlayer(p);
        return false;
    }
    Level_removePlayer(&level, p);
    freePlayer(p);
    return true;
}

void Minecraft_addChatLine(const char* text) {
    memmove(&chatLines[1], &chatLines[0], sizeof(ChatLine) * (size_t)chatLineCount);
    snprintf(chatLines[0].text, sizeof chatLines[0].text, "%s", text);
    chatLines[0].counter = 0;
    chatLineCount++;
    if (chatLineCount > MAX_CHAT_LINES) chatLineCount = MAX_CHAT_LINES;
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
    glNormal3f(0.0f, -1.0f, 0.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
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
        glFogf(GL_FOG_END, renderDistance);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
    }
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT);
    glEnable(GL_LIGHTING);
}

// entities get one light from above and to the side
static void toggleLight(bool on) {
    if (!on) {
        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);
        return;
    }
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    float a = 0.7f;
    float d = 0.3f;
    float len = (float)sqrt(0.0f * 0.0f + -1.0f * -1.0f + 0.5f * 0.5f);
    GLfloat pos[4] = { 0.0f / len, -1.0f / len, 0.5f / len, 0.0f };
    GLfloat diffuse[4] = { d, d, d, 1.0f };
    GLfloat ambient[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat model[4] = { a, a, a, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, model);
}

/* boot and shutdown */

static int init(void) {
    // a thread started once the gl context exists makes nvidia's driver stall a sync for seconds,
    // so every thread is running before the window opens
    Sound_init(&options);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 0;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if (gIsFullscreen) {
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.0.23a_01", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.0.23a_01", NULL, NULL);
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
    // glew's lookup leaves an error behind on some drivers
    glGetError();

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);

    checkGlError("Pre startup");
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
    checkGlError("Startup");

    Tile_registerAll();
    LavaFX_init(&lavaFX, TILE_LAVA.textureId);
    lavaFX.step(&lavaFX);
    WaterFX_init(&waterFX, TILE_WATER.textureId);
    waterFX.step(&waterFX);
    for (int i = 0; i < 9; i++) slots[i] = Minecraft_creativeTiles[i];

    texTerrain = loadTexture("resources/terrain.png", GL_NEAREST);
    texGui     = loadTexture("resources/gui.png", GL_NEAREST);
    texDirt    = loadTexture("resources/dirt.png", GL_NEAREST);
    Font_init(&gFont, "resources/default.png");

    int width, height;
    getSize(&width, &height);
    glViewport(0, 0, width, height);

    Timer_init(&timer, 20.0f);

    JHashMap_init(&netPlayers);
    if (!connectHost[0]) {
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
    Options_init(&options);
    Player_init(&player, hasLevel ? &level : NULL, &options);
    Entity_resetPosition(&player.e);
    if (hasLevel) LevelRenderer_setLevel(&levelRenderer, &level);

    checkGlError("Post startup");

    if (connectHost[0]) {
        hideGui = true;
        NetConnection_open(&connection, connectHost, connectPort, userName, mppass);
        networked = true;
    }
    Timer_highResolution();
    return 1;
}

static void destroy(void) {
    Sound_shutdown();
    // whatever level is here gets saved on the way out, a server's level too
    if (hasLevel) LevelIO_save(&level, "level.dat");
    else Level_saveNull("level.dat");
    if (networked) NetConnection_close(&connection);
    LevelRenderer_destroy(&levelRenderer);
    dropLevel();
    void* values[256];
    int n = JHashMap_values(&netPlayers, values, 256);
    for (int i = 0; i < n; i++) freePlayer((NetworkPlayer*)values[i]);
    JHashMap_free(&netPlayers);
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
    gluPerspective(70.0f, (float)width / (float)height, 0.05f, renderDistance);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    moveCameraToPlayer(a);
}

/* picking */

// a ray 5 blocks out from the eye
static void pick(float a) {
    float xRot = player.e.xRotO + (player.e.xRotation - player.e.xRotO) * a;
    float yRot = player.e.yRotO + (player.e.yRotation - player.e.yRotO) * a;
    float x = player.e.prevX + (player.e.x - player.e.prevX) * a;
    float y = player.e.prevY + (player.e.y - player.e.prevY) * a;
    float z = player.e.prevZ + (player.e.z - player.e.prevZ) * a;
    float yc = (float)cos((double)(-yRot) * M_PI / 180.0 + M_PI);
    float ys = (float)sin((double)(-yRot) * M_PI / 180.0 + M_PI);
    float xc = (float)cos((double)(-xRot) * M_PI / 180.0);
    float xs = (float)sin((double)(-xRot) * M_PI / 180.0);
    ys *= xc;
    yc *= xc;
    float reach = 5.0f;
    hasHit = Level_clip(&level, x, y, z, x + ys * reach, y + xs * reach, z + yc * reach, &hitResult);
}

/* input actions */

// only air and liquids can be built into, the server hears about it first
static void clickMouse(void) {
    if (!hasHit) return;
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
        // bedrock only breaks for ops
        if (oldTile == &TILE_UNBREAKABLE && userType < 100) return;
        bool changed = Level_netSetTile(&level, x, y, z, 0);
        if (oldTile && changed) {
            if (networked) NetConnection_sendSetBlock(&connection, x, y, z, editMode, slots[selectedSlot]);
            // tiles without a sound don't break into particles either
            if (oldTile->soundType != SOUND_NONE) {
                const SoundType* st = &SOUND_TYPES[oldTile->soundType];
                char name[32];
                snprintf(name, sizeof name, "step.%s", st->name);
                float volume = (SoundType_getVolume(st) + 1.0f) / 2.0f;
                Level_playSound(&level, name, (float)x, (float)y, (float)z, volume, SoundType_getPitch(st) * 0.8f);
                Tile_onDestroy(oldTile, &level, x, y, z, &particleEngine);
            }
        }
        return;
    }
    int id = slots[selectedSlot];
    const Tile* there = gTiles[Level_getTile(&level, x, y, z)];
    if (there && there != &TILE_WATER && there != &TILE_CALM_WATER && there != &TILE_LAVA && there != &TILE_CALM_LAVA) return;
    const Tile* tile = gTiles[id];
    AABB aabb;
    if (tile->getAABB(tile, x, y, z, &aabb)) {
        if (AABB_intersects(&player.e.boundingBox, &aabb) || !Level_isFree(&level, &aabb)) return;
    }
    if (networked) NetConnection_sendSetBlock(&connection, x, y, z, editMode, id);
    Level_netSetTile(&level, x, y, z, slots[selectedSlot]);
    tile->onPlace(tile, &level, x, y, z);
}

static void screenMouseEvent(Screen* s, const InputEvent* ev) {
    if (!ev->state || ev->key < 0) return;
    int width, height;
    getSize(&width, &height);
    int ex, ey;
    lwjglMouse(ev->x, ev->y, &ex, &ey);
    int xm = ex * s->width / width;
    int ym = s->height - ey * s->height / height - 1;
    s->mousePressed(s, xm, ym, ev->key);
}

// Screen.updateEvents, the screen keeps draining even if a click replaces it
static void screenUpdateEvents(Screen* s) {
    for (int e = 0; e < mouseEventCount; e++) screenMouseEvent(s, &mouseEvents[e]);
    mouseEventCount = 0;
    int e = 0;
    while (e < keyEventCount && !broke) {
        InputEvent ev = keyEvents[e++];
        if (ev.state) s->keyPressed(s, ev.c, ev.key);
    }
    // what's left after a throw waits for the next tick, which the error screen drains
    memmove(keyEvents, keyEvents + e, sizeof(InputEvent) * (size_t)(keyEventCount - e));
    keyEventCount -= e;
}

// a throw in the middle of the input loop leaves the rest of the events queued
static void dropMouseEvents(int done) {
    memmove(mouseEvents, mouseEvents + done, sizeof(InputEvent) * (size_t)(mouseEventCount - done));
    mouseEventCount -= done;
}

static void dropKeyEvents(int done) {
    memmove(keyEvents, keyEvents + done, sizeof(InputEvent) * (size_t)(keyEventCount - done));
    keyEventCount -= done;
}

static void tick(void) {
    Sound_tick();

    for (int i = 0; i < chatLineCount; i++) chatLines[i].counter++;

    glBindTexture(GL_TEXTURE_2D, texTerrain);
    for (int i = 0; i < 2; i++) {
        TextureFX* fx = textureFX[i];
        fx->step(fx);
        glTexSubImage2D(GL_TEXTURE_2D, 0, fx->tex % 16 << 4, fx->tex / 16 << 4, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, fx->pixels);
    }

    if (networked && !ErrorScreen_isOpen(screen)) {
        if (!NetConnection_isOpen(&connection)) {
            Minecraft_beginLevelLoading("Connecting..");
            Minecraft_levelLoadProgress(0);
        } else {
            if (connection.loggedIn && connection.connected && NetConnection_tick(&connection) == NET_THROW) {
                Minecraft_showErrorScreen("Disconnected!", "You've lost connection to the server");
                hideGui = false;
                NetConnection_close(&connection);
                networked = false;
                // the position goes through the connection that was just dropped
                clientError();
                return;
            }
            if (levelReceived) NetConnection_sendPosition(&connection, player.e.x, player.e.y, player.e.z, player.e.yRotation, player.e.xRotation);
        }
        // the connect finishes here, after the tick has drawn "Connecting.."
        // a connect left running alongside the loop stalls some drivers for seconds
        if (NetConnection_finishConnect(&connection) < 0) {
            hideGui = false;
            networked = false;
            Minecraft_showErrorScreen("Failed to connect", "You failed to connect to the server. It's probably down!");
        }
    }

    if (!screen || screen->allowUserInput) {
        for (int e = 0; e < mouseEventCount; e++) {
            const InputEvent* ev = &mouseEvents[e];
            if (ev->wheel != 0) scrollHotbar(ev->wheel);
            if (!screen) {
                if (!mouseGrabbed && ev->state) {
                    Minecraft_grabMouse();
                } else {
                    if (ev->key == GLFW_MOUSE_BUTTON_LEFT && ev->state) {
                        clickMouse();
                        if (broke) {
                            dropMouseEvents(e + 1);
                            return;
                        }
                        prevFrameTime = ticksRan;
                    }
                    if (ev->key == GLFW_MOUSE_BUTTON_RIGHT && ev->state) editMode = (editMode + 1) % 2;
                    if (ev->key == GLFW_MOUSE_BUTTON_MIDDLE && ev->state && hasHit) {
                        if (!hasLevel) {
                            clientError();
                            dropMouseEvents(e + 1);
                            return;
                        }
                        // a block in the slots gets selected, one from the list goes in the selected slot
                        int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
                        if (id == TILE_GRASS.id) id = TILE_DIRT.id;
                        int slot = containsTileAt(id);
                        if (slot >= 0) selectedSlot = slot;
                        else if (id > 0 && isCreativeTile(id)) Minecraft_inventorySetTile(id);
                    }
                }
            }
            if (screen) screenMouseEvent(screen, ev);
        }
        mouseEventCount = 0;

        // a key can open a screen, the rest of the queue goes to it then
        for (int e = 0; e < keyEventCount; e++) {
            const InputEvent ev = keyEvents[e];
            Player_setKey(&player, ev.key, ev.state);
            if (!ev.state) continue;
            if (screen) {
                screen->keyPressed(screen, ev.c, ev.key);
                if (broke) {
                    dropKeyEvents(e + 1);
                    return;
                }
            }
            if (!screen) {
                if (ev.key == KEY_ESCAPE) pauseGame();
                if (ev.key == options.load.key) Entity_resetPosition(&player.e);
                if (ev.key == options.save.key) {
                    if (!hasLevel) {
                        clientError();
                        dropKeyEvents(e + 1);
                        return;
                    }
                    Level_setSpawnPos(&level, (int)player.e.x, (int)player.e.y, (int)player.e.z, player.e.yRotation);
                    Entity_resetPosition(&player.e);
                }
                if (ev.key == KEY_G && !networked) {
                    if (!hasLevel) {
                        clientError();
                        dropKeyEvents(e + 1);
                        return;
                    }
                    if (level.entityCount + level.playerCount < 256) {
                        Zombie* z = (Zombie*)malloc(sizeof(Zombie));
                        Zombie_init(z, &level, player.e.x, player.e.y, player.e.z);
                        Level_addEntity(&level, z);
                    }
                }
                if (ev.key == options.build.key) Minecraft_setScreen(InventoryScreen_new());
                if (ev.key == options.chat.key && networked && NetConnection_isOpen(&connection)) {
                    Player_releaseAllKeys(&player);
                    Minecraft_setScreen(ChatScreen_new());
                }
            }
            for (int k = 0; k < 9; ++k) {
                if (ev.key == KEY_1 + k) selectedSlot = k;
            }
            if (ev.key == options.toggleFog.key) {
                Options_setOption(&options, 4, isKeyDown(KEY_LSHIFT) || isKeyDown(KEY_RSHIFT) ? -1 : 1);
            }
        }
        keyEventCount = 0;

        // holding the left button repeats every timer.ticksPerSecond / 4 ticks
        if (!screen && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
            (float)(ticksRan - prevFrameTime) >= timer.ticksPerSecond / 4.0f && mouseGrabbed) {
            clickMouse();
            if (broke) return;
            prevFrameTime = ticksRan;
        }
    }

    if (screen) prevFrameTime = ticksRan + 10000;

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

static void initGui(void) {
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

// InGameHud.render, a hovered tab list name only counts while a screen is open
static void renderHud(bool screenOpen, int xm, int ym) {
    int sw, sh;
    guiSize(&sw, &sh);
    initGui();

    glBindTexture(GL_TEXTURE_2D, texGui);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_BLEND);
    Screen_blit(sw / 2 - 91, sh - 22, 0, 0, 182, 22, -90.0f);
    Screen_blit(sw / 2 - 91 - 1 + selectedSlot * 20, sh - 22 - 1, 0, 22, 24, 22, -90.0f);
    glDisable(GL_BLEND);

    for (int k = 0; k < 9; ++k) {
        if (slots[k] <= 0) continue;
        const Tile* tile = gTiles[slots[k]];
        glPushMatrix();
        glTranslatef((float)(sw / 2 - 90 + k * 20), (float)(sh - 16), -50.0f);
        glScalef(10.0f, 10.0f, 10.0f);
        glTranslatef(1.0f, 0.5f, 0.0f);
        glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(-1.5f, 0.5f, 0.5f);
        glScalef(-1.0f, -1.0f, -1.0f);
        glBindTexture(GL_TEXTURE_2D, texTerrain);
        glEnable(GL_TEXTURE_2D);
        Tessellator_begin(&hudTess);
        tile->render(tile, &hudTess, &level, 0, -2, 0, 0);
        Tessellator_end(&hudTess);
        glDisable(GL_TEXTURE_2D);
        glPopMatrix();
    }

    Font_drawShadow(&gFont, &hudTess, "0.0.23a_01", 2, 2, 0xFFFFFF);
    if (options.showFPS) Font_drawShadow(&gFont, &hudTess, fpsString, 2, 12, 0xFFFFFF);
    // 10 lines for 10 seconds each, the chat screen shows 20 of any age
    int maxLines = 10;
    bool chatOpen = ChatScreen_isOpen(screen);
    if (chatOpen) maxLines = 20;
    for (int i = 0; i < chatLineCount && i < maxLines; i++) {
        if (chatLines[i].counter >= 200 && !chatOpen) continue;
        Font_drawShadow(&gFont, &hudTess, chatLines[i].text, 2, sh - 8 - i * 9 - 20, 0xFFFFFF);
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

    // tab lists the players in HashMap order after our own name
    hasHoveredUsername = false;
    if (isKeyDown(KEY_TAB) && networked && NetConnection_isOpen(&connection)) {
        void* values[256];
        int n = JHashMap_values(&netPlayers, values, 256);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
        glVertex2f((float)(wc + 128), (float)(hc - 68 - 12));
        glVertex2f((float)(wc - 128), (float)(hc - 68 - 12));
        glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
        glVertex2f((float)(wc - 128), (float)(hc + 68));
        glVertex2f((float)(wc + 128), (float)(hc + 68));
        glEnd();
        glDisable(GL_BLEND);
        const char* t = "Connected players:";
        Font_drawShadow(&gFont, &hudTess, t, wc - Font_width(&gFont, t) / 2, hc - 64 - 12, 0xFFFFFF);
        for (int i = 0; i <= n; i++) {
            const char* name = i == 0 ? userName : ((NetworkPlayer*)values[i - 1])->name;
            int x = wc + i % 2 * 120 - 120;
            int y = hc - 64 + (i / 2 << 3);
            if (screenOpen && xm >= x && ym >= y && xm < x + 120 && ym < y + 8) {
                snprintf(hoveredUsername, sizeof hoveredUsername, "%s", name);
                hasHoveredUsername = true;
                Font_draw(&gFont, &hudTess, name, x + 2, y, 0xFFFFFF);
                continue;
            }
            Font_draw(&gFont, &hudTess, name, x, y, 0xEEEEEE);
        }
    }
}

static void renderLevel(float a, int width, int height, int xm, int ym) {
    glViewport(0, 0, width, height);

    // the sky fades to white as the render distance shrinks
    float f = 1.0f / (float)(4 - options.renderDistance);
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

    pick(a);

    renderDistance = (float)(512 >> (options.renderDistance << 1));
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
    toggleLight(true);
    LevelRenderer_renderEntities(&levelRenderer, &frustum, a);
    toggleLight(false);
    setupFog();
    ParticleEngine_render(&particleEngine, &player, a);
    LevelRenderer_renderSurroundingGround(&levelRenderer);
    glDisable(GL_LIGHTING);
    // setupFog turns the lighting back on, so the clouds are lit
    setupFog();
    LevelRenderer_renderClouds(&levelRenderer, a);
    setupFog();
    glEnable(GL_LIGHTING);

    if (hasHit) {
        glDisable(GL_LIGHTING);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &player, &hitResult, editMode, slots[selectedSlot]);
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
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);

    if (hasHit) {
        glDepthFunc(GL_LESS);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &player, &hitResult, editMode, slots[selectedSlot]);
        LevelRenderer_renderHitOutline(&hitResult, editMode);
        glEnable(GL_ALPHA_TEST);
        glDepthFunc(GL_LEQUAL);
    }

    renderHud(screen != NULL, xm, ym);
}

// false where it throws, the frame isn't shown then
static bool render(float a) {
    bool active = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
    if (displayActive && !active) pauseGame();
    displayActive = active;

    if (mouseGrabbed) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        int ww, wh; glfwGetWindowSize(window, &ww, &wh);
        double cx = ww * 0.5, cy = wh * 0.5;

        // mouse deltas are whole pixels and y goes up
        int dx = (int)(mx - cx);
        int dy = (int)(cy - my);
        Player_turn(&player, (float)dx, (float)(dy * (options.invertMouse ? -1 : 1)));
        glfwSetCursorPos(window, cx, cy);
    }

    if (hideGui) return true;

    int width, height;
    getSize(&width, &height);
    int sw, sh;
    guiSize(&sw, &sh);
    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);
    int mx, my;
    lwjglMouse(cx, cy, &mx, &my);
    int xm = mx * sw / width;
    int ym = sh - my * sh / height - 1;

    if (hasLevel) {
        renderLevel(a, width, height, xm, ym);
    } else {
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        initGui();
    }

    if (screen) screen->render(screen, xm, ym);

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
        // connecting needs a user, it's guest unless one is given
        snprintf(userName, sizeof userName, "%s", argc >= 4 ? argv[3] : "guest");
        if (argc >= 5) snprintf(mppass, sizeof mppass, "%s", argv[4]);
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

        if (!broke) {
            checkGlError("Pre render");
            if (render(timer.partialTicks)) {
                Timer_sleepMillis(5);
                checkGlError("Post render");
                frames++;
            }
        }

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
