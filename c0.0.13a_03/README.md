# Classic 0.0.13a_03

[Back to all versions](../README.md)

Released May 22, 2009, the same day as 0.0.13a. The first obfuscated version.

## What changed

- World generation
  - Rolling hills: the heightmap step is filled in now, so the terrain is no longer flat
  - A new Soiling.. stage puts grass, dirt and rock on top of the heightmap
  - Lava only goes below depth / 2 - 4, so it stays under sea level
  - Water lakes get one try per 200 tiles of area instead of one per 5000
- No zombies at the start of a level anymore, **G** still spawns them
- **Escape** closes whatever menu is open
- The pause menu background is darker
- The loading screen has a progress bar
- Generate new level makes a 256x256x64 level instead of 32x512x64
- Save level.. and Load level.. now work through the website the applet runs on, for players who are logged in

## How it was ported

- From this version on every class, field and method in the jar is a single letter
- Ported on top of c0.0.13a: the new jar was decompiled and diffed against 0.0.13a, and what each changed piece does was figured out from the code itself
- Some of the world generation methods don't decompile at all, those were read from the bytecode

## Differences from the original

- It's a normal window, not an applet
- Save level.. and Load level.. only did something when you were logged in on minecraft.net, which is gone. Here you're never logged in, so they do nothing, same as the original without an account
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
cd mc-classic-c/c0.0.13a_03/src
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

- [minecraft.wiki: Java Edition Classic 0.0.13a_03](https://minecraft.wiki/w/Java_Edition_Classic_0.0.13a_03)
