# Classic 0.29_02

[Back to all versions](../README.md)

Released October 30, 2009. 0.29 and 0.29_01 came out the day before, they're folded in here.
A multiplayer fix release for 0.28.

## What changed

- Other players face the right way, in 0.28 everyone saw everyone else's yaw mirrored
- Other players are drawn a bit higher
- Names only show for the player the crosshair is on, out to 32 blocks, and they show through walls
- Ops get their op status as soon as the server changes it, and keep it when the level loads,
  so they can break bedrock now
- **F5** toggles rain in multiplayer too
- Dandelions and roses stop dying off in the dark or off grass, except in creative
- Aiming at slabs, flowers and other blocks that aren't full cubes uses the real hit point

## How it was ported

- Ported with names: the names for 0.28_01 carried to this jar with the matcher, it paired every class
- The whole diff was read with local names evened out, and every change checked against the bytecode

## Differences from the original

- It's a normal window, not an applet
- Fully offline: no skin downloads, always `char.png`, and Save level.. and Load level.. stay greyed out, they need a minecraft.net login that's gone
- At most 64 arrows and 256 dropped items exist at once, java has no limit
- The applet got the server from the web page, here it's the command line: `minecraft.exe [host] [port] [username]`

## Multiplayer

Start [server 1.9.1](../server/server1.9.1/), then from this version's folder:
```bash
./src/minecraft.exe 127.0.0.1 25565 yourname
```
No arguments means singleplayer.

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
cd mc-classic-c/c0.29_02/src
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

- [minecraft.wiki: Java Edition Classic 0.29_02](https://minecraft.wiki/w/Java_Edition_Classic_0.29_02)
- [minecraft.wiki: Java Edition Classic 0.29_01](https://minecraft.wiki/w/Java_Edition_Classic_0.29_01)
- [minecraft.wiki: Java Edition Classic 0.29](https://minecraft.wiki/w/Java_Edition_Classic_0.29)
