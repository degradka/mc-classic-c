// options.c

#include "options.h"
#include "keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// next to level.dat
#define OPTIONS_FILE "options.txt"

static const char* RENDER_DISTANCES[] = { "FAR", "NORMAL", "SHORT", "TINY" };

static void loadOptions(Options* o);
static void saveOptions(const Options* o);

void Options_init(Options* o) {
    o->music = true;
    o->sound = true;
    o->invertMouse = false;
    o->showFPS = false;
    o->renderDistance = 0;
    o->forward   = (KeyBinding){ "Forward", 17 };
    o->left      = (KeyBinding){ "Left", 30 };
    o->back      = (KeyBinding){ "Back", 31 };
    o->right     = (KeyBinding){ "Right", 32 };
    o->jump      = (KeyBinding){ "Jump", 57 };
    o->build     = (KeyBinding){ "Build", 48 };
    o->chat      = (KeyBinding){ "Chat", 20 };
    o->toggleFog = (KeyBinding){ "Toggle fog", 33 };
    o->save      = (KeyBinding){ "Save location", 28 };
    o->load      = (KeyBinding){ "Load location", 19 };
    KeyBinding* all[OPTIONS_KEY_COUNT] = { &o->forward, &o->left, &o->back, &o->right, &o->jump,
                                           &o->build, &o->chat, &o->toggleFog, &o->save, &o->load };
    memcpy(o->keyBindings, all, sizeof all);
    loadOptions(o);
}

void Options_getKeyBinding(const Options* o, int i, char* out, int size) {
    const char* name = Keys_name(o->keyBindings[i]->key);
    snprintf(out, (size_t)size, "%s: %s", o->keyBindings[i]->name, name ? name : "null");
}

void Options_setKeyBinding(Options* o, int i, int key) {
    o->keyBindings[i]->key = key;
    saveOptions(o);
}

void Options_setOption(Options* o, int n, int step) {
    if (n == 0) o->music = !o->music;
    if (n == 1) o->sound = !o->sound;
    if (n == 2) o->invertMouse = !o->invertMouse;
    if (n == 3) o->showFPS = !o->showFPS;
    if (n == 4) o->renderDistance = (o->renderDistance + step) & 3;
    saveOptions(o);
}

void Options_getOption(const Options* o, int n, char* out, int size) {
    out[0] = '\0';
    if (n == 0) snprintf(out, (size_t)size, "Music: %s", o->music ? "ON" : "OFF");
    if (n == 1) snprintf(out, (size_t)size, "Sound: %s", o->sound ? "ON" : "OFF");
    if (n == 2) snprintf(out, (size_t)size, "Invert mouse: %s", o->invertMouse ? "ON" : "OFF");
    if (n == 3) snprintf(out, (size_t)size, "Show FPS: %s", o->showFPS ? "ON" : "OFF");
    // a hand edited distance shows nothing
    if (n == 4 && o->renderDistance >= 0 && o->renderDistance < 4) {
        snprintf(out, (size_t)size, "Render distance: %s", RENDER_DISTANCES[o->renderDistance]);
    }
}

// Integer.parseInt, false where it would throw
static bool parseInt(const char* s, int* out) {
    const char* p = s;
    if (*p == '-' || *p == '+') p++;
    if (*p == '\0') return false;
    for (const char* q = p; *q; q++) {
        if (*q < '0' || *q > '9') return false;
    }
    long long v = strtoll(s, NULL, 10);
    if (v < -2147483648LL || v > 2147483647LL) return false;
    *out = (int)v;
    return true;
}

// a line that can't be parsed stops the loading there
static void loadOptions(Options* o) {
    FILE* f = fopen(OPTIONS_FILE, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        // split(":"), the second field is everything up to the next colon, trailing empty fields are dropped
        char* colon = strchr(line, ':');
        if (colon && strspn(colon, ":") == strlen(colon)) *colon = '\0', colon = NULL;
        if (!colon) {
            if (strcmp(line, "music") == 0 || strcmp(line, "sound") == 0 || strcmp(line, "invertYMouse") == 0 ||
                strcmp(line, "showFrameRate") == 0 || strcmp(line, "viewDistance") == 0) break;
            bool binding = false;
            for (int i = 0; i < OPTIONS_KEY_COUNT; i++) {
                char k[64];
                snprintf(k, sizeof k, "key_%s", o->keyBindings[i]->name);
                if (strcmp(line, k) == 0) binding = true;
            }
            if (binding) break;
            continue;
        }
        *colon = '\0';
        char* value = colon + 1;
        char* next = strchr(value, ':');
        if (next) *next = '\0';
        if (strcmp(line, "music") == 0) o->music = strcmp(value, "true") == 0;
        if (strcmp(line, "sound") == 0) o->sound = strcmp(value, "true") == 0;
        if (strcmp(line, "invertYMouse") == 0) o->invertMouse = strcmp(value, "true") == 0;
        if (strcmp(line, "showFrameRate") == 0) o->showFPS = strcmp(value, "true") == 0;
        if (strcmp(line, "viewDistance") == 0 && !parseInt(value, &o->renderDistance)) break;
        bool failed = false;
        for (int i = 0; i < OPTIONS_KEY_COUNT; i++) {
            char k[64];
            snprintf(k, sizeof k, "key_%s", o->keyBindings[i]->name);
            if (strcmp(line, k) != 0) continue;
            if (!parseInt(value, &o->keyBindings[i]->key)) failed = true;
            break;
        }
        if (failed) break;
    }
    fclose(f);
}

static void saveOptions(const Options* o) {
    FILE* f = fopen(OPTIONS_FILE, "w");
    if (!f) {
        printf("Failed to save options\n");
        return;
    }
    fprintf(f, "music:%s\n", o->music ? "true" : "false");
    fprintf(f, "sound:%s\n", o->sound ? "true" : "false");
    fprintf(f, "invertYMouse:%s\n", o->invertMouse ? "true" : "false");
    fprintf(f, "showFrameRate:%s\n", o->showFPS ? "true" : "false");
    fprintf(f, "viewDistance:%d\n", o->renderDistance);
    for (int i = 0; i < OPTIONS_KEY_COUNT; i++) {
        fprintf(f, "key_%s:%d\n", o->keyBindings[i]->name, o->keyBindings[i]->key);
    }
    fclose(f);
}
