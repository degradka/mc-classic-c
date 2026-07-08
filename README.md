# mc-classic-c

Early Minecraft Classic versions rewritten in plain C.

Sequel to [mc-preclassic-c](https://github.com/degradka/mc-preclassic-c), which covers the pre Classic rd builds. This one picks up where that left off and ports the early Classic single player versions, decompiling each Java release and rewriting it, keeping the original bugs and quirks intact rather than fixing them (mostly).

## Versions covered

### c0.0.11a

Released May 17 2009. This was the first version Notch made publicly available, posted on the TIGSource forum. It shipped as a quick fix over 0.0.10a, which had a broken start and destroy function that left the game unplayable.

* Break and place the six original tiles: rock, grass, dirt, stone brick, wood, bush
* Perlin noise hill terrain generation, with grass spreading onto lit dirt over time
* Zombies that wander the world, fall, and get removed once they drop below y level minus 100
* Tile destroy particles
* Save the level with Enter, spawn a zombie with G, toggle the mouse Y axis with Y
* Block picking with number keys

### c0.0.13a

Released May 22 2009. Added level saving for accounts, changed the lava, water, and sapling textures, made lava opaque, added a thin block selection outline, and opened a pause menu on Escape. Terrain generation also changed to guarantee a layer of grass over dirt over stone.

* Flat terrain generation with carved caves and flood filled water and lava lakes, replacing the old Perlin hills
* Non cubic water and lava: translucent water, opaque lava, flow and spread ticking, calm variants
* Pause menu with Generate new level and Back to game
* Level loading progress screen shown while a world generates
* Level save and load using the real level.dat format, including differently sized worlds
* Distance sorted chunk rendering, hit outline, and an infinite horizon ground and water plane
* Reset position with R, toggle draw distance with F
* Multiplayer classes ported and present in source, unused in the running game, matching the real client for this version

### c0.0.13a_03

Released May 22 2009. Starting from this version, the client jar is obfuscated: package, class, field, and method names are all reduced to single letters with no other information to recover them from. This port was rebuilt by diffing the obfuscated bytecode against the last known good c0.0.13a source and inferring what each piece does from its behavior, so some names and structure here are a best guess rather than a faithful match to what Mojang actually called things.

c0.0.13a_03 is also the last version with a standalone launcher jar available, so it's the last one built by playing the real client side by side with the port. From c0.0.14a_08 on, the client jars are applet only, with no standalone jar or launcher build available, so later versions are ported from reading the decompiled source and the wiki changelog rather than playing the original side by side. Treat behavior in c0.0.14a_08 and everything after it as best effort rather than verified.

* Real Perlin noise hills terrain with an erosion pass, replacing c0.0.13a's flat generation
* Mobs no longer auto spawn when a level generates
* Lava lakes capped below sea level, and interior water lakes seeded far more often
* Lava now falls through an entire open shaft in one tick instead of one block per tick
* Escape closes an open menu screen
* Darker pause menu overlay, and a real loading progress bar during world generation
* Bigger infinite horizon ground and water skirt
* "Generate new level" defaults to a 256x256x64 world, owned by "anonymous" instead of "noname"
* Networked level save/load intentionally not ported: the real jar's backend for it is long dead, so there is nothing left to faithfully port

### c0.0.14a_08

Released May 28 2009 and the last release of Early Classic before multiplayer began. Also obfuscated, ported the same way as c0.0.13a_03.

* Real tree generation: random walk placement, 4-5 block trunks, a leaf canopy that caps into a "+" shape on top
* Ore veins: coal, iron, and gold generate inside the rock layer
* Beach generation replaces grass with sand or gravel near sea level
* Sand and gravel fall through open air, both on a tick and instantly when their support is removed
* Shadows now fall on every tile face including water and lava surfaces, replacing the old flat lit/shadow split
* Daylight fog is linear instead of exponential, and now shrinks with the draw distance toggle
* Clouds
* Hotbar expanded to 9 slots, selectable with number keys, the scroll wheel, or middle click to pick the block under the crosshair
* Holding the mouse button now mines or places repeatedly instead of needing repeated clicks
* Enter sets the world's spawn point instead of saving the level
* Mob cap raised from 100 to 256

### c0.0.16a_02

Released June 7 2009, the final version of the Multiplayer Test series, paired with the dedicated server 1.2. This skips the whole intermediate 0.0.15a series, so the diff against c0.0.14a_08 covers everything that accumulated across those skipped versions, not just one version's worth of change. The real client is applet only with no standalone launcher jar.

* Multiplayer: connect to a server, see other players walk around with a name tag overhead, chat, and break/place blocks together
* Chat: press T to open a chat line, Enter sends it, Escape cancels, and incoming messages show above the hotbar for 10 seconds
* Generate new level now asks for a size (Small, Normal, Huge) instead of always generating the same 256x256x64 world
* G (spawn zombie debug key) is disabled while connected to a server
* Connect failure and mid session disconnects show a plain "Connection lost" style screen instead of just hanging

Multiplayer is a command line switch rather than an in-game menu, matching how the real applet only ever read a server address once at launch:

```
minecraft.exe [host] [port] [username]
```

No arguments starts singleplayer as before. `username` is optional and defaults to `guest` when connecting.

### c0.0.17a

Released June 10 2009, paired with the dedicated server 1.3. The real client is applet only with no standalone launcher jar.

* New Tab key: hold it while connected to see a list of everyone currently on the server
* Chat now shows a scrollback of recent messages while the chat line is open, instead of only ever showing the last few faded lines
* Chat can no longer be opened in singleplayer at all (previously it opened a chat line that did nothing when you tried to send)
* Pause menu now disables Generate new level too while connected to a server, on top of Save/Load already being disabled
* Draw distance (F) now cycles backward when held with Shift, instead of only ever cycling forward

### c0.0.18a_02

Released June 14 2009 as a bugfix pass over 0.0.18a (June 13 2009), paired with the dedicated server 1.4.1. A much smaller update than the previous two versions.

* Chat can now send `\`, `@`, `|`, and `$`, on top of the existing allowed characters
* Server admins can use `/teleport <name>` to warp to another connected player

### c0.0.19a_04

Released June 19-20 2009, accumulating five real point releases (0.0.19a through 0.0.19a_04) paired with dedicated server 1.6. The largest update since c0.0.17a.

* New Glass and Sponge blocks; Cobblestone and Sand are no longer in the default hotbar, replaced by Sponge and Glass. Sponge dries out a 5x5x5 area of water when placed and lets it flow back in when removed
* Real hotbar UI: a highlighted bar across the bottom of the screen showing all 9 quick-select tiles as small isometric blocks, instead of just a single held-item preview
* Water and lava now animate in place instead of using a static texture
* Other connected players no longer render mirrored left-right, and their walking/standing animation looks more natural (no more permanent slight body bob while standing still)
* Chat can now send `;`, on top of the existing allowed characters
* Singleplayer world generation: smaller beaches/shorelines, and caves are noticeably sparser and more broken up rather than long continuous tunnels

### c0.0.20a_02

Released June 20 2009, accumulating five real point releases (0.0.19a_05 through 0.0.20a_02) paired with dedicated server 1.8.2.

* New tiles: 16 dyed Cloth colors, Gold Block, Dandelion, Rose, Brown Mushroom, and Red Mushroom
* New inventory screen (B key): pick any placeable tile from a full grid instead of just the 9 hotbar slots
* Bedrock can no longer be broken unless the player has operator status
* Mouse wheel now cycles the hotbar in the opposite direction from before
* Other players and mobs now catch real directional lighting instead of flat shading, and usernames are easier to read against it
* The 100 FPS cap added last version is removed again
* Water no longer flickers or shows light colored seams at tile borders, and no longer sometimes looks fully opaque

## Building

### Windows

Install MSYS2 from msys2.org, then open the MSYS2 MinGW x64 shell (the icon says MINGW64, not the plain MSYS shell).

```
pacman -Syu
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-glfw \
  mingw-w64-x86_64-glew \
  mingw-w64-x86_64-zlib \
  make
```

Then:

```
git clone https://github.com/degradka/mc-classic-c.git
cd mc-classic-c/c0.0.13a/src/
make
```

The built exe needs these DLLs next to it: `zlib1.dll`, `libwinpthread-1.dll`, `glew32.dll`, `glfw3.dll`, along with the `resources/` folder from the same version.

### Linux

Not tested yet

```
sudo pacman -S glew glfw
```

```
git clone https://github.com/degradka/mc-classic-c.git
cd mc-classic-c/c0.0.13a/src/
make
```

The `resources/` folder from the same version needs to sit next to the built binary.

## Running the dedicated server

`server/server1.2/` is a from-scratch port of the official dedicated server, paired with the c0.0.16a_02 client above. It builds the same way as the client (`cd server/server1.2/src/ && make`), but the built `minecraft-server.exe`/`minecraft-server` should be copied up one level, into `server/server1.2/`, and run from there rather than from `src/` — that's where `server.properties`, `admins.txt`, `banned.txt`, and `banned-ip.txt` live, and the server reads/writes all of them (plus its `server_level.dat` save) relative to whatever directory it's actually launched from.

On Windows the built exe needs `zlib1.dll` next to it (the same one the client uses) — nothing else, since the server has no rendering and uses native Win32 threads instead of `libwinpthread-1.dll`.

`server/server1.3/` is the same idea, paired with the c0.0.17a client instead, and follows the identical build/run/DLL layout. It adds a couple of gameplay-relevant checks the original 1.2 didn't have: placing or breaking a block more than 8 blocks away gets you kicked for cheating, and only a specific set of "safe" tiles (stone brick, wood, log, leaves, sand, gravel, rock, dirt, bush) can be placed by hand at all. The real server's login name verification (checking a connecting player's name against minecraft.net's session servers) is intentionally not implemented here, same reasoning as the networked level save/load noted above for c0.0.13a_03 — there's no live session server to check against, so any username is accepted.

`server/server1.4.1/` pairs with the c0.0.18a_02 client, same build/run/DLL layout again. It adds `/teleport <name>` for admins, rejects usernames and chat messages containing control characters, and logs to a `server.log` file next to the console output.

`server/server1.6/` pairs with the c0.0.19a_04 client, same build/run/DLL layout again. Admin commands can now also be typed directly into the server's own console (no in-game connection needed) using the same names as the in-game chat commands but without the leading `/` — e.g. typing `kick SomePlayer` at the console. Chat is now rate-limited: sending too much too fast mutes that player for about 8 seconds, with a message telling them so. Glass and Sponge are placeable (matching the client's new hotbar), replacing Cobblestone and Sand in the placeable-tile list. `server.properties` gains a `max-connections` setting (default 3) controlling how many simultaneous connections are allowed from the same address.

`server/server1.8.2/` pairs with the c0.0.20a_02 client, same build/run/DLL layout again. Bedrock can no longer be destroyed by a regular player at all — server1.6 had no protection against this whatsoever. A new `/solid` admin command toggles placing unbreakable, Bedrock-backed "stone" instead of normal stone. `/tp` now works as a shorthand for `/teleport`. The placeable-tile whitelist grows to match the client's new tiles and its full inventory screen.

## References

* [Java Edition Classic 0.0.11a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.11a)
* [Java Edition Classic 0.0.13a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.13a)
* [Java Edition Classic 0.0.13a_03](https://minecraft.wiki/w/Java_Edition_Classic_0.0.13a_03)
* [Java Edition Classic 0.0.14a_08](https://minecraft.wiki/w/Java_Edition_Classic_0.0.14a_08)
* [Java Edition Classic 0.0.16a_02](https://minecraft.wiki/w/Java_Edition_Classic_0.0.16a_02)
* [Java Edition Classic 0.0.17a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.17a)
* [Java Edition Classic 0.0.18a_02](https://minecraft.wiki/w/Java_Edition_Classic_0.0.18a_02)
* [Java Edition Classic 0.0.19a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.19a)
* [Java Edition Classic 0.0.19a_01](https://minecraft.wiki/w/Java_Edition_Classic_0.0.19a_01)
* [Java Edition Classic 0.0.19a_02](https://minecraft.wiki/w/Java_Edition_Classic_0.0.19a_02)
* [Java Edition Classic 0.0.19a_03](https://minecraft.wiki/w/Java_Edition_Classic_0.0.19a_03)
* [Java Edition Classic 0.0.19a_04](https://minecraft.wiki/w/Java_Edition_Classic_0.0.19a_04)
* [Java Edition Classic 0.0.19a_05](https://minecraft.wiki/w/Java_Edition_Classic_0.0.19a_05)
* [Java Edition Classic 0.0.19a_06](https://minecraft.wiki/w/Java_Edition_Classic_0.0.19a_06)
* [Java Edition Classic 0.0.20a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.20a)
* [Java Edition Classic 0.0.20a_01](https://minecraft.wiki/w/Java_Edition_Classic_0.0.20a_01)
* [Java Edition Classic 0.0.20a_02](https://minecraft.wiki/w/Java_Edition_Classic_0.0.20a_02)
