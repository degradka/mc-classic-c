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

In progress. Released May 28 2009 and the last release of Early Classic before multiplayer began. Also obfuscated, ported the same way as c0.0.13a_03.

## c0.30

Planned, after c0.0.14a_08.

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

## References

* [Java Edition Classic 0.0.11a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.11a)
* [Java Edition Classic 0.0.13a](https://minecraft.wiki/w/Java_Edition_Classic_0.0.13a)
* [Java Edition Classic 0.0.13a_03](https://minecraft.wiki/w/Java_Edition_Classic_0.0.13a_03)
* [Java Edition Classic 0.0.14a_08](https://minecraft.wiki/w/Java_Edition_Classic_0.0.14a_08)
