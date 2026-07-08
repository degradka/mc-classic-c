// options.c

#include "options.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* const OPTIONS_VIEW_DISTANCE_NAMES[4] = { "FAR", "NORMAL", "SHORT", "TINY" };

void Options_init(Options* o) {
    o->music = true;
    o->sound = true;
    o->invertMouseY = false;
    o->showFrameRate = false;
    o->viewDistance = 0;

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
        else if (strncmp(key, "key_", 4) == 0) {
            const char* bindingName = key + 4;
            for (int i = 0; i < OPTIONS_KEY_COUNT; ++i) {
                if (strcmp(bindingName, o->keys[i].label) == 0) {
                    o->keys[i].glfwKey = atoi(value);
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
    for (int i = 0; i < OPTIONS_KEY_COUNT; ++i) {
        fprintf(f, "key_%s:%d\n", o->keys[i].label, o->keys[i].glfwKey);
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
        default: break;
    }
    Options_save(o);
}

// covers this port's own default bindings (W/A/S/D/Space/B/T/F/Enter/R) plus
// enough of GLFW's named key range that rebinding to something else still
// shows a readable label instead of falling back to "Key #<code>" for
// anything common
const char* Options_keyName(int glfwKey) {
    if (glfwKey >= GLFW_KEY_A && glfwKey <= GLFW_KEY_Z) {
        static char buf[2];
        buf[0] = (char)('A' + (glfwKey - GLFW_KEY_A));
        buf[1] = '\0';
        return buf;
    }
    if (glfwKey >= GLFW_KEY_0 && glfwKey <= GLFW_KEY_9) {
        static char buf[2];
        buf[0] = (char)('0' + (glfwKey - GLFW_KEY_0));
        buf[1] = '\0';
        return buf;
    }
    if (glfwKey >= GLFW_KEY_F1 && glfwKey <= GLFW_KEY_F25) {
        static char buf[4];
        snprintf(buf, sizeof buf, "F%d", glfwKey - GLFW_KEY_F1 + 1);
        return buf;
    }
    switch (glfwKey) {
        case GLFW_KEY_SPACE:         return "Space";
        case GLFW_KEY_ENTER:         return "Enter";
        case GLFW_KEY_ESCAPE:        return "Escape";
        case GLFW_KEY_TAB:           return "Tab";
        case GLFW_KEY_BACKSPACE:     return "Backspace";
        case GLFW_KEY_INSERT:        return "Insert";
        case GLFW_KEY_DELETE:        return "Delete";
        case GLFW_KEY_RIGHT:         return "Right";
        case GLFW_KEY_LEFT:          return "Left";
        case GLFW_KEY_DOWN:          return "Down";
        case GLFW_KEY_UP:            return "Up";
        case GLFW_KEY_PAGE_UP:       return "Page Up";
        case GLFW_KEY_PAGE_DOWN:     return "Page Down";
        case GLFW_KEY_HOME:          return "Home";
        case GLFW_KEY_END:           return "End";
        case GLFW_KEY_CAPS_LOCK:     return "Caps Lock";
        case GLFW_KEY_LEFT_SHIFT:    return "Left Shift";
        case GLFW_KEY_LEFT_CONTROL:  return "Left Ctrl";
        case GLFW_KEY_LEFT_ALT:      return "Left Alt";
        case GLFW_KEY_RIGHT_SHIFT:   return "Right Shift";
        case GLFW_KEY_RIGHT_CONTROL: return "Right Ctrl";
        case GLFW_KEY_RIGHT_ALT:     return "Right Alt";
        case GLFW_KEY_COMMA:         return ",";
        case GLFW_KEY_PERIOD:        return ".";
        case GLFW_KEY_SLASH:         return "/";
        case GLFW_KEY_SEMICOLON:     return ";";
        case GLFW_KEY_APOSTROPHE:    return "'";
        case GLFW_KEY_MINUS:         return "-";
        case GLFW_KEY_EQUAL:         return "=";
        case GLFW_KEY_GRAVE_ACCENT:  return "`";
        case GLFW_KEY_BACKSLASH:     return "\\";
        case GLFW_KEY_LEFT_BRACKET:  return "[";
        case GLFW_KEY_RIGHT_BRACKET: return "]";
        default: break;
    }
    static char buf[16];
    snprintf(buf, sizeof buf, "Key #%d", glfwKey);
    return buf;
}

void Options_keyBindingLabel(const Options* o, int index, char* out, int outSize) {
    snprintf(out, outSize, "%s: %s", o->keys[index].label, Options_keyName(o->keys[index].glfwKey));
}

void Options_setKeyBinding(Options* o, int index, int glfwKey) {
    o->keys[index].glfwKey = glfwKey;
    Options_save(o);
}
