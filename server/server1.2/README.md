# Dedicated server 1.2

[Back to all versions](../../README.md)

Released June 8, 2009, for the 0.0.16a_02 client. The first server in this project.

## What it has

- Up to `max-players` players on one level, block changes and movement go out to everyone
- Chat
- Admin commands in chat
  - /kick, /ban, /unban, /banip, /op, /deop, /say, /broadcast
  - like the original, there's no console, you become an admin by putting your name in `admins.txt`
- The level gets generated on the first start and saved to `server_level.dat`

## How it was ported

- Ported from the decompiled server jar, which is obfuscated like the clients

## Differences from the original

- No heartbeat to the minecraft.net server list, it's long gone. `public` in `server.properties` does nothing
- Error lines keep the original's wording, including Java's error names, but not the Java stack trace under them

## Playing on it

Works with the [c0.0.16a_02](../../c0.0.16a_02/) client here and with the original Java one. Start the server, then the client from its folder:
```bash
cd mc-classic-c/c0.0.16a_02
./src/minecraft.exe 127.0.0.1 25565 yourname
```

## Files

These are located in the folder the server runs from:
- `server.properties`: `server-name`, `motd`, `port`, `max-players`, `public`
- `admins.txt`, `banned.txt`, `banned-ip.txt`: one name or ip per line
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
cd mc-classic-c/server/server1.2/src
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

- [minecraft.wiki: Java Edition Classic server 1.2](https://minecraft.wiki/w/Java_Edition_Classic_server_1.2)
