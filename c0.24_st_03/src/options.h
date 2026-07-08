// options.h: music, sound, mouse, fps and render distance toggles and the key bindings, kept in options.txt

#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

#define OPTIONS_KEY_COUNT 10

typedef struct {
    const char* name;
    // a key code
    int key;
} KeyBinding;

typedef struct Options {
    bool music;
    bool sound;
    bool invertMouse;
    bool showFPS;
    int renderDistance;
    KeyBinding forward, left, back, right, jump, build, chat, toggleFog, save, load;
    // the bindings above in the controls screen's order
    KeyBinding* keyBindings[OPTIONS_KEY_COUNT];
} Options;

void Options_init(Options* o);
// "Forward: W", the name is "null" for a key without one
void Options_getKeyBinding(const Options* o, int i, char* out, int size);
void Options_setKeyBinding(Options* o, int i, int key);
// toggles option n, or moves the render distance by step
void Options_setOption(Options* o, int n, int step);
void Options_getOption(const Options* o, int n, char* out, int size);

#endif
