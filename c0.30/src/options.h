// options.h: key bindings and toggles, saved to options.txt

#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

#define OPTIONS_KEY_COUNT 10

// index into Options.keys[]
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
    // anaglyph3d also desaturates every texture and the chat colors
    bool bobView;
    bool anaglyph3d;
    bool limitFramerate;

    KeyBinding keys[OPTIONS_KEY_COUNT];

    char path[512]; // options.txt, next to the executable
} Options;

extern const char* const OPTIONS_VIEW_DISTANCE_NAMES[4];

void Options_init(Options* o);
void Options_load(Options* o);
void Options_save(Options* o);

// 0 music, 1 sound, 2 invert mouse, 3 show fps, 4 view distance,
// 5 bobbing, 6 anaglyph, 7 limit framerate
void Options_toggleLabel(const Options* o, int index, char* out, int outSize);
void Options_toggleValue(Options* o, int index, int direction); // toggles/cycles, then saves

// glfw key to its display name, "Key #<code>" when it's not in the table
const char* Options_keyName(int glfwKey);
void Options_keyBindingLabel(const Options* o, int index, char* out, int outSize);
void Options_setKeyBinding(Options* o, int index, int glfwKey); // sets, then saves

#endif
