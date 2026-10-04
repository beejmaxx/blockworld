# Developing Blockworld

[Back to the README](../README.md) · [Playing guide](PLAYING.md) · [Contributing](../CONTRIBUTING.md)

## Build and play

Requires macOS, CMake 3.30+, Ninja, SDL3 3.4+, GLM, and a compiler with C++26 mode.
The project has been built and tested with Apple Clang 21 and Homebrew LLVM 20.1.5.
Install [Homebrew](https://brew.sh/) and the Xcode command-line tools first.
Run these commands from the repository root:

```sh
brew install cmake ninja sdl3 glm llvm
export PATH="$(brew --prefix llvm)/bin:$PATH"
CC=clang CXX=clang++ cmake --preset dev
cmake --build --preset dev
ctest --preset dev
open build/blockworld.app
```

`./run.command` configures, builds, and launches the game. After the first build,
open `build/blockworld.app` to play without rebuilding.

The build requires C++26 (`CXX_STANDARD 26`, required, extensions disabled),
with a compile-time language-mode check. Compiler implementation of individual
C++26 features is still partial. The project uses the subset supported by these
toolchains; it does not require reflection or contracts.
See [Clang's language status](https://clang.llvm.org/cxx_status.html).

## Prototype scope

- Seeded hills, grass, sand, stone, and oak trees.
- 16 × 64 × 16 chunks, background terrain generation, nearby collision coverage,
  and eviction beyond the view radius.
- Exposed-face meshes, frustum culling, per-vertex ambient occlusion, procedural
  pixel textures, distance fog, a moving sun/moon, stars, drifting block clouds,
  and a persistent day/night cycle.
- Walking, sprinting, sneaking with ledge protection, jumping, flight, solid-block
  collision, and ray-based editing.
- Fixed Farm, Build, and Remove palettes with direct tool selection, visible held
  tools, a free removal hammer, and mode-specific click actions. Build uses
  left-click to place and right-click to remove, on foot or in flight. V uses or
  places in every mode.
- Placement previews with collision feedback, instant block removal,
  controlled hold repetition, bounded debris particles, and swing animations.
- An optional crafting lesson for planks, a workbench, an axe, and a pickaxe.
- A Farm page with planting shortcuts, a crop shop, harvest counts, flock management, animal
  naming, and markers to find animals.
- Wheat, carrots, strawberries, and pumpkins with distinct growth stages,
  reusable vegetable beds, flowering perennial bushes, and pumpkin spacing.
- Visible hoe, watering can, and compost; rain, wet soil, automatic sprinklers,
  and greenhouse growth bonuses with unlimited water and no wilting.
- A six-step gardening lesson and harvest sales that fund compost, irrigation,
  and greenhouse kits, with persistent coins and supplies.
- Four starter hens and a growing flock of up to twelve birds. Chicks hatch from
  collected eggs, follow their mothers, grow into adults, and can be petted.
- Animal movement, fence containment, food following, feeding, timed eggs, sleep,
  and clucks; connected fences, gates, growing wheat, and a guided starter pen.
- A furnished starter cabin on untouched home sites, a guided blueprint with live
  structural progress, optional assisted placement, a return-home shortcut, and
  a discoverable lantern cave.
- Glass windows, two-block doors with orientation and collision, and standing
  torches with flickering flames and warm local light.
- Two-part beds with low collision geometry, pillows, safe placement/removal,
  and a night-to-morning sleep transition.
- Procedural stereo audio: material footsteps, block edits, doors, sleep/wake
  chimes, wind, birds, crickets, and a car engine that revs with speed.
  Audio mixing runs on SDL's callback thread.
- Local save/load with atomic file replacement.

The first renderer supplies Metal shaders only. SDL's GPU abstraction can support
other backends, but Windows/Linux shader builds are future work. The world is
64 blocks tall and bounded to ±100,000 in X/Z. There are no hostile mobs, rivers or water simulation,
survival systems, or multiplayer yet.
Glass currently uses clear cutouts with visible frames and pale streaks; it does
not refract the scene. Torch light uses the nearest eight lights without shadow
occlusion. Underground ambient light is an approximation based on terrain height;
sunlight does not yet cast moving shadows. Sounds are synthesized rather than
field recordings. The mute preference lasts for the current session.

## Validation

```sh
ctest --preset dev
./build/blockworld.app/Contents/MacOS/blockworld \
  --smoke-test --screenshot artifacts/smoke.bmp
```

Core tests cover negative coordinates, seeded generation, chunk-boundary face
culling and invalidation, triangle winding, ray picking, reach limits, collision,
jumping, flight, save round trips, eviction, and damaged saves. The GPU smoke test
opens a real Metal window, streams terrain, breaks and places a block, resizes
twice, captures the final frame, and exits. It never reads or writes the player save.
It also completes a furnished cabin, opens/closes its door, renders sunset/night,
sleeps until the next morning, and checks the actual audio callback. Additional tests cover
thin-object raycasts, safe two-part door placement/removal, blocked door closing,
torch support, glass visibility, the entire guide-building sequence, the cave
passage, paired bed placement/removal across chunk boundaries, sleep reach and
headroom, day rollover, and all eleven save formats. Building tests exercise the
whole crafting lesson, workbench reach and blocked access, recipe costs, tool
speed, input cancellation, placement validity, and bounded debris. UI tests check
recipe and inventory hit areas, menu bounds, and all held items throughout their
swings at 800×600 and 1280×800. Inventory tests cover legacy slot compatibility, mode selection, removal protection, and ownership,
contextual use, sneaking at ledges, flight toggling and takeoff, landing out of
flight, saved selections,
version-5 migration, and atomic rejection of malformed hotbars. Audio tests check distinct
materials, audible effects, bounded output, and mute/pause fading to silence.
Farm tests cover planting through hatching and naming, family containment through
adulthood, feeding reach and walls, growth and replanting, population reservations,
night rest, floor removal, blocked and unloaded hatch sites, following mothers,
name validation, petting, time skips, version-6 migration, and atomic rejection of
malformed family saves. UI tests also cover both flock pages, renaming, farm
buttons, animal labels, and long names on markers. Garden tests cover all crop
yields, watering reach and walls, bonus expiry through long time steps and chunk
eviction, safe neglect, wet-soil mesh refresh, produce capacity, crop geometry,
saved watering and seed selection, version-7 migration, and invalid crop data.
The vegetable-garden suite exercises a complete hoe/plant/water/compost/harvest/sell/buy
sequence, perennial berries, pumpkin spacing, placement costs and refunds, weather
boundaries, roof changes, irrigation outside loaded chunks, safe starter placement,
greenhouse preflight, version-8 migration, and atomic rejection of malformed garden data.
Regression checks cover click repetition and cancellation, protected animal targets, blocked
chicken turning, hens climbing out of one-block holes, untouched starter cabins,
safe home landings, and reachable beds.
The livestock/car suite covers purchases and rejected deliveries, milk production
and sales, riding and safe exits, braking and reverse, wall collision, and version-9
migration. CPU previews also show the new models and shop.
The castle suite walks from the entrance to the tower roof and back without jumping,
drives through the arch, checks stair headroom and flight behavior, preserves edited
parcels, and round-trips the castle and stone steps through save version 11.
Ranch tests also cover narrow tree gaps, safe turning, reverse, terrain rises,
car recovery, petting sheep and foxes, and saving both new animal types. Audio
tests check engine idle, revs, reverse, braking, exit, mute, and pause.
UI previews cover antialiased Crafting, Inventory, and Farm text, the Garden and Shop pages, live garden guide, pause screen,
and all held garden tools. These CPU previews do not verify native input or Metal rendering.

For disposable visual previews:

```sh
./build/blockworld.app/Contents/MacOS/blockworld --demo-cabin --frames 180 --screenshot artifacts/cabin.bmp
./build/blockworld.app/Contents/MacOS/blockworld --demo-cave --frames 180 --screenshot artifacts/cave.bmp
./build/blockworld.app/Contents/MacOS/blockworld --demo-cabin --time 22 --frames 120 --screenshot artifacts/night.bmp
./build/blockworld.app/Contents/MacOS/blockworld --demo-bed --time 22
./build/blockworld.app/Contents/MacOS/blockworld --demo-farm --mute
./build/blockworld.app/Contents/MacOS/blockworld --demo-castle --time 10 --frames 120 --screenshot artifacts/castle.bmp --mute
./build/blockworld.app/Contents/MacOS/blockworld --demo-city --time 10 --frames 120 --hide-ui --screenshot artifacts/city.bmp --mute
./build/blockworld.app/Contents/MacOS/blockworld --demo-city --city-view roof --time 17 --mute
./build/sound_tests artifacts/soundscape.wav
./build/sound_tests artifacts/soundscape.wav artifacts/car-engine.wav
./build/farm_tests artifacts/garden-starter.ppm
./build/farm_tests artifacts/garden-upgraded.ppm upgraded
./build/ui_tests artifacts/garden-ui
```

The demo previews never read or write your worlds. Without `--frames`, demos
support normal arrow-key/WASD movement and mouse look. The bed preview starts aimed at a
usable bed; press V to try sleeping. `--time HOURS` accepts
0 up to (but excluding) 24 and overrides the starting clock; `--mute` starts
muted. `sound_tests` optionally writes a 20-second WAV preview of the mixer.
The farm demo starts at the vegetable beds with the watering can held. Nearby
are a finished pen and mixed crops; wheat is ready for feeding, and the flock
includes a chick and an egg being incubated. `farm_tests` can write starter and
upgraded garden geometry previews without a native window;
that preview checks the model layout, not the Metal shader's final appearance.

## Source layout

`world.*` owns terrain, chunks, edits, meshes, picking, saves, and the generation
worker. `player.*` owns movement and collision. `renderer.*` owns SDL GPU resources
and draw submission. `adventure.*` owns building interactions, the cabin blueprint,
and tutorial progression. `daylight.*` owns the world clock and sky colors.
`sound.*` synthesizes and mixes audio; `audio.*` connects it to SDL's
[playback stream](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream).
`ui.*` builds the interface. `main.cpp` connects input, simulation, streaming,
audio, and rendering. Metal shaders live in `shaders/`.
`ui_font_native.c` provides a small C bridge to Apple's font APIs, whose SDK
headers contain enum operations incompatible with standard C++26. All C++ sources
still compile in required C++26 mode.
`crafting.*` owns recipes, supplies, tools, and the optional lesson; `building.*`
owns placement previews, repeat input, mining progress, and debris. To inspect
the UI without opening a native window, run `./build/ui_tests artifacts/ui`;
it writes PPM previews using the same UI vertices as the game.
`farm.*` owns the flock, families, names, growth, incubation, interactions, farm guide, and animated chicken mesh;
`farm_state.hpp` defines the farm data saved by `world.*`.
`garden.*` owns the starter beds, hoe, compost, harvest shop, greenhouse kit, and
gardening guide. `world.*` advances crops, weather, moisture, and shelter bonuses.
`inventory.*` owns item/block/tool mapping, fixed mode palettes, and contextual use;
`inventory_state.hpp` defines stable item IDs and saved hotbar state.
`ranch.*` owns cows, horses, milk, vehicle delivery, riding, driving, and their meshes.
`castle.*` builds the castle in an untouched parcel and handles safe visits.
Use `--castle` to start at the castle in your saved world. `--demo-castle` is a
disposable preview and never reads or writes the saved world.

`city.*` generates the waterfront district before saved player edits are applied
to each streamed chunk. The save records its origin instead of a full copy of
the buildings. City tests walk all twelve staircases up and down, reach each
penthouse bed, drive the street loop, swim out of the lake, and check edit
persistence, legacy saves, and parcel protection. `--city` visits the saved city;
`--demo-city` uses a disposable world. Its `--city-view` can be `skyline`, `street`,
`roof`, or `interior`. `--hide-ui` omits overlays from real framebuffer captures.
