# Dedicated server 1.10.1

[Back to all versions](../../README.md)

Released December 11, 2009, for the 0.30 client. Server 1.10 is folded into this one, 1.10.1 only fixed
`grow-trees` again.

## What changed

- `admin-slot` in `server.properties`, off by default. When it's on, the last free slot is kept for admins,
  anyone else gets "The server is full!"
- A big warning in the log on every start while `verify-names` is off, with an extra line if the server is public
- Names shorter than 2 or longer than 16 characters get kicked with "Illegal name.". The login still goes
  on after that kick, so the player gets logged in for a moment until the connection closes
- A failed name check now says "The name wasn't verified by minecraft.net!"
- The protocol and ban checks come before the "logged in as" line in the log
- `grow-trees` now does what it says: saplings only grow while it's on, and while it's off they don't tick
  at all. Dandelions and roses always tick again
- A growing tree no longer replaces blocks around it, leaves and logs only go where there's air

## How it was ported

- Ported from the decompiled server jar, which is obfuscated like the clients

## Differences from the original

- No heartbeat to the minecraft.net server list, it's long gone. `public` only adds the extra lines to the warning,
  so the `server.properties` here has it off
- Error lines keep the original's wording, including Java's error names, but not the Java stack trace under them
- `verify-names` is off by default (the original had it on). It checks the key minecraft.net gave each player,
  and without minecraft.net nobody has one. It still works like the original if you turn it on.
  Because it's off, the big warning shows up on every start

Like the original, `max-connections` is read from `server.properties` but always saved back as 3.

## Playing on it

Works with the [c0.30](../../c0.30/) client here and with the original Java one. Start the server, then the client from its folder:
```bash
cd mc-classic-c/c0.30
./src/minecraft.exe 127.0.0.1 25565 yourname
```

A level saved by a 0.30 client can't be used as `server_level.dat`, the server has no mob or player classes.
It says so and generates a new level, like the original.

## Files

These are located in the folder the server runs from:
- `server.properties`: `server-name`, `motd`, `port`, `max-players`, `public`, `verify-names`, `max-connections`, `grow-trees`, `admin-slot`
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
cd mc-classic-c/server/server1.10.1/src
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

- [minecraft.wiki: Java Edition Classic server 1.10](https://minecraft.wiki/w/Java_Edition_Classic_server_1.10)
- [minecraft.wiki: Java Edition Classic server 1.10.1](https://minecraft.wiki/w/Java_Edition_Classic_server_1.10.1)
