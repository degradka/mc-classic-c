# Classic 0.0.11a

[Back to all versions](../README.md)

Released May 17, 2009, three minutes after 0.0.10a, to fix that one being unplayable.
This is where the project starts.

## What it has

- Six tiles: rock, grass, dirt, stone brick, wood and bush
- A 256x256x64 level made from noise heightmaps
- Grass spreads onto lit dirt next to it
- 10 zombies at the start, **G** spawns another one at the player, they get removed below y -100
- Particles when a tile breaks
- Left click breaks or places, right click switches between the two
- **1**, **2**, **3**, **4** and **6** pick rock, dirt, stone brick, wood and bush
- **Enter** saves the level, **Y** flips the mouse Y axis, **Escape** frees the mouse

## How it was ported

- The jar isn't obfuscated, so the decompiled code has all the real class and method names
- The files follow the packages of the jar
- The jar already has multiplayer classes in `comm/` and `server/` that nothing uses. They're ported too, and nothing uses them here either

## Differences from the original

- It's a normal window, not an applet
- G stops at 100 zombies, the jar has no limit

## Building and running

### Windows

Install [MSYS2](https://www.msys2.org/), open the **"MSYS2 MinGW x64"** shell (not the plain "MSYS2 MSYS" one) and get the toolchain and libraries:
```bash
pacman -Syu                    # if it asks, close the window and reopen MINGW64, then run again
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-glfw \
  mingw-w64-x86_64-glew \
  mingw-w64-x86_64-zlib \
  make
```

Build it:
```bash
git clone https://github.com/degradka/mc-classic-c.git
cd mc-classic-c/c0.0.11a/src
make
```

Run it from the version's folder, so it finds `resources/`:
```bash
cd ..
./src/minecraft.exe
```

To run it outside the MSYS2 shell, put these next to the exe (they're in `C:\msys64\mingw64\bin`):
- `zlib1.dll`
- `libwinpthread-1.dll`
- `glew32.dll`
- `glfw3.dll`

and the `resources/` folder from the version's folder.

### Linux and macOS
The Makefile has targets for both, but **neither has been tested yet**.

## References

- [minecraft.wiki: Java Edition Classic 0.0.11a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.11a)
