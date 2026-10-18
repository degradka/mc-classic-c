# Classic 0.30

[Back to all versions](../README.md)

Released November 10, 2009, the last Classic. It came out as two jars, one creative and one survival.
The two are the same code with one line different, the game mode the game starts in, so here it's one
folder that builds both: `minecraft.exe` is creative and `minecraft-survival.exe` is survival. The 2011
re-signed creative jar has the same code as the 2009 one, only its signature changed.

## What changed

- Survival
  - Blocks drop what they're made of: stone drops cobblestone, grass drops dirt, leaves drop saplings,
    logs drop planks. Gold and iron ore drop gold and iron blocks, and coal ore drops a slab
  - TNT you break gets lit and explodes after two seconds, it smokes while it burns. Hitting lit TNT
    puts it out and drops it
  - An explosion lights the TNT it hits with a shorter fuse, and the blocks it breaks drop things
  - All the mobs spawn: zombies, skeletons, pigs, creepers, spiders and sheep, before only sheep did
- Middle click on bedrock picks stone
- New levels generate a lot faster, the loading screen draws at most every 20 ms

## How it was ported

- Ported with names: the names for 0.29_02 carried to this jar with the matcher
- The creative and survival jars were compared class by class, they only differ in the game mode line
  and what the obfuscator removed because of it
- The whole diff was read with local names evened out, and every change checked against the bytecode

## Differences from the original

- It's a normal window, not an applet
- 0.30 throws away every level unless it runs as an applet on minecraft.net, that check is left out
- Fully offline: no skin downloads, always `char.png`, and Save level.. and Load level.. stay greyed out.
  0.30 can save to a file on your computer, but only from a logged in minecraft.net session
- At most 64 arrows, 256 dropped items and 256 lit TNT exist at once, java has no limit
- The applet got the server from the web page, here it's the command line: `minecraft.exe [host] [port] [username]`

## Multiplayer

Start [server 1.9.1](../server/server1.9.1/), then from this version's folder:
```bash
./src/minecraft.exe 127.0.0.1 25565 yourname
```
No arguments means singleplayer.

Servers are made for creative. `minecraft-survival.exe` can join one too, but a server only knows about
blocks, positions and chat, so everything survival adds stays on your own screen:
- your mobs spawn only for you, and nobody else sees them or gets attacked by them
- dropped items, health and inventory exist only on your side
- hitting another player only hurts your own copy of them, they don't notice, and if that copy dies
  they disappear from your screen until they rejoin
- creepers and TNT don't break anything

It technically works, but it's not a pleasing experience.

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
cd mc-classic-c/c0.30/src
make
```

Run it from the version's folder, so it finds `resources/`:
```bash
cd ..
./src/minecraft.exe
./src/minecraft-survival.exe
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

- [minecraft.wiki: Java Edition Classic 0.30](https://minecraft.wiki/w/Java_Edition_Classic_0.30)
