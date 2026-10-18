# Classic 0.0.16a_02

[Back to all versions](../README.md)

Released June 7, 2009. The first version with multiplayer. Everything since 0.0.14a_08,
the whole 0.0.15a multiplayer test included, is folded in here.

![c0.0.16a_02](../.assets/c0.0.16a_02.png)

## What changed

- Multiplayer
  - Connect to a server, other players walk around with their name over their head, Notch's is yellow
  - Blocks broken and placed by anyone show up for everyone
  - **T** opens the chat, **Enter** sends it
  - **G** doesn't spawn zombies while connected
  - Losing the connection shows a message screen
- Lava waits longer between spreading than water
- Leaf particles fall slower

## How it was ported

- Obfuscated, ported on top of the previous version by diffing the decompiled jars

## Differences from the original

- It's a normal window, not an applet
- The applet got the server from the web page, here it's the command line: `minecraft.exe [host] [port] [username]`
- Save level.. and Load level.. need you to be logged in on minecraft.net, which is gone, so they're always greyed out
- Sending a chat message in singleplayer crashes the original with a NullPointerException, here it does nothing

## Multiplayer

Pairs with [server 1.2](../server/server1.2/). Start the server, then from this version's folder:
```bash
./src/minecraft.exe 127.0.0.1 25565 yourname
```
No arguments means singleplayer. Without a name you're `guest`.

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
cd mc-classic-c/c0.0.16a_02/src
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

- [minecraft.wiki: Java Edition Classic 0.0.16a_02](https://minecraft.wiki/w/Java_Edition_Classic_0.0.16a_02)
- [minecraft.wiki: Java Edition Classic 0.0.15a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.15a)
