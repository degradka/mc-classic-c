// options.h: c0.0.23a_01 remappable keybindings and toggles, persisted to options.txt

#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

#define OPTIONS_KEY_COUNT 10

// index into Options.keys[], matches the real source's own binding order
enum {
    OPT_KEY_FORWARD = 0,
    OPT_KEY_LEFT,
    OPT_KEY_BACK,
    OPT_KEY_RIGHT,
    OPT_KEY_JUMP,
    OPT_KEY_BUILD,       // opens the inventory/select block screen, default B
    OPT_KEY_CHAT,        // default T
    OPT_KEY_TOGGLE_FOG,  // cycles view distance, default F
    OPT_KEY_SAVE_LOC,    // sets the world spawn to the player's position, default Enter
    OPT_KEY_LOAD_LOC     // resets the player to the world spawn, default R
};

typedef struct {
    const char* label;
    int glfwKey;
} KeyBinding;

typedef struct Options {
    bool music;
    bool sound;
    bool invertMouseY;
    bool showFrameRate;
    int viewDistance; // 0..3: FAR/NORMAL/SHORT/TINY
    // c0.25_05_st: matches Options.f/g exactly (both already present in this
    // version's real source, just never wired up here before). bobView
    // gates the camera view-bob effect (see applyViewBob in minecraft.c);
    // anaglyph3d gates the red/cyan stereo dual-pass render plus a
    // luminance-weighted desaturation of every loaded texture and chat
    // color code
    bool bobView;
    bool anaglyph3d;
    // c0.27_st: new toggle. When on, the main loop does an extra 5ms sleep
    // per frame after swapping buffers (a simple frame limiter, not
    // vsync/swap-interval based, matching Options.i and l.java's own
    // Thread.sleep(5) call exactly)
    bool limitFramerate;

    KeyBinding keys[OPTIONS_KEY_COUNT];

    char path[512]; // options.txt, next to the executable
} Options;

extern const char* const OPTIONS_VIEW_DISTANCE_NAMES[4];

void Options_init(Options* o);
void Options_load(Options* o);
void Options_save(Options* o);

// c0.0.23a_01: Options/Controls screen support. index is 0..7, matching the
// real source's j.b(int)/j.b(int,int): 0=music, 1=sound, 2=invertMouseY,
// 3=showFrameRate, 4=viewDistance, 5=bobView, 6=anaglyph3d, 7=limitFramerate
void Options_toggleLabel(const Options* o, int index, char* out, int outSize);
void Options_toggleValue(Options* o, int index, int direction); // toggles/cycles, then saves

// GLFW keycode -> a short display name ("W", "Space", "Left Shift", ...),
// falling back to "Key #<code>" for anything not in the table
const char* Options_keyName(int glfwKey);
void Options_keyBindingLabel(const Options* o, int index, char* out, int outSize);
void Options_setKeyBinding(Options* o, int index, int glfwKey); // sets, then saves

#endif
