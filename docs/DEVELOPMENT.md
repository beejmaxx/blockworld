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
- 16 × 128 × 16 chunks, background terrain generation, nearby collision coverage,
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
128 blocks tall and bounded to ±100,000 in X/Z. There are no hostile mobs, flowing-water simulation,
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
headroom, day rollover, and save compatibility through version 21. Building tests exercise the
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
./build/blockworld.app/Contents/MacOS/blockworld --demo-farms --frames 3 --hide-ui --screenshot artifacts/countryside.bmp --mute
./build/blockworld.app/Contents/MacOS/blockworld --demo-boost --map-overview --frames 600 --screenshot artifacts/boost.bmp --mute
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
persistence, legacy saves, and parcel protection. `--old-city` visits the original saved city;
`--demo-city` uses a disposable world. Its `--city-view` can be `skyline`, `street`,
`roof`, or `interior`. `--hide-ui` omits overlays from real framebuffer captures.


`coast.*` generates a protected 384 × 384 landscape before player edits. Connected
coves, a lagoon and tidal channel use a warped shoreline field; the road grades
only a corridor beside it. Hills, sand shelves, an island, tree crowns and the
swimming pier are deterministic across chunks. The outer terrain blends into the
original world. A saved origin avoids hundreds of thousands of saved block edits.
The existing city and farm terrain retain their original generation.

`--coast` visits the saved region; `--demo-coast` is disposable. Use `--coast-view
bay|beach|hills|lagoon` with the demo. For example:

```sh
./build/blockworld.app/Contents/MacOS/blockworld --demo-coast --coast-view hills --time 16 --frames 120 --hide-ui --screenshot artifacts/coast.bmp --mute
```

The coast streams a radius of 12 chunks, versus 6 elsewhere. Water vertices carry
column depth in their otherwise unused light attribute; the shader blends shallow
and deep colors, a sky tint, ripples, and a sun highlight. This is a surface shading
approximation, not reflection of buildings or transparent refraction. No extra
render pass or ray tracing is required.

Coastal tests cover connected, level roads; gradual terrain joins; island/lagoon
geometry; a complete walk/swim/pier return; car delivery, mounting and driving;
parcel protection; edited blocks after streaming; compact saves; v12 migration;
and atomic rejection of malformed or overlapping region metadata. Actual Metal
captures are verified separately. The city on this terrain is generated by `harbor.*`.


`harbor.*` places ten rotated buildings on individual coastal parcels, with paved
approaches to the existing road. It keeps the curved shoreline and hills between
plots. The layout includes apartments, balconies, a fourteen-floor crowned tower,
and a duplex penthouse with terraces, skylights, a pool and a pergola. Five-block
storeys use paired half-step stair flights. The height limit is 128 blocks.

Save version 14 adds an optional ten-bit parcel mask after the coast record, plus
new stable material/furniture IDs. Parcels with existing edits or animals are
excluded on initialization. Version 13 saves load with no city mask; visiting with
T or U initializes the city. Geometry is generated before player edits and is not
stored as thousands of edited blocks. The original city generator is unchanged.

`road.*` plans a two-lane connector from the farm outskirts to the southern coastal
loop. An A* search reserves clearance around saved edits, farms, animals and
landmark parcels. Visibility simplification and checked corner smoothing make
the route easier to drive. The road has a gentle grade, white markings, gravel
verges and reflector posts. Generation occurs before saved block edits.
Version 15 stores the stable centerline after the harbor record: a bounded count
followed by XYZ points, where Y is the driving surface. Versions 1–14 remain readable.

The stylized GT2 uses a faceted coupe mesh with oval headlights, rear wing and
spoked wheels. Its oriented collision body matches the longer model. Acceleration
is 12 blocks/s², forward speed is capped at 42 blocks/s (about 151 km/h), and
reverse at 8 blocks/s. Shift raises acceleration to 24 blocks/s² and forward speed
to 84 blocks/s (about 302 km/h). Releasing Shift decelerates to the normal limit.
Space is an immediate arcade brake; steering slows at speed.
Collision motion is substepped to at most 0.08 blocks, and road steps test the
entire rotated footprint. Fast driving preloads collision chunks ahead. Rendering
uses a collision-aware chase camera; engine pitch includes automatic gear changes.

`highway_and_gt2` drives the complete road in both directions using normal vehicle
physics and streamed chunks, including a detour around an existing wall. It checks
top speed, emergency braking, full-speed wall collision, model bounds, preserved
road edits, alternative coast locations and atomic save validation. Pass a save
copy to `road_tests INPUT.bw OUTPUT.bw` to verify a real world's route while keeping
its farm state intact. `--demo-road --road-view car|rear|highway|drive` captures
disposable previews; `--road` opens a saved world beside the GT2 at the road entrance.

`countryside.*` finds an untouched 112 × 96 parcel alongside the highway. It
generates barns, pasture fences, a glass nursery, irrigation channels and four
crop fields, with graded terrain around the edges. Crops are planted once as
ordinary saved crop records; harvested fields stay harvested after streaming or
reloading. Two cows, a horse and a sheep are added when the livestock capacity
allows. Version 16 stores the optional parcel origin after the road points.
It reads versions 1–15 and supports up to 2,048 growing plants. Procedural roofs
also participate in crop shelter checks when their chunks are unloaded.
`--farms` visits the saved district; `--demo-farms` is a disposable aerial preview.

`minimap.*` samples terrain and loaded block columns without generating chunks.
The 32 × 32 grid and clipped road lines refresh five times per second; player
position and heading update each frame. Local radius is 96 blocks on foot or
200 when driving. N toggles the landmark/route overview. UI text uses the native
font renderer. The countryside suite covers harvest sales, lane driving, bed
access, unloaded greenhouse growth, construction protection, migration, and map
projection/clipping; UI bounds are checked at both supported window sizes.

Sofas, tables, chairs and planters use small box meshes and matching collision
bounds. Furniture and additional materials live on the second Build picker page;
the first nine hotkeys are unchanged. Window frames have antialiased edges and
are drawn in a separate alpha-blended pass after opaque geometry. Blue facade
glass is opaque; the clear window blocks are cutouts, without scene refraction.

`--city` / `--harbor` visits the new city, and `--harbor --harbor-view roof` opens
on the penthouse terrace. `--demo-harbor` is disposable; its `--harbor-view` can be
`skyline`, `roof`, `street` or `interior`. For example:

```sh
./build/blockworld.app/Contents/MacOS/blockworld --demo-harbor --harbor-view roof --time 10 --frames 120 --hide-ui --screenshot artifacts/penthouse.bmp --mute
```

The coastal-city regression walks into every building, climbs and descends every
floor, reaches the duplex bedroom and checks sleeping, swims in the roof pool and
walks out. It also checks both travel shortcuts, high-altitude edits, furniture
persistence, protected plots, demolition after streaming, v13 migration and
atomic rejection of invalid parcel metadata.


## Downtown and city life (0.17)

`metropolis.*` generates 36 named towers across a 448 × 384 road grid, two
server buildings, a park, and a 20-bay garage. New sites reserve existing edits,
animals, landmarks, and their highway connection. `city_life.*` handles bank
transfers, once-per-day rent, resident dialogue, and independent dating state.
All new towers are owned; their fixed rents are an arcade economy.

Twenty cars use six body styles and the existing GT2 physics, engine audio,
boost, and collision system. Unselected cars occupy fixed garage bays. The active
car remains wherever the player drives it until recalled or parked.

The City menu (L) has Bank, Garage, Residents, and Properties pages. Native font
text is used throughout. T and U now visit downtown and its bank-tower rooftop;
the previous coastal city is accessible through L → Bank → Visit waterfront.

Save version 17 adds one city-state record after the countryside origin. It stores
bank balance, selected vehicle, last rental day, eight relationship records, and
up to eight transactions. Older saves receive no retroactive income. A new district
adds no saved block edits. Invalid metadata is rejected before mutating the world.

Full editable chunks are supplemented by distant tower meshes and coarse terrain
out to 850 blocks. This keeps the skyline visible without loading every interior.
Far geometry has no collision; nearby movement and editing use real voxel chunks.

The downtown suite walks all 36 buildings up and down, drives each of 20 cars out
of the garage, follows the highway connection, tests bank boundaries, daily income,
relationship persistence, wall-obstructed interactions, map coverage, and v16 migration.
Pass `city_life_tests INPUT.bw OUTPUT.bw` to install the district into a copy of a save.

```sh
./build/blockworld.app/Contents/MacOS/blockworld --downtown --play
./build/blockworld.app/Contents/MacOS/blockworld --garage --play
./build/blockworld.app/Contents/MacOS/blockworld --demo-metropolis --metro-view skyline --hide-ui --mute --frames 120 --screenshot artifacts/downtown.bmp
./build/blockworld.app/Contents/MacOS/blockworld --demo-metropolis --metro-view apartment --city-menu bank --mute --frames 2 --screenshot artifacts/bank.bmp
```

Other metro preview views: street, roof, garage, collection, bank, apartment, servers.

## Population and families (0.18)

84 street residents follow sidewalk routes derived from the saved game clock.
They render only on loaded, clear sidewalk cells, with animated arms and legs.
These decorative pedestrians do not block cars. Eight apartment residents retain
independent adult relationships. After dating, a shared family dialogue starts a
two-game-day pregnancy. Each household supports three children, rendered at home
with a smaller model on their first day. Children use a separate interaction kind
that opens their parent's household page.

Save version 18 appends a due day and three child birth dates for each adult
resident to the existing city-state line. Version 17 relationships migrate with
empty family records. Loading validates dates and household capacity before
changing the world. Birth updates are idempotent, including after sleep and reload.
The city tests cover independent households, birth timing, duplicate prevention,
child targeting, walking residents, rendering capacity, and save migration.

`--demo-population` and `--demo-family` open temporary preview worlds for real Metal
captures. Both disable saving; the family preview supplies a sample child.

## Furnished homes and family care (0.19)

The eight resident apartments receive distinct accent colors, floor inlays,
kitchens, dining rooms, lounges, book walls, artwork, lamps, bedrooms, and cribs.
Their stair landings and balcony approaches stay clear. Furniture generation runs
before player edits, preserving custom construction and removals. Detailed books,
artwork, lamps, and crib rails use their underlying blocks as removable anchors.

Newborns rest in the crib; feeding briefly moves them into the parent's arms.
Feeding older children supplies a snack. Each child stores its last meal in
absolute game seconds; hunger prompts return after 150 seconds of play. No health
or survival penalty is applied. Save version 19 appends 24 meal timestamps to the
city-state record, validates them against births and the saved clock, and accepts
all earlier formats. Tests exercise meal timing, household independence, safe
walks through every furnished home, save round trips, and v18 migration.

Use `--demo-home` for the furnished room or `--demo-feeding` for a mother and baby.
These preview worlds do not read or write the player's save.

## Partner nights (0.20)

Dating adult residents offer a non-explicit **Spend the night** action. The action
requires a visit and a complete, unobstructed bed in the apartment. A 3.2-second
fade advances the clock once at blackout, updates farm growth, income and existing
pregnancies, and saves before returning to the morning view. No new save fields
are required. Ordinary bed sleep keeps its existing behavior.

`--demo-night --frames 45` runs the real transition in a temporary world with a
fixed fade timestep and verifies the new morning and single daily income payment.
The city suite covers relationship, distance and bedroom validation and family
progress during consecutive nights.


## Waterfront estates, architecture and diagnostics (0.21)

`estate.cpp` generates a 640 by 416 block district east of downtown, on a parcel
without player edits or existing landmarks. The road connector is checked before
installation. Three mansions have two working half-step stair flights, editable
furniture, pools and roof gardens. The yacht has a boarding gangway, a salon,
cabin, bridge and sun deck. A terminal, control tower, two hangars, four parked
jets and a 603 block runway make up the airport. Boats and aircraft are static.
`L > Places` exposes six safe arrivals, also reachable by road or on foot.

Save version 20 appends the optional estate origin to the city-state line, after
feeding timestamps. The loader remains atomic and accepts versions 1 through 19;
new terrain and structures are generated before saved player edits are applied.
`estate_tests` covers all arrivals, mansion stairs up and down, yacht boarding
and upper decks, terminal access, vehicle clearance along the full road, map
markers, persistence and refusal to overwrite an edited parcel. Pass input/output
save paths to that executable to validate a real-world migration on a copy.

Downtown facades now have projecting fins, shades, planted balconies, office
spandrels and entrance canopies. Apartments add bathrooms, a study and media
wall. Glass is subtly tinted and transparent chunks draw from far to near; this
is approximate sorting, not order-independent transparency. Distant terrain
skips loaded chunks so it cannot draw the old hills through rooms or pools.
The voxel height cap remains 128; taller landmark towers and vertical streaming
are a separate future architectural change.

`Diagnostics` collects a bounded 120-frame history. CPU is getrusage user+system
time divided by elapsed wall time (100% per core); memory comes from TASK_VM_INFO
(physical footprint, resident and peak resident size). Stats sample every 0.5s.
The UI labels terrain GPU buffer allocation separately; no total GPU usage or
GPU percentage is inferred. `--debug` opens the overlay; Ctrl+D/F3 toggles it.

`Player::toggleNoclip` bypasses movement collision while keeping `collides`
truthful. Disabling inside solid geometry is rejected. The last clear pose is
used when saving from inside a wall; normal teleports restore ordinary movement.
`--noclip` enables it initially, Ctrl+N/F4 toggles it. Core tests cover wall
crossing, rejected unsafe exit, safe saving, descending and frame statistics.

Use `--demo-estate --estate-view villa|interior|marina|yacht|airport|terminal`
for captures in a temporary world. Demos never read or write the player's save.

## Homes and data center operations (0.22)

`city_management.cpp` connects the three estates to a persisted main-home choice,
one household and one favorite car per mansion. `cityResidentHome` is shared by
rendering, targeting, collision, visits, labels, child care and partner nights.
Household moves keep pregnancies, children and feeding timestamps; blocked arrivals
and occupied homes are rejected. R and the map follow the chosen home, and the farm
cabin remains accessible. Nursery props respect their editable table anchors.

Two `DataCenterState` values track racks, power/cooling levels and a contract bitset.
Each starts with two racks and a 160/day contract, preserving its former 128/day
net income after 16 electricity and 16 cooling. Electricity costs 8 per installed
rack; cooling costs 8 per contracted rack. Capacity is four times the lower of
power and cooling levels. Racks cost 300; power upgrades cost 600 times the current
level, cooling 450 times the current level. Contracts require 2, 2, 4 and 8 racks,
paying 160, 224, 512 and 1152 per day. There are sixteen rack bays per site.

Purchases settle pending days before changing state, debit the bank, and regenerate
only affected loaded chunks. Non-air construction and the player's body block new
equipment; explicitly cleared cells can receive purchased hardware. Net profit
uses the existing once-per-day settlement, bounded ledger and saturating balance.
All managed state saves immediately from menu actions and on ordinary world saves.

Save version 21 appends 20 fields after the estate origin: main home, three parked
car IDs, eight resident home IDs, and four integers per data center. Loading validates
house/car uniqueness, relationship prerequisites, capacities and contracts before
committing any world state. Versions 1–20 get the legacy cabin/apartments and starter
contracts without changing money, family history or edited blocks.

`city_management_tests` covers moves, care and nights, collision rejection,
reversible homes, parking and driving to the road, contract capacity, expenses,
purchase conservation, skipped days, old-save migration and corrupt-load atomicity.
UI tests cover every new action at 800×600 and 1280×800. Real Metal previews use
`--demo-household --city-menu home` and `--demo-business --city-menu business`;
both use disposable worlds and never read or write the player's save.

Tenant demand, resident work schedules, marriage, yacht driving and a stock market
remain future work. Property rent is still a fixed portfolio income; server profits
now depend on the player's business decisions.

## Rendering optimization (0.22.1)

Terrain meshes share identical corners within each face using 32-bit indices.
Triangle order, both ambient-occlusion diagonals, material/UV seams, and the
opaque/glass draw ranges are preserved. The diagnostics overlay counts actual
vertex plus index buffer allocation. Distant terrain and tower geometry have a
separate retained GPU buffer, uploaded on the existing skyline refresh cadence
instead of with animated people and animals every frame. Light selection sorts
only the nearest eight candidates.

Measured on an Apple M1 Mac with 16 GiB RAM, using RelWithDebInfo, Metal, and the
same 2,400-frame street preview (625 loaded chunks, 5,489,400 loaded triangles):

| Metric | 0.22.0 | 0.22.1 |
| --- | ---: | ---: |
| Terrain vertex/index buffers | 628 MiB | 481 MiB |
| Final process physical footprint | 1,061–1,069 MiB | 855–856 MiB |
| Whole-run average FPS | 76–82 | 104–116 |

These are local runs, not a hardware-independent FPS guarantee. Presentation
timing varies; the final 120-frame p95 ranged from 25–26 ms before and 10–26 ms
after, so the average throughput gain does not imply that all stutters are gone.
GPU buffer figures exclude render targets, dynamic geometry and driver overhead;
process footprint includes more than terrain buffers. No draw-distance or visual
quality settings were reduced. Save format remains version 21.

Reproduce without reading or writing the player save:

```sh
./build/blockworld.app/Contents/MacOS/blockworld \
  --demo-metropolis --metro-view street --debug --mute --frames 2400
```

Core regression tests reconstruct the original triangle stream from the indexed
mesh, including glass, furniture, terrain, UV seams and alternate AO diagonals.
