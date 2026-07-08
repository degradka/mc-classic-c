// keys.c

#include "keys.h"
#include <GLFW/glfw3.h>
#include <stddef.h>

typedef struct { int key; int glfw; const char* name; } KeyEntry;

static const KeyEntry KEYS[] = {
    {   1, GLFW_KEY_ESCAPE,        "ESCAPE" },
    {   2, GLFW_KEY_1,             "1" },
    {   3, GLFW_KEY_2,             "2" },
    {   4, GLFW_KEY_3,             "3" },
    {   5, GLFW_KEY_4,             "4" },
    {   6, GLFW_KEY_5,             "5" },
    {   7, GLFW_KEY_6,             "6" },
    {   8, GLFW_KEY_7,             "7" },
    {   9, GLFW_KEY_8,             "8" },
    {  10, GLFW_KEY_9,             "9" },
    {  11, GLFW_KEY_0,             "0" },
    {  12, GLFW_KEY_MINUS,         "MINUS" },
    {  13, GLFW_KEY_EQUAL,         "EQUALS" },
    {  14, GLFW_KEY_BACKSPACE,     "BACK" },
    {  15, GLFW_KEY_TAB,           "TAB" },
    {  16, GLFW_KEY_Q,             "Q" },
    {  17, GLFW_KEY_W,             "W" },
    {  18, GLFW_KEY_E,             "E" },
    {  19, GLFW_KEY_R,             "R" },
    {  20, GLFW_KEY_T,             "T" },
    {  21, GLFW_KEY_Y,             "Y" },
    {  22, GLFW_KEY_U,             "U" },
    {  23, GLFW_KEY_I,             "I" },
    {  24, GLFW_KEY_O,             "O" },
    {  25, GLFW_KEY_P,             "P" },
    {  26, GLFW_KEY_LEFT_BRACKET,  "LBRACKET" },
    {  27, GLFW_KEY_RIGHT_BRACKET, "RBRACKET" },
    {  28, GLFW_KEY_ENTER,         "RETURN" },
    {  29, GLFW_KEY_LEFT_CONTROL,  "LCONTROL" },
    {  30, GLFW_KEY_A,             "A" },
    {  31, GLFW_KEY_S,             "S" },
    {  32, GLFW_KEY_D,             "D" },
    {  33, GLFW_KEY_F,             "F" },
    {  34, GLFW_KEY_G,             "G" },
    {  35, GLFW_KEY_H,             "H" },
    {  36, GLFW_KEY_J,             "J" },
    {  37, GLFW_KEY_K,             "K" },
    {  38, GLFW_KEY_L,             "L" },
    {  39, GLFW_KEY_SEMICOLON,     "SEMICOLON" },
    {  40, GLFW_KEY_APOSTROPHE,    "APOSTROPHE" },
    {  41, GLFW_KEY_GRAVE_ACCENT,  "GRAVE" },
    {  42, GLFW_KEY_LEFT_SHIFT,    "LSHIFT" },
    {  43, GLFW_KEY_BACKSLASH,     "BACKSLASH" },
    {  44, GLFW_KEY_Z,             "Z" },
    {  45, GLFW_KEY_X,             "X" },
    {  46, GLFW_KEY_C,             "C" },
    {  47, GLFW_KEY_V,             "V" },
    {  48, GLFW_KEY_B,             "B" },
    {  49, GLFW_KEY_N,             "N" },
    {  50, GLFW_KEY_M,             "M" },
    {  51, GLFW_KEY_COMMA,         "COMMA" },
    {  52, GLFW_KEY_PERIOD,        "PERIOD" },
    {  53, GLFW_KEY_SLASH,         "SLASH" },
    {  54, GLFW_KEY_RIGHT_SHIFT,   "RSHIFT" },
    {  55, GLFW_KEY_KP_MULTIPLY,   "MULTIPLY" },
    {  56, GLFW_KEY_LEFT_ALT,      "LMENU" },
    {  57, GLFW_KEY_SPACE,         "SPACE" },
    {  58, GLFW_KEY_CAPS_LOCK,     "CAPITAL" },
    {  59, GLFW_KEY_F1,            "F1" },
    {  60, GLFW_KEY_F2,            "F2" },
    {  61, GLFW_KEY_F3,            "F3" },
    {  62, GLFW_KEY_F4,            "F4" },
    {  63, GLFW_KEY_F5,            "F5" },
    {  64, GLFW_KEY_F6,            "F6" },
    {  65, GLFW_KEY_F7,            "F7" },
    {  66, GLFW_KEY_F8,            "F8" },
    {  67, GLFW_KEY_F9,            "F9" },
    {  68, GLFW_KEY_F10,           "F10" },
    {  69, GLFW_KEY_NUM_LOCK,      "NUMLOCK" },
    {  70, GLFW_KEY_SCROLL_LOCK,   "SCROLL" },
    {  71, GLFW_KEY_KP_7,          "NUMPAD7" },
    {  72, GLFW_KEY_KP_8,          "NUMPAD8" },
    {  73, GLFW_KEY_KP_9,          "NUMPAD9" },
    {  74, GLFW_KEY_KP_SUBTRACT,   "SUBTRACT" },
    {  75, GLFW_KEY_KP_4,          "NUMPAD4" },
    {  76, GLFW_KEY_KP_5,          "NUMPAD5" },
    {  77, GLFW_KEY_KP_6,          "NUMPAD6" },
    {  78, GLFW_KEY_KP_ADD,        "ADD" },
    {  79, GLFW_KEY_KP_1,          "NUMPAD1" },
    {  80, GLFW_KEY_KP_2,          "NUMPAD2" },
    {  81, GLFW_KEY_KP_3,          "NUMPAD3" },
    {  82, GLFW_KEY_KP_0,          "NUMPAD0" },
    {  83, GLFW_KEY_KP_DECIMAL,    "DECIMAL" },
    {  87, GLFW_KEY_F11,           "F11" },
    {  88, GLFW_KEY_F12,           "F12" },
    { 100, GLFW_KEY_F13,           "F13" },
    { 101, GLFW_KEY_F14,           "F14" },
    { 102, GLFW_KEY_F15,           "F15" },
    { 141, GLFW_KEY_KP_EQUAL,      "NUMPADEQUALS" },
    { 156, GLFW_KEY_KP_ENTER,      "NUMPADENTER" },
    { 157, GLFW_KEY_RIGHT_CONTROL, "RCONTROL" },
    { 181, GLFW_KEY_KP_DIVIDE,     "DIVIDE" },
    { 183, GLFW_KEY_PRINT_SCREEN,  "SYSRQ" },
    { 184, GLFW_KEY_RIGHT_ALT,     "RMENU" },
    { 197, GLFW_KEY_PAUSE,         "PAUSE" },
    { 199, GLFW_KEY_HOME,          "HOME" },
    { 200, GLFW_KEY_UP,            "UP" },
    { 201, GLFW_KEY_PAGE_UP,       "PRIOR" },
    { 203, GLFW_KEY_LEFT,          "LEFT" },
    { 205, GLFW_KEY_RIGHT,         "RIGHT" },
    { 207, GLFW_KEY_END,           "END" },
    { 208, GLFW_KEY_DOWN,          "DOWN" },
    { 209, GLFW_KEY_PAGE_DOWN,     "NEXT" },
    { 210, GLFW_KEY_INSERT,        "INSERT" },
    { 211, GLFW_KEY_DELETE,        "DELETE" },
    { 219, GLFW_KEY_LEFT_SUPER,    "LWIN" },
    { 220, GLFW_KEY_RIGHT_SUPER,   "RWIN" },
    { 221, GLFW_KEY_MENU,          "APPS" },
};

#define KEY_COUNT ((int)(sizeof KEYS / sizeof KEYS[0]))

int Keys_fromGlfw(int glfwKey) {
    for (int i = 0; i < KEY_COUNT; i++) {
        if (KEYS[i].glfw == glfwKey) return KEYS[i].key;
    }
    return 0;
}

int Keys_toGlfw(int key) {
    for (int i = 0; i < KEY_COUNT; i++) {
        if (KEYS[i].key == key) return KEYS[i].glfw;
    }
    return -1;
}

const char* Keys_name(int key) {
    for (int i = 0; i < KEY_COUNT; i++) {
        if (KEYS[i].key == key) return KEYS[i].name;
    }
    return NULL;
}
