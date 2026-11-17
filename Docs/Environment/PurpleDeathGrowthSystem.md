# Purple Death growth system (design, 2026-11-16)

**Status: design only. Nothing is built.** This document collects the agreed ideas, the work that has to happen,
and how to measure the system once it exists. Numbers in the budget are targets and estimates, not measurements.

## What it has to do

- The Purple Death grows over everything on a map: landscape, buildings (vertically, until they are fully covered),
  props, wrecks, trees and other foliage.
- **It looks like the 1976 intro cinematic** (section 5 of `Docs/Campaign/IntroCinematicScript.md`; art in
  `/Game/RTS_Survival/Cinematics/Intro1976/`): plum-black wet glue and vessels, glowing only faintly. Not neon.
  Details in section 3E.
- **It hurts units.** Infantry without hazmat suits is injured very quickly and dies. Hazmat infantry is immune.
  Details in section 4.
- Designers place **nodes**. Growth starts from each node. When two growths meet they join into one sheet. They never
  stack or overlap.
- It is **fightable**. Units can target it, parts of it are destroyed procedurally, and the growth is pushed back. It
  grows back over time, so if the player stops pushing it, it keeps coming.
- **Nodes are killable.** A killed node stops feeding its growth (section 4).
- **Performance comes first.** The project is already near the limit for 60 FPS on an RTX 3060. The landscape material
  is already about 780 instructions (`Docs/Landscape/LandscapeMaster.md`), so the system must not make it bigger.

## Overview

```
 APurpleDeathNode (sources)            weapon impacts / killed growth targets
          |                                         |
          v                                         v
 UPurpleDeathGrowthSubsystem  -- one map-wide grid, 2 m cells (gameplay truth, CPU) --
          |   event-driven writes only (node added/removed, damage, regrowth)
          v
 GPU textures: growth times + growth info + ground height (written per dirty rectangle)
          |
          +--> A. ground tiles: instanced, vertex-displaced, opaque mesh over the landscape
          +--> B. wall/prop decals: chunked, climbing with time
          +--> C. foliage + landscape grass: read the growth texture in their own materials
          +--> D. growth geometry: pooled instanced veins, pustules, glue skirts (silhouette)
```

Proposed names (adjust to the project's conventions when building):

| What | Name |
|---|---|
| Code folder | `Source/RTS_Survival/Environment/PurpleDeathGrowth/` |
| World subsystem (simulation, textures, layers) | `UPurpleDeathGrowthSubsystem` |
| Node actor | `APurpleDeathNode` |
| Targetable growth cluster (pooled) | `APurpleDeathGrowthTarget` |
| Damage over time on units standing in it | `UPurpleDeathExposureBehaviour` (a `UBehaviour`, like `URadixiteDamageBehaviour`) |
| Tuning | `UPurpleDeathGrowthSettings` (Project Settings -> Game -> RTS Purple Death) |
| Benchmark | `UPurpleDeathGrowthBenchSubsystem` (`-PurpleDeathBench`), runner `Source/Tools/PurpleDeathGrowth/run_bench.sh` |
| Content | `/Game/RTS_Survival/Environment/PurpleDeathGrowth/` |
| Console variables | `rts.PurpleDeath` (0 = off), `rts.PurpleDeath.GroundTiles`, `.WallDecals`, `.Growths`, `.TileShadows`, `.Quality` |

## 1. Simulation (CPU, the gameplay truth)

### Grid

- One grid covers the whole playable map, **2 m cells**. One km² is 500 x 500 = 250k cells. Ger_2ndOreMountains
  (720 x 640 m) is about 115k cells. Per cell, about 8 bytes: roughly 2 MB per km².
- Nodes own nothing. They are only sources for the one field. **Merging and "no overlap" hold by construction**: each
  cell has a single arrival time, the earliest any node reaches it.

### Per cell

| Field | Meaning |
|---|---|
| `Arrival` | Game time at which the cell becomes covered. "Never" = max value. |
| `Cut` | Game time of the last damage on this cell (drives the retreat animation and the scar). |
| `Source` | Index of the node whose growth reached the cell first. Neighbours with different sources = a seam. |
| `Dir` | Direction the growth came from (2 x 8 bit), so veins point away from the node. |
| `WallDist` | Distance to the nearest building or prop footprint, baked once per map (ground bulge and glue skirts). |
| `Cost` | Spread cost of the terrain, baked once (slope, water = blocked, roads slower). |

### Growth

- Arrival times come from a multi-source Dijkstra (or fast marching) over the grid:
  `Arrival(cell) = min over neighbours (Arrival(n) + Cost(n -> cell) / Speed)`, plus per-cell noise so the front is
  organic, not circular.
- Each node has a strength (maximum reach and speed). Cells beyond every node's reach stay "never".
- **Event-driven.** The field is recalculated only when a node is placed or removed, or damage lands, and only in the
  region affected. The work is sliced over frames (budget per frame in the settings). With no events, the per-frame
  CPU cost is close to zero.
- The renderer animates growth between events from the stored times (section 2), so the CPU never ticks the front.

### Damage, retreat, regrowth

- Damage at a point carves a noisy hole: cells inside get `Cut = now` and a new `Arrival` in the future
  (`now + RegrowDelay`), then the local Dijkstra re-propagates from intact neighbours. The hole fills in from its
  edges, so the front visibly creeps back.
- Gameplay queries (`IsCovered(location)`, coverage under a unit) are a single grid lookup.

### Decisions (owner, 2026-11-16)

| Question | Decision |
|---|---|
| Does it affect units? | **Yes, it damages them.** Infantry without hazmat suits is injured very quickly and dies. Section 4. |
| Are nodes killable? | **Yes.** Section 4. |
| Look | **Match the cinematic**: not much neon, more cinematic. Section 3E. |

### Decisions (owner, 2026-11-17)

| Question | Decision |
|---|---|
| Withering | **Yes.** Growth that no living node feeds withers away slowly (section 4). |
| Vehicles | **A little damage** (section 4). |
| Which maps | **Every map where nodes are placed.** A map without nodes pays nothing: the subsystem creates no grid, textures, tiles or decals until the first node exists. The grid covers the landscape bounds of the map it is on. |
| Fog of war | **Ignored.** Growth updates and renders whenever it changes, visible or not; the fog of war post process darkens it like any other ground. No vision checks. |

### Requirements without a question

- Save/load: missions need the nodes (with health) plus the `Arrival`/`Cut` arrays saved and restored.
- Very large maps: if a map's grid would exceed the memory budget, allocate the grid in chunks only where growth can
  reach (around nodes), instead of one array for the whole landscape.

## 2. GPU data

| Texture | Format | Content | Updated |
|---|---|---|---|
| `GrowthTimes` | RG16 UNORM | R = `Arrival`, G = `Cut`, as time since an epoch in 0.25 s steps (65535 = never; ~4.5 h before the epoch has to be rebased) | Dirty rectangles on events |
| `GrowthInfo` | RGBA8 | RG = `Dir`, B = `Source` hash (seam detection), A = `WallDist` | Dirty rectangles on events |
| `GroundHeight` | R16 UNORM over the map's height range | Height of the **drawn** landscape surface at the **landscape's own vertex spacing** (not the 2 m grid, see below) | Once per map: baked offline into an asset, or sliced over frames at load |

- Times are filtered bilinearly, which is meaningful: interpolating arrival times gives a smooth sub-cell front.
- The current time and epoch go to the materials through a Material Parameter Collection
  (`MPC_PurpleDeath`: `Time`, map origin and size, climb speed, quality). **Append new scalars only**: re-setting an
  MPC's scalar list from Python gives the scalars new ids and breaks every material that uses it.
- Shader, per pixel or vertex:
  - `grow = saturate((Time - Arrival) / GrowInSeconds + noise)`
  - `retreat = (Cut == never) ? 0 : 1 - saturate((Time - Cut) / RetreatSeconds)`
  - `coverage = max(grow, retreat)`
  - `Cut` starts as "never" and the simulation only writes it on cells that were covered when hit. Without the
    "never" guard, `Time - Cut` is negative on every uncut cell and the whole map reads as covered.
  - `climb = min((Time - Arrival) * ClimbSpeed, MaxClimb)` (for walls, section 3B)
- Heights: use the drawn surface (`ALandscapeProxy::GetHeightAtLocation` or the landscape heightmap), not traces.
  Collision sat up to 90 cm off the drawn surface on Ger_MarderStugMission (`ImpactDecals.md`).
- Height resolution: tile vertices sit only a few cm above the ground. Heights interpolated from 2 m samples would be
  off by more than that on rough or terraced terrain (Ger_2ndOreMountains is terraced), so the landscape would poke
  through and the occlusion saving would be lost. Bake `GroundHeight` at the landscape's quad size (check it per
  map) and align tile vertices to the landscape vertices. One `GetHeightAtLocation` per sample at BeginPlay is a load
  hitch on a large map: bake offline or slice it.

## 3. Rendering layers

### A. Ground: displaced mesh tiles over the landscape (replaces any landscape material change)

The idea: an **opaque** mesh that covers the landscape where creep is. `DefaultEngine.ini` has no `r.EarlyZPass`
override, so the project should run on UE5's default full depth prepass (confirm with `r.EarlyZPass` in a running
game; bench step 4 checks the effect). The landscape's expensive material then does not run
under the tiles. The creep pays its own cheaper shader instead, and the landscape material is not extended at all.

- One static mesh tile, `SM_PurpleDeath_GroundTile`, 32 x 32 m. LOD0 64 x 64 quads (0.5 m), LOD1 32 x 32,
  LOD2 16 x 16. One HISM instance per 32 m chunk that contains any cell with an arrival time.
- The vertex shader reads `GroundHeight` and `GrowthTimes`. A covered vertex sits a few cm above the ground plus a goo
  thickness (5-15 cm, noisy). An uncovered vertex sinks below the ground (-50 cm). Near walls (`WallDist`) the tile
  bulges up into a glue skirt (up to 1-2 m).
- The front edge is where the tile cuts into the terrain: a thick wet lip, with no fade. An optional thin DBuffer
  decal ring outside it can add a wet stain.
- Rules for the saving to hold:
  - The tile material must be **opaque**, never masked (masked runs a pixel shader in the prepass) and never translucent.
  - The landscape still pays its depth prepass, shadows, Lumen, grass and RVT rendering. Only its colour shading is
    saved, which is the expensive part.
- Settings on the tile component: no collision (shots and units hit the landscape just below; damage uses the hit
  location), `bAffectDistanceFieldLighting = false` (the distance field would be the flat tile in the wrong place),
  shadow casting off at first (A/B in the bench), extended bounds for the displacement.
- Tiles **do not receive decals** (`bReceivesDecals = false`). Otherwise the wall decals (B) would paint the 1-2 m
  bulges near walls, which sit above their ~20 cm clip height.
- Side effects on other ground systems:
  - Crater decals are placed on landscape hits and project onto the ground; with tiles not receiving decals, craters
    stay hidden under the goo. That suits it: damage shows as a scar in the goo instead (section 4).
  - Track ruts live in the landscape material, so they vanish under the goo. Ruts in the goo could be added to the
    tile material later by reading the same trail render target.
- Risks to prove in the spike:
  - Distant landscape LODs are coarser and can push through a tile only a few cm above the ground. Raise the offset
    with distance, or check how far the RTS camera can zoom out.
  - Vertex count at maximum zoom-out (about 8k triangles per LOD0 tile, ~100 tiles on screen = ~800k).
  - The velocity pass (`r.BasePassForceOutputsVelocity=1` is set) with vertex offset.

### B. Walls, props, wrecks, tree trunks: chunked decals that climb

- DBuffer decal boxes, one per 32 m chunk, **only for chunks that contain non-landscape geometry** (found once per
  map at BeginPlay). Box height covers the tallest building in the chunk.
- The decal reconstructs the surface direction from depth derivatives and projects its textures triplanar, so it
  wraps vertical faces. Pixels lower than ~20 cm above `GroundHeight` are clipped (the ground tiles cover those).
- Climbing: a pixel is covered when its height above the ground is below `climb`. The goo creeps up walls and
  reaches roofs once `climb` is higher than the building.
- **Units must not receive these decals.** No C++ opt-out for unit meshes was found, so units probably receive
  decals (the engine default; also check the unit Blueprints). The crater decals are thin enough that nobody
  notices, but these boxes are tall. Turn off `bReceivesDecals` on unit meshes centrally. Wrecks should keep
  receiving them.
- To prove in the spike: the look on vertical faces with derived normals, whether leaf materials take decals
  acceptably, and whether Lumen sees the decals (if not, only the emissive bounce is lost).

### C. Foliage and landscape grass

- Tree trunks are covered by the decals (B).
- Leaves and landscape grass sample `GrowthTimes` in their own materials: darken and tint toward purple-black. Grass
  collapses (vertex offset to zero) under the goo; otherwise it pokes through the tiles. The blight needed an editor
  grass rebake; runtime creep cannot, so grass must handle it in its material.
- Leaf droop and grass collapse need World Position Offset, which **Python cannot author** (not in
  `MaterialProperty`). Author those parts in the editor or through the UECP material graph tools.

### D. Growth geometry (silhouette)

A shading-only layer cannot change an object's outline. In the cinematic, shader-only Purple Death attempts were
worse in A/B and were reverted, and the shipped version uses modelled growth stages (`IntroCinematicScript.md`,
build ledger). This layer is required for "completely covered in glue".

- Pooled instanced meshes with a fixed budget (setting, e.g. 3-6k instances): vein tubes along the front and up
  building corners, pustules and sacs, glue skirts and membranes where walls meet the ground.
- Placed by the CPU when cells mature (on the front, at wall bases, building corners found by traces). Grow-in by
  per-instance custom data (spawn time) with vertex offset scale, or by swapping baked stages as in the cinematic.
- Small pieces cast no shadows. No Nanite: Nanite passes run if any Nanite asset is loaded, so it is all-or-nothing
  for the project (`OptimizationLog.md`, D7).
- Emissive stays low and only on the thick veins (section 3E). No dynamic lights, except perhaps one faint
  unshadowed light per node.

### E. Look: match the cinematic (decided)

The owner's direction: **not much neon, more cinematic.** The target is the shipped Purple Death of the intro
cinematic (shots 5.2 and 5.3, the 44-stage version). Its build ledger in `Docs/Campaign/IntroCinematicScript.md`
records what worked.

- **Colour:** plum-black and wet. Very dark albedo (around 0.02 in the cinematic; the trench set read porcelain at
  0.07 in its sun). The purple comes from the vessels and the wet sheen, not from bright paint.
- **Vessels:** raised, continuous vessels that branch away from the source and taper to needle points. In the game
  they follow `Dir` (growth direction), so veins point away from the nodes. Thick vessels near nodes, fine capillaries
  at the front. A "mould sheet" between the vessels, with relief.
- **Glow:** only faintly, and only in the thick vessels, a slow pulse. No neon edges, no glowing front line. Emissive
  values stay low: in the cinematic, bright emissive lost its colour after grading.
- **Specular:** low (flesh specular about 0.04). High specular made the cinematic's covered helmets read pale pink-white
  under fixed exposure.
- **Shading model:** default lit. Subsurface turned the mould hot pink under a low sun in the cinematic.
- **Projection:** soft triplanar (exponent about 1.5) on walls, so vessels don't bend at hard L corners.
- **Covered metal:** use a non-specular variant on steel (the cinematic's `M_Intro_MouldSteel`, specular 0), so covered
  wrecks and vehicles don't shine through.
- **Start from the cinematic assets:** `M_Intro_PurpleDeath`, `MI_Intro_Mould`, `MI_Intro_PD_Veins` and the vein
  relief textures. They were built for close cinematic shots under a fixed exposure; rebuild them as cheaper game
  materials and retune at the RTS camera under the game's lighting and auto exposure.
- **Judge it blind:** A/B against the previous version at the gameplay camera, with the majority rule fixed in
  advance. Unreviewed tuning loops did not converge in the cinematic.

## 4. Making it fightable

- **Every explosion damages it.** Next to the crater decal hooks (`AProjectile::SpawnExplosionHandleAOE`,
  `AProjectile::HandleTimedExplosion`, `UWeaponStateTrace::OnAsyncTraceHitValidActor`, `ABombActor`, `AICBMActor`;
  see `ImpactDecals.md`), call `UPurpleDeathGrowthSubsystem::ApplyDamage(location, radius)`. The radius comes from
  calibre or TNT. Reject cheaply first: system off, or no covered cell within the radius. Artillery and flamethrowers
  then clear creep without special orders.
- **Targets.** A capped pool of `APurpleDeathGrowthTarget` (pustules or hearts, about one per 16 m of front, at most
  ~64) sits on the front. Each is an ordinary enemy actor with health, registered the way other enemies are, so
  `URTSTargetAcquisition` picks it up. Confirm how enemies register (`UGameUnitManager`) before building. When one
  dies it cuts a larger hole and the pool moves it to the new front.
- **Scar.** The shader uses `Cut` to draw a charred rim with briefly glowing exposed flesh that fades out.
- **Regrowth.** As in section 1: a delay, then the front re-advances from intact cells.

### Killable nodes (decided)

- `APurpleDeathNode` is an ordinary enemy target with health (`URTSComponent`, health component), registered like
  other enemies, so units attack it on their own or by order. It is tough: a node is an objective, not a speed bump.
  Starting value to tune: about a Soviet base building's toughness (6-16 Pz IV shots, `Docs/Balance/SovietBuildingHealth.md`),
  for example 10 Pz IV shots.
- The node sits on its own growth, so infantry attacking it at close range takes exposure damage (below). Killing a
  node is a job for vehicles, artillery or hazmat infantry.
- When a node dies:
  - it is removed as a source, and the field is recalculated around it;
  - cells another living node still reaches stay (that node's growth takes over);
  - cells no living node reaches **wither** (decided 2026-11-17): they get a `Cut` time spread outward from the
    dead node over tens of seconds, so the goo dries and recedes in a wave instead of popping out;
  - a death effect: the heart bursts, the thick vessels around it collapse and darken.
- Without withering, killing a node would only stop the spread, which would make it a weak objective.

### Effect on units (decided: it damages them)

**Who takes how much.** Exposure uses the existing radiation path: `ERTSDamageType::Radiation` with the per-armour
multipliers already in `DeveloperSettings.h` (`UnitResistances`). These give exactly the requested behaviour without
new resistance tables:

| Unit armour | Radiation multiplier (existing) | Result in the Purple Death |
|---|---|---|
| Basic infantry | 100% | Injured very quickly and dies |
| Heavy infantry armour | 70% | Dies a little slower |
| Commando infantry armour | 50% | Dies slower, still cannot hold ground in it |
| Hazmat infantry, armoured hazmat | 0% | Immune: the counter unit for clearing nodes on foot |

If radixite and the Purple Death later need different resistances, add a separate damage type then. Starting
with Radiation avoids touching every damage-type switch.

**How fast.** Target time to death for basic infantry (200 HP before the global health multiplier) standing in full
growth: **about 4-6 s**, so damage per second is about health / 5. Units at the thin front edge take less (damage
scales with coverage under the unit), so a squad that blunders onto the edge has a moment to get out.

**Contamination.** Leaving the growth does not stop the damage at once: the effect runs on for about 2-3 s, so
running through it is not free.

**Vehicles (decided 2026-11-17).** A little damage over time (corrosion), small enough that tanks can push into
the growth and fight it, large enough that they cannot park in it forever. Aircraft are unaffected.

**How it is built (cheap):**
- `UPurpleDeathExposureBehaviour`, a `UBehaviour` like `URadixiteDamageBehaviour`: damage per second, refreshed while
  the unit stands in the growth, lingering for the contamination time after it leaves. The behaviour system already
  shows a radiation damage icon (`EBehaviourIcon::RadiationDamage`) for the player.
- The subsystem checks units against the grid in staggered batches (each unit about 4 times a second, spread over
  frames). A check is one grid lookup at the unit's position, so even several hundred units cost microseconds.
- Infantry deaths in the growth can later get their own death look (bodies darkening and sinking into the goo).
  Not required for the first version.

## 5. Constraints and gotchas (from earlier work)

- Python cannot author World Position Offset (it is not in `MaterialProperty`). Author vertex offset in the editor
  or through the UECP material graph tools.
- Re-setting a Material Parameter Collection's scalar list from Python gives the scalars new ids; every material that
  uses the collection then fails to compile in render processes. Append new scalars only.
- The shader complexity view counts static instructions; branches do not lower it. Check every switch permutation
  with `get_statistics` (`LandscapeMaster.md`).
- Whole-frame time deltas drown in machine noise of several ms. Decide by ProfileGPU pass lines and the system's own
  timers (`TankTrails.md`, `OptimizationLog.md`).
- Another session may be rebuilding the Development DLL; coordinate builds and do not hold the DLL during a link.
- Auto exposure needs ~150 frames before captures are comparable.

## 6. Alternatives considered

| Alternative | Why not (for now) |
|---|---|
| Ground in the landscape material (a material function like the radixite blight) | Adds instructions to every landscape pixel on every map that uses it, creep or not, on top of ~780. Kept as the fallback if the tiles fail the bench. |
| Ground through the wall decals (decals for everything) | Costs only on covered pixels, but adds a full decal shader on top of the landscape shader there instead of replacing it, and gives no thickness or edge. |
| A runtime-built procedural mesh (`UProceduralMeshComponent` or a dynamic mesh) over the landscape | Same occlusion saving as the tiles, but geometry is rebuilt on the CPU whenever the front moves, with no LODs and slow updates. The instanced tiles keep the idea (an opaque mesh hiding the landscape) and move the shaping to the vertex shader, so growth needs no mesh rebuilds. |
| Overlay material on fully covered buildings | Draws each building a second time, translucent. Too expensive for large buildings. |

## 7. What needs to happen (steps)

Each step ends with something that can be checked. Steps 2 and 3 are the spike that decides the ground approach.

| # | Step | Done when |
|---|---|---|
| 1 | ~~Answer the open questions~~ **Done** 2026-11-16/17 (section 1) | All answers recorded in section 1 |
| 2 | Simulation: subsystem, node actor, settings, grid, Dijkstra with slicing, damage, regrowth, debug view (`rts.PurpleDeath.Debug` draws coverage and fronts), console commands to place nodes and apply damage | Headless test: two nodes grow and merge with no overlap; damage cuts a hole that refills; CPU timers logged |
| 3 | GPU textures and ground tiles (A) on Ger_Trains, plus the benchmark (section 9) | Bench shows the landscape's BasePass cost drops under the tiles and the ground pass criterion (section 9) is met; no landscape pokes through; look checked at the gameplay camera |
| 4 | Wall and prop decals with climbing (B); units stop receiving decals | Buildings in the Ger_Trains test area are fully covered over time; no unit is painted; bench numbers within budget |
| 5 | Fightable (section 4): damage hooks, growth targets, scars, killable nodes with withering, unit exposure damage | Tanks and artillery push the front back and it regrows; a killed node's growth withers; basic infantry dies in about 4-6 s in full growth, hazmat takes no damage; CPU peaks within budget |
| 6 | Foliage and grass (C) | Grass does not poke through; trees read as engulfed |
| 7 | Growth geometry (D) and VFX | Silhouette reads at RTS distance; instance budget respected |
| 8 | Look development | Blind A/B against the previous version with a majority rule fixed in advance, judged at the gameplay camera |
| 9 | Scalability levels (`rts.PurpleDeath.Quality`), save/load, full perf pass on the three audit maps | Section 9 pass criteria met on every map and quality level |

## 8. Performance budget

Targets for an **RTX 3060 at 1920 x 1080** at the game's default quality on that card. The development machine has
an RTX 4070. Published comparisons put it at very roughly 1.8-1.9x a 3060 in raster; this factor is not verified
here. Use a conservative **2.0** to estimate 3060 times from 4070 measurements, and treat a run on a real 3060 as the
actual gate before shipping.

| Item | Target |
|---|---|
| GPU, system on, no creep on screen | < 0.02 ms |
| GPU, ground tiles, screen full of creep | Net cost vs the same view without tiles (tile cost minus landscape saving) <= +0.1 ms; the hoped-for result is <= 0 |
| GPU, whole system, worst view (buildings + trees, full screen of creep) | <= 1.0 ms |
| Shadow depth and velocity passes | Each grows by <= 0.05 ms |
| CPU, average per frame | <= 0.05 ms |
| CPU, worst single frame (recompute burst) | <= 0.5 ms |
| Texture upload | <= 64 KB per frame on average |
| Memory (2 x 2 km map, all layers) | <= 16 MB |

## 9. How to test performance (after it is built)

### Tooling to build with the system

- `UPurpleDeathGrowthBenchSubsystem`, active only with `-PurpleDeathBench`, following the impact decal and track
  trail benches:
  - places nodes at a fixed spot, fast-forwards growth (pre-grow seconds) so the scene is fully covered,
  - moves the camera through fixed views,
  - steps through the scenarios below, toggling the console variables,
  - logs `RTS_PURPLE_DEATH_BENCH` lines (scenario, CPU timers, upload bytes, instance and tile counts, memory),
  - runs one ProfileGPU dump per scenario step (`-PurpleDeathBenchProfileGPU`),
  - saves a screenshot per view to `Saved/PurpleDeathBench/<run name>/`.
- `Source/Tools/PurpleDeathGrowth/run_bench.sh <run name> [switches]`: a copy of
  `Source/Tools/ImpactDecals/run_bench.sh` (offscreen `-game`, `r.SetRes 1920x1080w`, `r.VSync 0`, `t.MaxFPS 0`,
  `r.ProfileGPU.ShowUI 0`, `-benchmark -fps=30`, log to `Saved/Logs/PurpleDeathBench_<run name>.log`). Add
  `r.ShowMaterialDrawEvents 1` and `r.EmitMeshDrawEvents 1` so per-material lines appear. Default map Ger_Trains,
  `MAP=` to change it, `-PurpleDeathBenchAt=X,Y` for the spot.
- `Source/Tools/PurpleDeathGrowth/compare_profilegpu.py`: medians per pass and per material across dumps, modelled on
  `Source/Tools/TankTrails/compare_profilegpu.py` and `Source/Tools/PerfAudit/pass_medians.py`.
- CPU timing: a stat group `STATGROUP_PurpleDeath` with `SCOPE_CYCLE_COUNTER` around the recompute slice, damage,
  texture upload and growth placement; the subsystem also keeps its own average/max per frame and logs them.

### Scenarios

Each at the gameplay camera (pitch about -70 like the perf audit views), at default zoom and at maximum zoom-out.

| # | Scenario | Purpose |
|---|---|---|
| S0 | `rts.PurpleDeath 0` | Baseline |
| S1 | System on, no nodes | Idle cost (must be ~0: no grid, textures, tiles or decals are created) |
| S2 | Flat open ground fully covered, tiles on | Ground layer cost |
| S3 | Same view, `rts.PurpleDeath.GroundTiles 0` | The landscape cost the tiles should save (compare with S2) |
| S4 | Building cluster fully covered (walls, decals, skirts) | Wall decal and climb cost |
| S5 | Trees and orchard covered | Foliage and grass cost |
| S6 | Maximum zoom-out, everything visible covered | Worst vertex count and decal area |
| S7 | 60 s of combat: ~20 tanks and artillery firing into creep | Damage events, regrowth, CPU peaks, uploads |
| S8 | 10 nodes placed in one frame, then fronts merging | Recompute burst and slicing |
| S9 | Each layer toggled alone (`GroundTiles`, `WallDecals`, `Growths`, `TileShadows`) | Cost per layer |
| S10 | 300 infantry (basic and hazmat mixed) and 30 vehicles standing in and walking through growth | Exposure checks and behaviour cost on the CPU; also checks time to death and hazmat immunity in the log |
| S11 | Kill a node that feeds a large area alone | Withering recompute burst and the size of the texture uploads |

### Procedure

1. Build **Development** (DebugGame CPU numbers are inflated; use DebugGame only for debugging). Make sure no other
   session is linking.
2. Run each scenario twice (`-PurpleDeathBenchRepeats=2`), with a warmup of 150 frames for auto exposure and at least
   5 ProfileGPU dumps per step.
3. Read **medians** of the pass lines, not whole-frame times: `PrePass`, `BasePass`, `ShadowDepths`, `Lumen`,
   the DBuffer decal pass, `Translucency`, velocity, `PostProcessing`, plus the per-material lines for the
   `M_PurpleDeath_*` materials and the landscape material.
4. Check that the prepass works: in S2 vs S3 the landscape material's BasePass line must drop where the tiles cover
   it. If it does not, the tiles are not occluding: check for a masked tile material, a changed `r.EarlyZPass`, or
   tiles sitting under the landscape.
5. Read the CPU timer lines (average and max per frame, slice time) and the upload bytes in S7 and S8.
6. Memory: the subsystem's own log line (grid bytes, texture bytes, tile and instance counts).
7. Tile fit: the bench samples the drawn landscape height at every tile vertex in view and logs the largest gap
   between the displaced tile and the landscape (`tilefit maxBelow= maxAbove=`). Any vertex below the landscape means
   the landscape pokes through there.
8. Look at every screenshot at the gameplay camera: tiles poking under distant landscape LODs, units painted by
   decals, grass through the goo, seams.
9. Apply the 3060 factor (section 8) and compare with the budget. Record results in this document and in
   `Source/Docs/Performance/OptimizationLog.md`.
10. Before shipping, repeat steps 1-8 on the three perf audit maps (Ger_FirstMission, Ger_MarderStugMission,
   SM_Desert_01; views in `Source/Tools/PerfAudit/run_ab_all.sh`) and at every `rts.PurpleDeath.Quality` level, and
   once on a real RTX 3060.

### Pass criteria

- S1 vs S0: < 0.02 ms GPU, < 0.01 ms CPU.
- S2 vs S3: tile cost (BasePass, prepass, velocity, shadows if on) minus the landscape BasePass saving is at most
  +0.1 ms (after the 3060 factor). Above that, build the landscape material function alternative and measure it on the
  same views, including views without creep (where it still costs and the tiles do not); keep the cheaper one.
- Tile fit: no tile vertex below the drawn landscape in any view.
- S4, S5, S6: whole system <= 1.0 ms GPU (after the 3060 factor).
- S7, S8, S11: average CPU <= 0.05 ms per frame, no frame above 0.5 ms, uploads <= 64 KB per frame on average.
- S10: exposure checks and behaviours together <= 0.05 ms per frame on average; log shows basic infantry dying in
  about 4-6 s in full growth and hazmat infantry taking 0 damage.
- No regression above 0.05 ms in `ShadowDepths` or the velocity pass.
- Shader instruction counts for each `M_PurpleDeath_*` material (every permutation) recorded here, checked with
  `get_statistics`.
