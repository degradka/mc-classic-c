# mc-classic-c

Early Minecraft Classic versions rewritten from Java into plain C99 + OpenGL.

Sequel to [mc-preclassic-c](https://github.com/degradka/mc-preclassic-c).

## Versions

- [c0.0.11a](c0.0.11a/)
- [c0.0.13a](c0.0.13a/)
- [c0.0.13a_03](c0.0.13a_03/)
- [c0.0.14a_08](c0.0.14a_08/)

## Building

Windows, in the **MSYS2 MinGW x64** shell:
```bash
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-glfw mingw-w64-x86_64-glew mingw-w64-x86_64-zlib make
cd mc-classic-c/[version]/src/
make
cd ..
./src/minecraft.exe
```
