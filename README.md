# mc-classic-c

Early Minecraft Classic versions rewritten from Java into plain C99 + OpenGL.

Sequel to [mc-preclassic-c](https://github.com/degradka/mc-preclassic-c).

## Versions

- [c0.0.11a](c0.0.11a/)
- [c0.0.13a](c0.0.13a/)
- [c0.0.13a_03](c0.0.13a_03/)
- [c0.0.14a_08](c0.0.14a_08/)
- [c0.0.16a_02](c0.0.16a_02/)
- [c0.0.17a](c0.0.17a/)
- [c0.0.18a_02](c0.0.18a_02/)
- [c0.0.19a_04](c0.0.19a_04/)
- [c0.0.20a_02](c0.0.20a_02/)
- [c0.0.23a_01](c0.0.23a_01/)
- [c0.24_st_03](c0.24_st_03/)
- [c0.25_05_st](c0.25_05_st/)

Dedicated servers:
- [server 1.2](server/server1.2/)
- [server 1.3](server/server1.3/)
- [server 1.4.1](server/server1.4.1/)
- [server 1.6](server/server1.6/)
- [server 1.8.2](server/server1.8.2/)

Multiplayer works from c0.0.16a_02 on: start a server from its folder, then
`./src/minecraft.exe [host] [port] [name]`.

## Building

Windows, in the **MSYS2 MinGW x64** shell:
```bash
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-glfw mingw-w64-x86_64-glew mingw-w64-x86_64-zlib make
cd mc-classic-c/[version]/src/
make
cd ..
./src/minecraft.exe
```
