# Classic 0.27 SURVIVAL TEST

[Back to all versions](../README.md)

Released October 24, 2009. 0.26 SURVIVAL TEST is folded in here.

![c0.27_st](../.assets/c0.27_st.png)

## What changed

- TNT: you start a new level with 10, hitting it lights it, it explodes a bit later
- Spiders, they leap at you
- New tiles: slabs (two make a double slab), iron block, bricks, bookshelf, mossy cobblestone
- Coal ore drops slabs, stone drops cobblestone
- **F5** toggles rain, with splashes where it lands
- Explosions leave smoke behind
- A Limit framerate option
- Signs are gone
- Menus lost their background gradient

## How it was ported

- Ported with names: the RetroMCP names for 0.0.23a_01 carried to this jar with a matcher that pairs classes and methods by their structure, the rest named by hand
- Afterwards the whole C code was checked against the whole jar, not just the diff, and the bugs it found were fixed

## Differences from the original

- It's a normal window, not an applet
- No multiplayer. The original jar still has its connection code (the applet could get a server), it's left out here
- Fully offline: no skin downloads, always `char.png`, and Save level.. and Load level.. stay greyed out, they need a minecraft.net login that's gone
- At most 64 arrows and 256 dropped items exist at once, java has no limit
- Singleplayer only, like the original

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
cd mc-classic-c/c0.27_st/src
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

- [minecraft.wiki: Java Edition Classic 0.27 SURVIVAL TEST](https://minecraft.wiki/w/Java_Edition_Classic_0.27_SURVIVAL_TEST)
- [minecraft.wiki: Java Edition Classic 0.26 SURVIVAL TEST](https://minecraft.wiki/w/Java_Edition_Classic_0.26_SURVIVAL_TEST)
