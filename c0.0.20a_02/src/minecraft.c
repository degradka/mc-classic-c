// minecraft.c: entry point, window and GL init, camera, picking, main loop

// must come before any header that might drag in <windows.h> (GLEW does on
// Windows), or its old winsock.h beats winsock2.h to the punch and the
// build fails with redefinition errors
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

#include "level/level.h"
#include "level/level_renderer.h"
#include "renderer/frustum.h"
#include "level/tile/tile.h"
#include "renderer/textures.h"
#include "renderer/texture_fx.h"
#include "particle/particle_engine.h"
#include "player.h"
#include "user.h"
#include "character/zombie.h"
#include "timer.h"
#include "hitresult.h"
#include "gui/font.h"
#include "gui/screen.h"
#include "gui/pause_screen.h"
#include "gui/chat_input_screen.h"
#include "gui/message_screen.h"
#include "gui/inventory_screen.h"
#include "net/network_player.h"

#define MAX_MOBS 256 // c0.0.14a_08 raises the mob cap from 100
#define MAX_NET_PLAYERS 32

static GLFWwindow*    window;
static Level          level;
static LevelRenderer  levelRenderer;
static Player         player;
static Timer          timer;
static Zombie         mobs[MAX_MOBS];
static ParticleEngine particleEngine;
static User           user;

static int mobCount = 0;

static NetworkPlayer netPlayers[MAX_NET_PLAYERS];
static int netPlayerCount = 0;

static int prevLeft  = GLFW_RELEASE;
static int prevRight = GLFW_RELEASE;
static int prevMiddle = GLFW_RELEASE;
static int prevEnter = GLFW_RELEASE;
static int prevNumKeys[9]; // GLFW_KEY_1..GLFW_KEY_9, initialized to GLFW_RELEASE in init()
static int prevG    = GLFW_RELEASE;
static int prevY    = GLFW_RELEASE;
static int prevF    = GLFW_RELEASE;
static int prevR    = GLFW_RELEASE;
static int prevT    = GLFW_RELEASE;
static int prevB    = GLFW_RELEASE; // c0.0.20a_02: opens the inventory screen

// c0.0.14a_08: hotbar expanded from 6 individually wired keys to a real 9
// slot bar, selectable by keys 1-9, mouse wheel, or middle click pick block.
// c0.0.19a_04: stonebrick and sand dropped in favor of the two new tiles
// (rock, dirt, sponge, wood, sapling, log, leaves, glass, gravel), and this
// is also the first version to actually draw the bar on screen (see the Hud)
// c0.0.20a_02: the hotbar is now a mutable inventory, its slots overwritable
// from the new full inventory screen (B key), so it defaults to the first 9
// entries of PLACEABLE_TILE_IDS instead of a fixed named set, and selection
// is tracked as a slot index rather than a remembered tile id
static int gHotbar[9];
static int gSelectedSlot = 0;

static int gTickCount = 0;
static int gLastMineTick = 0;

static int gEditMode = 0;              // 0=destroy, 1=place
static int gYMouseAxis = 1;            // toggled by Y key, 1 or negative 1

// c0.0.16a_02: chat backlog, max 8 lines, each aged out after 10 real
// seconds (200 ticks at 20 TPS). Populated by incoming Message packets once
// networking exists; empty and inert until then.
#define CHAT_BACKLOG_MAX 50 // c0.0.17a: grew from 8
typedef struct {
    char text[96];
    int  age;
} ChatLine;
static ChatLine chatLines[CHAT_BACKLOG_MAX];
static int chatLineCount = 0;

static bool gConnected  = false;       // true once a server connection's Login packet went out
static bool gConnecting = false;       // c0.0.17a: true while the background connect thread is in flight
static bool gLoading    = false;       // true from connect attempt until LevelFinalize installs a level
static NetConnection gConn;
static char gConnectHost[256] = "";
static int  gConnectPort = 25565;
static char gConnectUsername[64] = ""; // empty = fall back to "guest"

static int texTerrain = 0;
static int texDirt = 0;
static int texGui = 0; // c0.0.19a_04: hotbar background/highlight atlas

// c0.0.19a_04: animated water/lava, one 16x16 patch per tile re-simulated
// and re-uploaded into texTerrain once per tick
static LavaTextureFX  gLavaFX;
static WaterTextureFX gWaterFX;
static TextureFX* gTextureFX[2];
static char gLoadTitle[64];

static Tessellator hudTess;
static Font gFont;                     // HUD font
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
void Minecraft_setScreen(Screen* screen);

/* input and GL state helpers */

// c0.0.13a: ESC, losing focus, or any other release mouse trigger, zeroes
// player input and opens the pause menu, matching Minecraft.releaseMouse().
static void releaseMouseAndOpenPause(void) {
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return; // already released

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;
    PauseScreen_init(&gPauseScreen, &gFont, screenWidth, screenHeight);
    Minecraft_setScreen((Screen*)&gPauseScreen);
}

// matches Minecraft.grabMouse(): grabs cursor again and closes any open screen
void Minecraft_closeScreenAndGrabMouse(void) {
    activeScreen = NULL;
    // ticks keep advancing while paused, so without this the auto repeat
    // mining check sees a stale gLastMineTick and fires the instant the
    // menu closes, destroying whatever's still targeted
    gLastMineTick = gTickCount;
    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
}

// c0.0.16a_02: PauseScreen no longer regenerates directly, it opens a size
// picker (Small/Normal/Huge) first, which calls this with 0/1/2. Size is
// 128 << sizePreset per axis, depth always 64 (Normal, preset 1, is the same
// 256x256x64 the boot map already used since c0.0.13a_03).
void Minecraft_generateNewLevelSized(int sizePreset) {
    int size = 128 << sizePreset;

    LevelRenderer_destroy(&levelRenderer);
    Level_resize(&level, size, size, 64);
    LevelRenderer_init(&levelRenderer, &level, texTerrain);

    Entity_resetPosition(&player.e);
    mobCount = 0;
}

// matches Minecraft.setScreen(): any screen release mouse look, letting the
// player click and read text normally instead of aiming the camera. Chat
// used to skip this, leaving the camera live behind the text box.
void Minecraft_setScreen(Screen* screen) {
    if (!screen) {
        Minecraft_closeScreenAndGrabMouse();
        return;
    }
    activeScreen = screen;
    Player_releaseAllKeys(&player);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

// c0.0.16a_02: sends a chat message packet (sender id -1) over the network
// connection. No-op in singleplayer, deliberately: the real client
// dereferences the connection here without a null check, which crashes with
// a NullPointerException if you send a message with no server connected.
// Not reproducing that.
void Minecraft_sendChat(const char* text) {
    if (!gConnected) return;
    NetConnection_sendMessage(&gConn, text);
}

// matches net.c's use of Minecraft's generic 2 line message screen for both
// connect failure and mid session disconnects
void Minecraft_openMessageScreen(const char* title, const char* message) {
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;
    MessageScreen_open(&gFont, screenWidth, screenHeight, title, message);
    gConnected = false;
    gLoading = false;
}

// matches LevelFinalize's handling: installs the freshly gunzipped level and
// swaps in a fresh LevelRenderer sized for it, then leaves loading mode.
// Spawn position arrives separately as a SpawnPlayer packet
void Minecraft_installNetworkLevel(int w, int h, int d, const byte* blocks) {
    LevelRenderer_destroy(&levelRenderer);
    Level_setDataFromNetwork(&level, w, h, d, blocks);
    LevelRenderer_init(&levelRenderer, &level, texTerrain);
    mobCount = 0;
    netPlayerCount = 0;
    gLoading = false;
}

// matches SetBlock server to client: applies directly, no local echo back.
// Uses Level_netSetTile (not the gated level_setTile) since this is exactly
// the server-authoritative path that's allowed to mutate a network level
void Minecraft_networkSetBlock(int x, int y, int z, int type) {
    Level_netSetTile(&level, x, y, z, type);
}

// c0.0.20a_02: from the Login reply's new trailing byte
void Minecraft_setUserType(int userType) {
    player.userType = userType;
}

// matches SpawnPlayer with id < 0: the server telling the client where its
// own player belongs, once the level transfer finishes
void Minecraft_networkTeleportSelf(float x, float y, float z, float yaw, float pitch) {
    Entity_setPosition(&player.e, x, y, z);
    player.e.yRotation = yaw;
    player.e.xRotation = pitch;
}

// c0.0.17a: own-spawn now also updates the level's own spawn point, so a
// later respawn returns to the server's spawn instead of nowhere
void Minecraft_setLevelSpawnPos(int x, int y, int z, float rot) {
    Level_setSpawnPos(&level, x, y, z, rot);
}

static NetworkPlayer* findNetworkPlayer(int id) {
    for (int i = 0; i < netPlayerCount; i++) {
        if (netPlayers[i].used && netPlayers[i].id == id) return &netPlayers[i];
    }
    return NULL;
}

// matches SpawnPlayer with id >= 0: a remote player joining
void Minecraft_spawnNetworkPlayer(int id, const char* name, int xRaw, int yRaw, int zRaw, float yaw, float pitch) {
    NetworkPlayer* existing = findNetworkPlayer(id);
    if (existing) {
        // re-spawn for an id already in use: reinitialize in place instead
        // of stacking a duplicate model at the same slot
        NetworkPlayer_init(existing, &level, id, name, xRaw, yRaw, zRaw, yaw, pitch);
        return;
    }
    if (netPlayerCount >= MAX_NET_PLAYERS) return;
    NetworkPlayer_init(&netPlayers[netPlayerCount++], &level, id, name, xRaw, yRaw, zRaw, yaw, pitch);
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

// matches DespawnPlayer: swap-remove, mirroring how mobs[] is compacted
void Minecraft_despawnNetworkPlayer(int id) {
    for (int i = 0; i < netPlayerCount; i++) {
        if (netPlayers[i].used && netPlayers[i].id == id) {
            netPlayers[i] = netPlayers[netPlayerCount - 1];
            netPlayerCount--;
            return;
        }
    }
}

// matches Minecraft.addChatMessage(): c0.0.17a prepends the newest line to
// the front instead of appending to the end, dropping the oldest line off
// the back once already at the 50 line cap (was 8, append/evict-front)
void Minecraft_addChatLine(const char* text) {
    if (chatLineCount == CHAT_BACKLOG_MAX) chatLineCount--;
    memmove(&chatLines[1], &chatLines[0], sizeof(ChatLine) * chatLineCount);
    chatLineCount++;
    ChatLine* line = &chatLines[0];
    snprintf(line->text, sizeof line->text, "%s", text);
    line->age = 0;
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

static char gLoadStatus[64] = "";

// matches the new c.java a(int): draws the dirt background, the title and
// status text, and a percent complete bar when percent is 0 or more
static void Minecraft_drawLoadingScreen(int percent) {
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int screenWidth  = fbw * 240 / fbh;
    int screenHeight = 240;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
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
        Tessellator_vertex(&hudTess, (float)m,       (float)(n + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(m + 100), (float)(n + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(m + 100), (float)n, 0.0f);
        Tessellator_vertex(&hudTess, (float)m,       (float)n, 0.0f);
        Tessellator_end(&hudTess);

        Tessellator_begin(&hudTess);
        Tessellator_color(&hudTess, 0.5019608f, 1.0f, 0.5019608f); // 0x80FF80 fill
        Tessellator_vertex(&hudTess, (float)m,             (float)(n + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(m + percent), (float)(n + 2), 0.0f);
        Tessellator_vertex(&hudTess, (float)(m + percent), (float)n, 0.0f);
        Tessellator_vertex(&hudTess, (float)m,             (float)n, 0.0f);
        Tessellator_end(&hudTess);
        glEnable(GL_TEXTURE_2D);
    }

    int titleWidth  = Font_width(&gFont, gLoadTitle);
    int statusWidth = Font_width(&gFont, gLoadStatus);
    Font_drawShadow(&gFont, &hudTess, gLoadTitle, (screenWidth - titleWidth) / 2, screenHeight / 2 - 4 - 16, 0xFFFFFF);
    Font_drawShadow(&gFont, &hudTess, gLoadStatus, (screenWidth - statusWidth) / 2, screenHeight / 2 - 4 + 8, 0xFFFFFF);

    glfwSwapBuffers(window);
    // c0.0.13a_03 drops the fixed 200ms sleep for an uncapped update rate.
    // Java relies on AWT's own message pump staying alive during the wait;
    // GLFW has none, so poll here to avoid the OS flagging the window as hung
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

const char* Minecraft_getUserName(void) {
    return user.name;
}

bool Minecraft_isConnected(void) {
    return gConnected;
}

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)w; (void)scancode; (void)mods;

    // c0.0.13a_03: while a screen is up, keys go to it instead of gameplay,
    // matching Screen.updateEvents() dispatching every key press to
    // keyPressed() (whose new default handles Escape by closing the screen).
    // c0.0.16a_02: text entry screens need REPEAT too, so backspace keeps
    // deleting while held, matching GLFW's own key repeat.
    if (activeScreen) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
        if (activeScreen->keyPressed) activeScreen->keyPressed(activeScreen, 0, key);
        return;
    }

    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) {
        releaseMouseAndOpenPause();
    }
}

// c0.0.16a_02: GLFW splits typed text from raw key codes, unlike the real
// source's single combined keyPressed(char, key) event, so printable
// characters for text entry screens (chat) come through here instead,
// dispatched with key 0 as a sentinel meaning "this is a character, not a
// special key"
static void charCallback(GLFWwindow* w, unsigned int codepoint) {
    (void)w;
    if (!activeScreen || !activeScreen->keyPressed) return;
    if (codepoint < 128) activeScreen->keyPressed(activeScreen, (char)codepoint, 0);
}

// c0.0.14a_08: mouse wheel now cycles the hotbar selection.
// c0.0.20a_02: selection is now a slot index into the mutable hotbar rather
// than a remembered tile id, and the wheel direction is inverted relative to
// before (subtracts instead of adds), matching the real source exactly.
static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset) {
    (void)w; (void)xoffset;
    if (activeScreen) return;

    int dir = (yoffset > 0.0) ? 1 : (yoffset < 0.0) ? -1 : 0;
    if (dir == 0) return;

    gSelectedSlot -= dir;
    while (gSelectedSlot < 0) gSelectedSlot += 9;
    while (gSelectedSlot >= 9) gSelectedSlot -= 9;
}

// c0.0.14a_08: daylight fog switched from exponential to linear, with its far
// end tied to the draw distance toggle (see LevelRenderer_getFogEndDistance).
// type -1 is new: sets up the same linear daylight fog for cloud rendering,
// but returns immediately without touching lighting/color material state
// (clouds render fully unlit).
static void setupFog(int type) {
    if (type == -1) {
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, LevelRenderer_getFogEndDistance(&levelRenderer));
        glFogfv(GL_FOG_COLOR, fogColorDaylight);
        GLfloat skyAmbient[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, skyAmbient);
        return;
    }

    glEnable(GL_FOG);

    int px = (int)floor(player.e.x);
    int py = (int)floor(player.e.y + 0.12);
    int pz = (int)floor(player.e.z);
    const Tile* currentTile = gTiles[Level_getTile(&level, px, py, pz)];

    GLfloat ambient[4];

    // whichever liquid the player's head is inside overrides the daylight
    // and shadow fog entirely, regardless of which type is being set up
    if (currentTile && currentTile->liquidType == LIQUID_WATER) {
        GLfloat waterFog[4] = { 0.02f, 0.02f, 0.2f, 1.0f };
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 0.1f);
        glFogfv(GL_FOG_COLOR, waterFog);
        ambient[0] = 0.3f; ambient[1] = 0.3f; ambient[2] = 0.7f; ambient[3] = 1.0f;
    } else if (currentTile && currentTile->liquidType == LIQUID_LAVA) {
        GLfloat lavaFog[4] = { 0.6f, 0.1f, 0.0f, 1.0f };
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 2.0f);
        glFogfv(GL_FOG_COLOR, lavaFog);
        ambient[0] = 0.4f; ambient[1] = 0.3f; ambient[2] = 0.3f; ambient[3] = 1.0f;
    } else if (type == 0) { // daylight, now linear instead of exponential
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, LevelRenderer_getFogEndDistance(&levelRenderer));
        glFogfv(GL_FOG_COLOR, fogColorDaylight);
        ambient[0] = 1.0f; ambient[1] = 1.0f; ambient[2] = 1.0f; ambient[3] = 1.0f;
    } else {                 // shadow, unchanged, only used for the hit highlight now
        glFogi(GL_FOG_MODE, GL_EXP);
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

// c0.0.20a_02: real per face lit shading for mobs/other players, layered on
// top of their own brightness tinted vertex color. Bracket the entity render
// loop with true then false, matching the real source exactly
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
    // the body scale this port applies via glScalef isn't baked into the
    // cube geometry itself, unlike the real source, so without this the
    // scaled down modelview matrix would also shrink the lighting normals
    glEnable(GL_NORMALIZE);

    float dx = 0.0f, dy = -1.0f, dz = 0.5f;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    GLfloat lightDir[4]     = { dx / len, dy / len, dz / len, 0.0f }; // w=0: directional
    GLfloat lightDiffuse[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat lightAmbient[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat modelAmbient[4] = { 0.7f, 0.7f, 0.7f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, modelAmbient);
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
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.0.20a_02", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.0.20a_02", NULL, NULL);
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

    // matches the real source: glCullFace/glFrontFace are set once here, but
    // GL_CULL_FACE is never actually enabled anywhere in the real client, so
    // this is inert state rather than active back-face culling. Confirmed by
    // checking every version's decompile for a GL_CULL_FACE enable call and
    // finding none
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);

    int ww, wh; glfwGetWindowSize(window, &ww, &wh);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ww * 0.5, wh * 0.5);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCharCallback(window, charCallback);

    Tile_registerAll();

    for (int i = 0; i < 9; ++i) gHotbar[i] = PLACEABLE_TILE_IDS[i];

    // c0.0.13a_03 defaults to "anonymous" instead of "noname" when no
    // session is set, matching Minecraft.run()'s fallback level owner name.
    // c0.0.16a_02: the connect path takes an optional username argument,
    // falling back to "guest" instead of "anonymous" if none is given.
    // Both fallbacks exist since there's no real login session either way
    const char* connectName = gConnectUsername[0] ? gConnectUsername : "guest";
    User_init(&user, gConnectHost[0] ? connectName : "anonymous");

    texTerrain = loadTexture("resources/terrain.png", GL_NEAREST);
    texDirt    = loadTexture("resources/dirt.png", GL_NEAREST);
    texGui     = loadTexture("resources/gui.png", GL_NEAREST);
    Font_init(&gFont, "resources/default.png"); // c0.0.17a: was default.gif

    // c0.0.19a_04: register the water/lava animators against their tile's
    // still-texture atlas cell, ticking once immediately so neither is blank
    // on the very first frame
    LavaTextureFX_init(&gLavaFX, TILE_LAVA.textureId);
    WaterTextureFX_init(&gWaterFX, TILE_WATER.textureId);
    gTextureFX[0] = &gLavaFX.base;
    gTextureFX[1] = &gWaterFX.base;
    gTextureFX[0]->tick(gTextureFX[0]);
    gTextureFX[1]->tick(gTextureFX[1]);

    Level_init(lvl, 256, 256, 64);
    LevelRenderer_init(lr, lvl, texTerrain);
    calcLightDepths(lvl, 0, 0, lvl->width, lvl->height);

    Player_init(p, lvl);

    ParticleEngine_init(&particleEngine, lvl, (GLuint)texTerrain);

    // c0.0.13a_03 no longer auto spawns any zombies at world start
    mobCount = 0;

    Timer_init(&timer, 20.0f);

    // c0.0.16a_02: a host argument switches to multiplayer. The boot level
    // above stays only as a placeholder until LevelFinalize replaces it
    if (gConnectHost[0]) {
        // matches the real client's Player starting at (0,0,0): its Level is
        // null at this point in multiplayer, so Entity's constructor leaves
        // it at the origin and resetPos() no-ops rather than searching a
        // (nonexistent) spawn point. This placeholder level's own spawn point
        // has no relation to the real server's, so keeping it here would
        // just broadcast a more random wrong position, not a more correct one
        Entity_setPosition(&p->e, 0.0f, 0.0f, 0.0f);
        // c0.0.17a: connecting is asynchronous now, matching net/d.java's
        // constructor spawning a background Thread instead of blocking here.
        // tick() polls NetConnection_pollConnecting every frame until it
        // resolves, showing the "Connecting.." screen in the meantime
        NetConnection_beginConnect(&gConn, gConnectHost, gConnectPort, Minecraft_getUserName());
        gConnecting = true;
        gLoading = true;
    }

    return 1;
}

static void destroy(Level* lvl) {
    // don't let a downloaded server level overwrite the local singleplayer save
    if (!gConnected) Level_save(lvl);
    if (gConnected) NetConnection_close(&gConn);
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

// c0.0.19a_04: blits a sprite from the 256x64 gui.png atlas at pixel
// coordinates (u,v,w,h) to screen position (x,y). Matches the real source's
// own blit helper's UV math exactly (divide U by 256, V by 64)
static void drawGuiBlit(int x, int y, int u, int v, int w, int h) {
    float u0 = u / 256.0f, u1 = (u + w) / 256.0f;
    float v0 = v / 64.0f,  v1 = (v + h) / 64.0f;
    Tessellator_begin(&hudTess);
    Tessellator_vertexUV(&hudTess, (float)x,     (float)(y + h), -90.0f, u0, v1);
    Tessellator_vertexUV(&hudTess, (float)(x+w), (float)(y + h), -90.0f, u1, v1);
    Tessellator_vertexUV(&hudTess, (float)(x+w), (float)y,       -90.0f, u1, v0);
    Tessellator_vertexUV(&hudTess, (float)x,     (float)y,       -90.0f, u0, v0);
    Tessellator_end(&hudTess);
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

    // c0.0.19a_04: the new visual hotbar replaces the old top-right held
    // block preview entirely, matching the real source: the Hud class's
    // only 3D icon rendering is the hotbar slot loop below, no separate
    // corner preview exists anymore (the highlighted hotbar slot is now
    // the only "what am I holding" indicator). Background bar and
    // selection highlight come from gui.png (a 256x64 atlas: bar sprite at
    // (0,0) 182x22, highlight sprite at (0,22) 24x22, sliding 20px per
    // slot). Item icons are real tiny 3D isometric block renders using the
    // terrain atlas, not flat sprites
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGui);
    drawGuiBlit(screenWidth / 2 - 91, screenHeight - 22, 0, 0, 182, 22);

    drawGuiBlit(screenWidth / 2 - 91 - 1 + gSelectedSlot * 20, screenHeight - 22 - 1, 0, 22, 24, 22);

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_ALPHA_TEST);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, texTerrain);
    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < 9; ++i) {
        glPushMatrix();
        glTranslated((double)(screenWidth / 2 - 90 + i * 20), (double)(screenHeight - 16), -50.0);
        glScalef(10.0f, 10.0f, 10.0f);
        glTranslatef(1.0f, 0.5f, 0.0f);
        glRotatef(-30.0f, 1, 0, 0);
        glRotatef(45.0f, 0, 1, 0);
        glTranslatef(-1.5f, 0.5f, 0.5f);
        glScalef(-1.0f, -1.0f, -1.0f);

        Tessellator_begin(&hudTess);
        const Tile* slotTile = gTiles[gHotbar[i]];
        if (slotTile && slotTile->render) {
            slotTile->render(slotTile, &hudTess, &level, 0, -2, 0, 0);
        }
        Tessellator_end(&hudTess);

        glPopMatrix();
    }
    glDisable(GL_TEXTURE_2D);

    // top left corner: version and stats
    Font_drawShadow(&gFont, &hudTess, "0.0.20a_02", 2, 2, 0xFFFFFF);

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

    // c0.0.17a: new Tab "Connected players" overlay, shown while held and
    // connected. Coordinates and colors match the real source exactly
    // (a 256x148 gradient backdrop centered on screen, 2 column name list)
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && gConnected) {
        Screen_fillGradient(cx - 128, cy - 80, cx + 128, cy + 68, 0xB3000000u, 0xCC333333u);

        const char* header = "Connected players:";
        Font_drawShadow(&gFont, &hudTess, header, cx - Font_width(&gFont, header) / 2, cy - 76, 0xFFFFFF);

        const char* names[1 + MAX_NET_PLAYERS];
        int nameCount = 0;
        names[nameCount++] = Minecraft_getUserName();
        for (int i = 0; i < netPlayerCount; i++) {
            if (netPlayers[i].used) names[nameCount++] = netPlayers[i].name;
        }
        for (int b = 0; b < nameCount; b++) {
            int x = cx - 120 + (b % 2) * 120;
            int y = cy - 64 + ((b / 2) << 3);
            Font_draw(&gFont, &hudTess, names[b], x, y, 0xFFFFFF);
        }
    }

    // chat backlog, newest line just above the hotbar, older lines stacked
    // upward in 8px rows. c0.0.17a: storage is newest first (index 0), and
    // normally only the newest 10 lines under 200 ticks (10 real seconds)
    // old are shown, matching the old fading toast look; while the chat
    // input screen itself is open, the newest 20 lines show regardless of
    // age instead, a scrollback view layered on top of the normal HUD
    {
        bool chatOpen = activeScreen && ChatInputScreen_isThis(activeScreen);
        int maxLines = chatOpen ? 20 : 10;
        int ageLimit = (int)(timer.ticksPerSecond * 10.0f);

        int visibleCount = 0;
        for (int i = 0; i < chatLineCount && visibleCount < maxLines; i++) {
            if (!chatOpen && chatLines[i].age >= ageLimit) break; // strictly increasing with index
            visibleCount++;
        }
        for (int slot = 0; slot < visibleCount; slot++) {
            int y = screenHeight - 20 - ((slot + 1) << 3);
            Font_drawShadow(&gFont, &hudTess, chatLines[slot].text, 2, y, 0xFFFFFF);
        }
    }

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

    // c0.0.14a_08: Enter no longer saves, it sets the spawn point instead
    int enter = glfwGetKey(w, GLFW_KEY_ENTER);
    if (enter == GLFW_PRESS && prevEnter == GLFW_RELEASE) {
        Level_setSpawnPos(&level, (int)player.e.x, (int)player.e.y, (int)player.e.z, player.e.yRotation);
        Entity_resetPosition(&player.e);
    }
    prevEnter = enter;

    // 1..9 = select hotbar slot directly
    for (int i = 0; i < 9; ++i) {
        int k = glfwGetKey(w, GLFW_KEY_1 + i);
        if (k == GLFW_PRESS && prevNumKeys[i] == GLFW_RELEASE) gSelectedSlot = i;
        prevNumKeys[i] = k;
    }

    // c0.0.20a_02: B opens the full inventory/select block screen
    int kB = glfwGetKey(w, GLFW_KEY_B);
    if (kB == GLFW_PRESS && prevB == GLFW_RELEASE) {
        int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
        InventoryScreen_open(&gFont, fbw * 240 / fbh, 240, &level, gHotbar, &gSelectedSlot, texTerrain);
    }
    prevB = kB;

    // G = spawn zombie at player. c0.0.16a_02 disables this debug key whenever
    // connected to a server
    int g = glfwGetKey(w, GLFW_KEY_G);
    if (g == GLFW_PRESS && prevG == GLFW_RELEASE && !gConnected && mobCount < MAX_MOBS) {
        Zombie_init(&mobs[mobCount++], &level, player.e.x, player.e.y, player.e.z);
    }
    prevG = g;

    // Y = invert mouse Y axis
    int kY = glfwGetKey(w, GLFW_KEY_Y);
    if (kY == GLFW_PRESS && prevY == GLFW_RELEASE) {
        gYMouseAxis *= -1;
    }
    prevY = kY;

    // F = cycle draw distance. c0.0.17a: reverse cycles when either Shift is held
    int kF = glfwGetKey(w, GLFW_KEY_F);
    if (kF == GLFW_PRESS && prevF == GLFW_RELEASE) {
        bool shiftHeld = glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                         glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        LevelRenderer_toggleDrawDistance(&levelRenderer, shiftHeld);
    }
    prevF = kF;

    // c0.0.16a_02: T opens chat. Minecraft_setScreen releases the held
    // movement keys itself, same as any other screen opening.
    // c0.0.17a: T now does nothing at all without an active connection,
    // instead of opening a chat screen whose Enter handler would crash.
    // This is the real client's own fix for that singleplayer NPE dead end,
    // not a deviation this port needs to keep working around anymore
    int kT = glfwGetKey(w, GLFW_KEY_T);
    if (kT == GLFW_PRESS && prevT == GLFW_RELEASE && gConnected) {
        int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
        ChatInputScreen_open(&gFont, fbw * 240 / fbh, 240);
    }
    prevT = kT;
}

static void mineOrPlace(void);

static void handleBlockClicks(GLFWwindow* w) {
    int left  = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT);
    int right = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT);
    int middle = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_MIDDLE);

    // boot time idle state, no screen open yet, cursor just has not been grabbed
    if (glfwGetInputMode(w, GLFW_CURSOR) != GLFW_CURSOR_DISABLED &&
        (left == GLFW_PRESS || right == GLFW_PRESS)) {
        Minecraft_closeScreenAndGrabMouse();

        // consume this edge so it doesn't also act this frame
        prevLeft = left;
        prevRight = right;
        prevMiddle = middle;
        return;
    }

    if (right == GLFW_PRESS && prevRight == GLFW_RELEASE) {
        gEditMode = (gEditMode + 1) % 2;
    }
    prevRight = right;

    // c0.0.14a_08: middle click picks the block under the crosshair (grass
    // picks dirt instead of grass itself), only if it's one of the 9 hotbar
    // tiles, matching the real hotbar-membership check
    if (middle == GLFW_PRESS && prevMiddle == GLFW_RELEASE && !isHitNull) {
        int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
        if (id == TILE_GRASS.id) id = TILE_DIRT.id;
        for (int i = 0; i < 9; ++i) {
            if (gHotbar[i] == id) gSelectedSlot = i;
        }
    }
    prevMiddle = middle;

    // left click performs the current mode: on a fresh press immediately,
    // then c0.0.14a_08 auto-repeats every ticksPerSecond/4 ticks (~5 ticks at
    // 20 TPS) for as long as the button stays held, instead of requiring
    // discrete repeated clicks
    if (left == GLFW_PRESS && prevLeft == GLFW_RELEASE && !isHitNull) {
        mineOrPlace();
        gLastMineTick = gTickCount;
    } else if (left == GLFW_PRESS && !isHitNull && (gTickCount - gLastMineTick) >= timer.ticksPerSecond / 4.0f) {
        mineOrPlace();
        gLastMineTick = gTickCount;
    }
    prevLeft = left;
}

static void mineOrPlace(void) {
    if (gEditMode == 0) {
        // destroy
        int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
        // c0.0.20a_02: Bedrock can't be broken unless userType>=100 (an
        // operator flag from the Login reply). Stays 0 in singleplayer, so
        // Bedrock is unbreakable there too, matching the real source exactly
        if (id == TILE_BEDROCK.id && player.userType < 100) return;
        const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
        // c0.0.19a_04: the player's own break/place always uses the
        // ungated Level_netSetTile, matching the real source calling
        // netSetTile directly here, not the gated setTile. Only derivative
        // world-simulation reactions (grass spread, liquid flow, falling
        // blocks, sponge) route through the gated level_setTile/Level_swap
        bool changed = Level_netSetTile(&level, hitResult.x, hitResult.y, hitResult.z, 0);
        if (t && changed) {
            // type field is the currently selected tile even on destroy,
            // matching the real source; the server ignores it for mode 0
            if (gConnected) NetConnection_sendSetBlock(&gConn, hitResult.x, hitResult.y, hitResult.z, 0, gHotbar[gSelectedSlot]);
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

        // AABB collision check, disallow placing inside player, mobs, or
        // connected network players. Matches Level.isFree(AABB), which walks
        // the real source's unified entities list. This port keeps
        // mobs[]/netPlayers[] as separate arrays instead, so both need
        // checking here to get the same result
        AABB aabb = Level_getTilePickAABB(&level, x, y, z);
        if (!AABB_intersects(&player.e.boundingBox, &aabb)) {
            bool blocked = false;
            for (int i = 0; i < mobCount; ++i) {
                if (AABB_intersects(&mobs[i].base.boundingBox, &aabb)) { blocked = true; break; }
            }
            for (int i = 0; !blocked && i < netPlayerCount; ++i) {
                if (netPlayers[i].used && AABB_intersects(&netPlayers[i].base.boundingBox, &aabb)) { blocked = true; }
            }
            if (!blocked) {
                if (gConnected) NetConnection_sendSetBlock(&gConn, x, y, z, 1, gHotbar[gSelectedSlot]);
                Level_netSetTile(&level, x, y, z, gHotbar[gSelectedSlot]);
            }
        }
    }
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

// same reasoning as handleScreenClicks' prevLeft/prevRight sync above, but
// for handleGameplayKeys' trackers: Enter still held down from sending a
// chat message read as a fresh press the frame gameplay polling resumes,
// setting the spawn point right after chat closes.
static void syncGameplayKeyEdges(GLFWwindow* w) {
    prevR     = glfwGetKey(w, GLFW_KEY_R);
    prevEnter = glfwGetKey(w, GLFW_KEY_ENTER);
    for (int i = 0; i < 9; ++i) prevNumKeys[i] = glfwGetKey(w, GLFW_KEY_1 + i);
    prevG = glfwGetKey(w, GLFW_KEY_G);
    prevY = glfwGetKey(w, GLFW_KEY_Y);
    prevF = glfwGetKey(w, GLFW_KEY_F);
    prevT = glfwGetKey(w, GLFW_KEY_T);
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

    // c0.0.14a_08 merges the old lit+shadow solid passes into one, colored
    // per face by Level_getBrightness instead of a binary lit/shadow choice,
    // so zombies and particles no longer need a separate lit/shadow split
    // either (they now tint themselves via Entity_getBrightness)
    setupFog(0);
    LevelRenderer_render(lr, p, 0);
    // c0.0.20a_02: unlit sapling/flower/mushroom sprites, split out of the
    // liquid layer into their own single pass list so their self-intersecting
    // cross-quad geometry doesn't get cut off by the liquid list's
    // depth-prepass-then-color double draw further down
    LevelRenderer_render(lr, p, 2);

    setupEntityLighting(true);
    for (int i = 0; i < mobCount; ++i) {
        const Zombie* z = &mobs[i];
        if (frustum_isVisible(&frustum, &z->base.boundingBox)) {
            Zombie_render(z, t);
        }
    }

    for (int i = 0; i < netPlayerCount; ++i) {
        const NetworkPlayer* np = &netPlayers[i];
        if (frustum_isVisible(&frustum, &np->base.boundingBox)) {
            NetworkPlayer_render(np, t, p->e.yRotation, &gFont);
        }
    }
    setupEntityLighting(false);

    ParticleEngine_render(&particleEngine, p, t);

    LevelRenderer_renderSurroundingGround(lr);

    // new cloud layer, rendered fully unlit under its own linear fog setup,
    // sandwiched between the ground skirt and the hit highlight
    glDisable(GL_LIGHTING);
    setupFog(-1);
    LevelRenderer_renderClouds(lr, t);
    setupFog(1);
    glEnable(GL_LIGHTING);

    if (!isHitNull) {
        glDisable(GL_LIGHTING);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &player, &hitResult, gEditMode, gHotbar[gSelectedSlot]);
        LevelRenderer_renderHitOutline(&hitResult, gEditMode);
        glEnable(GL_ALPHA_TEST);
        glEnable(GL_LIGHTING);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setupFog(0);
    LevelRenderer_renderSurroundingWater(lr);

    // liquid layer (renumbered down from 2, no more separate shadow pass),
    // drawn twice: once depth only so the translucent surface does not
    // overdraw itself, then for real with color writes back on
    glEnable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    LevelRenderer_render(lr, p, 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    LevelRenderer_render(lr, p, 1);
    glDisable(GL_BLEND);

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    if (!isHitNull) {
        glDepthFunc(GL_LESS);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &player, &hitResult, gEditMode, gHotbar[gSelectedSlot]);
        LevelRenderer_renderHitOutline(&hitResult, gEditMode);
        glEnable(GL_ALPHA_TEST);
        glDepthFunc(GL_LEQUAL);
    }

    drawGui(t);

    glfwSwapBuffers(window);

    // c0.0.20a_02: the 100fps cap added in c0.0.19a_04 (Display.sync(100) in
    // the real source) is removed with no replacement, uncapped again
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
        // captured before glfwPollEvents, since a key callback (Enter, Esc)
        // can close activeScreen right there, and gameplay key polling must
        // not run the same frame a screen closed, or the key still physically
        // held down reads as a fresh press, e.g. closing chat with Enter also
        // setting the spawn point
        bool screenWasActive = activeScreen != NULL;

        glfwPollEvents();

        Timer_advanceTime(&timer);
        for (int i = 0; i < timer.ticks; ++i) tick(p, window);

        // while connecting/downloading a level, skip world interaction and
        // the normal render entirely. The loading screen is drawn
        // synchronously from inside tick()'s packet handling instead,
        // matching the real source's separate !isLoading render gate
        if (!gLoading) {
            pick(timer.partialTicks);

            if (!screenWasActive) {
                handleBlockClicks(window);
                handleGameplayKeys(window);
                // handleGameplayKeys can open a screen mid frame (T for chat),
                // so recheck before polling raw key state back into the player,
                // otherwise the still held movement key gets re-armed the same
                // frame Player_releaseAllKeys just cleared it
                if (!activeScreen) Player_pollKeys(p, window);
            } else {
                handleScreenClicks(window);
                syncGameplayKeyEdges(window);
                // handleScreenClicks's button callback can close the screen,
                // setting activeScreen to null mid frame, so recheck before use.
                if (activeScreen && activeScreen->tick) activeScreen->tick(activeScreen);
            }

            int built = LevelRenderer_updateDirtyChunks(&levelRenderer, &player);
            gChunkUpdatesAccum += built;

            render(lvl, lr, p, window, timer.partialTicks);
        }

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

// c0.0.16a_02: the real client picks singleplayer vs multiplayer from an
// applet host parameter set once at launch. Porting that as command line
// args to the exe instead: mc_client.exe [host] [port] [username],
// no args = singleplayer, username defaults to "guest" if omitted
int main(int argc, char** argv) {
    if (argc >= 2) {
        snprintf(gConnectHost, sizeof gConnectHost, "%s", argv[1]);
        if (argc >= 3) gConnectPort = atoi(argv[2]);
        if (argc >= 4) snprintf(gConnectUsername, sizeof gConnectUsername, "%s", argv[3]);
    }

    run(&level, &levelRenderer, &player);
    return EXIT_SUCCESS;
}

static void tick(Player* p, GLFWwindow* w) {
    (void)w;

    gTickCount++;

    // c0.0.19a_04: advance the water/lava animation one step and patch the
    // result directly into the live terrain.png texture at that tile's
    // atlas cell (16 pixels per cell, 16 cells per row)
    glBindTexture(GL_TEXTURE_2D, texTerrain);
    for (int i = 0; i < 2; ++i) {
        TextureFX* fx = gTextureFX[i];
        fx->tick(fx);
        int cellX = (fx->tileId % 16) << 4;
        int cellY = (fx->tileId / 16) << 4;
        glTexSubImage2D(GL_TEXTURE_2D, 0, cellX, cellY, TEXTURE_FX_SIZE, TEXTURE_FX_SIZE,
                         GL_RGBA, GL_UNSIGNED_BYTE, fx->pixels);
    }

    // c0.0.17a: age no longer evicts a line from the backlog, only the 50
    // line cap does (see Minecraft_addChatLine). Age now only affects
    // render time visibility in the HUD, letting old messages persist in a
    // scrollback instead of truly expiring
    for (int i = 0; i < chatLineCount; i++) {
        chatLines[i].age++;
    }

    // matches f(): drains queued packets, then unconditionally broadcasts
    // the local player's own position every tick, no dirty check, no rate
    // limiting, even before its real SpawnPlayer(-1) arrives. The real
    // client's own Player starts at exactly (0,0,0) in multiplayer (Entity's
    // constructor always does setPos(0,0,0), and resetPos() is a no-op when
    // level is null, which it is until LevelFinalize), so other clients
    // genuinely do see a newly connecting player appear briefly at world
    // origin before snapping to their real spawn point. Confirmed original
    // quirk, not a bug, see the matching (0,0,0) reset in the connect
    // branch of init()
    if (gConnecting) {
        if (NetConnection_pollConnecting(&gConn)) {
            gConnecting = false;
            gConnected = gConn.connected; // false here means the connect failed and the message screen is already up
        }
    }
    if (gConnected) {
        NetConnection_tick(&gConn);
        // c0.0.19a_04: withheld until the level finishes downloading, so the
        // server doesn't see movement data arrive mid-download and throttle
        // the client for "moving too fast" before it's even finished loading
        if (gConnected && gConn.levelLoaded) {
            NetConnection_sendTeleportSelf(&gConn, p->e.x, p->e.y, p->e.z, p->e.yRotation, p->e.xRotation);
        }
    }

    LevelRenderer_tick(&levelRenderer); // scrolls the new cloud layer

    // c0.0.16a_02: the server is authoritative for block state in
    // multiplayer, so the client never runs its own random tile ticks
    // (grass growth/decay) while connected
    if (!gConnected) Level_onTick(&level);

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

    for (int i = 0; i < netPlayerCount; i++) {
        NetworkPlayer_onTick(&netPlayers[i]);
    }
}