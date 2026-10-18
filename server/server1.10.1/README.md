# Dedicated server 1.9.1

[Back to all versions](../../README.md)

Released October 29, 2009, for the 0.29 clients.

## What changed

- `/op` and `/deop` update the player's client right away, no relog needed. With the 0.29_02 client an op
  can break bedrock as soon as they're opped
- `/deop` also turns `/solid` off
- `grow-trees` in `server.properties`, off by default. Despite the name it doesn't touch saplings,
  while it's on dandelions and roses stop ticking, so they stop dying off in the dark or off grass
- The level is saved as creative

## How it was ported

- Ported from the decompiled server jar, which is obfuscated like the clients

## Differences from the original

- No heartbeat to the minecraft.net server list, it's long gone. `public` in `server.properties` does nothing
- Error lines keep the original's wording, including Java's error names, but not the Java stack trace under them
- `verify-names` is off by default (the original had it on). It checks the key minecraft.net gave each player,
  and without minecraft.net nobody has one. It still works like the original if you turn it on

Like the original, `max-connections` is read from `server.properties` but always saved back as 3.

## Playing on it

Works with the [c0.29_02](../../c0.29_02/) client here and with the original Java one. Start the server, then the client from its folder:
```bash
cd mc-classic-c/c0.29_02
./src/minecraft.exe 127.0.0.1 25565 yourname
```

A level saved by a 0.28 client can't be used as `server_level.dat`, the server has no mob or player classes.
It says so and generates a new level, like the original.

## Files

These are located in the folder the server runs from:
- `server.properties`: `server-name`, `motd`, `port`, `max-players`, `public`, `verify-names`, `max-connections`, `grow-trees`
- `admins.txt`, `banned.txt`, `banned-ip.txt`, `players.txt`: one name or ip per line
- `server.log`: the log
- `server_level.dat`: the level, saved every minute or so

## Building and running

### Windows

Install [MSYS2](https://www.msys2.org/), open the **"MSYS2 MinGW x64"** shell (not the plain "MSYS2 MSYS" one) and get the toolchain and libraries:
```bash
pacman -Syu                    # if it asks, close the window and reopen MINGW64, then run again
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-zlib \
  make
```

Build it:
```bash
git clone https://github.com/degradka/mc-classic-c.git
cd mc-classic-c/server/server1.9.1/src
make
```

Run it from the server's folder, it reads and writes `server.properties`, the player lists and the level there:
```bash
cd ..
./src/minecraft-server.exe
```

To run it outside the MSYS2 shell, put `zlib1.dll` (from `C:\msys64\mingw64\bin`) next to the exe.

### Linux and macOS
The Makefile has targets for both, but **neither has been tested yet**.

## References

- [minecraft.wiki: Java Edition Classic server 1.9.1](https://minecraft.wiki/w/Java_Edition_Classic_server_1.9.1)
