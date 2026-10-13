# mc-classic-c
**Minecraft Classic rewritten as God intended.**

---

<!-- screenshot goes here -->

---

## What is this?
This is the sequel to [mc-preclassic-c](https://github.com/degradka/mc-preclassic-c).
**Minecraft Classic**, from **c0.0.11a** to **c0.28_01**, rewritten from Java into plain C99 + OpenGL.

<details>
<summary>Why some versions are missing</summary>

Not every Classic version is here. Some only had minor changes or bug fixes over the one before,
and some I skipped because their files didn't survive and thus can't be ported at all,
their changes come in with the next version that did.

If you want one of the skipped versions ported, ask me, or port it yourself and open a pull request.

</details>

The rules I stick to:
- **It has to be the same game.** Same constants, same formulas, same tick order, same render order,
  same bugs. If something looks stupid or illogical by modern standards, that's how it was in the
  original, and it stays.
- **As few dependencies as possible.** GLFW, GLEW, zlib, stb_image and stb_vorbis. That's it.
- **Fully offline.** No login, no skin downloads, no saving levels to minecraft.net. Those servers
  are no longer alive.

---

## Versions covered
Every version has its own README with what changed, how it was ported and the weird stuff found along the way.

- [c0.0.11a](c0.0.11a/)
- [c0.0.13a](c0.0.13a/)
- [c0.0.13a_03](c0.0.13a_03/)
- [c0.0.14a_08](c0.0.14a_08/)
- [c0.0.16a_02](c0.0.16a_02/)
- [c0.0.17a](c0.0.17a/)
- [c0.0.18a_02](c0.0.18a_02/)
- [c0.0.19a_04](c0.0.19a_04/)
- [c0.0.20a_02](c0.0.20a_02/)
- [c0.0.23a_01](c0.0.23a_01/)
- [c0.24_st_03](c0.24_st_03/)
- [c0.25_05_st](c0.25_05_st/)
- [c0.27_st](c0.27_st/)
- [c0.28_01](c0.28_01/)

Dedicated servers:

- [server 1.2](server/server1.2/)
- [server 1.3](server/server1.3/)
- [server 1.4.1](server/server1.4.1/)
- [server 1.6](server/server1.6/)
- [server 1.8.2](server/server1.8.2/)
- [server 1.9](server/server1.9/)

---

## Multiplayer
Multiplayer got added in c0.0.16a_02 and it works here too, dedicated servers included.
The servers are C ports of the original dedicated server jars.
They work with the original Java clients too, not just with these.

| Client | Server |
|---|---|
| c0.0.16a_02 | [server 1.2](server/server1.2/) |
| c0.0.17a | [server 1.3](server/server1.3/) |
| c0.0.18a_02 | [server 1.4.1](server/server1.4.1/) |
| c0.0.19a_04 | [server 1.6](server/server1.6/) |
| c0.0.20a_02, c0.0.23a_01 | [server 1.8.2](server/server1.8.2/) |
| c0.28_01 | [server 1.9](server/server1.9/) |

The Survival Test versions (c0.24_st_03 to c0.27_st) are singleplayer only here. Their jars still have the
multiplayer code, but it's left out of the port.

Build the server like a client (see [Building](#building)), then start it from its folder in the MINGW64 shell.
It reads `server.properties` from the folder it runs in, the port is 25565 by default:
```bash
cd mc-classic-c/server/server1.8.2
./src/minecraft-server.exe
```
Outside the MSYS2 shell it only needs `zlib1.dll` next to the exe.

Then start the client with the host, port and your name:
```bash
cd mc-classic-c/c0.0.23a_01
./src/minecraft.exe 127.0.0.1 25565 degradka
```
No arguments means singleplayer. Without a name you're `guest`.

<!-- multiplayer video goes here -->

---

## Building

### Windows

#### Install MSYS2
Download and install from [msys2.org](https://www.msys2.org/).

#### Open the right shell
Launch **"MSYS2 MinGW x64"** (the icon says *MINGW64*).
:warning: Do **not** use the plain "MSYS2 MSYS" shell.

#### Update and install toolchain + libraries
```bash
pacman -Syu                    # if it asks, close the window and reopen MINGW64, then run again
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-glfw \
  mingw-w64-x86_64-glew \
  mingw-w64-x86_64-zlib \
  make
```

#### Build
```bash
git clone https://github.com/degradka/mc-classic-c.git
cd mc-classic-c/[version]/src/
make
```

#### Run
From the MINGW64 shell, run it from the version's folder, so it finds `resources/`:
```bash
cd ..
./src/minecraft.exe
```

To run it outside the shell, put these next to the exe (they're in `C:\msys64\mingw64\bin`):
- `zlib1.dll`
- `libwinpthread-1.dll`
- `glew32.dll`
- `glfw3.dll`

and the `resources/` folder from the version's folder.

### Linux and macOS
The Makefile has targets for both (ALSA for sound on Linux, AudioToolbox on macOS),
but **neither has been tested yet**. If you try it, tell me how it went.

---

## How the versions were ported
I don't have Notch's source. Every version starts from its original jar.

- **c0.0.11a and c0.0.13a** weren't obfuscated, so the decompiled code still has its real class
  and method names. Those were the easy ones.
- **From c0.0.13a_03 on** every class, field and method is a single letter. For those I decompile
  the new jar, diff it against the previous version, figure out what changed from what the code does,
  and port only that.
- **From c0.0.23a_01 on** there are community names: RetroMCP has mappings for c0.0.23a_01, and I carry
  them forward to every newer jar with a small matcher that pairs classes and methods by their structure.
  Whatever it can't match gets named by hand.
- Decompilers lie sometimes. When something looks off, I check the bytecode, not the decompiled Java.
- Versions with no surviving jar are folded into the next one that has one.
- Diffing alone misses bugs that were carried over from older ports, so the later versions were also
  checked line by line against their whole jar, not only the diff.

---

## Contributing

I have a love-hate relationship with this project. I've been working on it on and off,
so I've definitely missed something while testing. If you try it out, play it with
friends or just poke around and find something that's off, I'd love to hear about it.

Open an issue or a pull request, or reach out to me on social media or email.

---

## References
- [minecraft.wiki: Java Edition Classic](https://minecraft.wiki/w/Java_Edition_Classic), every version's page is linked from its own README
- [#minecraft IRC logs from 2009](https://archive.org/details/Minecraft_IRC_Logs_2009)
- [RetroMCP](https://github.com/MCPHackers/RetroMCP-Java), the c0.0.23a_01 names everything else is mapped from
- [CFR](https://github.com/leibnitz27/cfr), the decompiler
- [mc-preclassic-c](https://github.com/degradka/mc-preclassic-c), the part before this one

---

If you think this is a dumb project, you're right. But it runs.
