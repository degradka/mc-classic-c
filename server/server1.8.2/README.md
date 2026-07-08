# Dedicated server 1.8.2

[Back to all versions](../../README.md)

Released June 20, 2009, for the 0.0.20a_02 client. It also works with 0.0.23a_01, there was no new server for that one.

## What changed

- Only admins can break bedrock
- /solid makes stone you place come out as bedrock
- /tp works the same as /teleport
- Cloth, gold block and the new plants can be placed, cobblestone and sand again too
- Flowers and mushrooms die off dirt or grass or in the dark, like saplings (mushrooms too)
- The login tells the client if it's an op
- Commands can start with any number of `/`
- "Too much lag" only past 1200 waiting block changes and moves

## How it was ported

- Ported from the decompiled server jar, which is obfuscated like the clients

## Differences from the original

- No heartbeat to the minecraft.net server list, it's long gone. `public` in `server.properties` does nothing
- Error lines keep the original's wording, including Java's error names, but not the Java stack trace under them
- `verify-names` is off by default (the original had it on). It checks the key minecraft.net gave each player,
  and without minecraft.net nobody has one. It still works like the original if you turn it on

Like the original, `max-connections` is read from `server.properties` but always saved back as 3.

## Playing on it

Works with the [c0.0.20a_02](../../c0.0.20a_02/) and [c0.0.23a_01](../../c0.0.23a_01/) clients here and with the original Java ones. Start the server, then the client from its folder:
```bash
cd mc-classic-c/c0.0.20a_02
./src/minecraft.exe 127.0.0.1 25565 yourname
```

## Files

These are located in the folder the server runs from:
- `server.properties`: `server-name`, `motd`, `port`, `max-players`, `public`, `verify-names`, `max-connections`
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
cd mc-classic-c/server/server1.8.2/src
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

- [minecraft.wiki: Java Edition Classic server 1.8.2](https://minecraft.wiki/w/Java_Edition_Classic_server_1.8.2)
