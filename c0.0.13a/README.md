# Classic 0.0.13a

[Back to all versions](../README.md)

Released May 22, 2009. The wiki says the jar that survived is an early development build of 0.0.13a,
not the one that was released. That build is what's ported here.

## What changed

- Water and lava, flowing and still. Where they meet they turn into rock
- Bedrock
- A new world generator with the stages Raising, Eroding, Carving, Watering and Melting
  - Caves, water and lava lakes
  - The heightmap step is still empty in this build, so the terrain comes out flat
- Ground and water around the map, so the world doesn't just end
- A thin black outline around the targeted block
- **Escape** opens a pause menu: Generate new level, Save level.., Load level.., Back to game
- The level is saved to and loaded from `level.dat`
- Chunks are sorted by distance before they get rebuilt
- The window is 854x480

## How it was ported

- This jar isn't obfuscated either, all the names are real
- Ported on top of c0.0.11a, following the diff between the two jars

## Quirks kept from the original

- Save level.. and Load level.. are in the pause menu but do nothing, the jar has no code behind them

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
cd mc-classic-c/c0.0.13a/src
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

- [minecraft.wiki: Java Edition Classic 0.0.13a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.13a)
