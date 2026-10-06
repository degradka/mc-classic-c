// options.c

#include "options.h"
#include "renderer/textures.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* const OPTIONS_VIEW_DISTANCE_NAMES[4] = { "FAR", "NORMAL", "SHORT", "TINY" };

static int toLwjgl(int glfwKey);
static int fromLwjgl(int code);

void Options_init(Options* o) {
    o->music = true;
    o->sound = true;
    o->invertMouseY = false;
    o->showFrameRate = false;
    o->viewDistance = 0;
    o->bobView = true;
    o->anaglyph3d = false;

    o->keys[OPT_KEY_FORWARD]   = (KeyBinding){ "Forward",       GLFW_KEY_W };
    o->keys[OPT_KEY_LEFT]      = (KeyBinding){ "Left",          GLFW_KEY_A };
    o->keys[OPT_KEY_BACK]      = (KeyBinding){ "Back",          GLFW_KEY_S };
    o->keys[OPT_KEY_RIGHT]     = (KeyBinding){ "Right",         GLFW_KEY_D };
    o->keys[OPT_KEY_JUMP]      = (KeyBinding){ "Jump",          GLFW_KEY_SPACE };
    o->keys[OPT_KEY_BUILD]     = (KeyBinding){ "Build",         GLFW_KEY_B };
    o->keys[OPT_KEY_CHAT]      = (KeyBinding){ "Chat",          GLFW_KEY_T };
    o->keys[OPT_KEY_TOGGLE_FOG]= (KeyBinding){ "Toggle fog",    GLFW_KEY_F };
    o->keys[OPT_KEY_SAVE_LOC]  = (KeyBinding){ "Save location", GLFW_KEY_ENTER };
    o->keys[OPT_KEY_LOAD_LOC]  = (KeyBinding){ "Load location", GLFW_KEY_R };

    snprintf(o->path, sizeof o->path, "options.txt");
    Options_load(o);
}

void Options_load(Options* o) {
    FILE* f = fopen(o->path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof line, f)) {
        char* colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char* key = line;
        char* value = colon + 1;
        size_t vlen = strlen(value);
        while (vlen > 0 && (value[vlen-1] == '\n' || value[vlen-1] == '\r')) value[--vlen] = '\0';

        if (strcmp(key, "music") == 0) o->music = (strcmp(value, "true") == 0);
        else if (strcmp(key, "sound") == 0) o->sound = (strcmp(value, "true") == 0);
        else if (strcmp(key, "invertYMouse") == 0) o->invertMouseY = (strcmp(value, "true") == 0);
        else if (strcmp(key, "showFrameRate") == 0) o->showFrameRate = (strcmp(value, "true") == 0);
        else if (strcmp(key, "viewDistance") == 0) o->viewDistance = atoi(value) & 3;
        else if (strcmp(key, "bobView") == 0) o->bobView = (strcmp(value, "true") == 0);
        else if (strcmp(key, "anaglyph3d") == 0) o->anaglyph3d = (strcmp(value, "true") == 0);
        else if (strncmp(key, "key_", 4) == 0) {
            const char* bindingName = key + 4;
            for (int i = 0; i < OPTIONS_KEY_COUNT; ++i) {
                if (strcmp(bindingName, o->keys[i].label) == 0) {
                    o->keys[i].glfwKey = fromLwjgl(atoi(value));
                    break;
                }
            }
        }
    }
    fclose(f);
}

void Options_save(Options* o) {
    FILE* f = fopen(o->path, "w");
    if (!f) return;

    fprintf(f, "music:%s\n", o->music ? "true" : "false");
    fprintf(f, "sound:%s\n", o->sound ? "true" : "false");
    fprintf(f, "invertYMouse:%s\n", o->invertMouseY ? "true" : "false");
    fprintf(f, "showFrameRate:%s\n", o->showFrameRate ? "true" : "false");
    fprintf(f, "viewDistance:%d\n", o->viewDistance);
    fprintf(f, "bobView:%s\n", o->bobView ? "true" : "false");
    fprintf(f, "anaglyph3d:%s\n", o->anaglyph3d ? "true" : "false");
    for (int i = 0; i < OPTIONS_KEY_COUNT; ++i) {
        fprintf(f, "key_%s:%d\n", o->keys[i].label, toLwjgl(o->keys[i].glfwKey));
    }
    fclose(f);
}

void Options_toggleLabel(const Options* o, int index, char* out, int outSize) {
    switch (index) {
        case 0: snprintf(out, outSize, "Music: %s", o->music ? "ON" : "OFF"); break;
        case 1: snprintf(out, outSize, "Sound: %s", o->sound ? "ON" : "OFF"); break;
        case 2: snprintf(out, outSize, "Invert mouse: %s", o->invertMouseY ? "ON" : "OFF"); break;
        case 3: snprintf(out, outSize, "Show FPS: %s", o->showFrameRate ? "ON" : "OFF"); break;
        case 4: snprintf(out, outSize, "Render distance: %s", OPTIONS_VIEW_DISTANCE_NAMES[o->viewDistance]); break;
        case 5: snprintf(out, outSize, "View bobbing: %s", o->bobView ? "ON" : "OFF"); break;
        case 6: snprintf(out, outSize, "3d anaglyph: %s", o->anaglyph3d ? "ON" : "OFF"); break;
        default: out[0] = '\0'; break;
    }
}

void Options_toggleValue(Options* o, int index, int direction) {
    switch (index) {
        case 0: o->music = !o->music; break;
        case 1: o->sound = !o->sound; break;
        case 2: o->invertMouseY = !o->invertMouseY; break;
        case 3: o->showFrameRate = !o->showFrameRate; break;
        case 4: o->viewDistance = (o->viewDistance + direction) & 3; break;
        case 5: o->bobView = !o->bobView; break;
        case 6:
            o->anaglyph3d = !o->anaglyph3d;
            Textures_setAnaglyph(o->anaglyph3d);
            break;
        default: break;
    }
    Options_save(o);
}

// key codes and Keyboard.getKeyName, options.txt stores these
static const struct { int glfw, lwjgl; const char* name; } KEYS[] = {
    { GLFW_KEY_ESCAPE, 1, "ESCAPE" },
    { GLFW_KEY_1, 2, "1" },
    { GLFW_KEY_2, 3, "2" },
    { GLFW_KEY_3, 4, "3" },
    { GLFW_KEY_4, 5, "4" },
    { GLFW_KEY_5, 6, "5" },
    { GLFW_KEY_6, 7, "6" },
    { GLFW_KEY_7, 8, "7" },
    { GLFW_KEY_8, 9, "8" },
    { GLFW_KEY_9, 10, "9" },
    { GLFW_KEY_0, 11, "0" },
    { GLFW_KEY_MINUS, 12, "MINUS" },
    { GLFW_KEY_EQUAL, 13, "EQUALS" },
    { GLFW_KEY_BACKSPACE, 14, "BACK" },
    { GLFW_KEY_TAB, 15, "TAB" },
    { GLFW_KEY_Q, 16, "Q" },
    { GLFW_KEY_W, 17, "W" },
    { GLFW_KEY_E, 18, "E" },
    { GLFW_KEY_R, 19, "R" },
    { GLFW_KEY_T, 20, "T" },
    { GLFW_KEY_Y, 21, "Y" },
    { GLFW_KEY_U, 22, "U" },
    { GLFW_KEY_I, 23, "I" },
    { GLFW_KEY_O, 24, "O" },
    { GLFW_KEY_P, 25, "P" },
    { GLFW_KEY_LEFT_BRACKET, 26, "LBRACKET" },
    { GLFW_KEY_RIGHT_BRACKET, 27, "RBRACKET" },
    { GLFW_KEY_ENTER, 28, "RETURN" },
    { GLFW_KEY_LEFT_CONTROL, 29, "LCONTROL" },
    { GLFW_KEY_A, 30, "A" },
    { GLFW_KEY_S, 31, "S" },
    { GLFW_KEY_D, 32, "D" },
    { GLFW_KEY_F, 33, "F" },
    { GLFW_KEY_G, 34, "G" },
    { GLFW_KEY_H, 35, "H" },
    { GLFW_KEY_J, 36, "J" },
    { GLFW_KEY_K, 37, "K" },
    { GLFW_KEY_L, 38, "L" },
    { GLFW_KEY_SEMICOLON, 39, "SEMICOLON" },
    { GLFW_KEY_APOSTROPHE, 40, "APOSTROPHE" },
    { GLFW_KEY_GRAVE_ACCENT, 41, "GRAVE" },
    { GLFW_KEY_LEFT_SHIFT, 42, "LSHIFT" },
    { GLFW_KEY_BACKSLASH, 43, "BACKSLASH" },
    { GLFW_KEY_Z, 44, "Z" },
    { GLFW_KEY_X, 45, "X" },
    { GLFW_KEY_C, 46, "C" },
    { GLFW_KEY_V, 47, "V" },
    { GLFW_KEY_B, 48, "B" },
    { GLFW_KEY_N, 49, "N" },
    { GLFW_KEY_M, 50, "M" },
    { GLFW_KEY_COMMA, 51, "COMMA" },
    { GLFW_KEY_PERIOD, 52, "PERIOD" },
    { GLFW_KEY_SLASH, 53, "SLASH" },
    { GLFW_KEY_RIGHT_SHIFT, 54, "RSHIFT" },
    { GLFW_KEY_KP_MULTIPLY, 55, "MULTIPLY" },
    { GLFW_KEY_LEFT_ALT, 56, "LMENU" },
    { GLFW_KEY_SPACE, 57, "SPACE" },
    { GLFW_KEY_CAPS_LOCK, 58, "CAPITAL" },
    { GLFW_KEY_F1, 59, "F1" },
    { GLFW_KEY_F2, 60, "F2" },
    { GLFW_KEY_F3, 61, "F3" },
    { GLFW_KEY_F4, 62, "F4" },
    { GLFW_KEY_F5, 63, "F5" },
    { GLFW_KEY_F6, 64, "F6" },
    { GLFW_KEY_F7, 65, "F7" },
    { GLFW_KEY_F8, 66, "F8" },
    { GLFW_KEY_F9, 67, "F9" },
    { GLFW_KEY_F10, 68, "F10" },
    { GLFW_KEY_NUM_LOCK, 69, "NUMLOCK" },
    { GLFW_KEY_SCROLL_LOCK, 70, "SCROLL" },
    { GLFW_KEY_KP_7, 71, "NUMPAD7" },
    { GLFW_KEY_KP_8, 72, "NUMPAD8" },
    { GLFW_KEY_KP_9, 73, "NUMPAD9" },
    { GLFW_KEY_KP_SUBTRACT, 74, "SUBTRACT" },
    { GLFW_KEY_KP_4, 75, "NUMPAD4" },
    { GLFW_KEY_KP_5, 76, "NUMPAD5" },
    { GLFW_KEY_KP_6, 77, "NUMPAD6" },
    { GLFW_KEY_KP_ADD, 78, "ADD" },
    { GLFW_KEY_KP_1, 79, "NUMPAD1" },
    { GLFW_KEY_KP_2, 80, "NUMPAD2" },
    { GLFW_KEY_KP_3, 81, "NUMPAD3" },
    { GLFW_KEY_KP_0, 82, "NUMPAD0" },
    { GLFW_KEY_KP_DECIMAL, 83, "DECIMAL" },
    { GLFW_KEY_F11, 87, "F11" },
    { GLFW_KEY_F12, 88, "F12" },
    { GLFW_KEY_F13, 100, "F13" },
    { GLFW_KEY_F14, 101, "F14" },
    { GLFW_KEY_F15, 102, "F15" },
    { GLFW_KEY_KP_EQUAL, 141, "NUMPADEQUALS" },
    { GLFW_KEY_KP_ENTER, 156, "NUMPADENTER" },
    { GLFW_KEY_RIGHT_CONTROL, 157, "RCONTROL" },
    { GLFW_KEY_KP_DIVIDE, 181, "DIVIDE" },
    { GLFW_KEY_PRINT_SCREEN, 183, "SYSRQ" },
    { GLFW_KEY_RIGHT_ALT, 184, "RMENU" },
    { GLFW_KEY_PAUSE, 197, "PAUSE" },
    { GLFW_KEY_HOME, 199, "HOME" },
    { GLFW_KEY_UP, 200, "UP" },
    { GLFW_KEY_PAGE_UP, 201, "PRIOR" },
    { GLFW_KEY_LEFT, 203, "LEFT" },
    { GLFW_KEY_RIGHT, 205, "RIGHT" },
    { GLFW_KEY_END, 207, "END" },
    { GLFW_KEY_DOWN, 208, "DOWN" },
    { GLFW_KEY_PAGE_DOWN, 209, "NEXT" },
    { GLFW_KEY_INSERT, 210, "INSERT" },
    { GLFW_KEY_DELETE, 211, "DELETE" },
    { GLFW_KEY_LEFT_SUPER, 219, "LMETA" },
    { GLFW_KEY_RIGHT_SUPER, 220, "RMETA" },
    { GLFW_KEY_MENU, 221, "APPS" },
};
#define KEY_COUNT ((int)(sizeof KEYS / sizeof KEYS[0]))

static int toLwjgl(int glfwKey) {
    for (int i = 0; i < KEY_COUNT; ++i) if (KEYS[i].glfw == glfwKey) return KEYS[i].lwjgl;
    return 0;
}

static int fromLwjgl(int code) {
    for (int i = 0; i < KEY_COUNT; ++i) if (KEYS[i].lwjgl == code) return KEYS[i].glfw;
    return GLFW_KEY_UNKNOWN;
}

const char* Options_keyName(int glfwKey) {
    int code = toLwjgl(glfwKey);
    for (int i = 0; i < KEY_COUNT; ++i) if (KEYS[i].lwjgl == code) return KEYS[i].name;
    return "NONE";
}

void Options_keyBindingLabel(const Options* o, int index, char* out, int outSize) {
    snprintf(out, outSize, "%s: %s", o->keys[index].label, Options_keyName(o->keys[index].glfwKey));
}

void Options_setKeyBinding(Options* o, int index, int glfwKey) {
    o->keys[index].glfwKey = glfwKey;
    Options_save(o);
}
