// keys.h: key codes and names, options.txt and the game logic use these

#ifndef KEYS_H
#define KEYS_H

#define KEY_ESCAPE 1
#define KEY_1      2
#define KEY_BACK   14
#define KEY_TAB    15
#define KEY_RETURN 28
#define KEY_G      34
#define KEY_LSHIFT 42
#define KEY_RSHIFT 54

// 0 when there's no code for it
int Keys_fromGlfw(int glfwKey);
// -1 when glfw has no key for it
int Keys_toGlfw(int key);
// Keyboard.getKeyName, NULL for a code without a name
const char* Keys_name(int key);

#endif
