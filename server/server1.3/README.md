# Dedicated server 1.3

[Back to all versions](../../README.md)

Released June 10, 2009, for the 0.0.17a client.

## What changed

- /setspawn sets the spawn to where the admin stands
- Placing or breaking a block more than 8 blocks away kicks you
- Only some tiles can be placed by hand
- A 4th connection from the same ip is refused
- Logging in again under the same name kicks the old one
- A connection that doesn't log in within 5 seconds is kicked
- `verify-names` in `server.properties`: a name has to come with the key minecraft.net handed the client
- /banip also takes an ip, and announces "<names> got ip banned!" (with the original's stray comma in front)
- `players.txt` lists who's online

## How it was ported

- Ported from the decompiled server jar, which is obfuscated like the clients

## Differences from the original

- No heartbeat to the minecraft.net server list, it's long gone. `public` in `server.properties` does nothing
- Error lines keep the original's wording, including Java's error names, but not the Java stack trace under them
- `verify-names` is off by default (the original had it on). It checks the key minecraft.net gave each player,
  and without minecraft.net nobody has one. It still works like the original if you turn it on

## Playing on it

Works with the [c0.0.17a](../../c0.0.17a/) client here and with the original Java one. Start the server, then the client from its folder:
```bash
cd mc-classic-c/c0.0.17a
./src/minecraft.exe 127.0.0.1 25565 yourname
```

## Files

These are located in the folder the server runs from:
- `server.properties`: `server-name`, `motd`, `port`, `max-players`, `public`, `verify-names`
- `admins.txt`, `banned.txt`, `banned-ip.txt`, `players.txt`: one name or ip per line
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
cd mc-classic-c/server/server1.3/src
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

- [minecraft.wiki: Java Edition Classic server 1.3](https://minecraft.wiki/w/Java_Edition_Classic_server_1.3)
