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
#include "options.h"
#include "audio/sound.h"
#include "user.h"
#include "character/creature.h"
#include "character/cube.h"
#include "item/arrow.h"
#include "item/item.h"
#include "item/sign.h"
#include "timer.h"
#include "hitresult.h"
#include "gui/font.h"
#include "gui/screen.h"
#include "gui/pause_screen.h"
#include "gui/chat_input_screen.h"
#include "gui/message_screen.h"
#include "gui/game_over_screen.h"
#include "net/network_player.h"

#define MAX_MOBS 256 // c0.0.14a_08 raises the mob cap from 100
#define MAX_NET_PLAYERS 32
#define MAX_ARROWS 64 // no cap in the real source; bounded here same as mobs/netPlayers
#define MAX_ITEMS 256 // no cap in the real source either; same reasoning
#define MAX_SIGNS 64 // no cap in the real source either; same reasoning

static GLFWwindow*    window;
static Level          level;
static LevelRenderer  levelRenderer;
static Player         player;
static Timer          timer;
static Creature       mobs[MAX_MOBS];
static Arrow          arrows[MAX_ARROWS];
static Item           items[MAX_ITEMS];
static Sign           signs[MAX_SIGNS];
static ParticleEngine particleEngine;
static User           user;

static int mobCount = 0;
static int arrowCount = 0;
static int itemCount = 0;
static int signCount = 0;

// c0.25_05_st: first person hand/held item state, matches the real source's
// own HeldItemRenderer fields (h2.b/c/d/e/f in the decompile): heldTileId is
// the tile id currently shown in hand (-1 for the empty hand pose, mirrors
// h2.b being null), heldBlend/prevHeldBlend crossfade toward 1.0 whenever
// the inventory's own selected tile matches heldTileId (and toward 0.0 the
// moment it changes, at which point heldTileId snaps to the new selection
// once the fade has mostly bottomed out), driving both a rise/dip animation
// and, if ever needed, a transparency style blend. handSwinging/handSwingTicks
// track the 7 tick punch animation, triggered once per left click event
// regardless of whether that click actually hit anything
static int   heldTileId = -1;
static float heldBlend = 0.0f, prevHeldBlend = 0.0f;
static bool  handSwinging = false;
static int   handSwingTicks = -1;
static Cube  handArmCube;
static bool  handArmCubeBuilt = false;

static NetworkPlayer netPlayers[MAX_NET_PLAYERS];
static int netPlayerCount = 0;

static int prevLeft  = GLFW_RELEASE;
static int prevRight = GLFW_RELEASE;
static int prevMiddle = GLFW_RELEASE;
static int prevEnter = GLFW_RELEASE;
static int prevNumKeys[9]; // GLFW_KEY_1..GLFW_KEY_9, initialized to GLFW_RELEASE in init()
static int prevF    = GLFW_RELEASE;
static int prevR    = GLFW_RELEASE;
static int prevT    = GLFW_RELEASE;
static int prevTab  = GLFW_RELEASE; // c0.24_st_03: shoots an arrow, singleplayer only
static int prevB    = GLFW_RELEASE; // c0.24_st_03: places a Sign, singleplayer only

// c0.24_st_03: the hotbar is now backed directly by player.inventory (real
// survival inventory: 9 slots, each an id+count, starting empty), replacing
// every earlier version's Creative style always-full gHotbar[]/gSelectedSlot
// pair. No more free block picking (the B key full inventory/select block
// screen and middle click "pick block" are both gone, matching the real
// source having no such thing here). The only way to get a tile into a
// slot is to break one and walk over the Item it drops

static int gTickCount = 0;
static int gLastMineTick = 0;

// c0.24_st_03: real progressive mining tracker (matches SurvivalGameMode's
// own c/d/e=target x/y/z, f=progress ticks, g=captured hardness, h=short
// post-break cooldown). gMineFraction is the partial-tick-blended 0..1
// progress used to drive the crack overlay texture, recomputed every frame;
// everything else only advances once per elapsed real game tick so it's
// frame rate independent
static int gMineX = -1, gMineY = -1, gMineZ = -1;
static int gMineProgress = 0;
static int gMineLastTick = 0;
static int gMineCooldownUntil = 0;
static float gMineFraction = 0.0f;

static int gEditMode = 0;              // 0=destroy, 1=place

// c0.0.23a_01: remappable keybindings and toggles, persisted to options.txt.
// Replaces the old fixed Y key mouse invert, F key draw distance, and
// always on FPS display
static Options gOptions;

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
static int texIcons = 0; // c0.24_st_03: crosshair/hearts/air bubbles atlas

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

// c0.0.23a_01 (wiki confirmed, matches c/m.java): name currently hovered in
// the Tab "connected players" overlay, empty when none/not held. Read by
// ChatInputScreen's own mouseClicked to paste it into the message being typed
static char gHoveredTabName[65] = "";

// c0.25_05_st: fixed, was this port's own sky color (0.5,0.8,1.0) copied in
// here rather than the real source's own distinct Level.fogColor field.
// Bytecode confirmed: fogColor and skyColor are two separate fields, and
// fogColor's only ever-reached default (0xFFFFFF, plain white) is never
// overwritten anywhere else in this version's own code, so hardcoding it
// exactly like this port already hardcodes skyColor is a safe simplification
static GLfloat fogColorDaylight[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static GLfloat fogColorShadow  [4] = {  14.0f/255.0f,  11.0f/255.0f,  10.0f/255.0f, 1.0f };

static int      isHitNull = 1;
static HitResult hitResult;

static PauseScreen gPauseScreen;
static GameOverScreen gGameOverScreen;
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
    PauseScreen_init(&gPauseScreen, &gFont, screenWidth, screenHeight, &gOptions);
    Minecraft_setScreen((Screen*)&gPauseScreen);
}

// matches Minecraft.grabMouse(): grabs cursor again and closes any open screen
void Minecraft_closeScreenAndGrabMouse(void) {
    // c0.24_st_03: grabMouse() calls setScreen(null) itself as its last
    // step, so a dead player's own setScreen(null) substitution back to the
    // Game over screen applies here too. Without this, Escape on the Game
    // over screen (routed here by Screen_defaultKeyPressed) would grab the
    // mouse and hide the cursor with no screen left to show
    if (player.e.health <= 0) {
        Minecraft_setScreen(NULL);
        return;
    }
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

    // must happen before Level_resize, not after. World gen's own
    // "Spawning.." stage, level_gen.c's LevelGen_maybeSpawnMobs, now
    // populates mobs[] as part of Level_resize's own Level_generateMap call
    // below, via the same mobCount indexed array this resets. Clearing
    // mobCount afterward would silently wipe every mob world gen just spawned
    mobCount = 0;
    arrowCount = 0;
    itemCount = 0;
    signCount = 0;

    LevelRenderer_destroy(&levelRenderer);
    Level_resize(&level, size, size, 64);
    LevelRenderer_init(&levelRenderer, &level, texTerrain);
    levelRenderer.drawDistance = gOptions.viewDistance;

    // undo Player_die's shrunk hitbox, near zero heightOffset, and dead
    // health state, so a fresh level after dying isn't stuck permanently
    // dead. Matches real source's own Player.resetPos() override exactly:
    // heightOffset=1.62f plus setSize(0.6,1.8) before resetPos's own
    // position search runs. Player_die sets heightOffset to 0.1f for the
    // keel over death pose, and every subsequent Entity_move computes eye
    // height as feetY plus heightOffset, so restoring it here is what puts
    // eye level back at standing height instead of pinned near the floor.
    // Size must be restored before resetPosition, since that rebuilds the
    // bounding box from whatever width and height is currently set
    player.e.heightOffset = 1.62f;
    Entity_setSize(&player.e, 0.6f, 1.8f);
    Mob_init(&player.e);
    player.score = 0;

    // real source's own level change path, k.java's setLevel, constructs a
    // brand new Player object every single time a level is generated or
    // loaded, whether or not the previous one died. A fresh Player
    // naturally has a fresh, empty inventory and arrows back to 20,
    // Player.java's own field initializers. This port keeps one persistent
    // Player struct instead of reallocating it, so the equivalent is
    // resetting these explicitly here. Both cases, dying or not, should
    // behave identically: hotbar and arrow count always reset on regen
    Inventory_init(&player.inventory);
    player.arrows = 20;

    Entity_resetPosition(&player.e);
}

// matches Minecraft.setScreen(): any screen release mouse look, letting the
// player click and read text normally instead of aiming the camera. Chat
// used to skip this, leaving the camera live behind the text box.
void Minecraft_setScreen(Screen* screen) {
    // c0.24_st_03: matches the real source's own substitution inside
    // setScreen: a request to close down to no screen at all is overridden
    // to the Game over screen whenever the player is dead, so it can't be
    // escaped except through its own "Generate new level..." button
    if (!screen && player.e.health <= 0) {
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        int screenWidth  = fbw * 240 / fbh;
        int screenHeight = 240;
        GameOverScreen_init(&gGameOverScreen, &gFont, screenWidth, screenHeight, player.score);
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
    levelRenderer.drawDistance = gOptions.viewDistance;
    mobCount = 0;
    arrowCount = 0;
    itemCount = 0;
    signCount = 0;
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

// c0.0.23a_01: lets gui/screen.c's shared button renderer bind gui.png's new
// button texture strip without every screen needing its own copy of texGui
int Minecraft_getGuiTexture(void) {
    return texGui;
}

// c0.0.23a_01: returns the name currently hovered in the Tab overlay, or
// NULL if none/not held, for ChatInputScreen's own click handler to paste
const char* Minecraft_getHoveredTabName(void) {
    return gHoveredTabName[0] ? gHoveredTabName : NULL;
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
// c0.24_st_03: the c0.0.21a "Select block screen stays scrollable while
// open" exception is gone along with that screen itself (see the removed
// InventoryScreen note above), so this is back to a plain "only while no
// screen is up" gate
static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset) {
    (void)w; (void)xoffset;
    if (activeScreen) return;

    int dir = (yoffset > 0.0) ? 1 : (yoffset < 0.0) ? -1 : 0;
    if (dir == 0) return;

    Inventory_scroll(&player.inventory, dir);
}

// c0.0.14a_08: daylight fog switched from exponential to linear, with its far
// end tied to the draw distance toggle (see LevelRenderer_getFogEndDistance).
// type -1 is new: sets up the same linear daylight fog for cloud rendering,
// but returns immediately without touching lighting/color material state
// (clouds render fully unlit).
static void setupFog(int type) {
    if (type == -1) {
        // every other branch below explicitly enables GL_FOG; this one
        // skipped it and relied on it still being enabled from the earlier
        // terrain pass, which is fragile (order dependent, and drawGui's own
        // glDisable(GL_FOG) or any future reordering would silently leave
        // clouds/sky completely unfogged)
        glEnable(GL_FOG);
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
        window = glfwCreateWindow(mode->width, mode->height, "Minecraft 0.25_05   SURVIVAL TEST", monitor, NULL);
    } else {
        window = glfwCreateWindow(gWinWidth, gWinHeight, "Minecraft 0.25_05   SURVIVAL TEST", NULL, NULL);
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
    Options_init(&gOptions);
    Textures_setAnaglyph(gOptions.anaglyph3d);
    Sound_init(gOptions.music, gOptions.sound);

    // c0.24_st_03: no more pre-filled Creative hotbar. player.inventory
    // (initialized in Player_init below) starts empty, matching the real
    // source's own player/d.java constructor

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
    texIcons   = loadTexture("resources/icons.png", GL_NEAREST);
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

    // must happen before Level_init, not after. World gen's own
    // "Spawning.." stage, level_gen.c's LevelGen_maybeSpawnMobs, populates
    // mobs[] as part of Level_init's own Level_generateMap call below, when
    // booting into a freshly generated map rather than loading a save. These
    // are already 0 from their own static initializers at this point in the
    // program, but resetting them again after Level_init would silently wipe
    // every mob world gen just spawned
    mobCount = 0;
    arrowCount = 0;
    itemCount = 0;
    signCount = 0;

    Level_init(lvl, 256, 256, 64);
    LevelRenderer_init(lr, lvl, texTerrain);
    lr->drawDistance = gOptions.viewDistance; // c0.0.23a_01: persisted option, not always 0 (FAR) on init
    calcLightDepths(lvl, 0, 0, lvl->width, lvl->height);

    Player_init(p, lvl);
    Level_setPlayer(lvl, &p->e); // c0.24_st_03: mob AI chase/attack target

    ParticleEngine_init(&particleEngine, lvl, (GLuint)texTerrain);

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
    Sound_shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}

/* camera */

// matches a/d.java's a(float): an always-on camera rotation, not gated by
// any option. Spins the view when dead (death-spiral, formula/constants
// bytecode matched exactly), and shakes it side to side, decaying over
// hurtDuration, whenever the player was just hit. hurtDir is the angle
// toward whatever hurt the player (or a random 0/180 flip for environmental
// damage, see Mob_hurt/Mob_knockback), so the shake itself always happens
// around that same axis regardless of view direction
static void applyHurtTilt(Player* p, float t) {
    Entity* e = &p->e;
    float hurtProgress = (float)e->hurtTime - t;
    if (e->health <= 0) {
        float deathProgress = (float)e->deathTime + t;
        glRotatef(40.0f - 8000.0f / (deathProgress + 200.0f), 0.0f, 0.0f, 1.0f);
    }
    if (hurtProgress < 0.0f) return;
    hurtProgress /= (float)e->hurtDuration;
    float shake = sinf(hurtProgress * hurtProgress * hurtProgress * hurtProgress * (float)M_PI);
    float dir = e->hurtDir;
    glRotatef(-dir, 0.0f, 1.0f, 0.0f);
    glRotatef(-shake * 14.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(dir, 0.0f, 1.0f, 0.0f);
}

// matches a/d.java's b(float): the view-bob effect, gated by the bobView
// option. walkDistBob's phase drives a sideways sway (sin) and a bounce
// (abs cos) timed to footsteps, tilt adds a small pitch-back as the player
// falls, all smoothed between ticks by the partial-tick blend just like
// every other rendered/interpolated value in this port
static void applyViewBob(Player* p, float t) {
    float walkDelta = p->walkDistBob - p->walkDistBobO;
    float walk = p->walkDistBob + walkDelta * t;
    float bob = p->oBob + (p->bob - p->oBob) * t;
    float tilt = p->oTilt + (p->tilt - p->oTilt) * t;
    glTranslatef(sinf(walk * (float)M_PI) * bob * 0.5f,
                 -fabsf(cosf(walk * (float)M_PI) * bob), 0.0f);
    glRotatef(sinf(walk * (float)M_PI) * bob * 3.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(fabsf(cosf(walk * (float)M_PI + 0.2f) * bob) * 5.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(tilt, 1.0f, 0.0f, 0.0f);
}

// pass is 0/1, only meaningful (and only ever nonzero-effect) when
// anaglyph3d is on: the whole frame renders twice, once per eye, and this
// is the modelview half of that eye separation, matching l.java's own
// (pass*2-1)*0.1f translate exactly, applied before anything else so it
// shifts the entire camera including hurt-tilt/bob/look-rotation
static void moveCameraToPlayer(Player* p, float t, int pass) {
    if (gOptions.anaglyph3d) {
        glTranslatef((float)(pass * 2 - 1) * 0.1f, 0.0f, 0.0f);
    }

    applyHurtTilt(p, t);
    if (gOptions.bobView) applyViewBob(p, t);

    // matches l.java's own -0.1f eye offset here exactly (was wrongly -0.3f,
    // a stale constant from before hurt-tilt/view-bob existed in this port)
    glTranslatef(0.0f, 0.0f, -0.1f);

    glRotatef(p->e.xRotation, 1.0f, 0.0f, 0.0f);
    glRotatef(p->e.yRotation, 0.0f, 1.0f, 0.0f);

    double x = p->e.prevX + (p->e.x - p->e.prevX) * t;
    double y = p->e.prevY + (p->e.y - p->e.prevY) * t;
    double z = p->e.prevZ + (p->e.z - p->e.prevZ) * t;

    glTranslated(-x, -y, -z);
}

static void setupCamera(Player* p, float t, int pass) {
    int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // matches l.java's own -(pass*2-1)*0.07f translate here: the projection
    // half of the anaglyph eye separation, an asymmetric frustum shift in
    // the opposite direction of moveCameraToPlayer's modelview shift, so
    // both eyes still converge on the same focal plane
    if (gOptions.anaglyph3d) {
        glTranslatef(-(float)(pass * 2 - 1) * 0.07f, 0.0f, 0.0f);
    }

    // c0.24_st_03: FOV narrows as the death animation plays, formula and
    // constants matched exactly against the real source (k.java, the block
    // right before its own gluPerspective call)
    double fov = 70.0;
    if (p->e.health <= 0) {
        double deathTime = (double)p->e.deathTime + (double)t;
        fov /= (1.0 - 500.0 / (deathTime + 500.0)) * 2.0 + 1.0;
    }
    gluPerspective(fov, (double)fbw / (double)fbh, 0.05, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    moveCameraToPlayer(p, t, pass);
}

// c0.0.19a_04: blits a sprite from the 256x64 gui.png atlas at pixel
// coordinates (u,v,w,h) to screen position (x,y). Matches the real source's
// own blit helper's UV math exactly (divide U by 256, V by 64)
static void drawGuiBlit(int x, int y, int u, int v, int w, int h) {
    float u0 = u / 256.0f, u1 = (u + w) / 256.0f;
    // c0.0.23a_01: gui.png grew from a 256x64 atlas to the real 256x256 one
    // (adding the button texture strip further down), so v is now also /256
    float v0 = v / 256.0f, v1 = (v + h) / 256.0f;
    Tessellator_begin(&hudTess);
    Tessellator_vertexUV(&hudTess, (float)x,     (float)(y + h), -90.0f, u0, v1);
    Tessellator_vertexUV(&hudTess, (float)(x+w), (float)(y + h), -90.0f, u1, v1);
    Tessellator_vertexUV(&hudTess, (float)(x+w), (float)y,       -90.0f, u1, v0);
    Tessellator_vertexUV(&hudTess, (float)x,     (float)y,       -90.0f, u0, v0);
    Tessellator_end(&hudTess);
}

static void drawGui(float partialTicks) {
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
    // selection highlight come from gui.png (256x256 as of c0.0.23a_01,
    // was 256x64 before the button texture strip was added lower down):
    // bar sprite at (0,0) 182x22, highlight sprite at (0,22) 24x22, sliding
    // 20px per slot. Item icons are real tiny 3D isometric block renders using the
    // terrain atlas, not flat sprites
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGui);
    drawGuiBlit(screenWidth / 2 - 91, screenHeight - 22, 0, 0, 182, 22);

    drawGuiBlit(screenWidth / 2 - 91 - 1 + player.inventory.selected * 20, screenHeight - 22 - 1, 0, 22, 24, 22);

    // c0.24_st_03: crosshair, moved from a manual untextured quad draw (a
    // leftover from before icons.png existed as an asset) to the real
    // source's own 16x16 sprite at icons.png (0,0)
    glBindTexture(GL_TEXTURE_2D, texIcons);
    drawGuiBlit(screenWidth / 2 - 7, screenHeight / 2 - 7, 0, 0, 16, 16);

    // c0.24_st_03: health hearts, matches c\l.java's own Hud.a() exactly.
    // invulnerableTime flicker: on 2 ticks out of every 6 (invulnerableTime/3
    // %2==1), except forced off during the last 10 ticks of the window, so
    // the "ghost preview of pending damage" hearts (drawn from lastHealth)
    // only show during the flicker-on frames early in a fresh hit
    {
        int flicker = (player.e.invulnerableTime / 3 % 2 == 1) ? 1 : 0;
        if (player.e.invulnerableTime < 10) flicker = 0;
        int health = player.e.health;
        int lastHealth = player.e.lastHealth;
        for (int i = 0; i < 10; ++i) {
            int bgFrame = flicker ? 1 : 0;
            int hx = screenWidth / 2 - 91 + (i << 3);
            int hy = screenHeight - 32;
            // c0.24_st_03: real source reseeds a fresh java.util.Random from
            // a frame counter each call for this jitter; this port uses
            // plain rand() instead (purely cosmetic, not save-file/network
            // visible, so exact PRNG-sequence fidelity isn't needed here)
            if (health <= 4 && (rand() % 2)) hy += 1;
            drawGuiBlit(hx, hy, 16 + bgFrame * 9, 0, 9, 9);
            if (flicker) {
                if ((i << 1) + 1 < lastHealth) drawGuiBlit(hx, hy, 70, 0, 9, 9);
                if ((i << 1) + 1 == lastHealth) drawGuiBlit(hx, hy, 79, 0, 9, 9);
            }
            if ((i << 1) + 1 < health) drawGuiBlit(hx, hy, 52, 0, 9, 9);
            if ((i << 1) + 1 == health) drawGuiBlit(hx, hy, 61, 0, 9, 9);
        }
    }

    // c0.24_st_03: air bubbles, only while actually submerged (eye level tile
    // is water), matches c/l.java:90 exactly. Standing in shallow/waist deep
    // water with isInWater() true but eyes above the surface must not show
    // or drain bubbles at all
    if (Entity_isUnderWater(&player.e)) {
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
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_ALPHA_TEST);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, texTerrain);
    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < 9; ++i) {
        // c0.24_st_03: empty slots (id -1) draw nothing, matching the real
        // source's own hotbar skipping unpopulated slots entirely
        int slotId = player.inventory.id[i];
        if (slotId < 0) continue;

        int hx = screenWidth / 2 - 90 + i * 20;
        int hy = screenHeight - 16;

        // re-enable and re-bind every iteration, not just once before the
        // loop. Font_drawShadow below, the stack count text once count[i]>1,
        // calls Font_draw_internal, which disables GL_TEXTURE_2D at its own
        // end in font.c, not just a different bind, texturing itself goes
        // off. Without redoing both here, every hotbar slot rendered after
        // the first stacked one in a given frame drew fully untextured,
        // flat white with whatever glColor was last set to, instead of a
        // wrong texture. Rebinding the texture alone is not enough since
        // glBindTexture is a no op while GL_TEXTURE_2D itself is disabled
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texTerrain);

        glPushMatrix();
        glTranslatef((float)hx, (float)hy, -50.0f);

        // c0.24_st_03: pickup "pop in" bounce/squash animation, driven by
        // the slot's own flash timer (Inventory.flash[], matches d.c[]),
        // matches the real source's exact formula
        int flash = player.inventory.flash[i];
        if (flash > 0) {
            float f3 = ((float)flash - partialTicks) / 5.0f;
            float f4 = -(float)sin((double)(f3 * f3) * M_PI) * 8.0f;
            float f5 = (float)sin((double)(f3 * f3) * M_PI) + 1.0f;
            float f6 = (float)sin((double)f3 * M_PI) + 1.0f;
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

        Tessellator_begin(&hudTess);
        const Tile* slotTile = gTiles[slotId];
        if (slotTile && slotTile->render) {
            slotTile->render(slotTile, &hudTess, &level, 0, -2, 0, 0);
        }
        Tessellator_end(&hudTess);

        glPopMatrix();

        // c0.24_st_03: stack count, right aligned like the real source,
        // only shown once there's more than 1 (a single item needs no count)
        if (player.inventory.count[i] > 1) {
            char countStr[8];
            snprintf(countStr, sizeof countStr, "%d", player.inventory.count[i]);
            int tw = Font_width(&gFont, countStr);
            Font_drawShadow(&gFont, &hudTess, countStr, hx + 19 - tw, hy + 6, 0xFFFFFF);
        }
    }
    glDisable(GL_TEXTURE_2D);

    // top left corner: version and stats
    // c0.25_05_st: real source's own version string for this build, note the
    // literal 3 spaces before "SURVIVAL", confirmed via decompile
    Font_drawShadow(&gFont, &hudTess, "0.25_05   SURVIVAL TEST", 2, 2, 0xFFFFFF);

    if (gOptions.showFrameRate) {
        char stats[64];
        snprintf(stats, sizeof stats, "%d fps, %d chunk updates", gFPS, gChunkUpdatesPerSec);
        Font_drawShadow(&gFont, &hudTess, stats, 2, 12, 0xFFFFFF);
    }

    // c0.25_05_st: score, top right corner, matches the HUD class's own
    // "Score: &e" + player.getScore(), right aligned by its own live
    // (color code stripped) string width
    char scoreMsg[48];
    snprintf(scoreMsg, sizeof scoreMsg, "Score: &e%d", player.score);
    Font_drawShadow(&gFont, &hudTess, scoreMsg, screenWidth - Font_width(&gFont, scoreMsg) - 2, 2, 0xFFFFFF);

    // c0.25_05_st: arrow count, just above the hotbar, matches the HUD
    // class's own "Arrows: " + player.arrows exactly, including its fixed
    // (not right aligned) position
    char arrowsMsg[32];
    snprintf(arrowsMsg, sizeof arrowsMsg, "Arrows: %d", player.arrows);
    Font_drawShadow(&gFont, &hudTess, arrowsMsg, screenWidth / 2 + 8, screenHeight - 33, 0xFFFFFF);

    int cx = screenWidth / 2;
    int cy = screenHeight / 2;

    // c0.24_st_03: the crosshair itself is now drawn earlier, alongside the
    // hearts/air bubbles, using the real icons.png sprite instead of a
    // manual untextured quad (see above)

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

        // c0.0.23a_01: hovering a name here highlights it and lets an open
        // chat screen's own click handler paste it into the message being
        // typed (confirmed against c/m.java + c/b.java, wiki calls this out
        // as new for this version). The real source only ever computes this
        // while a screen is open at all (the boolean passed into Hud.render
        // is literally "this.o != null"), matching the practical reality
        // that the mouse cursor is only visible/usable while a screen (in
        // practice, chat) already has it released: without that, the cursor
        // is a hidden, arbitrary accumulated position and hovering off it
        // would highlight a name you have no way to see yourself pointing at
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
                         HitResult* out, double* outDist) {
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

        // matches Level.clip() exactly, bytecode verified: the real
        // source's own tile collision type getter defaults to SOLID for
        // every tile and is only overridden by the liquid tile class, so
        // clip()'s collision type equals SOLID check is really just testing
        // for not a liquid, the same as this port's own mayPick, not the
        // physical movement isSolid test. Using isSolid here would wrongly
        // exclude any tile that is walkable but still solid for clip
        // purposes, such as Leaves, flowers, saplings, and glass, from
        // being targetable or breakable at all
        int id = Level_getTile(lvl, x, y, z);
        if (id != 0) {
            const Tile* tile = gTiles[id];
            if (tile && tile->mayPick(tile)) {
                if (out) hitresult_create(out, x, y, z, 0, (face < 0 ? 0 : face));
                if (outDist) *outDist = t;
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
    if (outDist) *outDist = maxDist;
    return 0;
}

static bool aabbContainsPoint(const AABB* box, double x, double y, double z) {
    return x >= box->minX && x <= box->maxX &&
           y >= box->minY && y <= box->maxY &&
           z >= box->minZ && z <= box->maxZ;
}

static void pick(float t) {
    double x = player.e.prevX + (player.e.x - player.e.prevX) * t;
    double y = player.e.prevY + (player.e.y - player.e.prevY) * t;
    double z = player.e.prevZ + (player.e.z - player.e.prevZ) * t;

    double dx, dy, dz;
    get_look_dir(&player, &dx, &dy, &dz);

    // nudge origin to match the 0.3 view space translate
    x += dx * 0.3; y += dy * 0.3; z += dz * 0.3;

    // c0.0.23a_01: real source's reach is a straight 5.0 block cast (Level.clip),
    // matching vanilla Minecraft's well known reach distance. The old axis
    // aligned 3 block cube check this replaces could reject a valid hit near
    // 5 blocks away when looking nearly along one axis (sqrt(a^2+b^2+c^2)<=5
    // does not imply each axis alone is <=3), so the raycast's own distance
    // cap is now the sole reach authority, matching the real single mechanism
    const double reach = 5.0;
    HitResult hr;
    double blockDist;
    bool hasBlock = raycast_block(&level, x, y, z, dx, dy, dz, reach, &hr, &blockDist) != 0;
    double bestDist = hasBlock ? blockDist : reach;

    // c0.24_st_03: entity picking, matches the real source's own combined
    // pick(): samples the ray in fixed 0.05 steps up to whichever distance
    // is currently closest (the block hit, or a nearer entity already
    // found), testing each candidate's own bounding box grown by 0.1. Only
    // mobs are scanned, matching this task's scope (melee/arrows resolve
    // against Creatures; connected NetworkPlayers have no server side
    // damage model to report a hit back to)
    Entity* hitEntity = NULL;
    for (int i = 0; i < mobCount; i++) {
        Entity* e = &mobs[i].e;
        if (e->removed) continue;
        AABB box = AABB_grow(&e->boundingBox, 0.1f, 0.1f, 0.1f);
        for (double s = 0.0; s < bestDist; s += 0.05) {
            if (aabbContainsPoint(&box, x + dx * s, y + dy * s, z + dz * s)) {
                bestDist = s;
                hitEntity = e;
                break;
            }
        }
    }

    if (hitEntity) {
        hitresult_createEntity(&hitResult, hitEntity);
        isHitNull = 0;
    } else if (hasBlock) {
        hitResult = hr;
        isHitNull = 0;
    } else {
        isHitNull = 1;
    }
}

// entity-vs-box overlap query for Arrow's own tick (item/arrow.c), matching
// the real source's level.blockMap query narrowed to isShootable entities.
// Linear scan of mobs[] plus the live player, since Player also extends Mob
// (isShootable) in the real source but isn't kept in mobs[] here; the owner
// is only excluded during its own first 5 ticks of flight, matching Arrow's
// own `entity == this.owner && this.time <= 5` grace window exactly, so an
// arrow can hit its own shooter once it's had a moment to actually leave
bool Minecraft_findArrowTarget(const AABB* box, const Entity* owner, int flightTime, Entity** outHit) {
    for (int i = 0; i < mobCount; i++) {
        Entity* e = &mobs[i].e;
        if (e->removed) continue;
        if (e == owner && flightTime <= 5) continue;
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

// implemented for mob_ai.c/creature.c's own Skeleton hooks: shooting while
// chasing, and its own death arrow scatter, spawns into arrows[]
bool Minecraft_spawnArrow(Level* lvl, Entity* owner, float x, float y, float z, float yaw, float pitch, float speed) {
    if (arrowCount >= MAX_ARROWS) return false;
    Arrow_init(&arrows[arrowCount++], lvl, owner, x, y, z, yaw, pitch, speed);
    return true;
}

// implemented for player.c's own Player_onTick, matches Arrow.playerTouch(Player):
// only picks back up an arrow this same player is owner of (a self fired
// shot, or a Skeleton's own death scatter, credited to the player at spawn
// time), and only once it's actually stuck, capped at the player's own carry
// limit. Starts the same 3 tick streak-to-player animation as
// Item_startPickup, see Arrow_onTick
void Minecraft_checkArrowPickups(Entity* playerEntity) {
    AABB box = AABB_grow(&playerEntity->boundingBox, 1.0f, 0.0f, 1.0f);
    Player* p = (Player*)playerEntity;
    for (int i = 0; i < arrowCount; i++) {
        Arrow* a = &arrows[i];
        if (a->e.removed || a->pickedUp || !a->hasHit) continue;
        if (a->owner != playerEntity) continue;
        if (p->arrows >= PLAYER_MAX_ARROWS) continue;
        if (!AABB_intersects(&a->e.boundingBox, &box)) continue;
        Arrow_startPickup(a, playerEntity);
        p->arrows++;
    }
}

// implemented for level.c's own Level_explode, matching the real source's
// own blockMap.getEntities(source,...) scan plus per entity distance based
// hurt call: linear scan of mobs[] plus the live player (this port's usual
// stand-in for a unified entity list), excluding the explosion's own
// source. Bytecode confirmed the distance ratio check compares the freshly
// computed distanceTo(source)/radius value itself, not the locally
// uninitialized variable CFR's own decompile of this method mis-scopes it as
// implemented for Creeper's own death explosion, matches the real source's
// own particle spawn there exactly: 500 particles textured as Leaves (real
// source's a.y constant, id 18 tex 22, confirmed via the tile registry;
// purely a texture reuse, not an actual leaves block spawn)
void Minecraft_spawnExplosionParticle(Level* lvl, float x, float y, float z, float mx, float my, float mz) {
    Particle p;
    Particle_init(&p, lvl, x, y, z, mx, my, mz, &TILE_LEAVES);
    ParticleEngine_add(&particleEngine, &p);
}

void Minecraft_hurtEntitiesInExplosion(Level* lvl, Entity* source, float x, float y, float z, float radius) {
    (void)lvl;
    for (int i = 0; i < mobCount; i++) {
        Entity* e = &mobs[i].e;
        if (e->removed || e == source) continue;
        float dx = e->x - x, dy = e->y - y, dz = e->z - z;
        float ratio = sqrtf(dx * dx + dy * dy + dz * dz) / radius;
        if (ratio <= 1.0f) Mob_hurt(e, source, (int)((1.0f - ratio) * 15.0f + 1.0f));
    }
    if (&player.e != source) {
        float dx = player.e.x - x, dy = player.e.y - y, dz = player.e.z - z;
        float ratio = sqrtf(dx * dx + dy * dy + dz * dz) / radius;
        if (ratio <= 1.0f) Mob_hurt(&player.e, source, (int)((1.0f - ratio) * 15.0f + 1.0f));
    }
}

// implemented for level.c's own Level_onTick, matching Level.tick()'s own
// mob population cap check, level.blockMap.all filtered to instanceof Mob.
// Player also extends Mob in the real source and lives in that same count,
// so the +1 stands in for the always present local player, which this
// port's own mobs[] array never holds
int Minecraft_countMobs(void) {
    return mobCount + 1;
}

// implemented for level_gen.c's own world gen mob population, level/a/a.java's
// "Spawning.." stage, spawns into mobs[]. Matches the real source's own
// order of constructing the mob, then discarding it without adding if its
// bounding box isn't actually free. Initializes into a scratch Creature
// first so the freeness check has a real bounding box to test against, the
// same as every other placement check in this port, see Entity_isFree
bool Minecraft_spawnMob(Level* lvl, int kind, float x, float y, float z) {
    if (mobCount >= MAX_MOBS) return false;
    Creature scratch;
    Creature_init(&scratch, (CreatureKind)kind, lvl, x, y, z);
    if (!Entity_isFree(&scratch.e, 0.0f, 0.0f, 0.0f)) return false;
    mobs[mobCount++] = scratch;
    mobs[mobCount - 1].e.ai = &mobs[mobCount - 1].ai; // reanchor self referential pointer after the copy
    return true;
}

// implemented for tile.c's own Tile_dropItems, spawns into items[]
void Minecraft_spawnItem(Level* lvl, float x, float y, float z, int resource) {
    if (itemCount >= MAX_ITEMS) return;
    Item_init(&items[itemCount++], lvl, x, y, z, resource);
}

// c0.24_st_03: matches Player.aiStep()'s own
// level.findEntities(this, bb.grow(1,0,1)).playerTouch(this) loop, narrowed
// to Item specifically since that's this port's only playerTouch consumer.
// Called from Player_onTick every tick
void Minecraft_checkItemPickups(Entity* playerEntity) {
    AABB box = AABB_grow(&playerEntity->boundingBox, 1.0f, 0.0f, 1.0f);
    for (int i = 0; i < itemCount; i++) {
        Item* it = &items[i];
        if (it->e.removed || it->pickedUp) continue;
        if (!AABB_intersects(&it->e.boundingBox, &box)) continue;
        if (Inventory_addResource(&player.inventory, it->resource)) {
            Item_startPickup(it, playerEntity);
        }
    }
}

/* input actions */

// c0.0.21a: pulled out of handleGameplayKeys so the (since removed, see
// c0.24_st_03's own InventoryScreen removal note above) Select block screen
// could also call it directly. Kept as its own function since
// handleGameplayKeys still calls it the same way
static void handleHotbarNumberKeys(GLFWwindow* w) {
    for (int i = 0; i < 9; ++i) {
        int k = glfwGetKey(w, GLFW_KEY_1 + i);
        if (k == GLFW_PRESS && prevNumKeys[i] == GLFW_RELEASE) player.inventory.selected = i;
        prevNumKeys[i] = k;
    }
}

static void handleGameplayKeys(GLFWwindow* w) {
    // R = reset position (c0.0.23a_01: remappable, "Load location")
    int r = glfwGetKey(w, gOptions.keys[OPT_KEY_LOAD_LOC].glfwKey);
    if (r == GLFW_PRESS && prevR == GLFW_RELEASE) {
        Entity_resetPosition(&player.e);
    }
    prevR = r;

    // c0.0.14a_08: Enter no longer saves, it sets the spawn point instead
    // c0.0.23a_01: remappable, "Save location"
    int enter = glfwGetKey(w, gOptions.keys[OPT_KEY_SAVE_LOC].glfwKey);
    if (enter == GLFW_PRESS && prevEnter == GLFW_RELEASE) {
        Level_setSpawnPos(&level, (int)player.e.x, (int)player.e.y, (int)player.e.z, player.e.yRotation);
        Entity_resetPosition(&player.e);
    }
    prevEnter = enter;

    // 1..9 = select hotbar slot directly
    handleHotbarNumberKeys(w);

    // c0.24_st_03: the "Build" key used to open the full inventory/select
    // block screen (c0.0.20a_02). That screen doesn't exist in the real
    // source for this version at all, since Survival Test has no Creative
    // style free block picker. The exact same binding (`this.B.n.b`, "Build"
    // is literally the field name in the real Options class) is repurposed
    // to place a Sign instead, singleplayer only, matching the real source's
    // own `this.y == null` gate exactly like the Tab/Arrow binding above
    int kB = glfwGetKey(w, gOptions.keys[OPT_KEY_BUILD].glfwKey);
    if (kB == GLFW_PRESS && prevB == GLFW_RELEASE && !gConnected && signCount < MAX_SIGNS) {
        Sign_init(&signs[signCount++], &level, player.e.x, player.e.y, player.e.z, player.e.yRotation);
    }
    prevB = kB;

    // Tab shoots an arrow, singleplayer only (matches the real source's own
    // `this.y == null` gate. `y` there is a connection reference, not the
    // Controls screen its own declared type would suggest; CFR picked the
    // wrong same-named obfuscated class across two different packages,
    // confirmed via raw bytecode). No conflict with the existing Tab
    // "connected players" overlay below: that one only ever shows while
    // gConnected, this only ever fires while !gConnected. c0.25_05_st: now
    // also gated on the player's own arrow count (spawns with 20, matches
    // Player.arrows), decremented per shot, speed bumped from the old debug
    // 0.8 to the real source's own 1.2 for a player's own shot
    int tab = glfwGetKey(w, GLFW_KEY_TAB);
    if (tab == GLFW_PRESS && prevTab == GLFW_RELEASE && !gConnected && player.arrows > 0 && arrowCount < MAX_ARROWS) {
        Arrow_init(&arrows[arrowCount++], &level, &player.e,
                   player.e.x, player.e.y, player.e.z, player.e.yRotation, player.e.xRotation, 1.2f);
        player.arrows--;
    }
    prevTab = tab;

    // c0.0.23a_01: Y no longer does anything at all as a direct key press
    // (confirmed absent from the real source's key handling entirely, not
    // just moved to a binding), matching the wiki's "no longer inverts the
    // mouse" note. invertMouseY is Options screen only now

    // F = cycle draw distance. c0.0.17a: reverse cycles when either Shift is
    // held. c0.0.23a_01: cycles gOptions.viewDistance directly (the now
    // persisted, canonical value) and syncs it into levelRenderer.drawDistance,
    // and the key itself is remappable ("Toggle fog")
    int kF = glfwGetKey(w, gOptions.keys[OPT_KEY_TOGGLE_FOG].glfwKey);
    if (kF == GLFW_PRESS && prevF == GLFW_RELEASE) {
        bool shiftHeld = glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                         glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        gOptions.viewDistance = (gOptions.viewDistance + (shiftHeld ? -1 : 1)) & 0x3;
        levelRenderer.drawDistance = gOptions.viewDistance;
        Options_save(&gOptions);
    }
    prevF = kF;

    // c0.0.16a_02: T opens chat. Minecraft_setScreen releases the held
    // movement keys itself, same as any other screen opening.
    // c0.0.17a: T now does nothing at all without an active connection,
    // instead of opening a chat screen whose Enter handler would crash.
    // This is the real client's own fix for that singleplayer NPE dead end,
    // not a deviation this port needs to keep working around anymore.
    // c0.0.23a_01: remappable, "Chat"
    int kT = glfwGetKey(w, gOptions.keys[OPT_KEY_CHAT].glfwKey);
    if (kT == GLFW_PRESS && prevT == GLFW_RELEASE && gConnected) {
        int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
        ChatInputScreen_open(&gFont, fbw * 240 / fbh, 240);
    }
    prevT = kT;
}

static void mineOrPlace(void);
static void resetMining(void);
static void updateMining(void);

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
    // picks dirt instead of grass itself). c0.24_st_03: this no longer
    // force-acquires the tile into a fixed Creative hotbar slot. It only
    // switches to a slot that already holds that tile id (Inventory_findSlot,
    // matches the real source's own inventory.a(tileId) lookup, returning -1
    // and leaving selection untouched if the player doesn't have any)
    if (middle == GLFW_PRESS && prevMiddle == GLFW_RELEASE && !isHitNull && hitResult.type == HITRESULT_TILE) {
        int id = Level_getTile(&level, hitResult.x, hitResult.y, hitResult.z);
        if (id == TILE_GRASS.id) id = TILE_DIRT.id;
        int slot = Inventory_findSlot(&player.inventory, id);
        if (slot >= 0) player.inventory.selected = slot;
    }
    prevMiddle = middle;

    // c0.25_05_st: the first person hand's own punch swing, matches the real
    // source triggering it unconditionally on every fresh left click event,
    // even one that hits nothing at all, before any mine/place/attack logic
    // even runs
    if (left == GLFW_PRESS && prevLeft == GLFW_RELEASE) {
        handSwinging = true;
        handSwingTicks = -1;
    }

    // c0.24_st_03: destroy mode on a tile is now real progressive mining
    // (see updateMining), checked every frame while held so progress
    // accumulates smoothly; entity attacks and placement keep the
    // pre-existing press-then-quarter-second-repeat cadence unchanged
    if (left == GLFW_PRESS && !isHitNull && hitResult.type == HITRESULT_TILE && gEditMode == 0) {
        updateMining();
        // matches l.java's own click repeat calling b(0) every quarter
        // second while held (separately from the per-tick progressive mining
        // path updateMining implements above), whose first action is
        // unconditionally re-triggering the arm swing regardless of whether
        // that call's own instant-destroy attempt did anything, so the arm
        // keeps swinging throughout a multi-tick mine, not just on the
        // initial click
        if (gTickCount - gLastMineTick >= timer.ticksPerSecond / 4.0f) {
            handSwinging = true;
            handSwingTicks = -1;
            gLastMineTick = gTickCount;
        }
    } else {
        resetMining();
    }

    bool wantsPressAction = !isHitNull && (hitResult.type == HITRESULT_ENTITY || gEditMode == 1);
    if (left == GLFW_PRESS && prevLeft == GLFW_RELEASE && wantsPressAction) {
        mineOrPlace();
        gLastMineTick = gTickCount;
    } else if (left == GLFW_PRESS && wantsPressAction && (gTickCount - gLastMineTick) >= timer.ticksPerSecond / 4.0f) {
        mineOrPlace();
        gLastMineTick = gTickCount;
    }
    prevLeft = left;
}

// c0.24_st_03: shared block-removal steps (sound, break particles, survival
// drops, netSetTile), used by the real progressive mining tracker below
static void destroyTargetTile(int x, int y, int z) {
    int id = Level_getTile(&level, x, y, z);
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
    bool changed = Level_netSetTile(&level, x, y, z, 0);
    if (t && changed) {
        // type field is the currently selected tile even on destroy,
        // matching the real source; the server ignores it for mode 0
        if (gConnected) NetConnection_sendSetBlock(&gConn, x, y, z, 0, Inventory_getSelected(&player.inventory));
        // c0.0.23a_01: block break sound, same "step." family as
        // footsteps, matching the real source's own volume/pitch formula
        // exactly ((volume+1)/2, pitch*0.8, both already jittered)
        if (t->soundType != SOUND_NONE) {
            char name[32];
            snprintf(name, sizeof name, "step.%s", SOUND_TYPES[t->soundType].name);
            Sound_play(name, (SoundType_getVolume(t->soundType) + 1.0f) / 2.0f,
                       SoundType_getPitch(t->soundType) * 0.8f,
                       x + 0.5f, y + 0.5f, z + 0.5f);
        }
        Tile_onDestroy(t, &level, x, y, z, &particleEngine);
        // c0.24_st_03: survival tile drops, matching Tile.e()/Tile.a()'s
        // own count/resource hooks. Not gated on gConnected: the real
        // source drops these client side regardless, same as the block
        // break particles just above
        if (!gConnected) Tile_dropItems(t, &level, x, y, z);
    }
}

// c0.24_st_03: matches SurvivalGameMode's own a(), which abandons mining
// progress the instant the target changes, the mouse is released, or the
// player switches to place mode
static void resetMining(void) {
    gMineX = gMineY = gMineZ = -1;
    gMineProgress = 0;
    gMineFraction = 0.0f;
}

// c0.24_st_03: matches SurvivalGameMode's own a(x,y,z,face)/a(float) pair,
// implementing real hardness gated progressive mining. Looking away resets
// progress; holding the same block accumulates one tick of progress per
// elapsed real game tick (frame rate independent) until it exceeds the
// tile's own hardnessTicks, then actually breaks it and starts a short 5
// tick cooldown before the next block can be started, matching the real
// source exactly
static void updateMining(void) {
    if (gTickCount < gMineCooldownUntil) {
        gMineFraction = 0.0f;
        return;
    }

    if (hitResult.x != gMineX || hitResult.y != gMineY || hitResult.z != gMineZ) {
        gMineX = hitResult.x; gMineY = hitResult.y; gMineZ = hitResult.z;
        gMineProgress = 0;
        gMineLastTick = gTickCount;
    }

    int elapsed = gTickCount - gMineLastTick;
    if (elapsed > 0) {
        gMineProgress += elapsed;
        gMineLastTick = gTickCount;
    }

    int id = Level_getTile(&level, gMineX, gMineY, gMineZ);
    const Tile* t = (id >= 0 && id < 256) ? gTiles[id] : NULL;
    int hardness = t ? t->hardnessTicks : 0;

    if (gMineProgress > hardness) {
        destroyTargetTile(gMineX, gMineY, gMineZ);
        gMineCooldownUntil = gTickCount + 5;
        resetMining();
        return;
    }

    // matches the real source's own partial-tick blend exactly:
    // ((f + partialTicks - 1) / hardness)
    gMineFraction = hardness > 0 ? ((float)gMineProgress + timer.partialTicks - 1.0f) / (float)hardness : 0.0f;
    if (gMineFraction < 0.0f) gMineFraction = 0.0f;
    if (gMineFraction > 1.0f) gMineFraction = 1.0f;
}

static void mineOrPlace(void) {
    // matches the real source's own combined click handler, which checks
    // for an entity hit before any block mine/place logic and, if found,
    // attacks instead. Flat 4 damage, no weapon variance, since no weapons
    // exist yet. c0.24_st_03's own real source let either mouse button land
    // this hit; c0.25_05_st changed that to left click only (bytecode
    // confirmed: the entity branch now checks which button fired before
    // calling hurt(), where it used to fire unconditionally), matching the
    // wiki's "players can no longer right click to deal damage". This
    // port's own right click was already a dedicated mine/place mode
    // toggle, never itself reaching mineOrPlace(), so it already matched
    // the fixed behavior before this version even existed; nothing to
    // change here beyond this comment
    if (hitResult.type == HITRESULT_ENTITY) {
        Mob_hurt(hitResult.entity, &player.e, 4);
        return;
    }

    // c0.24_st_03: real progressive mining now owns tile-destroy entirely
    // (see updateMining, called every frame from handleBlockClicks), so by
    // the time mineOrPlace runs (only ever called for an entity hit, handled
    // above, or gEditMode==1) it's always the place action below
    {
        // c0.24_st_03: nothing selected (empty handed, or the selected
        // stack just ran out) means there is nothing to place, since a -1
        // "tile" cannot be placed
        int placeId = Inventory_getSelected(&player.inventory);
        if (placeId < 0) return;

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
                if (AABB_intersects(&mobs[i].e.boundingBox, &aabb)) { blocked = true; break; }
            }
            for (int i = 0; !blocked && i < netPlayerCount; ++i) {
                if (netPlayers[i].used && AABB_intersects(&netPlayers[i].base.boundingBox, &aabb)) { blocked = true; }
            }
            if (!blocked) {
                if (gConnected) NetConnection_sendSetBlock(&gConn, x, y, z, 1, placeId);
                Level_netSetTile(&level, x, y, z, placeId);
                // c0.24_st_03: placing spends one of the selected stack,
                // matches Inventory_consume (player/d.java's own c(int))
                if (!gConnected) Inventory_consume(&player.inventory, placeId);
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
    prevR     = glfwGetKey(w, gOptions.keys[OPT_KEY_LOAD_LOC].glfwKey);
    prevEnter = glfwGetKey(w, gOptions.keys[OPT_KEY_SAVE_LOC].glfwKey);
    // c0.24_st_03: the c0.0.21a Select block screen carve-out this guarded
    // is gone along with that screen (see the removed InventoryScreen note
    // above), so number key edges always resync here now
    for (int i = 0; i < 9; ++i) prevNumKeys[i] = glfwGetKey(w, GLFW_KEY_1 + i);
    prevF = glfwGetKey(w, gOptions.keys[OPT_KEY_TOGGLE_FOG].glfwKey);
    prevT = glfwGetKey(w, gOptions.keys[OPT_KEY_CHAT].glfwKey);
}

/* frame */

// c0.25_05_st: first person hand, matches the real source's own
// HeldItemRenderer.render() exactly: drawn as a small object attached to the
// camera (translated forward/right/down within the same view transform
// everything else in the scene already uses, not a separate reset overlay),
// so it naturally stays in the same relative spot as the player looks
// around. Either a small terrain.png cube for the currently selected tile,
// or, empty handed, char.png bound to a single forearm box borrowed from
// this port's own humanoid arm shape (mob/mob_zombie_model.c's own rightArm:
// same box, same texture origin; the real source's own mirrored UV variant
// isn't reproduced, invisible at this scale and this port's own Cube has no
// mirror flag to begin with). No custom skin download, matches this port's
// own offline-only scope decision for this feature
static void renderHand(Player* p, float partialTicks, int pass) {
    if (!handArmCubeBuilt) {
        // matches the exact arm the real source's first person hand draws:
        // the humanoid model's own field f (d/h.java), box origin (-1,-2,-2)
        // size 4x12x4, texture origin (40,16). Crucially the real source
        // draws it with glCallList on the compiled geometry alone (l.java
        // line 776), NOT the model part's own render() call, so the part's
        // own position offset (5,2,0) is deliberately never applied. This
        // port's Cube_render always applies its stored position, so that
        // position is left at (0,0,0) here (no Cube_setPos) to match: the arm
        // is drawn at its box local origin, the same as the real source's own
        // bare display list. The real source's field f also sets a horizontal
        // UV mirror flag, which this port's Cube has no support for; that only
        // affects texture wrap direction, negligible at this scale on the near
        // solid skin tone
        Cube_init(&handArmCube, 40, 16);
        Cube_addBox(&handArmCube, -1, -2, -2, 4, 12, 4);
        handArmCubeBuilt = true;
    }

    // matches the real source clearing the depth buffer immediately before
    // its own first person hand pass (l.java: glClear(GL_DEPTH_BUFFER_BIT)
    // right after the world, particle, cloud and water passes, right before
    // resetting the modelview for the hand). This is what keeps the hand
    // always drawn on top of the world instead of clipping into a nearby
    // block face, e.g. when standing right against a wall or in a tunnel. It
    // requires the hand to be the very last 3D thing rendered this frame,
    // which is why render() calls this right before drawGui, after every
    // other 3D pass
    glClear(GL_DEPTH_BUFFER_BIT);

    // the hand is lit like every other entity; sets up its own lighting
    // bracket here since it now renders outside the main entity loop's own
    setupEntityLighting(true);

    float blend = prevHeldBlend + (heldBlend - prevHeldBlend) * partialTicks;

    glPushMatrix();

    // setupCamera left the modelview as the full world-to-eye transform (the
    // camera's own look rotation plus a translate by the negative player
    // position). A first person hand is NOT drawn in that space: it must
    // stay pinned to the same screen corner no matter where the player
    // looks, so it's drawn in pure eye space instead, matching the real
    // source exactly. l.java's own hand pass does glLoadIdentity then layers
    // only hurt-camera tilt (d.a) and view bobbing (d.b) on top, never the
    // look rotation itself (the xRot/yRot rotate in that method is inside its
    // own push/pop, used only to aim the light, then discarded)
    glLoadIdentity();
    if (gOptions.anaglyph3d) {
        glTranslatef((float)(pass * 2 - 1) * 0.1f, 0.0f, 0.0f);
    }
    applyHurtTilt(p, partialTicks);
    if (gOptions.bobView) applyViewBob(p, partialTicks);

    const float baseScale = 0.8f;
    if (handSwinging) {
        float swing = ((float)handSwingTicks + partialTicks) / 7.0f;
        float dip = sinf(swing * (float)M_PI);
        float rise = sinf(sqrtf(swing) * (float)M_PI);
        glTranslatef(-rise * 0.4f, sinf(sqrtf(swing) * (float)M_PI * 2.0f) * 0.2f, -dip * 0.2f);
    }
    glTranslatef(0.7f * baseScale, -0.65f * baseScale - (1.0f - blend) * 0.6f, -0.9f * baseScale);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    if (handSwinging) {
        float swing = ((float)handSwingTicks + partialTicks) / 7.0f;
        float rise = sinf(sqrtf(swing) * (float)M_PI);
        glRotatef(rise * 80.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(-sinf(swing * swing * (float)M_PI) * 20.0f, 1.0f, 0.0f, 0.0f);
    }

    float brightness = Level_getBrightness(&level, (int)p->e.x, (int)p->e.y, (int)p->e.z);
    glColor4f(brightness, brightness, brightness, 1.0f);
    glEnable(GL_TEXTURE_2D);

    if (heldTileId > 0) {
        // the block is shaded purely by per face vertex color (this port's
        // renderFace emits no normals), the same as the hotbar's own 3D
        // icons, so lighting is turned off for it to avoid the directional
        // light dimming those colors. The arm branch below keeps lighting on
        // since its own Cube geometry does emit normals
        glDisable(GL_LIGHTING);
        glScalef(0.4f, 0.4f, 0.4f);
        glTranslatef(-0.5f, -0.5f, -0.5f);
        glBindTexture(GL_TEXTURE_2D, texTerrain);
        // matches the real source's held item drawing the tile's own
        // Tile.a(Tessellator): a standalone render of the block at the
        // origin, no neighbor culling and no world brightness. This port's
        // own renderItem does the same, dispatching per tile type: a full
        // cube for solid blocks, or the flat crossed sprite for a
        // sapling/flower/mushroom, so a held plant reads as its sprite rather
        // than a solid cube
        const Tile* t = (heldTileId < 256) ? gTiles[heldTileId] : NULL;
        if (t && t->renderItem) {
            t->renderItem(t, &hudTess, brightness);
        }
    } else {
        static int texChar = 0;
        if (!texChar) texChar = loadTexture("resources/char.png", GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, texChar);
        glScalef(1.0f, -1.0f, -1.0f);
        glTranslatef(0.0f, 0.2f, 0.0f);
        glRotatef(-120.0f, 0.0f, 0.0f, 1.0f);
        // real source bakes this same 0.0625 (pixel unit box -> world block
        // unit, matches Creature_render's own identical constant) into the
        // arm's own display list at first compile instead of a live
        // transform (bytecode confirmed: Cuboid.b(float) takes the scale as
        // a build time parameter, called only once, guarded by the same
        // already-built flag this port's own Cube.built is). This port's
        // own Cube_render never bakes scale in, so it has to be applied
        // live here every frame instead
        glScalef(0.0625f, 0.0625f, 0.0625f);
        Cube_render(&handArmCube);
    }

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();

    setupEntityLighting(false);
}

static void render(Level* lvl, LevelRenderer* lr, Player* p, GLFWwindow* w, float t) {
    (void)w;
    (void)lvl;

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        releaseMouseAndOpenPause();
    }

    Sound_setListener(p->e.x, p->e.y, p->e.z, p->e.yRotation);

    // c0.0.23a_01: keeps the renderer and the sound module in sync whenever
    // gOptions.viewDistance/music/sound change, regardless of source (F key,
    // or the Options screen's toggle buttons, neither of which has a
    // reference back into this file's own levelRenderer/Sound module to sync
    // directly at the point of the click)
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

    // matches l.java's own for(i3<2) loop: when anaglyph3d is off this runs
    // once (pass 0, no color mask applied, gOptions.anaglyph3d gates every
    // eye-shift above to a no-op). When it's on, the entire world+entity+
    // hand pass below runs twice, once per eye, each restricted to its own
    // color channels and shifted by setupCamera/moveCameraToPlayer/
    // renderHand's own anaglyph3d checks, composing into a red/cyan
    // stereo image
    int passCount = gOptions.anaglyph3d ? 2 : 1;
    for (int pass = 0; pass < passCount; ++pass) {
        if (gOptions.anaglyph3d) {
            if (pass == 0) glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
            else           glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        }

    // matches the real source: glClearColor is recomputed every frame from
    // the current fog color, not set once at init and left alone. This was
    // still the one-time init call's stale hardcoded 0.5/0.8/1.0 (the old
    // pre-Survival-Test sky tint), so any empty space (world edge, a gap
    // between blocks, geometry beyond the tiny draw distance's chunk
    // radius) always showed that fixed blue instead of matching the actual
    // fog color. This was most visible as a persistent unfogged blue sliver
    // right at the horizon on the TINY view distance setting, since fog
    // there is otherwise supposed to fully hide it
    glClearColor(fogColorDaylight[0], fogColorDaylight[1], fogColorDaylight[2], 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_MULTISAMPLE);

    setupCamera(p, t, pass);

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
        const Creature* c = &mobs[i];
        if (frustum_isVisible(&frustum, &c->e.boundingBox)) {
            Creature_render(c, t);
        }
    }

    for (int i = 0; i < netPlayerCount; ++i) {
        const NetworkPlayer* np = &netPlayers[i];
        if (frustum_isVisible(&frustum, &np->base.boundingBox)) {
            NetworkPlayer_render(np, t, p->e.yRotation, &gFont);
        }
    }

    for (int i = 0; i < arrowCount; ++i) {
        const Arrow* a = &arrows[i];
        if (frustum_isVisible(&frustum, &a->e.boundingBox)) {
            Arrow_render(a, t);
        }
    }

    for (int i = 0; i < itemCount; ++i) {
        const Item* it = &items[i];
        if (frustum_isVisible(&frustum, &it->e.boundingBox)) {
            Item_render(it, t);
        }
    }

    for (int i = 0; i < signCount; ++i) {
        const Sign* s = &signs[i];
        if (frustum_isVisible(&frustum, &s->e.boundingBox)) {
            Sign_render(s, t, &gFont);
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

    // c0.24_st_03: only tile hits get the wireframe/break-progress overlay;
    // an entity hit has no meaningful x/y/z block position to draw one at
    if (!isHitNull && hitResult.type == HITRESULT_TILE) {
        glDisable(GL_LIGHTING);
        glDisable(GL_ALPHA_TEST);
        // digFraction 0 here: this pass runs before the translucent water
        // layer and gets redrawn again below after it, so the crack overlay
        // (a multiplicative blend, unlike the additive wireframe pulse above
        // it) only needs to actually draw once, not darken twice
        LevelRenderer_renderHit(&levelRenderer, &player, &hitResult, gEditMode, Inventory_getSelected(&player.inventory), 0.0f);
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

    if (!isHitNull && hitResult.type == HITRESULT_TILE) {
        glDepthFunc(GL_LESS);
        glDisable(GL_ALPHA_TEST);
        LevelRenderer_renderHit(&levelRenderer, &player, &hitResult, gEditMode, Inventory_getSelected(&player.inventory), gMineFraction);
        LevelRenderer_renderHitOutline(&hitResult, gEditMode);
        glEnable(GL_ALPHA_TEST);
        glDepthFunc(GL_LEQUAL);
    }

    // c0.25_05_st: first person hand, drawn last among the 3D passes,
    // matching the real source's own order exactly. It clears the depth
    // buffer itself (see renderHand) so it always sits on top of the world
    // rather than clipping into nearby blocks, which only works correctly as
    // the final 3D pass, right before the 2D HUD. Drawn whenever a level is
    // loaded, screen open or not, matching the real source; render() is
    // already only reached once a level exists
    renderHand(p, t, pass);
    }

    if (gOptions.anaglyph3d) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    }

    drawGui(t);

    glfwSwapBuffers(window);

    // c0.0.23a_01: a 200fps cap returns (0.0.22a), a flat 5ms sleep per frame
    // (1000/5 = 200) rather than the old c0.0.19a_04 cap's Display.sync-style
    // budget tracking. timeBeginPeriod(1) in timer.c already requests the 1ms
    // Sleep() granularity this needs to actually land on 200fps on Windows
    sleepMillis(5);
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

        // c0.24_st_03: force the Game over screen open the moment nothing
        // else is showing and the player is dead, matching the real
        // source's own per frame check right before its mouse wheel
        // handling. Minecraft_setScreen(NULL)'s own substitution (see
        // above) does the actual redirect to the Game over screen
        if (!activeScreen && p->e.health <= 0) {
            Minecraft_setScreen(NULL);
        }

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
                if (!activeScreen) Player_pollKeys(p, window, &gOptions);
            } else {
                handleScreenClicks(window);
                syncGameplayKeyEdges(window);
                // handleScreenClicks's button callback can close the screen,
                // setting activeScreen to null mid frame, so recheck before use.
                if (activeScreen && activeScreen->tick) activeScreen->tick(activeScreen);

                // c0.24_st_03: the c0.0.21a Select block screen's Screen.e
                // passthrough (walking/hotbar switching while a screen is
                // open) is gone along with that screen, since nothing in
                // this version's real source ever sets that flag anymore
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

    Sound_tickMusic();

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

    // c0.25_05_st: first person hand tick, matches the real source's own
    // HeldItemRenderer update exactly: advance the swing counter to
    // completion, then crossfade heldTileId toward whatever's now selected
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

    for (int i = 0; i < mobCount; ) {
        Creature_onTick(&mobs[i]);
        if (mobs[i].e.removed) {
            mobs[i] = mobs[mobCount - 1];
            // Entity.ai points at this same struct's own embedded Ai field,
            // so a raw struct copy above leaves it pointing at the old slot;
            // re-anchor it to the copy's new home before that field goes away
            mobs[i].e.ai = &mobs[i].ai;
            mobCount--;
            continue;
        }
        i++;
    }

    for (int i = 0; i < arrowCount; ) {
        Arrow_onTick(&arrows[i]);
        if (arrows[i].e.removed) {
            arrows[i] = arrows[arrowCount - 1];
            arrowCount--;
            continue;
        }
        i++;
    }

    for (int i = 0; i < itemCount; ) {
        Item_onTick(&items[i]);
        if (items[i].e.removed) {
            items[i] = items[itemCount - 1];
            itemCount--;
            continue;
        }
        i++;
    }

    for (int i = 0; i < signCount; ) {
        Sign_onTick(&signs[i]);
        if (signs[i].e.removed) {
            signs[i] = signs[signCount - 1];
            signCount--;
            continue;
        }
        i++;
    }

    for (int i = 0; i < netPlayerCount; i++) {
        NetworkPlayer_onTick(&netPlayers[i]);
    }
}