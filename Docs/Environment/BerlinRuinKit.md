# Berlin ruin kit Blueprints (procedural shell holes, 2026-11-12)

One destructible Blueprint per Berlin ruin kit mesh, for `Ger_Berlin`. The meshes follow
`D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\BerlinRuinKit\KIT_CONTRACT.md`; the material is in
`BerlinRuinKitMaterial.md`; the hole system is "A + B: procedural holes" in `WallChunkDamage.md`.

- **Builder:** `Tools/BerlinMap/ruinkit/ruin_blueprints_build.py`, `run()`. It is idempotent and picks up every
  `SM_BerlinRuin_*` in `RuinKit/Meshes`, so new or changed kit meshes only need a re-run. It never deletes a Blueprint.
  Report: `Saved/Berlin/ruinkit_bp/build_report.json`.
- **Test:** `Tools/BerlinMap/ruinkit/ruin_holes_headless_test.py`, real tank fire on `RTS_ToCopySmall`. Pictures and log
  go to `Saved/Berlin/ruinkit_test/`.

## Blueprints

All are in `/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/`.

- **`BP_BerlinRuin_Base`** is the shared parent: the C++ `ADestructableEnvActor` with a Movable `DefaultSceneRoot`. It
  holds the kit's one graph, the death (see "Death"), and the variables that graph reads (category `Death`).
- **`BP_BerlinRuin_<suffix>`**, one per mesh. The suffix is the mesh name after `SM_BerlinRuin_`, for example
  `BP_BerlinRuin_E1_CornerTower`. Each one has these components, all its own:
  - `StaticMesh` (Movable, like the PostApo ruins and the Gate): the kit mesh with its slot materials
    (`MI_BerlinRuin_Wall`, `MI_BerlinRuin_Rubble`).
    - Its collision is set on the template. It is what `SetupComponentCollisions` would set at BeginPlay: object type
      Destructible; blocks Visibility, Projectile, TraceEnemy and TracePlayer; overlaps BuildingPlacement; ignores
      pawns; no decals.
    - It does not affect navigation.
  - `WallHoleDamage` on `StaticMesh`: NoGrid; `DamagedMaterials[0] = MI_BerlinRuin_Wall_Holes`, and the rubble slot has
    none; debris `Debris/SM_BerlinRuin_Debris_0..5`; `bCollapseIntoChunksOnDeath` false, because the base graph runs
    the death (true again, by the builder, if the base ever loses its death variables).
  - `NavBlock_NN`: navigation boxes (see "Navigation").
- **Class defaults:** `Health`, `DamageReduction`, `AimOffsetPoints`, and the death values (see "Death"). The aim
  points sit in the tallest wall hulls, at about 40 % of the wall's height, at most 6.5 m, and at least 1.2 m above any
  rubble heap they stand on.
- **No graphs in the children.** Headless Python cannot author Blueprint graphs, and the UECP tools may not be used on
  the shared editor. This is why the collision is on the template rather than coming from a BeginPlay call. The one
  graph, in the base, was authored through a throwaway editor (see "Death").
- **`B8_DebrisBed`** is rubble only. It uses the same destructible setup with empty `DamagedMaterials`:
  - hits never show a hole, but they throw a few chips;
  - on death it settles 30 % with a small dust puff and stays, like every other piece (see "Death").

  It was not made a plain static mesh because it blocks navigation, and a static mesh never stops doing so.

## Settings

Hole lengths are in mesh cm; the meshes stand at scale 1. Fillers may be scaled 1.0-1.15, never below 1.0, because a
50 cm wall would drop under the contract's 45 cm minimum.

| Class | Pieces: health / reduction (A and B variants alike) | Hole cm per mm / max / min | Debris cm² per piece / per hit | Death pieces | Aim points |
|---|---|---|---|---|---|
| hero | E1 6000/15, E2 5500/15 | 1.1 / 220 / 15 | 6000 / 4 | 120 | 5 |
| core | E3 8000/20, B7 7000/20 | 1.0 / 200 / 15 | 6000 / 3 | 100 | 5 |
| slab | B1 1200/10, B2 2200/10, B3 4000/10, B4 4000/10, B5 3000/10 | 1.0 / 180 / 15 | 5000 / 3 | 70 | 4 |
| filler | E5 1400/8, E6 1100/8, E7 1800/8 | 0.9 / 150 / 12 | 4500 / 3 | 45 | 3 |
| stub | B6 700/5, E4 700/5 | 0.8 / 100 / 12 | 4000 / 2 | 25 | 2 |
| rubble | B8 1500/25 | 0.8 / 120 / 12 | 4000 / 2 | 40 | 2 |

Nav boxes per Blueprint: E1 11, E2 14, E3 4, E4 7, E5 5, E6 1, E7 3, B1 1-3, B2 3-4, B3 4, B4 4, B5 3, B6 1-3, B7 1, B8 6.

- **Comparison with the PostApo ruins:**
  - PostApo is 1500-7500 health with reduction 10, 1.1 cm per mm, max 220, debris 3600 / 5.
  - Here the per-hit debris is lower, as the owner wants visible holes rather than many pieces.
  - The death pieces stay near PostApo's 120, because the C++ caps a piece at 1.2x its mesh (the kit debris is
    38-68 cm). Fewer pieces would not get bigger; a dead ruin would just thin out.
- **Rule of thumb:** a Pz IV 75 mm hit does about 210 after reduction 10, and makes a 75 cm hole on slabs and cores
  (82 cm on heroes).
- Unknown future mesh suffixes fall back by their id: E1/E2 hero, E3/B7 core, B6/E4 stub, B8 rubble, other E filler,
  other B slab. Edit `PIECES` and `CLASSES` in the builder to tune.

## Navigation

`RuntimeGeneration=DynamicModifiersOnly`: at runtime only navigation modifiers change. A mesh baked into the navmesh as
geometry would keep blocking after the ruin is gone.

- **What the PostApo ruins do:**
  - `SetupComponentCollisions` takes the mesh off the navmesh.
  - Their inherited `RTSNavCollisionEnvObstacle` is never set up (`BP_DestructableEnvActor` has no BeginPlay logic), so it
    falls back to its failsafe extent: one 2 x 2 x 8 m `NavArea_Obstacle` box at the actor origin.
  - `NavArea_Obstacle` is costly but walkable. In effect the PostApo ruins hardly touch the navmesh.
- **What these Blueprints do:**
  - `NavBlock_NN` are hidden `UBoxComponent`s with no collision, set as dynamic navigation obstacles
    (`bDynamicObstacle`, `AreaClassOverride = NavArea_Null`).
  - There is one box per ground-touching wall run or rubble heap: the mesh's UCX hulls that start below 1.5 m, merged
    where they line up (at most 12 % or 0.25 m² of extra footprint), up to 4 m high.
  - Upper hulls (slab remains, balconies) are skipped, so open interiors and gaps stay as open as the hulls leave them.
  - `NavArea_Null` is a hard block for every agent, so the wall cores (E3, B7, whose mound fills the whole footprint)
    are reliable blockers.
  - They are invisible in the editor too; tick `Visible` on a `NavBlock` to see it.
  - At a death the boxes of what still stands are kept: the death graph detaches them, so they stay at ground level
    under the lowered walls. Boxes of rubble that goes under are destroyed in the first frame (see "What a remnant is
    for the game"). They are runtime modifiers, so only the tiles under the ruin are rebuilt.
- **Rebuild the navmesh after placing (required).** The boxes are cut into the navmesh when it is built. On `Ger_Berlin`,
  rebuild navigation after placing the ruins and save the map. `BerlinMap.md`'s build already saves after the navmesh finishes.
  - In the test, ruins placed in the editor without a rebuild did not block at all in PIE, and neither did the
    Rus-wall control.
  - **Not tested:** ruins spawned at runtime, and the `bForceRebuildOnLoad` rebuild of a packaged game.
- **Why boxes, not the mesh:**
  - `bOverrideNavigationExport` / `bForceNavigationObstacle` on a `StaticMeshComponent` are transient, so they can't be set
    on a template.
  - `bHasCustomNavigableGeometry` isn't reachable from Python.
  - A `NavModifierComponent` only sees the collision of a component that also exports itself as geometry.
- **Counts:** 1-14 boxes per Blueprint, 94 for the 22 Blueprints (average 4.3).

## Death (remnants; last change 2026-11-14)

**No ruin vanishes** (owner, 2026-11-14). A ruin that dies throws a little debris, goes down in a dust puff sized to
the piece, turns a few degrees, and stays where it is as a dead remnant. Small and medium pieces go down "a bit"
(30-35 % of their height); the large ones keep the deep sink (55-65 %). Most pieces with a modelled collapse heap
leave it inside or beside the remnant. The heap is a cover Blueprint (but see "Cover of the spawned heaps").

- **Where it lives:** one graph in `BP_BerlinRuin_Base`; the 22 children have none. The values it reads are class
  defaults that `ruin_blueprints_build.py` sets per child from **one table, `DEATH`** (one row per death class, with
  per-piece exceptions in `DEATH_PIECE` and moved heap spots in `HEAP_SPOT`), the mesh size and
  `footprint_table.json`. The table is repeated below.
- **The graph was authored through UECP** (`synthesize_graph`) in a throwaway editor. The calls are kept in
  `Tools/BerlinMap/ruinkit/ruin_death_graph_uecp.json`, in replay order, with the pitfalls that were hit.
- **History:** 2026-11-13 first version (everything sank away under large dust clouds), then large pieces kept as
  remnants; 2026-11-14 every piece a remnant and the dust cut down after the owner saw it in play.

### What happens

`BP_OnUnitDies` (any death type; the ruins are not crushable) -> if `SinkOnDeath`:

1. **`StartRuinDeath`**, in the frame of the death:
   - stores the actor transform (`DeathStartTransform`): the sink moves and turns the actor, and the heaps and the dust
     are placed against where it stood;
   - hides every component tagged `BerlinAmbient` or `BerlinCrowPerch` (curtains, wires, lamps, crows);
   - `NavBlock_NN` boxes tagged `RemnantNav` are detached (keep world), so they stay at ground level and keep blocking
     while the actor sinks away from them; the others are destroyed;
   - `WallHoleDamage.CollapseIntoPieces(false)`: a few kit debris pieces fly off the wall, the mesh stays;
   - `SpawnDust`: the first dust burst, at the middle of the footprint;
   - `VerticalDestruction`: the actor sinks `SinkDeltaZ` in `SinkTime` seconds, with a random tilt between
     `SinkMinTilt` and `SinkMaxTilt` and a random yaw between `SinkMinYaw` and `SinkMaxYaw` to either side;
     `bDestroyPostVerticalCollapse` = not `LeaveRemnant` (true on every piece now). Its `CollapseFX` only plays
     `DeathSound` (`Collapse_Cue`, with `Att_Collapse` and `SC_Collapse`);
   - starts a looping timer on `DustPulse`.
2. **`DustPulse`**, every `DustPulseInterval` seconds until the sink ends: one more burst at a random spot of the
   footprint (`DeathVfxOffset` +- `DustSpreadX/Y`, local space). The builder sets the interval so that the wanted
   number of bursts fits into the sink time.
3. After `HeapDustDelay`: **`DustAtHeaps`**, a burst on every spot where a heap will stand.
4. `HeapDustLead` later: **`SpawnDeathHeaps`** spawns `BP_BerlinCoverHeap_Collapse_S` for every transform in
   `DeathHeapsSmall` and `BP_BerlinCoverHeap_Collapse_L` for every one in `DeathHeapsLarge` (actor-relative, composed
   with `DeathStartTransform`).
5. At `SinkTime` the C++ stops its sink timer and calls `BP_OnVerticalDestructionComplete`, which clears the
   `DustPulse` timer. The actor stays where the sink ended.

- **`SpawnDust(location, scale)`** is the one place a dust effect is spawned. It tints it (`DeathDustColor` into the
  user parameters `Color Smoke`, `GroundSmokeColor` and `Color`; an effect ignores the ones it does not have) and
  sets `Velocity Speed Mlt` from `DeathDustSpeed`.
- **Ticking:** nothing ticks before the death. During it there is the C++ sink timer, one Blueprint timer and two
  delays; all end with the sink. A remnant has no tick and no timer.
- The footprint skirt (an instanced mesh of the map) is not touched.

### The two dust effects

| Name in the builder | Asset | Look | Used for |
|---|---|---|---|
| `PUFF` | `NS_Artillery_StoneImpact` | brown dust plume, no stones, about 6 m across at scale 1 and speed 1, gone after 1.5 s; a lower speed makes it lower and rounder | stubs, slabs, shells, half houses, tall slabs |
| `CLOUD` | `NS_RTSDebrisExplosion_Huge` | ring of rolling smoke, about 7 m across at scale 1, 2.5 s, and about 25 large stones thrown 10-15 m | wall cores and heroes only |

- **Why two:** the stones of `CLOUD` do not shrink with the component scale, so on a small piece it throws
  hero-sized rocks out of a tiny ring. That, at scale 0.7-2.1 with 6-13 clouds alive at once, was what the owner saw
  filling the view.
- **Tint:** untinted, `CLOUD` reads white-grey on Ger_Berlin. `DeathDustColor` (0.30, 0.27, 0.23) makes it grey-brown.
- **Not usable:** the white `NS_DestructionSmoke(_Large)` (steam); `NS_RTS_GroundDust*`, `NS_RTS_DustSmoke_07`,
  `NS_RTS_StaticDust_22` (they loop and never end); the Medium / Heavy / SuperHeavy stone impacts (pale 2 m splashes).
- Contact sheets: `ruin_death_run.sh <log> 260 fx` (`ruin_death_fx_probe.py`, three variants per picture behind a
  one-storey slab, game camera). Kept sheets: `Saved/Berlin/ruin_death_test/fx_sheet1/`, `fx_sheet2/` and `fx_*.png`.

### The table (`DEATH` in `ruin_blueprints_build.py`)

| Death class | Pieces | In the ground | Sink time | Tilt | Yaw | Dust: effect, scale, bursts, speed | Thrown debris | Heap |
|---|---|---|---|---|---|---|---|---|
| hero | E1, E2 | 62 % (E2 60 %) | 6.0 s | 2-4 deg | 1-3 deg | CLOUD 1.4 x 6, speed 1 | up to 70 | yes |
| core | E3, B7 | 60 % / 55 % | 5.0 s | 2-4 deg | 1-3 deg | CLOUD 1.2 x 4, speed 1 | up to 50 | yes |
| tall | B3, B4, B5 | 65 % (B5 60 %) | 4.0 s | 2-4 deg | 2-5 deg | PUFF 1.6 x 4, speed 0.65 | up to 28 | B5 only (table) |
| two_storey | B2, E7 | 35 % | 2.2 s | 1-3 deg | 3-7 deg | PUFF 1.3 x 3, speed 0.6 | up to 16 | E7 (table) |
| one_storey | B1, E5, E6 | 30 % | 1.8 s | 1-3 deg | 3-7 deg | PUFF 1.0 x 2, speed 0.6 | up to 10 | E5; not E6 |
| stub | B6, E4 | 35 % | 1.2 s | 1-3 deg | 3-8 deg | PUFF 0.7 x 2, speed 0.6 | up to 5 | no |
| bed | B8 | 30 % | 1.5 s | 0-1.5 deg | 2-5 deg | PUFF 0.9 x 2, speed 0.55 | up to 6 | no |

- **In the ground** is a share of the mesh height. Left standing: E1 7.9 m, E2 6.4 m, E3 7.3 m, B7 5.2 m, B3 5.4 m,
  B4 5.9 m, B5 4.4 m, B2 5.1-5.5 m, E7 5.2 m, B1 3.5-3.8 m, E5 3.0-3.2 m, E6 4.1-4.3 m, stubs 0.75-1.0 m, B8 1.7 m.
- **Tilt and yaw** are picked by the C++ sink per death: a random size in the range, a random tilt direction, a
  random yaw sign. A row of dead ruins does not lean alike. It is not derived from the actor position.
- **Bursts:** the first at the death in the middle of the footprint, the others spread over the sink time and over
  +-30 % of each ground side. One more burst (scale `heap_fx` x heap scale, 0.8-1.3) on each heap spot.
- **Thrown debris** is the hole component's death burst (`max_collapse_pieces`, `death_speed_scale`): kit pieces of
  38-68 cm. `CLOUD` adds its own stones on cores and heroes.
- **B8 debris bed** is a remnant like the rest: it settles 30 % (75 cm) with a small turn and keeps the nav boxes of
  the parts still 50 cm high (4 of 6). It was not kept as the one piece that disappears: a bed that vanished would
  leave a hole in the plot it anchors.

### Heaps: which piece leaves one

Spots are the `dead` entries of `footprint_table.json` (x, y in cm in the piece's local space, yaw added to the piece
yaw, uniform scale), except where `HEAP_SPOT` moves one.

| Piece | Heap | Why |
|---|---|---|
| E1_CornerTower | L at (150, -150) | inside the wall ring |
| E2_Courtyard | L at (620, 0), S at (-650, 560) | inside the two courts |
| E3_CoreRow | L at (0, 750), L at (0, -750) | fill the shell; spill 0.5-1 m through the front wall foot |
| B7_WallCore | L x 0.9 at (**400**, 0) | moved 4 m to the street side (`HEAP_SPOT`): at the table's centre it lay astride the wall stump |
| B5_CornerL | S at (0, 0) | in the inner corner |
| E5_Shell1St_A/B | S at (0, 0) | inside the shell, walls 3 m around it |
| E7_HalfHouse_A/B | S x 0.8 at (0, 0) | inside the half house |
| E6_HeapFin_A/B | none (table has one) | the fin already stands on its own modelled heap; a second one buried it |
| E4_StubRing_A/B | none (table has one) | a 2 m heap would bury a ring of 0.8 m stubs that now stays |
| B1-B4, B6, B8 | none | no entry in the table |

- The heap appears 30 % into the sink (0.54-1.8 s), 0.35 s after the dust on its spot. With the smaller dust it is
  no longer fully hidden when it appears: on the small classes the puff covers the middle of the heap, not its rim.
- The table's `B_PLOT` / `B_PLOT_STUB` heaps belong to a whole plot, not to one actor; no piece spawns them.

### What a remnant is for the game

- **Dead.** `IsUnitAlive` is false: more damage only lowers `Health` further. Tested on every class: 10,000 more
  damage, point damage and `TriggerDestroyActor` gave no second death, no new heap or dust, no hole, no movement.
  Hits are no longer forwarded to the hole component: a remnant takes no new shell holes (the old ones stay).
- **Still solid.** The mesh keeps its collision: it blocks shells, weapon traces and line of sight where it now
  stands, lowered and turned, exactly as drawn.
- **Still blocks movement.** A nav box is kept when the hulls it was made from still stand 50 cm or more after the
  sink (`REMNANT_NAV_MIN_STAND`), so every lowered wall, including a 0.8 m stub, blocks as before; only boxes of
  rubble that went under are dropped (E1 keeps 8 of 11, E2 11 of 14, E3 2 of 4, E4 4-6 of 7, E5 4 of 5, E7 2 of 3,
  B5 2 of 3, B8 4 of 6; all others keep all). The kept boxes are the upright footprints from before the death: they
  do not follow the turn. With 3-8 degrees of yaw the end of a 10 m wall moves 0.3-0.7 m off its box, so a unit can
  clip that corner or be held 0.5 m off it. Following the turn needs the boxes to be re-placed after the sink, which
  the graph could do in `BP_OnVerticalDestructionComplete`; not done.
- **A piece that could be walked around still can; what was open stays open.** Doorways and gaps are where they were.
- **No cover sockets** (the ruins never had any). The heap is the cover.
- **Still accepts an attack order.** `AttackActor` on a remnant returns no error (the command does not ask whether
  an environment actor is alive), so a player can waste shells on it. Refusing it is a C++ change in the attack
  command.
- **Cost:** a remnant is the dead actor with its mesh, the idle hole component and the kept boxes; no tick, no timer.
  A map where everything has been shot still has all 725 actors.

### Cover of the spawned heaps (not working: needs C++)

A heap spawned by a death has its eight `prone_n` sockets and its nav boxes, and it is destructible, but **its sockets
are not published as cover points**. In the test the control heap placed before play published 8 of 8, every spawned
heap 0 of 8.

- **Cause:** `URTSCoverFinderWorldSubsystem::RegisterSocketCoverOnce` reads the socket-cover classes
  (`M_SocketCoverActorClasses`, which holds `BP_NaturalCover`) once, on the subsystem's first tick. Nothing registers
  an actor of those classes that is spawned later. `RefreshCoverSocketsOfActor` only serves `IRTSCoverSocketSource`
  actors, and `RegisterAuthoredCoverProvider` is not callable from Blueprint.
- **Smallest fix (C++, not done):** one public, Blueprint-callable entry on the subsystem, for example
  `RegisterSocketCoverActor(AActor*)`, that runs the body of the loop in `RegisterSocketCoverOnce` for one actor
  (build the socket points, `RegisterAuthoredCoverProvider`, add to `M_SocketCoverProviders`) and does nothing before
  the one-time pass. `ADestructableActorHealth::BeginPlay` (or `BP_NaturalCover`) calls it, so every natural cover
  spawned at runtime registers, not only these heaps. The removal on destruction is already there
  (`RemoveSocketCoverOfDestroyedActors`). The call should use the retry that `RefreshCoverSocketsOfActor` has for
  points that are off the navmesh, because the tiles under a fresh remnant may still be rebuilding.

### Other limits

- **Heap tint:** the map's heaps get the lot's tint as custom primitive data. A spawned heap has the material's
  default tint; Blueprint cannot read the ruin's custom primitive data to pass it on.
- **The sink is a lift.** The mesh goes down in one piece. With the small dust this is now plainly visible on the
  large pieces: a building sliding 10-13 m into the ground while it turns.
- **Pure-node loops:** `K2_GetComponentsByClass` is const, so its Blueprint node is pure and a `ForEach` re-evaluates
  it on every pass. Destroying components in such a loop skips every second one. The graph copies the boxes into
  `DeathNavBoxes` first.

### Changing the death

- **Values** (depth, times, turn, dust, debris, heaps): edit `DEATH`, `DEATH_PIECE` or `HEAP_SPOT` in
  `ruin_blueprints_build.py`, re-run `run()` (headless commandlet). No graph work.
- **Graph:** replay or edit the calls of `ruin_death_graph_uecp.json` against a UECP-enabled throwaway editor
  (`-DisablePlugins=Fab` only, its own port from `%LOCALAPPDATA%/UECP/instances/<pid>.json`), compile, save the base,
  then `run()`. The builder checks that the base has every variable in `DEATH_VARIABLES`; if one is missing it builds
  every child with the old chunk death and says so in the report (`death_graph_in_base`, `errors`).
- **While editors are open:** any editor with a ruin loaded (the shared editor on `Ger_Berlin`, another agent's
  throwaway on that map) holds the 23 `.uasset` files and the saves fail.
  - The shared editor lets go when it saves the 23 packages unchanged (`BerlinRuinKitMaterial.md`, "Rebuild while
    editors are open"); reload them there afterwards.
  - An editor that is not yours cannot be asked to. Then write the packages to a staging folder from the authoring
    editor (`OBJ SAVEPACKAGE PACKAGE="..." FILE="<staged>/<name>.uasset" SILENT=true`, one per package) and let
    `ruin_death_install_staged.py` copy them over the moment the files are free.
- **After any change:** `ruin_death_compile_check.py` (headless, fresh load of all 23), then the death test.

## Cost per actor (about 1,000 on the map)

- **Nothing ticks.** The actor, mesh, hole and box components all have ticking off.
- **Components:** root, mesh, hole component and 1-14 boxes. At 1,000 actors that is about 7,000 components, 4,300 of
  them boxes. The boxes have no physics body, no collision and no render proxy in game. They cost memory and
  registration time at load.
- **Mesh:**
  - Movable, so no lightmaps; the meshes are not Nanite.
  - 2 sections (draw calls) per ruin.
  - 6-31 convex hulls in the physics scene as one kinematic body.
  - **This is the biggest cost.** Consider Nanite on the kit meshes, or Static mobility if the sink is never added.
- **Navigation:** about 4,300 Null boxes in the navmesh build; a death rebuilds 1-9 tiles (10 m each) per agent navmesh.
- **Undamaged:** an intact ruin renders the opaque `MI_BerlinRuin_Wall`. The masked holes instance (PS +132) and the
  custom primitive data arrive only with the first hole.
- **No geometry collections**, no `RTSNavCollisionEnvObstacle`. The one Blueprint graph (the death, in the base)
  has no BeginPlay and no Tick: nothing runs until the ruin dies.

## Test

- **Run:**

  ```
  # Git Bash, MSYS_NO_PATHCONV=1, throwaway editor, never Ger_Berlin
  UnrealEditor.exe RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall"
    -ExecCmds="py Source/Tools/BerlinMap/ruinkit/ruin_holes_headless_test.py" -LogCmds="LogWallHoleDamage Verbose"
    -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>
  ```

  Launcher: `Tools/BerlinMap/ruinkit/ruin_holes_run.sh <logname> [timeout_s]`.
- **What it does:**
  - spawns E1, E3, B3, B7 and E5_A (yaw 90, so the front faces the sunlit +Y), each with a player Pz IV G 35 m in front,
    plus B8 without a tank;
  - runs `RebuildNavigation` in the editor world before PIE;
  - checks the runtime setup and the navmesh, on the default (Character) navmesh and the HeavyTank one;
  - takes pictures;
  - orders `AttackActor` (Python `tank.attack_actor(ruin, True)`) for 36 s. A ruin still at full health after 12 s
    gets `attack_ground` on its first aim point instead;
  - gives labelled debug hits (`Debug_FireTestHit`) to ruins with fewer than 2 holes, and to B8;
  - takes more pictures, kills E3, E5, B7 and B8, and checks the navmesh where they stood. (Written for the chunk
    death; with the sink its `death_*` pictures show the dust, and its "killed" line reports a visible mesh and a life
    span of 0. The death has its own test, below.)
- **Proof of real shells:** the `RTS_WALL_HOLES hit ... direct=1` lines in the log.
- **Picture lag:** in the throwaway editor a screenshot can land a second after it is requested. The camera holds 2.5 s
  per picture, and a `_warmup` shot goes first.

- **Navigation test gotchas:**
  - Run `RebuildNavigation` in the editor world after spawning and wait for it. The PIE navmesh is a copy of the editor
    one; without the rebuild, nothing spawned before PIE is in it, not even the Rus wall control.
  - Unpause the game: navigation building and actor life spans don't tick while it is paused.
  - Put probe points at ground height. A downward trace lands on top of the ruin.

### Result (run6, 2026-11-12, `Saved/Berlin/ruinkit_test/run6.log`; run5 the same without B8 and AttackGround)

- **Setup at runtime (all six):**
  - Health, reduction and aim points match the table.
  - Mesh: Destructible; Projectile, TraceEnemy, TracePlayer and Visibility block; Pawn ignore; BuildingPlacement
    overlap; does not affect navigation; Movable.
  - Boxes: dynamic obstacle, `NavArea_Null`, no collision, invisible.
  - No `ReportError` from the ruins. The log's errors are the map's enemy AI and squads.
- **Real shells** (one Pz IV G per ruin, 36 s), counted from the `direct=1` lines:
  - B3: 5 hits, 4 holes;
  - B7: 5 hits, 4 holes;
  - E5_A: 5 hits;
  - E1: 4 hits, 2 holes, all after the AttackGround fallback (see the first weak spot);
  - E3: 1 hit.
  - Every first hole swapped slot 0 to `MI_BerlinRuin_Wall_Holes`; slot 1 kept the rubble.
  - A Pz IV hit takes about 210 health and cuts a 75 cm hole.
  - In run3 the E5 filler (1400 health) died under fire alone. It threw its pieces and was gone 4.5 s later.
- **Debug hits** (labelled `DEBUG` in the log):
  - Every aim point of E1 (run5), E3 and E5 landed on the wall collision (gap to the render surface 0 cm, -13 cm once)
    and cut a hole.
  - B8 (rubble, no hole material): 1 hit was counted and slot 0 stayed `MI_BerlinRuin_Rubble`. Its second aim point
    (then 1.25 m up) missed the heap. The aim points of low hulls are now a third of the way up, 0.8 m on B8; not
    re-run.
- **Navigation**, on the Character navmesh; the HeavyTank path goes in brackets after it. The path runs from 4 m in
  front to 4 m behind, through the middle:
  - **Alive:**
    - 0 of 204 points inside the boxes were on the navmesh; all 48 open points around and inside were.
    - Every path detoured: E1 39.9 m (45.0) against 29.0 m straight, E3 47.2 (53.2) against 23.4, B3 21.5 (26.8)
      against 13.6, B7 32.0 (37.8) against 20.8, E5 26.6 (32.2) against 18.6, B8 24.2 (28.7) against 19.1.
  - **After the deaths of E3, E5, B7 and B8:** every box point was back on the navmesh, and the paths ran straight on
    both navmeshes (E3's HeavyTank path still bent round the living neighbours).
- **Pictures** (`Saved/Berlin/ruinkit_test/`):
  - `undamaged_*`: E1, E3, B3, B7, E5, plus `_close` of E1 and B3.
  - `holes_*`: the same set, plus `_close` of E1, B3 and B7. The E3 holes are mostly debug hits.
  - `death_*_0_4s` / `_2_0s`: E3, E5, B7, B8.
  - Older runs are in `run2_old/`..`run5_old/`.
- **Look (honest):**
  - **Material and vertex colours:** they come through as the modellers painted them (compare
    `Model/E_Heroes/Previews/E1_rts_look.png`).
    - Grey-beige plaster dominates the street faces.
    - Bare brick shows on tops, broken edges, piers and the backs of walls; the chipped plaster edge reads well up
      close.
    - Soot reads as soft dark smudges above openings.
    - Dust is hard to see on this sandy map.
    - The strength looks right; nothing suggests a linear/sRGB mix-up.
    - At RTS distance under `RTS_ToCopySmall`'s orange light the plaster reads light, slightly green-grey, and the
      brick tops reddish. It is not uniformly sooty.
    - The B rubble beds and E3/B7 mounds are a pale, busy brick-plaster speckle that reads lighter than the walls.
  - **Holes:**
    - Ragged, noise-edged holes with a dark scorched rim; slots swap on the first hole.
    - Through 45-60 cm walls the hole shows sky or the dark inside of the building. The two-sided back faces are
      darkened (inner tint), so the hole reads as depth.
    - In one E1 close-up the hole's far side showed a pale flat patch, the back face of the inner wall/heap lit
      through the hole. It is not wrong, but it reads blotchy.
    - At RTS distance a 75 mm hole is a small dark spot; a 105-150 mm hole reads clearly.
    - Hits that land on window openings or right at a pier edge make half-holes that are easy to miss.
  - **Death (as it was in this run, the chunk death; replaced on 2026-11-13, see "Death" and "Death test"):**
    - The whole ruin, mound included, vanished in one frame. 100-120 small pieces (at most 82 cm) burst outward and
      shrank away; there was no dust and nothing stayed.

## Death test (`ruin_death_headless_test.py`)

- **Run:** `Tools/BerlinMap/ruinkit/ruin_death_run.sh <logname> [timeout_s]`: a throwaway editor on `RTS_ToCopySmall`
  (never Ger_Berlin), log, `result.json` and pictures in `Saved/Berlin/ruin_death_test/`. About 6 minutes.
  Third argument `large2`: the second set (`result_large2.json`); `fx`: the effect contact sheet.
- **Placed before play** (yaw 90, then `RebuildNavigation`), one piece per death class: B6_Stub_A (killed by a player
  Pz IV G's shells), B1_Facade1St_A, E5_Shell1St_A, B2_Facade2St_B (dressed through `ambient_place.py`: curtain, lamp,
  four crows), E7_HalfHouse_A, E1_CornerTower, and a control `BP_BerlinCoverHeap_Collapse_S` placed like a map heap.
  Second set: B7, E3, E2, B5, B3 and E4_StubRing_A.
- **Camera:** the game camera for every piece (28 m up, 19 m back, the same framing), so the sizes compare.
- **Per death** (`apply_damage`, the TakeDamage path; the stub by real shells): samples every tick on the world clock
  (mesh z, tilt, nav boxes, dressing, live dust systems, heap actors), pictures before and 0.5 / 2 / 5 s after, 4 s
  after the sink ended, and 10 s after that. Then more damage, point damage, `TriggerDestroyActor` and an attack
  order on the remnant.
- **Afterwards:** heap transforms against the builder's values and the transform before the death, navmesh points
  and paths, cover points at the heap sockets.
- **Fresh-load check:** `ruin_death_compile_check.py` (commandlet) loads and compiles all 23 Blueprints from disk; the
  verdict is 0 `LogBlueprint: Error` lines.
- **Reading the navmesh numbers:** a point count only means something when the two open-ground path ends read True
  and the box points of the living ruin read False in the same run (they do below).
- **Dust size in the table** is the reach of the effects' bounds from the piece's centre, as the engine reports them;
  Niagara bounds are generous, so read it with the pictures.

### Result (run9, 2026-11-14, from disk after a headless build, `Saved/Berlin/ruin_death_test/run9.log`, pictures also in `run9_final/`; burst counts include the one on each heap spot)

| Piece (footprint, height cm) | Death by | Drop (cm) | Pitch / roll / yaw (deg) | Nav boxes kept | Heaps (at s) | Dust bursts, effect, scale | Dust bounds reach (cm) |
|---|---|---|---|---|---|---|---|
| B1_Facade1St_A (410 x 700, 496) | damage | 149 | -1.4 / 0.0 / -6.2 | 3 of 3 | none | 2, PUFF, 1.0 | 609 |
| E7_HalfHouse_A (662 x 850, 796) | damage | 278 | -1.8 / 0.7 / 3.2 | 2 of 3 | S (1.02, set 0.66) | 4, PUFF, 0.96 / 1.3 | 866 |
| E5_Shell1St_A (1055 x 1261, 460) | damage | 138 | -1.1 / -1.9 / 6.0 | 4 of 5 | S (0.93, set 0.54) | 3, PUFF, 1.0 / 1.1 | 2204 |
| E1_CornerTower (1855 x 2155, 2080) | damage | 1289 | 2.0 / -0.0 / 1.9 | 8 of 11 | L (2.06, set 1.80) | 7, CLOUD, 1.2 / 1.4 | 1385 |
| B2_Facade2St_B (466 x 1304, 844) | damage | 295 | 0.6 / 1.2 / 5.8 | 4 of 4 | none | 3, PUFF, 1.3 | 3027 |
| B6_Stub_A (489 x 1000, 124) | tank shells | 44 | -0.1 / 2.6 / 8.0 | 1 of 1 | none | 2, PUFF, 0.7 | 1069 |

- **Sink:** the mesh z falls linearly to the set depth and does not change in any later sample; every actor is still
  valid at the end, with actor tick off, no ticking component and no dust system left.
- **Dead:** after 10,000 damage, point damage and `TriggerDestroyActor` on each remnant: same z, same hole count, no
  new heap, no dust; only `Health` went further down. The attack order returned no error (see "What a remnant is for
  the game").
- **Sound:** `ListWaves` 0.25 s after each death listed a collapse wave on 5 of 6 deaths (`Collapse01` or `Collapse02`; the probe of the first death, B1, caught nothing).
- **Heaps:** every expected heap was found at 0.0 cm location error and 0.0 degrees yaw error with its scale. They
  appeared 0.26-0.39 s later than set (frame steps of this editor around the death).
- **Navigation** (Character navmesh, HeavyTank in brackets; path from 4 m in front to 4 m behind):
  - B1_Facade1St_A: alive 0/12 of its box points on the navmesh, path 1637 (2025) against 1300 straight. After: 0/12 kept-box points on the navmesh, 0 old box points free, 12 under heap or kept boxes, 0 blocked for no reason; path 1637 (2025); open ends on.
  - E7_HalfHouse_A: alive 0/17 of its box points on the navmesh, path 1829 (2251) against 1455 straight. After: 0/12 kept-box points on the navmesh, 0 old box points free, 17 under heap or kept boxes, 0 blocked for no reason; path 1839 (2278); open ends on.
  - E5_Shell1St_A: alive 0/42 of its box points on the navmesh, path 2675 (3185) against 1855 straight. After: 0/36 kept-box points on the navmesh, 0 old box points free, 42 under heap or kept boxes, 0 blocked for no reason; path 2675 (3185); open ends on.
  - E1_CornerTower: alive 0/79 of its box points on the navmesh, path 3993 (4504) against 2901 straight. After: 0/60 kept-box points on the navmesh, 6 old box points free, 73 under heap or kept boxes, 0 blocked for no reason; path 3993 (4504); open ends on.
  - B2_Facade2St_B: alive 0/17 of its box points on the navmesh, path 2155 (2678) against 1360 straight. After: 0/17 kept-box points on the navmesh, 0 old box points free, 17 under heap or kept boxes, 0 blocked for no reason; path 2155 (2678); open ends on.
  - B6_Stub_A: alive 0/8 of its box points on the navmesh, path 1844 (2343) against 1280 straight. After: 0/8 kept-box points on the navmesh, 0 old box points free, 8 under heap or kept boxes, 0 blocked for no reason; path 1844 (2343); open ends on.
- **Cover:** control heap 8 of 8 sockets published at the start and at the end; spawned heaps 0 of 8 each (see
  "Cover of the spawned heaps").
- **Dressing:** 3 of 3 tagged components visible before the death of B2, all hidden in the first sample (0.04 s).
- **Holes still work:** not re-run on 2026-11-14 (the hole settings were not touched, only the death debris counts). Last run `ruin_holes_run.sh run7_after_death` on 2026-11-13: 19 direct shell hits, holes E1 2, E3 2, B3 5, B7 3, E5 1, slot 0 swapped to `MI_BerlinRuin_Wall_Holes` on each. In run9 the stub took a hole from the tank before it died (hole count 1).
- **Second set:** `run10_large2.log`, `result_large2.json`: B7 drop 630 cm (yaw -2.6), E3 1102 (2.0), E2 962 (1.0), B5 666 (4.2), B3 1010 (4.6), E4 stub ring 40 (-6.3); kept boxes 1/1, 2/4, 11/14, 2/3, 4/4, 4/7 with 0 points on the navmesh; B7's heap found 400 cm from the table's spot (the moved spot), E4 without a heap, as set; every remnant still and dead afterwards.
- **Pictures** (`Saved/Berlin/ruin_death_test/`): `death_<piece>_before`, `_0_5s`, `_2_0s`, `_5_0s`, `_after`,
  `_remnant_plus10s`, and the comparison sheet `death_comparison_sheet.png` (six pieces x 0.5 s, 2 s, after). Older
  looks: `run4_vanish_all/`, `run5_inmemory_remnant/`, `run6_final/` (2026-11-13: big dust, small pieces vanish).
- **How it looks:** a stub gives a low brown puff no wider than the wall and a few stones, and settles half a metre.
  A one-storey slab or shell: two puffs, the wall drops a good metre and turns. A two-storey facade or half house:
  three puffs along it, well inside the view. A hero: six grey-brown ring clouds over six seconds with stones flying,
  the building visible through them the whole time while it goes down 13 m.

### Not covered, or weaker than asked

- Tested on the unit-test map only: flat ground, no footprint skirt under the heaps, orange light. Not on Ger_Berlin,
  where the owner saw the first version read white; the tint was chosen on this map.
- The dust bounds are engine bounds, not measured smoke; the comparison is the pictures.
- The kept nav boxes do not follow the remnant's yaw (up to 0.7 m off at the end of a long wall).
- A heap is not hidden by dust when it appears on the small classes.
- A remnant can still be given an attack order; spawned heaps publish no cover and have no lot tint.
- B4, the B variants and E6 were built and compile but were not killed in a test. No scaled ruin was killed.

## Known weak spots

- **Death:** every piece ends as a lowered, turned remnant (see "Death"). Open: the spawned heaps publish no cover
  points until the cover finder gets a registration call for actors spawned at runtime (C++); they do not take the
  lot's tint; a remnant still accepts an attack order; its kept nav boxes do not follow its yaw.
- **`AttackActor` barely engages the two big heroes (gameplay finding, cause not found):**
  - Over five runs the tanks ordered to `AttackActor` E1 (18 x 21 m, 21 m tall) and E3 (30 m row) did not move or turn,
    and hit their ruin 0-2 times each run.
  - In run6 the E3 tank fired 3 projectiles in 36 s. E1's tank count is polluted by the map's own fire.
  - The tanks ordered onto B3, B7 and E5 drove a few metres, turned, and kept firing (4-6 hits each).
  - `AttackGround` on E1's first aim point made the same tank hit E1 4 times.
  - So the hole setup works on the heroes; the attack command against them is the problem. Suspects, unchecked:
    - the attack command's range or line-of-sight test against a very large target's origin or bounds;
    - the aim point choice.
  - Worth a look by whoever owns the attack command before players try to shell the heroes. Debug hits at every hero aim
    point land on the wall.
- **Rubble hits:** a shell on the rubble slot (heaps, mounds) takes one of the 6 hole slots and throws debris, but shows
  no hole. The aim points avoid the mounds (at least 1.2 m above any heap); splash and misses still land there.
- **Six holes:** with NoGrid, older holes merge into their nearest neighbour, so on E1/E2/E3 (20-30 m) old holes far from
  new ones disappear (the same as on the Gate).
- **Kit debris is small:** 38-68 cm, and the C++ caps the piece scale at 1.2. Death bursts look like gravel next to
  PostApo's 25-120 cm pieces. Bigger debris meshes (1-1.5 m slabs) would help more than any setting.
- **Debris material:** the kit debris was switched to `MI_BerlinRuin_Wall_Holes` (coordinator's go-ahead), because
  `M_BerlinRuin` lacks `bUsedWithNiagaraMeshParticles`. The builder does this whenever the flag is off.
- **Nav boxes:**
  - They are axis-aligned in mesh space; the kit's walls are, so they hug the hulls.
  - A diagonal wall in a future mesh would get its AABB box, which over-seals.
  - Merging can bridge a gap of under 0.25 m² between two hulls. The builder report lists `nav_box_m2` against
    `ground_hull_m2` per piece; no piece was overstated in this build.
  - Pawns don't collide with the mesh (pawn channels ignored, as on every destructible env actor), so navigation is the
    only thing that keeps units out.
- **Navmesh:** the navmesh must be rebuilt after the ruins are placed (see "Navigation"). Runtime-spawned ruins are
  untested.
- **1,000 placements:** per-actor components, Movable meshes without Nanite, and 2 draw calls each (see "Cost per
  actor").
- **Scale:** fillers must not be scaled below 1.0 (contract wall thickness). Hole sizes scale with the actor.
