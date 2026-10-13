// minecraft.c: window, main loop, input, camera, picking and the network glue

// must come before anything that pulls in windows.h, glew does
#include "net/connection.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include <GL/glew.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include "common.h"
#include "math_helper.h"

#include "level/level.h"
#include "level/level_renderer.h"
#include "renderer/frustum.h"
#include "level/tile/tile.h"
#include "renderer/textures.h"
#include "renderer/texture_fx.h"
#include "particle/particle_engine.h"
#include "player.h"
#include "options.h"
#include "audio/sound.h"
#include "user.h"
#include "gui/block_select_screen.h"
#include "level/levelgen/level_gen.h"
#include "character/creature.h"
#include "character/cube.h"
#include "item/arrow.h"
#include "item/item.h"
#include "timer.h"
#include "hitresult.h"
#include "gui/font.h"
#include "gui/screen.h"
#include "gui/pause_screen.h"
#include "gui/chat_input_screen.h"
#include "gui/message_screen.h"
#include "gui/game_over_screen.h"
#include "net/network_player.h"
#include "save.h"

#define MAX_MOBS 4096 // the in play cap is 320 on a normal map and 1280 on a huge one
#define MAX_NET_PLAYERS 256
#define MAX_ARROWS 64
#define MAX_ITEMS 256

static GLFWwindow*    window;
static Level          level;
static LevelRenderer  levelRenderer;
static Player         player;
static Timer          timer;
static Creature       mobs[MAX_MOBS];
static Arrow          arrows[MAX_ARROWS];
static Item           items[MAX_ITEMS];
static ParticleEngine particleEngine;
static User           user;

static int mobCount = 0;
static int arrowCount = 0;
static int itemCount = 0;
// F5, offline only
static bool gRaining = false;

static World currentWorld(void) {
    return (World){ &level, &player, mobs, &mobCount, MAX_MOBS, items, &itemCount, MAX_ITEMS,
                    arrows, &arrowCount, MAX_ARROWS };
}

// the first person hand, heldBlend fades the held block out and back in when it changes
// handSwingTicks counts the 7 tick punch, every left click starts one
static int   heldTileId = -1;
static float heldBlend = 0.0f, prevHeldBlend = 0.0f;
static bool  handSwinging = false;
static int   handSwingTicks = -1;
static Cube  handArmCube;
static bool  handArmCubeBuilt = false;

static NetworkPlayer netPlayers[MAX_NET_PLAYERS];
// the slots of the network players in the level, in the order they were added
static int netOrder[MAX_NET_PLAYERS];
static int netPlayerCount = 0;

// gameplay mouse and key events are queued by the glfw callbacks and drained in tick
#define EVENT_KEY   0
#define EVENT_MOUSE 1
#define EVENT_WHEEL 2
typedef struct { int kind; int code; bool pressed; } InputEvent;
static InputEvent inputQueue[256];
static int inputQueueCount = 0;

static void queueInput(int kind, int code, bool pressed) {
    if (inputQueueCount < 256) inputQueue[inputQueueCount++] = (InputEvent){ kind, code, pressed };
}

static int gTickCount = 0;
// the tick of the last click, drives the quarter second click repeat
static int gLastClickTick = 0;

// SurvivalGameMode state
static int curBlockX = -1, curBlockY = -1, curBlockZ = -1;
static int curBlockDamage = 0;
static int blockHardness = 0;
static int blockHitWait = 0;

static Options gOptions;

#define CHAT_BACKLOG_MAX 50
typedef struct {
    char text[96];
    int  age;
} ChatLine;
static ChatLine chatLines[CHAT_BACKLOG_MAX];
static int chatLineCount = 0;

static bool gConnected  = false;       // set once the Login packet went out
static bool gMultiplayer = false;      // Minecraft.connectionManager != null
static bool gTickBroke = false;
static bool gConnecting = false;       // the connect thread is running
static bool gLoading    = false;       // from the connect until LevelFinalize
static NetConnection gConn;
static char gConnectHost[256] = "";
static int  gConnectPort = 25565;
static char gConnectUsername[64] = ""; // empty falls back to "guest"

static int texTerrain = 0;
static int texDirt = 0;
static int texRock = 0;
static int texWater = 0;
static int texGui = 0;
static int texIcons = 0;

static LavaTextureFX  gLavaFX;
static WaterTextureFX gWaterFX;
static TextureFX* gTextureFX[2];
static char gLoadTitle[64];

static Tessellator hudTess;
static Font gFont;
static char gFpsText[64] = "";         // starts as an empty line
static int gHudTicks = 0;
static JRandom gHudRandom;
static int gChunkUpdatesAccum = 0;

static int gWinWidth  = 854;
static int gWinHeight = 480;
static int gIsFullscreen = 0;

// the name under the mouse in the Tab list, the chat screen pastes it on click
static char gHoveredTabName[65] = "";

static float fogRed, fogGreen, fogBlue;

static int      isHitNull = 1;
static HitResult hitResult;

static PauseScreen gPauseScreen;
static GameOverScreen gGameOverScreen;
static Screen*     activeScreen = NULL; // not null means a menu is open
static int prevScreenClick = GLFW_RELEASE;

static void tick(Player* player, GLFWwindow* window);
static void computeGuiMouse(int* xm, int* ym);
void Minecraft_setScreen(Screen* screen);

/* input and GL state helpers */

// Minecraft.releaseMouse, clears the keys and opens the pause menu
static void releaseMouseAndOpenPause(void) {
    if (activeScreen) return;

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;
    PauseScreen_init(&gPauseScreen, &gFont, screenWidth, screenHeight, &gOptions);
    Minecraft_setScreen((Screen*)&gPauseScreen);
}

// Minecraft.grabMouse
void Minecraft_closeScreenAndGrabMouse(void) {
    // setScreen(null) puts the Game over screen back while the player is dead
    if (player.e.health <= 0) {
        Minecraft_setScreen(NULL);
        return;
    }
    activeScreen = NULL;
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
}

void Minecraft_levelLoadUpdate(const char* status);
int Minecraft_countMobs(void);

// the jar always builds a CreativeGameMode, survival is only there by editing the code
static const bool gSurvival = false;

// survival spawns the first mobs of a new level
static void GameMode_spawnInitial(Level* lvl) {
    if (!gSurvival) return;
    Minecraft_levelLoadUpdate("Spawning..");
    MobSpawner_spawn(lvl, lvl->width * lvl->height * lvl->depth / 800, NULL, true);
}

// creative takes everything but players out of the grid (not out of the level)
static void GameMode_setLevel(Level* lvl) {
    lvl->creativeMode = !gSurvival;
    if (gSurvival) return;
    for (int i = 0; i < mobCount; ++i) mobs[i].e.inGrid = false;
    for (int i = 0; i < arrowCount; ++i) arrows[i].e.inGrid = false;
    for (int i = 0; i < itemCount; ++i) items[i].e.inGrid = false;
}

// only for a player the level didn't already have
static void GameMode_newPlayer(Player* p) {
    if (!gSurvival) return;
    p->inventory.id[8] = TILE_TNT.id;
    p->inventory.count[8] = 10;
}

// creative fills the empty hotbar slots with the first blocks of the list
static void GameMode_adjustPlayer(Player* p) {
    if (gSurvival) return;
    for (int i = 0; i < 9; ++i) {
        p->inventory.count[i] = 1;
        if (p->inventory.id[i] > 0) continue;
        p->inventory.id[i] = USER_CREATIVE_TILES[i];
    }
}

// survival tops the mobs up every tick
static void GameMode_tick(void) {
    if (!gSurvival) return;
    int n = level.width * level.height * level.depth / 64 / 64 / 64;
    if (JRandom_nextIntN(&level.random, 100) < n && Minecraft_countMobs() < n * 20) {
        MobSpawner_spawn(&level, n, &player.e, false);
    }
}

void Minecraft_generateNewLevelSized(int sizePreset) {
    int size = 128 << sizePreset;

    Sound_detachAll();
    mobCount = 0;
    arrowCount = 0;
    itemCount = 0;

    LevelRenderer_destroy(&levelRenderer);
    Level_resize(&level, size, size, 64);
    GameMode_spawnInitial(&level);
    GameMode_setLevel(&level);
    LevelRenderer_init(&levelRenderer, &level, texTerrain);
    levelRenderer.drawDistance = gOptions.viewDistance;

    // a new player, and the particles are cleared
    memset(&player, 0, sizeof player);
    Player_init(&player, &level);
    GameMode_newPlayer(&player);
    GameMode_adjustPlayer(&player);
    ParticleEngine_clear(&particleEngine);
}

// Minecraft.setScreen, any screen frees the mouse
void Minecraft_setScreen(Screen* screen) {
    // closing every screen while dead brings the Game over screen back
    if (!screen && player.e.health <= 0) {
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        int screenWidth  = fbw * 240 / fbh;
        int screenHeight = 240;
        GameOverScreen_init(&gGameOverScreen, &gFont, screenWidth, screenHeight, &player.score);
        screen = (Screen*)&gGameOverScreen;
    }
    if (!screen) {
        Minecraft_closeScreenAndGrabMouse();
        return;
    }
    activeScreen = screen;
    Player_releaseAllKeys(&player);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

// no check for a connection here
void Minecraft_sendChat(const char* text) {
    if (!gConnected) return;
    NetConnection_sendMessage(&gConn, text);
}

// connect failures and disconnects
void Minecraft_openMessageScreen(const char* title, const char* message) {
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;
    MessageScreen_open(&gFont, screenWidth, screenHeight, title, message);
    gConnected = false;
    gLoading = false;
}

// ConnectionManager.players, a HashMap<Byte, NetworkPlayer>. its iteration
// order is the tab list order
static int playerMapCap = 16, playerMapSize = 0;
static int playerMapHead[256];
static int playerMapNext[128];
static int playerMapSlot[128];

static void PlayerMap_clear(void) {
    playerMapCap = 16;
    playerMapSize = 0;
    for (int i = 0; i < 256; ++i) playerMapHead[i] = -1;
    for (int i = 0; i < 128; ++i) playerMapSlot[i] = -1;
}

static int javaHash(int h) {
    unsigned int u = (unsigned int)h;
    u ^= (u >> 20) ^ (u >> 12);
    return (int)(u ^ (u >> 7) ^ (u >> 4));
}

// new keys go to the head of their bucket, a resize walks the old buckets in order
static void PlayerMap_put(int key, int slot) {
    if (playerMapSlot[key] >= 0) {
        playerMapSlot[key] = slot;
        return;
    }
    int b = javaHash(key) & (playerMapCap - 1);
    playerMapNext[key] = playerMapHead[b];
    playerMapHead[b] = key;
    playerMapSlot[key] = slot;
    if (playerMapSize++ >= playerMapCap * 3 / 4) {
        int cap = playerMapCap * 2;
        int head[256];
        for (int i = 0; i < 256; ++i) head[i] = -1;
        for (int j = 0; j < playerMapCap; ++j) {
            for (int k = playerMapHead[j]; k >= 0; ) {
                int next = playerMapNext[k];
                int nb = javaHash(k) & (cap - 1);
                playerMapNext[k] = head[nb];
                head[nb] = k;
                k = next;
            }
        }
        memcpy(playerMapHead, head, sizeof head);
        playerMapCap = cap;
    }
}

static int PlayerMap_remove(int key) {
    int slot = playerMapSlot[key];
    if (slot < 0) return -1;
    int* link = &playerMapHead[javaHash(key) & (playerMapCap - 1)];
    while (*link != key) link = &playerMapNext[*link];
    *link = playerMapNext[key];
    playerMapSize--;
    playerMapSlot[key] = -1;
    return slot;
}

static NetworkPlayer* findNetworkPlayer(int id) {
    if (id < 0 || id > 127 || playerMapSlot[id] < 0) return NULL;
    return &netPlayers[playerMapSlot[id]];
}

// a slot is free once neither the map nor the level holds its player
static void releaseNetPlayer(int slot) {
    if (netPlayers[slot].mapped || netPlayers[slot].inLevel) return;
    NetworkPlayer_free(&netPlayers[slot]);
}

static void removeNetPlayerFromLevel(int slot) {
    netPlayers[slot].inLevel = false;
    for (int i = 0; i < netPlayerCount; ++i) {
        if (netOrder[i] != slot) continue;
        memmove(&netOrder[i], &netOrder[i + 1], (size_t)(netPlayerCount - i - 1) * sizeof(int));
        netPlayerCount--;
        break;
    }
    releaseNetPlayer(slot);
}

void Minecraft_dropConnectionManager(void) {
    gMultiplayer = false;
}

// the lost connection handler sets connectionManager to null and then reads it
// again, so run() catches a NullPointerException and swaps in its own error screen
void Minecraft_connectionBroke(void) {
    Minecraft_openMessageScreen("Client error", "The game broke! [java.lang.NullPointerException]");
    gMultiplayer = false;
    gTickBroke = true;
}

// LevelFinalize, the spawn comes later in a SpawnPlayer packet
void Minecraft_installNetworkLevel(int w, int h, int d, const byte* blocks) {
    LevelRenderer_destroy(&levelRenderer);
    Level_setDataFromNetwork(&level, w, h, d, blocks);
    LevelRenderer_init(&levelRenderer, &level, texTerrain);
    levelRenderer.drawDistance = gOptions.viewDistance;
    Sound_detachAll();
    mobCount = 0;
    arrowCount = 0;
    itemCount = 0;
    // the new level has none of the network players, the map still does
    while (netPlayerCount > 0) removeNetPlayerFromLevel(netOrder[0]);
    GameMode_setLevel(&level);
    // the new level has no Player, so it's a new one, userType back to 0
    memset(&player, 0, sizeof player);
    Player_init(&player, &level);
    GameMode_newPlayer(&player);
    GameMode_adjustPlayer(&player);
    ParticleEngine_clear(&particleEngine);
    gLoading = false;
}

// SetBlock from the server, the only thing that changes a network level
void Minecraft_networkSetBlock(int x, int y, int z, int type) {
    Level_netSetTile(&level, x, y, z, type);
}

void Minecraft_setUserType(int userType) {
    player.userType = userType;
}

// SpawnPlayer with id < 0, our own player
void Minecraft_networkTeleportSelf(float x, float y, float z, float yaw, float pitch) {
    Entity_setPosition(&player.e, x, y, z);
    player.e.yRotation = yaw;
    player.e.xRotation = pitch;
}

void Minecraft_setLevelSpawnPos(int x, int y, int z, float rot) {
    Level_setSpawnPos(&level, x, y, z, rot);
}

// SpawnPlayer with id >= 0, a HashMap put, so an id already in use
// just loses its mapping and the old player stays in the level
void Minecraft_spawnNetworkPlayer(int id, const char* name, int xRaw, int yRaw, int zRaw, float yaw, float pitch) {
    if (id < 0 || id > 127) return;
    int slot = -1;
    for (int i = 0; i < MAX_NET_PLAYERS; ++i) {
        if (!netPlayers[i].mapped && !netPlayers[i].inLevel) { slot = i; break; }
    }
    if (slot < 0) return;
    NetworkPlayer_init(&netPlayers[slot], &level, id, name, xRaw, yRaw, zRaw, yaw, pitch);
    int old = playerMapSlot[id];
    PlayerMap_put(id, slot);
    if (old >= 0) {
        netPlayers[old].mapped = false;
        releaseNetPlayer(old);
    }
    netPlayers[slot].mapped = true;
    netPlayers[slot].inLevel = true;
    netOrder[netPlayerCount++] = slot;
}

void Minecraft_teleportNetworkPlayer(int id, int xRaw, int yRaw, int zRaw, float yaw, float pitch) {
    NetworkPlayer* np = findNetworkPlayer(id);
    if (np) NetworkPlayer_teleport(np, xRaw, yRaw, zRaw, yaw, pitch);
}

void Minecraft_queueNetworkPlayerMoveLook(int id, int dx, int dy, int dz, float yaw, float pitch) {
    NetworkPlayer* np = findNetworkPlayer(id);
    if (np) NetworkPlayer_queueMoveLook(np, dx, dy, dz, yaw, pitch);
}

void Minecraft_queueNetworkPlayerMove(int id, int dx, int dy, int dz) {
    NetworkPlayer* np = findNetworkPlayer(id);
    if (np) NetworkPlayer_queueMove(np, dx, dy, dz);
}

void Minecraft_queueNetworkPlayerLook(int id, float yaw, float pitch) {
    NetworkPlayer* np = findNetworkPlayer(id);
    if (np) NetworkPlayer_queueLook(np, yaw, pitch);
}

// DespawnPlayer, out of the map and out of the level
void Minecraft_despawnNetworkPlayer(int id) {
    if (id < 0 || id > 127) return;
    int slot = PlayerMap_remove(id);
    if (slot < 0) return;
    netPlayers[slot].mapped = false;
    if (netPlayers[slot].inLevel) removeNetPlayerFromLevel(slot);
    else releaseNetPlayer(slot);
}

// Minecraft.addChatMessage, the newest line goes first
void Minecraft_addChatLine(const char* text) {
    if (chatLineCount == CHAT_BACKLOG_MAX) chatLineCount--;
    memmove(&chatLines[1], &chatLines[0], sizeof(ChatLine) * chatLineCount);
    chatLineCount++;
    ChatLine* line = &chatLines[0];
    snprintf(line->text, sizeof line->text, "%s", text);
    line->age = 0;
}

void Minecraft_beginLevelLoading(const char* title) {
    int i = 0;
    for (; title[i] != '\0' && i < 63; ++i) gLoadTitle[i] = title[i];
    gLoadTitle[i] = '\0';

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;

    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, screenWidth, screenHeight, 0.0, 100.0, 300.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);
}

static char gLoadStatus[64] = "";

// the dirt background, the title, the status and the bar when percent >= 0
static void Minecraft_drawLoadingScreen(int percent) {
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, texDirt);

    Tessellator_begin(&hudTess);
    Tessellator_color(&hudTess, 0.2509804f, 0.2509804f, 0.2509804f); // 0x404040
    float s = 32.0f;
    Tessellator_vertexUV(&hudTess, 0.0f, (float)screenHeight, 0.0f, 0.0f, screenHeight / s);
    Tessellator_vertexUV(&hudTess, (float)screenWidth, (float)screenHeight, 0.0f, screenWidth / s, screenHeight / s);
    Tessellator_vertexUV(&hudTess, (float)screenWidth, 0.0f, 0.0f, screenWidth / s, 0.0f);
    Tessellator_vertexUV(&hudTess, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    Tessellator_end(&hudTess);

    if (percent >= 0) {
        glDisable(GL_TEXTURE_2D);
        int m = screenWidth / 2 - 50;
        int n = screenHeight / 2 + 16;

        Tessellator_begin(&hudTess);
        Tessellator_color(&hudTess, 0.5019608f, 0.5019608f, 0.5019608f); // 0x808080 track
        Tessellator_vertex(&hudTess, (float)m,         (float)n,       0.0f);
        Tessellator_vertex(&hudTess, (float)m,         (float)(n + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(m + 100), (float)(n + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(m + 100), (float)n,       0.0f);
        Tessellator_color(&hudTess, 0.5019608f, 1.0f, 0.5019608f); // 0x80FF80 fill
        Tessellator_vertex(&hudTess, (float)m,             (float)n,       0.0f);
        Tessellator_vertex(&hudTess, (float)m,             (float)(n + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(m + percent), (float)(n + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(m + percent), (float)n,       0.0f);
        Tessellator_end(&hudTess);
        glEnable(GL_TEXTURE_2D);
    }

    int titleWidth  = Font_width(&gFont, gLoadTitle);
    int statusWidth = Font_width(&gFont, gLoadStatus);
    Font_drawShadow(&gFont, &hudTess, gLoadTitle, (screenWidth - titleWidth) / 2, screenHeight / 2 - 4 - 16, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, gLoadStatus, (screenWidth - statusWidth) / 2, screenHeight / 2 - 4 + 8, 0xFFFFFF);

    glfwSwapBuffers(window);
    // glfw needs a poll to keep the window responding
    glfwPollEvents();
}

void Minecraft_levelLoadUpdate(const char* status) {
    int i = 0;
    for (; status[i] != '\0' && i < 63; ++i) gLoadStatus[i] = status[i];
    gLoadStatus[i] = '\0';
    Minecraft_drawLoadingScreen(-1);
}

void Minecraft_levelLoadProgress(int percent) {
    Minecraft_drawLoadingScreen(percent);
}

int Minecraft_getTerrainTexture(void) {
    return texTerrain;
}

const char* Minecraft_getUserName(void) {
    return user.name;
}

bool Minecraft_isMultiplayer(void) {
    return gMultiplayer;
}

int Minecraft_getGuiTexture(void) {
    return texGui;
}

const char* Minecraft_getHoveredTabName(void) {
    return gHoveredTabName[0] ? gHoveredTabName : NULL;
}

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)w; (void)scancode; (void)mods;

    // keys go to the open screen, only the chat screen gets key repeat
    if (activeScreen && !activeScreen->allowUserInput) {
        if (action == GLFW_REPEAT && !ChatInputScreen_isThis(activeScreen)) return;
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
        if (activeScreen->keyPressed) activeScreen->keyPressed(activeScreen, 0, key);
        return;
    }

    if (action == GLFW_PRESS || action == GLFW_RELEASE) queueInput(EVENT_KEY, key, action == GLFW_PRESS);
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
    (void)w; (void)mods;
    if (activeScreen && !activeScreen->allowUserInput) return;
    if (action == GLFW_PRESS || action == GLFW_RELEASE) queueInput(EVENT_MOUSE, button, action == GLFW_PRESS);
}

// glfw gives typed characters separately, they reach the screen with key 0
static void charCallback(GLFWwindow* w, unsigned int codepoint) {
    (void)w;
    if (!activeScreen || !activeScreen->keyPressed) return;
    if (codepoint < 128) activeScreen->keyPressed(activeScreen, (char)codepoint, 0);
}

static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset) {
    (void)w; (void)xoffset;
    if (activeScreen) return;

    int dir = (yoffset > 0.0) ? 1 : (yoffset < 0.0) ? -1 : 0;
    if (dir == 0) return;

    queueInput(EVENT_WHEEL, dir, true);
}

static const Tile* tileAtEyes(void) {
    return gTiles[Level_getTile(&level, (int)player.e.x, (int)(player.e.y + 0.12f), (int)player.e.z)];
}

static void anaglyphColor(float* r, float* g, float* b) {
    float nr = (*r * 30.0f + *g * 59.0f + *b * 11.0f) / 100.0f;
    float ng = (*r * 30.0f + *g * 70.0f) / 100.0f;
    float nb = (*r * 30.0f + *b * 70.0f) / 100.0f;
    *r = nr; *g = ng; *b = nb;
}

static void updateFogColor(void) {
    float f = 1.0f / (float)(4 - gOptions.viewDistance);
    f = 1.0f - (float)pow(f, 0.25);
    float sr = (float)(level.skyColor >> 16 & 0xFF) / 255.0f;
    float sg = (float)(level.skyColor >> 8 & 0xFF) / 255.0f;
    float sb = (float)(level.skyColor & 0xFF) / 255.0f;
    fogRed = (float)(level.fogColor >> 16 & 0xFF) / 255.0f;
    fogGreen = (float)(level.fogColor >> 8 & 0xFF) / 255.0f;
    fogBlue = (float)(level.fogColor & 0xFF) / 255.0f;
    fogRed += (sr - fogRed) * f;
    fogGreen += (sg - fogGreen) * f;
    fogBlue += (sb - fogBlue) * f;
    const Tile* t = tileAtEyes();
    if (t && t->liquidType == LIQUID_WATER) {
        fogRed = 0.02f; fogGreen = 0.02f; fogBlue = 0.2f;
    } else if (t && t->liquidType == LIQUID_LAVA) {
        fogRed = 0.6f; fogGreen = 0.1f; fogBlue = 0.0f;
    }
    if (gOptions.anaglyph3d) anaglyphColor(&fogRed, &fogGreen, &fogBlue);
}

static void setupFog(void) {
    GLfloat color[4] = { fogRed, fogGreen, fogBlue, 1.0f };
    glFogfv(GL_FOG_COLOR, color);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    const Tile* t = tileAtEyes();
    if (t && t->liquidType != LIQUID_NONE) {
        GLfloat ambient[4] = { 0.4f, 0.3f, 0.3f, 1.0f };
        glFogi(GL_FOG_MODE, GL_EXP);
        if (t->liquidType == LIQUID_WATER) {
            glFogf(GL_FOG_DENSITY, 0.1f);
            ambient[1] = 0.4f; ambient[2] = 0.9f;
        } else {
            glFogf(GL_FOG_DENSITY, 2.0f);
        }
        if (gOptions.anaglyph3d) anaglyphColor(&ambient[0], &ambient[1], &ambient[2]);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    } else {
        GLfloat ambient[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, LevelRenderer_getFogEndDistance(&levelRenderer));
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    }
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT);
}

static bool entityVisible(const Frustum* fr, const Entity* e, float camX, float camY, float camZ) {
    if (!e->inGrid || !Entity_shouldRender(e, camX, camY, camZ)) return false;
    int cx, cy, cz;
    Entity_gridCell(e, &cx, &cy, &cz);
    float x0 = (float)((cx << 4) - 2), x1 = (float)(((cx + 1) << 4) + 2);
    float y0 = (float)((cy << 4) - 2), y1 = (float)(((cy + 1) << 4) + 2);
    float z0 = (float)((cz << 4) - 2), z1 = (float)(((cz + 1) << 4) + 2);
    if (!frustum_cubeIn(fr, x0, y0, z0, x1, y1, z1)) return false;
    return frustum_cubeFullyIn(fr, x0, y0, z0, x1, y1, z1) || frustum_isVisible(fr, &e->boundingBox);
}

// Level.isSolid(x, y, z, r), any of the 8 corners in a solid tile
static bool isSolidAround(float x, float y, float z, float r) {
    for (int i = 0; i < 8; ++i) {
        float cx = (i & 4) ? x + r : x - r;
        float cy = (i & 2) ? y + r : y - r;
        float cz = (i & 1) ? z + r : z - r;
        if (Level_isSolidTile(&level, (int)cx, (int)cy, (int)cz)) return true;
    }
    return false;
}

// the entity lighting, on before the entities and off after
static void setupEntityLighting(bool enable) {
    if (!enable) {
        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);
        return;
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    // the models are scaled with glScalef instead of in the cubes,
    // so the normals need GL_NORMALIZE
    glEnable(GL_NORMALIZE);

    float dx = 0.0f, dy = -1.0f, dz = 0.5f;
    float len = MathHelper_sqrt(dx * dx + dy * dy + dz * dz);
    GLfloat lightDir[4]     = { dx / len, dy / len, dz / len, 0.0f }; // w=0, directional
    GLfloat lightDiffuse[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat lightAmbient[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat modelAmbient[4] = { 0.7f, 0.7f, 0.7f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, modelAmbient);
}

/* boot and shutdown */

// any gl error at these points quits
static void checkGlError(const char* where) {
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) return;
    printf("########## GL ERROR ##########\n");
    printf("@ %s\n", where);
    printf("%d: %s\n", (int)err, (const char*)gluErrorString(err));
    exit(0);
}

static int init(Level* lvl, LevelRenderer* lr, Player* p) {
    // a thread started once the gl context exists makes nvidia's driver stall a sync for seconds,
    // so every thread is running before the window opens
    Options_init(&gOptions);
    Sound_init(gOptions.music, gOptions.sound);
    if (gConnectHost[0]) {
        NetConnection_startConnect(&gConn, gConnectHost, gConnectPort, gConnectUsername[0] ? gConnectUsername : "guest");
    }

    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 0;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    if (gIsFullscreen) {
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.28_01", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.28_01", NULL, NULL);
    }

    if (!window) {
        glfwTerminate();
        fprintf(stderr, "Failed to create GLFW window\n");
        return 0;
    }
    glfwMakeContextCurrent(window);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glfwSwapInterval(0); // uncapped

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW init failed: %s\n", glewGetErrorString(err));
        return 0;
    }
    // glew's lookup leaves an error behind on some drivers
    glGetError();
    checkGlError("Pre startup");

    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_LIGHTING);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.5f, 0.8f, 1.0f, 0.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // culling gets switched on at the start of every world frame
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
    checkGlError("Startup");

    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCharCallback(window, charCallback);

    Tile_registerAll();
    Textures_setAnaglyph(gOptions.anaglyph3d);

    // "guest" when connecting without a name, "anonymous" otherwise

    const char* connectName = gConnectUsername[0] ? gConnectUsername : "guest";
    User_init(&user, gConnectHost[0] ? connectName : "anonymous");

    texTerrain = loadTexture("resources/terrain.png", GL_NEAREST);
    texDirt    = loadTexture("resources/dirt.png", GL_NEAREST);
    texRock    = loadTexture("resources/rock.png", GL_NEAREST);
    texWater   = loadTexture("resources/water.png", GL_NEAREST);
    texGui     = loadTexture("resources/gui.png", GL_NEAREST);
    texIcons   = loadTexture("resources/icons.png", GL_NEAREST);
    Font_init(&gFont, "resources/default.png");

    LavaTextureFX_init(&gLavaFX, TILE_LAVA.textureId);
    WaterTextureFX_init(&gWaterFX, TILE_WATER.textureId);
    gTextureFX[0] = &gLavaFX.base;
    gTextureFX[1] = &gWaterFX.base;
    gTextureFX[0]->tick(gTextureFX[0]);
    gTextureFX[1]->tick(gTextureFX[1]);

    Sound_detachAll();
    mobCount = 0;
    arrowCount = 0;
    itemCount = 0;

    // nothing gets loaded or generated when it's about to connect
    if (gConnectHost[0]) Level_initPlaceholder(lvl);
    else if (!Level_init(lvl, 256, 256, 64)) GameMode_spawnInitial(lvl);
    LevelRenderer_init(lr, lvl, texTerrain);
    lr->drawDistance = gOptions.viewDistance;
    calcLightDepths(lvl, 0, 0, lvl->width, lvl->height);

    Player_init(p, lvl);
    Level_setPlayer(lvl, &p->e);
    World world = currentWorld();
    bool loadedPlayer = !gConnectHost[0] && Save_loadEntities(&world);
    GameMode_setLevel(lvl);
    if (!loadedPlayer) GameMode_newPlayer(p);
    GameMode_adjustPlayer(p);

    ParticleEngine_init(&particleEngine, lvl, (GLuint)texTerrain);

    Timer_init(&timer, 20.0f);

    // a host argument means multiplayer
    if (gConnectHost[0]) {
        // tick polls the connect thread and shows "Connecting.." meanwhile
        PlayerMap_clear();
        gMultiplayer = true;
        NetConnection_beginConnect(&gConn);
        gConnecting = true;
        gLoading = true;
    }
    checkGlError("Post startup");

    return 1;
}

static void destroy(Level* lvl) {
    // never in multiplayer, that would overwrite level.dat with the server's level or the empty one
    if (!gConnectHost[0]) {
        World world = currentWorld();
        Save_write(&world, "level.dat");
    }
    if (gConnected) NetConnection_close(&gConn);
    Level_destroy(lvl);
    Sound_shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}

/* camera */

// the death spin and the hurt shake, hurtDir sets the shake axis
static void applyHurtTilt(Player* p, float t) {
    Entity* e = &p->e;
    float hurtProgress = (float)e->hurtTime - t;
    if (e->health <= 0) {
        float deathProgress = (float)e->deathTime + t;
        glRotatef(40.0f - 8000.0f / (deathProgress + 200.0f), 0.0f, 0.0f, 1.0f);
    }
    if (hurtProgress < 0.0f) return;
    hurtProgress /= (float)e->hurtDuration;
    float shake = MathHelper_sin(hurtProgress * hurtProgress * hurtProgress * hurtProgress * 3.1415927f);
    float dir = e->hurtDir;
    glRotatef(-dir, 0.0f, 1.0f, 0.0f);
    glRotatef(-shake * 14.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(dir, 0.0f, 1.0f, 0.0f);
}

// view bobbing, walkDist sways and bounces, tilt pitches back when falling
static void applyViewBob(Player* p, float t) {
    float walkDelta = p->e.walkDist - p->e.walkDistO;
    float walk = p->e.walkDist + walkDelta * t;
    float bob = p->oBob + (p->bob - p->oBob) * t;
    float tilt = p->oTilt + (p->tilt - p->oTilt) * t;
    glTranslatef(MathHelper_sin(walk * 3.1415927f) * bob * 0.5f,
                 -fabsf(MathHelper_cos(walk * 3.1415927f) * bob), 0.0f);
    glRotatef(MathHelper_sin(walk * 3.1415927f) * bob * 3.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(fabsf(MathHelper_cos(walk * 3.1415927f + 0.2f) * bob) * 5.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(tilt, 1.0f, 0.0f, 0.0f);
}

// pass is the anaglyph eye, 0 or 1
static void moveCameraToPlayer(Player* p, float t, int pass) {
    if (gOptions.anaglyph3d) {
        glTranslatef((float)(pass * 2 - 1) * 0.1f, 0.0f, 0.0f);
    }

    applyHurtTilt(p, t);
    if (gOptions.bobView) applyViewBob(p, t);

    glTranslatef(0.0f, 0.0f, -0.1f);

    glRotatef(p->e.prevXRotation + (p->e.xRotation - p->e.prevXRotation) * t, 1.0f, 0.0f, 0.0f);
    glRotatef(p->e.prevYRotation + (p->e.yRotation - p->e.prevYRotation) * t, 0.0f, 1.0f, 0.0f);

    double x = p->e.prevX + (p->e.x - p->e.prevX) * t;
    double y = p->e.prevY + (p->e.y - p->e.prevY) * t;
    double z = p->e.prevZ + (p->e.z - p->e.prevZ) * t;

    glTranslated(-x, -y, -z);
}

static void setupCamera(Player* p, float t, int pass) {
    int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // the projection half of the anaglyph eye shift
    if (gOptions.anaglyph3d) {
        glTranslatef(-(float)(pass * 2 - 1) * 0.07f, 0.0f, 0.0f);
    }

    // the fov narrows while dying
    double fov = 70.0;
    if (p->e.health <= 0) {
        double deathTime = (double)p->e.deathTime + (double)t;
        fov /= (1.0 - 500.0 / (deathTime + 500.0)) * 2.0 + 1.0;
    }
    gluPerspective(fov, (double)fbw / (double)fbh, 0.05, LevelRenderer_getFogEndDistance(&levelRenderer));
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    moveCameraToPlayer(p, t, pass);
}

// u v w h are pixels in the 256x256 atlas
static void drawGuiBlit(int x, int y, int u, int v, int w, int h) {
    float u0 = u / 256.0f, u1 = (u + w) / 256.0f;
    float v0 = v / 256.0f, v1 = (v + h) / 256.0f;
    Tessellator_begin(&hudTess);
    Tessellator_vertexUV(&hudTess, (float)x,     (float)(y + h), -90.0f, u0, v1);
    Tessellator_vertexUV(&hudTess, (float)(x+w), (float)(y + h), -90.0f, u1, v1);
    Tessellator_vertexUV(&hudTess, (float)(x+w), (float)y,       -90.0f, u1, v0);
    Tessellator_vertexUV(&hudTess, (float)x,     (float)y,       -90.0f, u0, v0);
    Tessellator_end(&hudTess);
}

static void drawGui(float partialTicks) {
    glClear(GL_DEPTH_BUFFER_BIT);

    // the boot loading screen gets here with fog still on
    glDisable(GL_FOG);

    // the hud is 240 pixels tall
    int fbw, fbh; 
    glfwGetFramebufferSize(window, &fbw, &fbh);

    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, screenWidth, screenHeight, 0.0, 100.0, 300.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -200.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, texGui);
    drawGuiBlit(screenWidth / 2 - 91, screenHeight - 22, 0, 0, 182, 22);

    drawGuiBlit(screenWidth / 2 - 91 - 1 + player.inventory.selected * 20, screenHeight - 22 - 1, 0, 22, 24, 22);

    glBindTexture(GL_TEXTURE_2D, texIcons);
    drawGuiBlit(screenWidth / 2 - 7, screenHeight / 2 - 7, 0, 0, 16, 16);

    // the lastHealth ghost hearts flicker while invulnerable, not in the last 10 ticks
    {
        int flicker = (player.e.invulnerableTime / 3 % 2 == 1) ? 1 : 0;
        if (player.e.invulnerableTime < 10) flicker = 0;
        int health = player.e.health;
        int lastHealth = player.e.lastHealth;
        JRandom_setSeed(&gHudRandom, (int32_t)((uint32_t)gHudTicks * 312871u));
        // only survival has hearts and air
        if (gSurvival) for (int i = 0; i < 10; ++i) {
            int bgFrame = flicker ? 1 : 0;
            int hx = screenWidth / 2 - 91 + (i << 3);
            int hy = screenHeight - 32;
            if (health <= 4) hy += JRandom_nextIntN(&gHudRandom, 2);
            drawGuiBlit(hx, hy, 16 + bgFrame * 9, 0, 9, 9);
            if (flicker) {
                if ((i << 1) + 1 < lastHealth) drawGuiBlit(hx, hy, 70, 0, 9, 9);
                if ((i << 1) + 1 == lastHealth) drawGuiBlit(hx, hy, 79, 0, 9, 9);
            }
            if ((i << 1) + 1 < health) drawGuiBlit(hx, hy, 52, 0, 9, 9);
            if ((i << 1) + 1 == health) drawGuiBlit(hx, hy, 61, 0, 9, 9);
        }
    }

    // bubbles only with the eyes under water
    if (gSurvival && Entity_isUnderWater(&player.e)) {
        int full = (int)ceil((player.e.airSupply - 2) * 10.0 / 300.0);
        int popping = (int)ceil(player.e.airSupply * 10.0 / 300.0) - full;
        for (int i = 0; i < full + popping; ++i) {
            int bx = screenWidth / 2 - 91 + (i << 3);
            int by = screenHeight - 32 - 9;
            if (i < full) drawGuiBlit(bx, by, 16, 18, 9, 9);
            else          drawGuiBlit(bx, by, 25, 18, 9, 9);
        }
    }

    glDisable(GL_BLEND);
    for (int i = 0; i < 9; ++i) {
        int slotId = player.inventory.id[i];
        if (slotId <= 0) continue;

        int hx = screenWidth / 2 - 90 + i * 20;
        int hy = screenHeight - 16;

        glPushMatrix();
        glTranslatef((float)hx, (float)hy, -50.0f);

        // the pickup pop
        int flash = player.inventory.flash[i];
        if (flash > 0) {
            float f3 = ((float)flash - partialTicks) / 5.0f;
            float f4 = -MathHelper_sin(f3 * f3 * 3.1415927f) * 8.0f;
            float f5 = MathHelper_sin(f3 * f3 * 3.1415927f) + 1.0f;
            float f6 = MathHelper_sin(f3 * 3.1415927f) + 1.0f;
            glTranslatef(10.0f, f4 + 10.0f, 0.0f);
            glScalef(f5, f6, 1.0f);
            glTranslatef(-10.0f, -10.0f, 0.0f);
        }

        glScalef(10.0f, 10.0f, 10.0f);
        glTranslatef(1.0f, 0.5f, 0.0f);
        glRotatef(-30.0f, 1, 0, 0);
        glRotatef(45.0f, 0, 1, 0);
        glTranslatef(-1.5f, 0.5f, 0.5f);
        glScalef(-1.0f, -1.0f, -1.0f);

        glBindTexture(GL_TEXTURE_2D, texTerrain);
        Tessellator_begin(&hudTess);
        const Tile* slotTile = gTiles[slotId];
        if (slotTile) slotTile->renderGuiItem(slotTile, &hudTess);
        Tessellator_end(&hudTess);

        glPopMatrix();

        // the count only shows above 1
        if (player.inventory.count[i] > 1) {
            char countStr[8];
            snprintf(countStr, sizeof countStr, "%d", player.inventory.count[i]);
            int tw = Font_width(&gFont, countStr);
            Font_drawShadow(&gFont, &hudTess, countStr, hx + 19 - tw, hy + 6, 0xFFFFFF);
        }
    }

    Font_drawShadow(&gFont, &hudTess, "0.28_01", 2, 2, 0xFFFFFF);

    if (gOptions.showFrameRate) {
        Font_drawShadow(&gFont, &hudTess, gFpsText, 2, 12, 0xFFFFFF);
    }

    if (gSurvival) {
        char scoreMsg[48];
        snprintf(scoreMsg, sizeof scoreMsg, "Score: &e%d", player.score);
        Font_drawShadow(&gFont, &hudTess, scoreMsg, screenWidth - Font_width(&gFont, scoreMsg) - 2, 2, 0xFFFFFF);

        char arrowsMsg[32];
        snprintf(arrowsMsg, sizeof arrowsMsg, "Arrows: %d", player.arrows);
        Font_drawShadow(&gFont, &hudTess, arrowsMsg, screenWidth / 2 + 8, screenHeight - 33, 0xFFFFFF);
    }

    // the newest 10 lines under 10 seconds old, the newest 20 while the chat screen is open
    {
        bool chatOpen = activeScreen && ChatInputScreen_isThis(activeScreen);
        int maxLines = chatOpen ? 20 : 10;
        int ageLimit = (int)(timer.ticksPerSecond * 10.0f);

        int visibleCount = 0;
        for (int i = 0; i < chatLineCount && visibleCount < maxLines; i++) {
            if (!chatOpen && chatLines[i].age >= ageLimit) break; // the ages only grow down the list
            visibleCount++;
        }
        for (int slot = 0; slot < visibleCount; slot++) {
            int y = screenHeight - 8 - slot * 9 - 20;
            Font_drawShadow(&gFont, &hudTess, chatLines[slot].text, 2, y, 0xFFFFFF);
        }
    }

    int cx = screenWidth / 2;
    int cy = screenHeight / 2;

    // the Tab player list

    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && gConnected) {
        glEnable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
        glVertex2f((float)(cx + 128), (float)(cy - 68 - 12));
        glVertex2f((float)(cx - 128), (float)(cy - 68 - 12));
        glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
        glVertex2f((float)(cx - 128), (float)(cy + 68));
        glVertex2f((float)(cx + 128), (float)(cy + 68));
        glEnd();
        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);

        const char* header = "Connected players:";
        Font_drawShadow(&gFont, &hudTess, header, cx - Font_width(&gFont, header) / 2, cy - 76, 0xFFFFFF);

        const char* names[1 + 128];
        int nameCount = 0;
        names[nameCount++] = Minecraft_getUserName();
        for (int b = 0; b < playerMapCap; ++b) {
            for (int k = playerMapHead[b]; k >= 0; k = playerMapNext[k]) names[nameCount++] = netPlayers[playerMapSlot[k]].name;
        }

        // the hover is only checked while a screen is open, the mouse is hidden otherwise
        bool canHover = (activeScreen != NULL);
        int xm = 0, ym = 0;
        if (canHover) computeGuiMouse(&xm, &ym);
        gHoveredTabName[0] = '\0';

        for (int b = 0; b < nameCount; b++) {
            int x = cx - 120 + (b % 2) * 120;
            int y = cy - 64 + ((b / 2) << 3);
            bool hovered = canHover && (xm >= x && ym >= y && xm < x + 120 && ym < y + 8);
            if (hovered) {
                snprintf(gHoveredTabName, sizeof gHoveredTabName, "%s", names[b]);
                Font_draw(&gFont, &hudTess, names[b], x + 2, y, 0xFFFFFF);
            } else {
                Font_draw(&gFont, &hudTess, names[b], x, y, 0xEEEEEE);
            }
        }
    } else {
        gHoveredTabName[0] = '\0';
    }

    if (activeScreen) {
        int xm, ym;
        computeGuiMouse(&xm, &ym);
        activeScreen->render(activeScreen, xm, ym);
    }
}

/* picking */

// strict, a point on the edge is outside
static bool aabbContainsPoint(const AABB* box, float x, float y, float z) {
    return x > box->minX && x < box->maxX &&
           y > box->minY && y < box->maxY &&
           z > box->minZ && z < box->maxZ;
}

// one pickable entity against the ray, shrinking dist on a hit
static bool pickEntity(Entity* e, const AABB* reachBox, float px, float py, float pz,
                       float dx, float dy, float dz, float* dist) {
    if (e->removed || !AABB_intersects(&e->boundingBox, reachBox)) return false;
    AABB box = AABB_grow(&e->boundingBox, 0.1f, 0.1f, 0.1f);
    for (float s = 0.0f; s < *dist; s += 0.05f) {
        if (aabbContainsPoint(&box, px + dx * s, py + dy * s, pz + dz * s)) {
            *dist = s;
            return true;
        }
    }
    return false;
}

// the pick in Minecraft's render: Level.clip out to the game mode's reach, then pickable entities
static void pick(float t) {
    float px = player.e.prevX + (player.e.x - player.e.prevX) * t;
    float py = player.e.prevY + (player.e.y - player.e.prevY) * t;
    float pz = player.e.prevZ + (player.e.z - player.e.prevZ) * t;
    float pitch = player.e.prevXRotation + (player.e.xRotation - player.e.prevXRotation) * t;
    float yaw = player.e.prevYRotation + (player.e.yRotation - player.e.prevYRotation) * t;
    float cy = MathHelper_cos(-yaw * (3.1415927f / 180.0f) - 3.1415927f);
    float sy = MathHelper_sin(-yaw * (3.1415927f / 180.0f) - 3.1415927f);
    float cp = MathHelper_cos(-pitch * (3.1415927f / 180.0f));
    float sp = MathHelper_sin(-pitch * (3.1415927f / 180.0f));
    float dx = sy * cp, dy = sp, dz = cy * cp;
    // survival reach is 4
    const float reach = gSurvival ? 4.0f : 5.0f;

    float sx = px, sy2 = py, sz = pz;
    int hx, hy, hz, face;
    float vx, vy, vz;
    bool hasBlock = Level_clipHit(&level, &sx, &sy2, &sz, px + dx * reach, py + dy * reach, pz + dz * reach,
                                  &hx, &hy, &hz, &face, &vx, &vy, &vz);
    float dist = reach;
    if (hasBlock) {
        float ax = px - vx, ay = py - vy, az = pz - vz;
        dist = MathHelper_sqrt(ax * ax + ay * ay + az * az);
    }

    AABB reachBox = AABB_expand(&player.e.boundingBox, dx * dist, dy * dist, dz * dist);
    Entity* hitEntity = NULL;
    for (int i = 0; i < mobCount; i++) {
        if (mobs[i].e.inGrid && pickEntity(&mobs[i].e, &reachBox, px, py, pz, dx, dy, dz, &dist)) hitEntity = &mobs[i].e;
    }
    for (int i = 0; i < netPlayerCount; i++) {
        Entity* e = &netPlayers[netOrder[i]].e;
        if (e->inGrid && pickEntity(e, &reachBox, px, py, pz, dx, dy, dz, &dist)) hitEntity = e;
    }

    if (hitEntity) {
        hitresult_createEntity(&hitResult, hitEntity);
        isHitNull = 0;
    } else if (hasBlock) {
        hitresult_create(&hitResult, hx, hy, hz, 0, face);
        isHitNull = 0;
    } else {
        isHitNull = 1;
    }
}

// the first shootable entity in the box, the owner only after 5 ticks of flight
bool Minecraft_findArrowTarget(const AABB* box, const Entity* owner, int flightTime, Entity** outHit) {
    for (int i = 0; i < mobCount; i++) {
        Entity* e = &mobs[i].e;
        if (e->removed || !e->inGrid) continue;
        if (e == owner && flightTime <= 5) continue;
        if (AABB_intersects(&e->boundingBox, box)) {
            *outHit = e;
            return true;
        }
    }
    for (int i = 0; i < netPlayerCount; i++) {
        Entity* e = &netPlayers[netOrder[i]].e;
        if (e->removed || !e->inGrid) continue;
        if (AABB_intersects(&e->boundingBox, box)) {
            *outHit = e;
            return true;
        }
    }
    if (!(&player.e == owner && flightTime <= 5) && AABB_intersects(&player.e.boundingBox, box)) {
        *outHit = &player.e;
        return true;
    }
    return false;
}

bool Minecraft_spawnArrow(Level* lvl, Entity* owner, float x, float y, float z, float yaw, float pitch, float speed) {
    if (arrowCount >= MAX_ARROWS) return false;
    Arrow_init(&arrows[arrowCount++], lvl, owner, x, y, z, yaw, pitch, speed);
    return true;
}

// Arrow.playerTouch, only the owner picks up a stuck arrow, up to the carry limit
void Minecraft_checkArrowPickups(Entity* playerEntity) {
    AABB box = AABB_grow(&playerEntity->boundingBox, 1.0f, 0.0f, 1.0f);
    Player* p = (Player*)playerEntity;
    for (int i = 0; i < arrowCount; i++) {
        Arrow* a = &arrows[i];
        if (a->e.removed || !a->e.inGrid || a->pickedUp || !a->hasHit) continue;
        if (a->owner != playerEntity) continue;
        if (p->arrows >= PLAYER_MAX_ARROWS) continue;
        if (!AABB_intersects(&a->e.boundingBox, &box)) continue;
        Arrow_startPickup(a, playerEntity);
        p->arrows++;
    }
}

// the leaf textured particle of the creeper explosion
void Minecraft_spawnExplosionParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz) {
    Particle p;
    Particle_initTerrain(&p, lvl, x, y, z, mx, my, mz, &TILE_LEAVES);
    ParticleEngine_add(&particleEngine, &p);
}

void Minecraft_hurtEntitiesInExplosion(Level* lvl, Entity* source, float x, float y, float z, float radius) {
    (void)lvl;
    for (int i = 0; i < mobCount; i++) {
        Entity* e = &mobs[i].e;
        if (e->removed || !e->inGrid || e == source) continue;
        float dx = e->x - x, dy = e->y - y, dz = e->z - z;
        float ratio = MathHelper_sqrt(dx * dx + dy * dy + dz * dz) / radius;
        if (ratio <= 1.0f) Mob_hurt(e, source, (int)((1.0f - ratio) * 15.0f + 1.0f));
    }
    for (int i = 0; i < netPlayerCount; i++) {
        Entity* e = &netPlayers[netOrder[i]].e;
        if (e->removed || !e->inGrid || e == source) continue;
        float dx = e->x - x, dy = e->y - y, dz = e->z - z;
        float ratio = MathHelper_sqrt(dx * dx + dy * dy + dz * dz) / radius;
        if (ratio <= 1.0f) Mob_hurt(e, source, (int)((1.0f - ratio) * 15.0f + 1.0f));
    }
    if (&player.e != source) {
        float dx = player.e.x - x, dy = player.e.y - y, dz = player.e.z - z;
        float ratio = MathHelper_sqrt(dx * dx + dy * dy + dz * dz) / radius;
        if (ratio <= 1.0f) Mob_hurt(&player.e, source, (int)((1.0f - ratio) * 15.0f + 1.0f));
    }
}

// every Mob counts, the local player and network players too
int Minecraft_countMobs(void) {
    return mobCount + 1 + netPlayerCount;
}

// the push at the end of BasicAI.tick, every pushable mob overlapping the grown box
void Minecraft_pushNearby(Entity* mob) {
    AABB box = AABB_grow(&mob->boundingBox, 0.2f, 0.0f, 0.2f);
    for (int i = 0; i < mobCount; ++i) {
        Entity* e = &mobs[i].e;
        if (e == mob || e->removed || !e->inGrid || !AABB_intersects(&e->boundingBox, &box)) continue;
        Entity_pushEntity(e, mob);
    }
    for (int i = 0; i < netPlayerCount; ++i) {
        Entity* e = &netPlayers[netOrder[i]].e;
        if (e == mob || e->removed || !e->inGrid || !AABB_intersects(&e->boundingBox, &box)) continue;
        Entity_pushEntity(e, mob);
    }
    if (&player.e != mob && !player.e.removed && AABB_intersects(&player.e.boundingBox, &box)) {
        Entity_pushEntity(&player.e, mob);
    }
}

// MobSpawner builds the rolled mob and drops it, only its Math.random calls matter
void Minecraft_buildDiscardedMob(Level* lvl, int kind, float x, float y, float z) {
    Creature scratch;
    Creature_init(&scratch, (CreatureKind)kind, lvl, x, y, z);
}

// MobSpawner builds the mob first and drops it if its box overlaps any entity
static bool isFreeOfEntities(const AABB* box);

bool Minecraft_spawnMob(Level* lvl, int kind, float x, float y, float z) {
    if (mobCount >= MAX_MOBS) return false;
    Creature scratch;
    Creature_init(&scratch, (CreatureKind)kind, lvl, x, y, z);
    if (!isFreeOfEntities(&scratch.e.boundingBox)) return false;
    mobs[mobCount++] = scratch;
    mobs[mobCount - 1].e.ai = &mobs[mobCount - 1].ai; // the ai pointer moved with the copy
    return true;
}

void Minecraft_spawnItem(Level* lvl, float x, float y, float z, int resource) {
    if (itemCount >= MAX_ITEMS) return;
    Item_init(&items[itemCount++], lvl, x, y, z, resource);
}

// Player.aiStep touching the items
void Minecraft_checkItemPickups(Entity* playerEntity) {
    AABB box = AABB_grow(&playerEntity->boundingBox, 1.0f, 0.0f, 1.0f);
    for (int i = 0; i < itemCount; i++) {
        Item* it = &items[i];
        if (it->e.removed || !it->e.inGrid || it->pickedUp) continue;
        if (!AABB_intersects(&it->e.boundingBox, &box)) continue;
        if (Inventory_addResource(&player.inventory, it->resource)) {
            Item_startPickup(it, playerEntity);
        }
    }
}

/* input actions */

// creative breaks right away, survival only breaks tiles with no hardness
static void destroyBlock(int x, int y, int z);

static void clickBlock(int x, int y, int z) {
    if (!gSurvival) {
        destroyBlock(x, y, z);
        return;
    }
    int id = Level_getTile(&level, x, y, z);
    if (id > 0 && gTiles[id] && gTiles[id]->hardnessTicks == 0) destroyBlock(x, y, z);
}

// survival rolls the drops first
static void destroyBlock(int x, int y, int z) {
    int id = Level_getTile(&level, x, y, z);
    const Tile* t = (id > 0 && id < 256) ? gTiles[id] : NULL;
    if (gSurvival && t) Tile_dropItems(t, &level, x, y, z);
    bool changed = Level_netSetTile(&level, x, y, z, 0);
    if (t && changed) {
        if (gConnected) NetConnection_sendSetBlock(&gConn, x, y, z, 0, Inventory_getSelected(&player.inventory));
        if (t->soundType != SOUND_NONE) {
            char name[32];
            snprintf(name, sizeof name, "step.%s", SOUND_TYPES[t->soundType].name);
            Sound_playAt(name, (float)x, (float)y, (float)z, (SoundType_getVolume(t->soundType) + 1.0f) / 2.0f,
                         SoundType_getPitch(t->soundType) * 0.8f);
        }
        Tile_onDestroy(t, &level, x, y, z, &particleEngine);
    }
}

// one mining step per tick while left is held on a tile
static void hitBlock(int x, int y, int z, int face) {
    if (blockHitWait > 0) {
        blockHitWait--;
        return;
    }
    if (x == curBlockX && y == curBlockY && z == curBlockZ) {
        int id = Level_getTile(&level, x, y, z);
        if (id == 0) return;
        const Tile* t = gTiles[id];
        blockHardness = t->hardnessTicks;
        Tile_spawnHitParticle(t, &level, x, y, z, face, &particleEngine);
        curBlockDamage++;
        if (curBlockDamage == blockHardness + 1) {
            destroyBlock(x, y, z);
            curBlockDamage = 0;
            blockHitWait = 5;
        }
        return;
    }
    curBlockDamage = 0;
    curBlockX = x;
    curBlockY = y;
    curBlockZ = z;
}

static void resetBlockRemoving(void) {
    curBlockDamage = 0;
    blockHitWait = 0;
}

// crack progress, recomputed every frame
static float blockDamageFraction(float partialTicks) {
    if (curBlockDamage <= 0) return 0.0f;
    return ((float)curBlockDamage + partialTicks - 1.0f) / (float)blockHardness;
}

// survival eats a mushroom on right click
static bool useItem(int id) {
    if (!gSurvival) return false;
    if (id == TILE_MUSHROOM_RED.id && Inventory_consume(&player.inventory, id)) {
        Mob_hurt(&player.e, NULL, 3);
        return true;
    }
    if (id == TILE_MUSHROOM_BROWN.id && Inventory_consume(&player.inventory, id)) {
        Mob_heal(&player.e, 5);
        return true;
    }
    return false;
}

// Level.isFree(AABB): no entity of any kind overlaps the box
static bool isFreeOfEntities(const AABB* box) {
    if (AABB_intersects(&player.e.boundingBox, box)) return false;
    for (int i = 0; i < mobCount; ++i) {
        if (mobs[i].e.inGrid && AABB_intersects(&mobs[i].e.boundingBox, box)) return false;
    }
    for (int i = 0; i < arrowCount; ++i) {
        if (arrows[i].e.inGrid && AABB_intersects(&arrows[i].e.boundingBox, box)) return false;
    }
    for (int i = 0; i < itemCount; ++i) {
        if (items[i].e.inGrid && AABB_intersects(&items[i].e.boundingBox, box)) return false;
    }
    for (int i = 0; i < netPlayerCount; ++i) {
        const Entity* e = &netPlayers[netOrder[i]].e;
        if (e->inGrid && AABB_intersects(&e->boundingBox, box)) return false;
    }
    return true;
}

// button 0 attacks or mines, button 1 eats or places
// a left click on nothing blocks left clicks and mining for 10 ticks
static int gClickCooldown = 0;

static void clickMouse(int button) {
    if (button == 0 && gClickCooldown > 0) return;
    if (button == 0) {
        handSwingTicks = -1;
        handSwinging = true;
    }
    if (button == 1) {
        int sel = Inventory_getSelected(&player.inventory);
        if (sel > 0 && useItem(sel)) {
            heldBlend = 0.0f;
            return;
        }
    }
    if (isHitNull) {
        if (button == 0 && gSurvival) gClickCooldown = 10;
        return;
    }
    if (hitResult.type == HITRESULT_ENTITY) {
        if (button == 0) Mob_hurt(hitResult.entity, &player.e, 4);
        return;
    }

    int x = hitResult.x, y = hitResult.y, z = hitResult.z;
    if (button != 0) {
        if (hitResult.f == 0) y--;
        if (hitResult.f == 1) y++;
        if (hitResult.f == 2) z--;
        if (hitResult.f == 3) z++;
        if (hitResult.f == 4) x--;
        if (hitResult.f == 5) x++;
    }
    int targetId = Level_getTile(&level, x, y, z);
    if (button == 0) {
        if (targetId != TILE_BEDROCK.id || player.userType >= 100) clickBlock(x, y, z);
        return;
    }

    int sel = Inventory_getSelected(&player.inventory);
    if (sel <= 0) return;
    if (targetId != 0 && targetId != TILE_WATER.id && targetId != TILE_CALM_WATER.id &&
        targetId != TILE_LAVA.id && targetId != TILE_CALM_LAVA.id) return;
    const Tile* placed = gTiles[sel];
    AABB box;
    if (placed->getAABB && placed->getAABB(placed, x, y, z, &box)) {
        if (!isFreeOfEntities(&box)) return;
    }
    if (gSurvival && !Inventory_consume(&player.inventory, sel)) return;
    if (gConnected) NetConnection_sendSetBlock(&gConn, x, y, z, 1, sel);
    Level_netSetTile(&level, x, y, z, sel);
    heldBlend = 0.0f;
    if (placed->onBlockAdded) placed->onBlockAdded(placed, &level, x, y, z);
}

static bool isMouseGrabbed(void) {
    return glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

// MovementInputFromOptions.setKey
static void setPlayerKey(int key, bool pressed) {
    if (key == gOptions.keys[OPT_KEY_FORWARD].glfwKey) Player_setKey(&player, PLAYER_KEY_UP, pressed);
    if (key == gOptions.keys[OPT_KEY_BACK].glfwKey)    Player_setKey(&player, PLAYER_KEY_DOWN, pressed);
    if (key == gOptions.keys[OPT_KEY_LEFT].glfwKey)    Player_setKey(&player, PLAYER_KEY_LEFT, pressed);
    if (key == gOptions.keys[OPT_KEY_RIGHT].glfwKey)   Player_setKey(&player, PLAYER_KEY_RIGHT, pressed);
    if (key == gOptions.keys[OPT_KEY_JUMP].glfwKey)    Player_setKey(&player, PLAYER_KEY_JUMP, pressed);
}

static void handleMouseEvent(const InputEvent* ev) {
    if (ev->kind == EVENT_WHEEL) {
        Inventory_scroll(&player.inventory, ev->code);
        return;
    }
    if (activeScreen || !ev->pressed) return;
    if (!isMouseGrabbed()) {
        Minecraft_closeScreenAndGrabMouse();
        return;
    }
    if (ev->code == GLFW_MOUSE_BUTTON_LEFT) {
        clickMouse(0);
        gLastClickTick = gTickCount;
    }
    if (ev->code == GLFW_MOUSE_BUTTON_RIGHT) {
        clickMouse(1);
        gLastClickTick = gTickCount;
    }
    if (ev->code == GLFW_MOUSE_BUTTON_MIDDLE && !isHitNull) {
        int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
        if (id == TILE_GRASS.id) id = TILE_DIRT.id;
        if (id == TILE_DOUBLE_SLAB.id) id = TILE_SLAB.id;
        Inventory_grabTexture(&player.inventory, id, !gSurvival);
    }
}

static void handleKeyEvent(const InputEvent* ev) {
    setPlayerKey(ev->code, ev->pressed);
    if (!ev->pressed) return;
    int key = ev->code;
    if (activeScreen && activeScreen->keyPressed) activeScreen->keyPressed(activeScreen, 0, key);
    if (!activeScreen) {
        if (key == GLFW_KEY_ESCAPE) releaseMouseAndOpenPause();
        if (!gSurvival) {
            if (key == gOptions.keys[OPT_KEY_LOAD_LOC].glfwKey) Player_resetPos(&player);
            if (key == gOptions.keys[OPT_KEY_SAVE_LOC].glfwKey) {
                Level_setSpawnPos(&level, (int)player.e.x, (int)player.e.y, (int)player.e.z, player.e.yRotation);
                Player_resetPos(&player);
            }
        }
        // F5 toggles rain, offline only
        if (key == GLFW_KEY_F5 && !gMultiplayer) gRaining = !gRaining;
        if (key == GLFW_KEY_TAB && !gMultiplayer && player.arrows > 0 && arrowCount < MAX_ARROWS) {
            Arrow_init(&arrows[arrowCount++], &level, &player.e,
                       player.e.x, player.e.y, player.e.z, player.e.yRotation, player.e.xRotation, 1.2f);
            player.arrows--;
        }
        // creative opens the block select screen
        if (key == gOptions.keys[OPT_KEY_BUILD].glfwKey && !gSurvival) {
            int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
            BlockSelectScreen_open(&gFont, fbw * 240 / fbh, 240, &player.inventory, texTerrain);
        }
        if (key == gOptions.keys[OPT_KEY_CHAT].glfwKey && gConnected) {
            Player_releaseAllKeys(&player);
            int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
            ChatInputScreen_open(&gFont, fbw * 240 / fbh, 240);
        }
    }
    for (int i = 0; i < 9; ++i) {
        if (key == GLFW_KEY_1 + i) player.inventory.selected = i;
    }
    if (key == gOptions.keys[OPT_KEY_TOGGLE_FOG].glfwKey) {
        bool shiftHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                         glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        gOptions.viewDistance = (gOptions.viewDistance + (shiftHeld ? -1 : 1)) & 0x3;
        levelRenderer.drawDistance = gOptions.viewDistance;
        Options_save(&gOptions);
    }
}

// the gameplay half of Minecraft.tick: queued mouse and key events, click repeat, then mining
static void tickInput(void) {
    if (!activeScreen && player.e.health <= 0) Minecraft_setScreen(NULL);
    if (!activeScreen || activeScreen->allowUserInput) {
        int n = inputQueueCount;
        inputQueueCount = 0;
        for (int i = 0; i < n; ++i) {
            const InputEvent* ev = &inputQueue[i];
            if (ev->kind == EVENT_KEY) continue;
            if (activeScreen && ev->kind == EVENT_MOUSE) {
                if (ev->pressed && activeScreen->mouseClicked) {
                    int xm, ym;
                    computeGuiMouse(&xm, &ym);
                    activeScreen->mouseClicked(activeScreen, xm, ym, ev->code == GLFW_MOUSE_BUTTON_LEFT ? 0 : ev->code == GLFW_MOUSE_BUTTON_RIGHT ? 1 : 2);
                }
                continue;
            }
            handleMouseEvent(ev);
        }
        if (gClickCooldown > 0) gClickCooldown--;
        for (int i = 0; i < n; ++i) {
            if (inputQueue[i].kind == EVENT_KEY) handleKeyEvent(&inputQueue[i]);
        }
        if (!activeScreen) {
            float repeat = timer.ticksPerSecond / 4.0f;
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
                (float)(gTickCount - gLastClickTick) >= repeat && isMouseGrabbed()) {
                clickMouse(0);
                gLastClickTick = gTickCount;
            }
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS &&
                (float)(gTickCount - gLastClickTick) >= repeat && isMouseGrabbed()) {
                clickMouse(1);
                gLastClickTick = gTickCount;
            }
        }
        bool mining = !activeScreen && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && isMouseGrabbed();
        if (gSurvival && gClickCooldown <= 0) {
            if (mining && !isHitNull && hitResult.type == HITRESULT_TILE) {
                hitBlock(hitResult.x, hitResult.y, hitResult.z, hitResult.f);
            } else {
                resetBlockRemoving();
            }
        }
    } else {
        inputQueueCount = 0;
    }
    if (activeScreen) gLastClickTick = gTickCount + 10000;
}

// the cursor in the 240 tall gui space
static void computeGuiMouse(int* xm, int* ym) {
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;

    double curX, curY;
    glfwGetCursorPos(window, &curX, &curY);
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);

    int mx = (int)curX;
    int my = wh - 1 - (int)curY;
    *xm = mx * screenWidth / ww;
    *ym = screenHeight - my * screenHeight / wh - 1;
}

static void handleScreenClicks(GLFWwindow* w) {
    int left = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT);
    if (left == GLFW_PRESS && prevScreenClick == GLFW_RELEASE && activeScreen && activeScreen->mouseClicked) {
        int xm, ym;
        computeGuiMouse(&xm, &ym);
        activeScreen->mouseClicked(activeScreen, xm, ym, 0);
    }
    prevScreenClick = left;
}

/* frame */

// the held block, or the bare arm from char.png
static void renderHand(Player* p, float partialTicks, int pass) {
    if (!handArmCubeBuilt) {
        // the left arm box at its own origin, the arm is drawn
        // without the part's offset
        Cube_init(&handArmCube, 40, 16);
        handArmCube.mirror = 1;
        Cube_addBox(&handArmCube, -1, -2, -2, 4, 12, 4);
        handArmCubeBuilt = true;
    }

    // cleared so the hand never clips into blocks, it has to be the last 3d pass
    glClear(GL_DEPTH_BUFFER_BIT);

    float blend = prevHeldBlend + (heldBlend - prevHeldBlend) * partialTicks;

    glLoadIdentity();
    if (gOptions.anaglyph3d) {
        glTranslatef((float)(pass * 2 - 1) * 0.1f, 0.0f, 0.0f);
    }
    applyHurtTilt(p, partialTicks);
    if (gOptions.bobView) applyViewBob(p, partialTicks);

    // the light is placed in world orientation, then the hand is drawn in eye space
    glPushMatrix();
    glRotatef(p->e.prevXRotation + (p->e.xRotation - p->e.prevXRotation) * partialTicks, 1.0f, 0.0f, 0.0f);
    glRotatef(p->e.prevYRotation + (p->e.yRotation - p->e.prevYRotation) * partialTicks, 0.0f, 1.0f, 0.0f);
    setupEntityLighting(true);
    glPopMatrix();

    glPushMatrix();

    const float baseScale = 0.8f;
    if (handSwinging) {
        float swing = ((float)handSwingTicks + partialTicks) / 7.0f;
        float dip = MathHelper_sin(swing * 3.1415927f);
        float rise = MathHelper_sin(MathHelper_sqrt(swing) * 3.1415927f);
        glTranslatef(-rise * 0.4f, MathHelper_sin(MathHelper_sqrt(swing) * 3.1415927f * 2.0f) * 0.2f, -dip * 0.2f);
    }
    glTranslatef(0.7f * baseScale, -0.65f * baseScale - (1.0f - blend) * 0.6f, -0.9f * baseScale);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    glEnable(GL_RESCALE_NORMAL);
    if (handSwinging) {
        float swing = ((float)handSwingTicks + partialTicks) / 7.0f;
        float rise = MathHelper_sin(MathHelper_sqrt(swing) * 3.1415927f);
        glRotatef(rise * 80.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(-MathHelper_sin(swing * swing * 3.1415927f) * 20.0f, 1.0f, 0.0f, 0.0f);
    }

    float brightness = Level_getBrightness(&level, (int)p->e.x, (int)p->e.y, (int)p->e.z);
    glColor4f(brightness, brightness, brightness, 1.0f);

    if (heldTileId > 0) {
        glScalef(0.4f, 0.4f, 0.4f);
        glTranslatef(-0.5f, -0.5f, -0.5f);
        glBindTexture(GL_TEXTURE_2D, texTerrain);
        const Tile* t = (heldTileId < 256) ? gTiles[heldTileId] : NULL;
        if (t && t->renderItem) {
            t->renderItem(t, &hudTess);
        }
    } else {
        static int texChar = 0;
        if (!texChar) texChar = loadTexture("resources/char.png", GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, texChar);
        glScalef(1.0f, -1.0f, -1.0f);
        glTranslatef(0.0f, 0.2f, 0.0f);
        glRotatef(-120.0f, 0.0f, 0.0f, 1.0f);
        // the arm is drawn at 0.0625, Cube_render can't bake it in
        glScalef(0.0625f, 0.0625f, 0.0625f);
        Cube_render(&handArmCube);
    }

    glDisable(GL_RESCALE_NORMAL);
    glPopMatrix();

    setupEntityLighting(false);
}

// counts every tick
static int gRainTicks = 0;
static JRandom gRainRandom;

// falling rain on the columns around the player
static void renderRain(const Player* p, float t) {
    static int tex = 0;
    if (!tex) tex = loadTexture("resources/rain.png", GL_NEAREST);
    int px = (int)p->e.x, py = (int)p->e.y, pz = (int)p->e.z;
    glDisable(GL_CULL_FACE);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, tex);
    for (int x = px - 5; x <= px + 5; ++x) {
        for (int z = pz - 5; z <= pz + 5; ++z) {
            int top = Level_getHighestTile(&level, x, z);
            int y0 = py - 5, y1 = py + 5;
            if (y0 < top) y0 = top;
            if (y1 < top) y1 = top;
            if (y0 == y1) continue;
            float scroll = ((float)((gRainTicks + x * 3121 + z * 418711) % 32) + t) / 32.0f;
            float dx = (float)x + 0.5f - p->e.x;
            float dz = (float)z + 0.5f - p->e.z;
            float d = MathHelper_sqrt(dx * dx + dz * dz) / 5.0f;
            glColor4f(1.0f, 1.0f, 1.0f, (1.0f - d * d) * 0.7f);
            float v0 = (float)y0 * 2.0f / 8.0f + scroll * 2.0f;
            float v1 = (float)y1 * 2.0f / 8.0f + scroll * 2.0f;
            Tessellator_begin(&hudTess);
            Tessellator_vertexUV(&hudTess, (float)x,     (float)y0, (float)z,     0.0f, v0);
            Tessellator_vertexUV(&hudTess, (float)x + 1, (float)y0, (float)z + 1, 2.0f, v0);
            Tessellator_vertexUV(&hudTess, (float)x + 1, (float)y1, (float)z + 1, 2.0f, v1);
            Tessellator_vertexUV(&hudTess, (float)x,     (float)y1, (float)z,     0.0f, v1);
            Tessellator_vertexUV(&hudTess, (float)x,     (float)y0, (float)z + 1, 0.0f, v0);
            Tessellator_vertexUV(&hudTess, (float)x + 1, (float)y0, (float)z,     2.0f, v0);
            Tessellator_vertexUV(&hudTess, (float)x + 1, (float)y1, (float)z,     2.0f, v1);
            Tessellator_vertexUV(&hudTess, (float)x,     (float)y1, (float)z + 1, 0.0f, v1);
            Tessellator_end(&hudTess);
        }
    }
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

static void render(Level* lvl, LevelRenderer* lr, Player* p, GLFWwindow* w, float t) {
    (void)w;
    (void)lvl;

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    // only when focus is lost
    static bool wasFocused = true;
    bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
    if (wasFocused && !focused) releaseMouseAndOpenPause();
    wasFocused = focused;

    Sound_update(&p->e);

    // the options screen and the F key only change gOptions, sync it here
    levelRenderer.drawDistance = gOptions.viewDistance;
    Sound_setEnabled(gOptions.music, gOptions.sound);

    int grabbed = (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED);
    if (grabbed) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        int ww, wh; glfwGetWindowSize(window, &ww, &wh);
        double cx = ww * 0.5, cy = wh * 0.5;

        float dx = (float)(mx - cx);
        float dy = (float)(my - cy);

        dy = -dy * (gOptions.invertMouseY ? -1.0f : 1.0f);

        Player_turn(p, window, dx, dy);

        // recenter so next frame's delta is from the middle
        glfwSetCursorPos(window, cx, cy);
    }

    // anaglyph draws everything twice, once per eye
    int passCount = gOptions.anaglyph3d ? 2 : 1;
    for (int pass = 0; pass < passCount; ++pass) {
        if (gOptions.anaglyph3d) {
            if (pass == 0) glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
            else           glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        }

    updateFogColor();
    glClearColor(fogRed, fogGreen, fogBlue, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);

    setupCamera(p, t, pass);
    pick(t);

    Frustum frustum;
    frustum_calculate(&frustum);
    LevelRenderer_cull(lr, &frustum);
    gChunkUpdatesAccum += LevelRenderer_updateDirtyChunks(lr, p);

    setupFog();
    glEnable(GL_FOG);
    LevelRenderer_render(lr, p, 0);
    if (isSolidAround((float)p->e.x, (float)p->e.y, (float)p->e.z, 0.1f)) {
        int bx = (int)p->e.x, by = (int)p->e.y, bz = (int)p->e.z;
        for (int x = bx - 1; x <= bx + 1; ++x)
            for (int y = by - 1; y <= by + 1; ++y)
                for (int z = bz - 1; z <= bz + 1; ++z)
                    LevelRenderer_renderInside(lr, x, y, z);
    }

    setupEntityLighting(true);

    // BlockMap.render: the entity's grid cell (2 blocks bigger) has to be in view, a cell fully in
    // view draws everything in it, otherwise the entity's box decides
    float camX = p->e.prevX + (p->e.x - p->e.prevX) * t;
    float camY = p->e.prevY + (p->e.y - p->e.prevY) * t;
    float camZ = p->e.prevZ + (p->e.z - p->e.prevZ) * t;

    for (int i = 0; i < mobCount; ++i) {
        const Creature* c = &mobs[i];
        if (entityVisible(&frustum, &c->e, camX, camY, camZ)) {
            Creature_render(c, t);
        }
    }

    for (int i = 0; i < netPlayerCount; ++i) {
        const NetworkPlayer* np = &netPlayers[netOrder[i]];
        if (entityVisible(&frustum, &np->e, camX, camY, camZ)) {
            NetworkPlayer_render(np, t, p->e.yRotation, &gFont);
        }
    }

    for (int i = 0; i < arrowCount; ++i) {
        const Arrow* a = &arrows[i];
        if (entityVisible(&frustum, &a->e, camX, camY, camZ)) {
            Arrow_render(a, t);
        }
    }

    for (int i = 0; i < itemCount; ++i) {
        const Item* it = &items[i];
        if (entityVisible(&frustum, &it->e, camX, camY, camZ)) {
            Item_render(it, t);
        }
    }

    setupEntityLighting(false);
    setupFog();
    ParticleEngine_render(&particleEngine, p, t);
    glBindTexture(GL_TEXTURE_2D, texRock);
    glEnable(GL_TEXTURE_2D);
    LevelRenderer_renderSurroundingGround(lr);
    setupFog();
    LevelRenderer_renderClouds(lr, t, gOptions.anaglyph3d);
    setupFog();

    // drawn for entity hits too, at 0 0 0
    if (!isHitNull) {
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &hitResult, blockDamageFraction(t));
        LevelRenderer_renderHitOutline(&level, &hitResult);
        glEnable(GL_ALPHA_TEST);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setupFog();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, texWater);
    LevelRenderer_renderSurroundingWater(lr);
    glDisable(GL_BLEND);
    glEnable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    LevelRenderer_render(lr, p, 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (gOptions.anaglyph3d) {
        if (pass == 0) glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
        else           glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
    }
    LevelRenderer_render(lr, p, 1);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
    if (gRaining) renderRain(p, t);

    // the hand clears the depth buffer, so it goes last
    renderHand(p, t, pass);
    }

    if (gOptions.anaglyph3d) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    }

    drawGui(t);

    glfwSwapBuffers(window);

    if (gOptions.limitFramerate) sleepMillis(5);
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
        // taken before polling, a key callback can close the screen
        // and the held key mustn't read as a fresh press the same frame
        bool screenWasActive = activeScreen != NULL;

        glfwPollEvents();

        Timer_advanceTime(&timer);
        for (int i = 0; i < timer.ticks; ++i) tick(p, window);
        checkGlError("Pre render");

        // the Game over screen comes back whenever nothing is open and the player is dead
        if (!activeScreen && p->e.health <= 0) {
            Minecraft_setScreen(NULL);
        }

        glEnable(GL_TEXTURE_2D);

        // while loading, tick draws the loading screen itself
        if (!gLoading) {
            if (screenWasActive && activeScreen && !activeScreen->allowUserInput) {
                handleScreenClicks(window);
            }

            render(lvl, lr, p, window, timer.partialTicks);
            checkGlError("Post render");
        }

        frames++;
        while (currentTimeMillis() >= last + 1000LL) {
            snprintf(gFpsText, sizeof gFpsText, "%d fps, %d chunk updates", frames, gChunkUpdatesAccum);
        #ifndef NDEBUG
            printf("%s\n", gFpsText);
        #endif
            last += 1000LL;
            frames = 0;
            gChunkUpdatesAccum = 0;
        }
    }

    destroy(lvl);
}

// minecraft.exe [host] [port] [username], no args is singleplayer
int main(int argc, char** argv) {
    if (argc >= 2) {
        snprintf(gConnectHost, sizeof gConnectHost, "%s", argv[1]);
        if (argc >= 3) gConnectPort = atoi(argv[2]);
        if (argc >= 4) snprintf(gConnectUsername, sizeof gConnectUsername, "%s", argv[3]);
    }
    MathHelper_init();

    run(&level, &levelRenderer, &player);
    return EXIT_SUCCESS;
}

// removed mobs stay alive while something still references them
// so targets and arrow owners pointing at a removed mob get a ghost copy
static Entity ghosts[MAX_MOBS + MAX_ARROWS];

static void retargetEntity(const Entity* from, Entity* to) {
    for (int k = 0; k < mobCount; ++k) {
        if (mobs[k].ai.attackTarget == from) mobs[k].ai.attackTarget = to;
    }
    for (int k = 0; k < arrowCount; ++k) {
        if (arrows[k].owner == from) arrows[k].owner = to;
        if (arrows[k].e.killCredit == from) arrows[k].e.killCredit = to;
    }
}

static bool isEntityReferenced(const Entity* e) {
    for (int k = 0; k < mobCount; ++k) {
        if (mobs[k].ai.attackTarget == e) return true;
    }
    for (int k = 0; k < arrowCount; ++k) {
        if (arrows[k].owner == e || arrows[k].e.killCredit == e) return true;
    }
    return false;
}

static void markGhost(const Entity* e, bool* used) {
    if (e >= ghosts && e < ghosts + MAX_MOBS + MAX_ARROWS) used[e - ghosts] = true;
}

static Entity* allocGhost(void) {
    bool used[MAX_MOBS + MAX_ARROWS] = {0};
    for (int k = 0; k < mobCount; ++k) markGhost(mobs[k].ai.attackTarget, used);
    for (int k = 0; k < arrowCount; ++k) {
        markGhost(arrows[k].owner, used);
        markGhost(arrows[k].e.killCredit, used);
    }
    // every holder points at one entity at most so this never runs out
    int g = 0;
    while (used[g]) g++;
    return &ghosts[g];
}

static void removeMob(int i) {
    Entity* dead = &mobs[i].e;
    Sound_retargetEntity(dead, NULL);
    if (isEntityReferenced(dead)) {
        Entity* ghost = allocGhost();
        *ghost = *dead;
        ghost->ai = NULL;
        ghost->onDeath = NULL;
        retargetEntity(dead, ghost);
    }
    int last = mobCount - 1;
    if (i != last) {
        mobs[i] = mobs[last];
        mobs[i].e.ai = &mobs[i].ai;
        retargetEntity(&mobs[last].e, &mobs[i].e);
        Sound_retargetEntity(&mobs[last].e, &mobs[i].e);
    }
    mobCount--;
}

static void tick(Player* p, GLFWwindow* w) {
    (void)w;

    gTickCount++;
    GameMode_tick();

    glBindTexture(GL_TEXTURE_2D, texTerrain);
    for (int i = 0; i < 2; ++i) {
        TextureFX* fx = gTextureFX[i];
        fx->anaglyph = gOptions.anaglyph3d;
        fx->tick(fx);
        int cellX = (fx->tileId % 16) << 4;
        int cellY = (fx->tileId / 16) << 4;
        glTexSubImage2D(GL_TEXTURE_2D, 0, cellX, cellY, TEXTURE_FX_SIZE, TEXTURE_FX_SIZE,
                         GL_RGBA, GL_UNSIGNED_BYTE, fx->pixels);
    }

    gHudTicks++;
    for (int i = 0; i < chatLineCount; i++) {
        chatLines[i].age++;
    }

    Sound_tickMusic();

    // our position goes out every tick, even before our SpawnPlayer arrives,
    // so others see us at 0,0,0 for a moment
    if (gConnecting) {
        if (NetConnection_pollConnecting(&gConn)) {
            gConnecting = false;
            gConnected = gConn.connected; // false means it failed and the message screen is up
        }
    }
    if (gConnected) {
        NetConnection_tick(&gConn);
        // the exception leaves the rest of the tick undone
        if (gTickBroke) {
            gTickBroke = false;
            return;
        }
        // not while the level is still downloading
        if (gConnected && gConn.levelLoaded) {
            NetConnection_sendTeleportSelf(&gConn, p->e.x, p->e.y, p->e.z, p->e.yRotation, p->e.xRotation);
        }
    }

    tickInput();
    if (activeScreen && activeScreen->tick) activeScreen->tick(activeScreen);

    // the hand swing and the held block fade
    gRainTicks++;
    prevHeldBlend = heldBlend;
    if (handSwinging) {
        handSwingTicks++;
        if (handSwingTicks == 7) {
            handSwingTicks = 0;
            handSwinging = false;
        }
    }
    {
        int selected = Inventory_getSelected(&p->inventory);
        float target = (selected == heldTileId) ? 1.0f : 0.0f;
        float delta = target - heldBlend;
        if (delta < -0.4f) delta = -0.4f;
        if (delta > 0.4f) delta = 0.4f;
        heldBlend += delta;
        if (heldBlend < 0.1f) heldTileId = selected;
    }
    if (gRaining) {
        static bool seeded = false;
        if (!seeded) { JRandom_initRandom(&gRainRandom); seeded = true; }
        int px = (int)p->e.x, py = (int)p->e.y, pz = (int)p->e.z;
        for (int i = 0; i < 50; ++i) {
            int x = px + JRandom_nextIntN(&gRainRandom, 9) - 4;
            int z = pz + JRandom_nextIntN(&gRainRandom, 9) - 4;
            int y = Level_getHighestTile(&level, x, z);
            if (y > py + 4 || y < py - 4) continue;
            float fx = JRandom_nextFloat(&gRainRandom);
            float fz = JRandom_nextFloat(&gRainRandom);
            Particle drop;
            Particle_initWaterDrop(&drop, &level, (float)x + fx, (float)y + 0.1f, (float)z + fz);
            ParticleEngine_add(&particleEngine, &drop);
        }
    }

    LevelRenderer_tick(&levelRenderer); // scrolls the clouds

    for (int i = 0; i < mobCount; ) {
        Entity_gridInsert(&mobs[i].e);
        Creature_onTick(&mobs[i]);
        if (mobs[i].e.removed) {
            removeMob(i);
            continue;
        }
        Entity_gridTick(&mobs[i].e);
        i++;
    }

    // world gen mobs were added before the player, so they tick first
    Entity_gridInsert(&p->e);
    Player_onTick(p);
    Entity_gridTick(&p->e);

    for (int i = 0; i < arrowCount; ) {
        Entity_gridInsert(&arrows[i].e);
        Arrow_onTick(&arrows[i]);
        if (arrows[i].e.removed) {
            arrows[i] = arrows[arrowCount - 1];
            arrowCount--;
            continue;
        }
        Entity_gridTick(&arrows[i].e);
        i++;
    }

    for (int i = 0; i < itemCount; ) {
        Entity_gridInsert(&items[i].e);
        Item_onTick(&items[i]);
        if (items[i].e.removed) {
            items[i] = items[itemCount - 1];
            itemCount--;
            continue;
        }
        Entity_gridTick(&items[i].e);
        i++;
    }

    for (int i = 0; i < netPlayerCount; ) {
        NetworkPlayer* np = &netPlayers[netOrder[i]];
        Entity_gridInsert(&np->e);
        NetworkPlayer_onTick(np);
        if (np->e.removed) {
            removeNetPlayerFromLevel(netOrder[i]);
            continue;
        }
        Entity_gridTick(&np->e);
        i++;
    }

    // the server owns the tiles in multiplayer
    if (!gMultiplayer) Level_onTick(&level);

    ParticleEngine_onTick(&particleEngine);
}