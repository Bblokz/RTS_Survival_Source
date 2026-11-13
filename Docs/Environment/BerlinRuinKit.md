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
  - on death it sinks with dust like the others, leaves no heap, and frees the navmesh.

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
  - A death destroys them in its first frame (the death graph), before the ruin starts to sink: one tile rebuild,
    not one per sink step. They are runtime modifiers, so only the tiles under the ruin are rebuilt. The collapse
    heaps that appear a second or two later bring their own `NavArea_Null` boxes.
    - Before 2026-11-13 (chunk death) they went with the actor, 4.5 s after the death.
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

## Death (2026-11-13: sink, dust, collapse heap, remnant)

A ruin that dies sinks into the ground inside a dust cloud and leaves the collapse heap that was modelled for its
footprint. The heap is a cover Blueprint, so a destroyed ruin becomes new cover (but see "Cover of the spawned heaps").

**Large ruins do not vanish** (owner, 2026-11-13). Their sink stops with 55-65 % of their height in the ground and a
tilt of a few degrees, and the dead actor stays as a remnant. Small pieces sink completely and are destroyed. The
split and the values are one table, `REMNANTS` in `ruin_blueprints_build.py`, repeated under "Large and small".

- **Where it lives:** one graph in `BP_BerlinRuin_Base`; the 22 children have none. The values it reads are class
  defaults that `ruin_blueprints_build.py` sets per child (`DEATH`, `death_defaults()`), from the mesh size, the size
  class and `footprint_table.json`.
- **The graph was authored through UECP** (`synthesize_graph`) in a throwaway editor. The calls are kept in
  `Tools/BerlinMap/ruinkit/ruin_death_graph_uecp.json`, in replay order, with the pitfalls that were hit.

### What happens

`BP_OnUnitDies` (any death type; the ruins are not crushable) -> if `SinkOnDeath`:

1. **`StartRuinDeath`**, in the frame of the death:
   - stores the actor transform (`DeathStartTransform`): the sink moves and tilts the actor, and the heaps and the dust
     are placed against where it stood;
   - hides every component tagged `BerlinAmbient` or `BerlinCrowPerch` (curtains, wires, lamps, crows);
   - destroys the `NavBlock_NN` boxes. On a remnant (`LeaveRemnant`) the boxes tagged `RemnantNav` are detached
     instead (keep world): they stay at ground level and keep blocking while the actor sinks away from them;
   - `WallHoleDamage.CollapseIntoPieces(false)`: the standing surface flies off as kit debris, the mesh stays;
   - `VerticalDestruction`: the actor sinks `SinkDeltaZ` in `SinkTime` seconds with a small random tilt and yaw,
     `bDestroyPostVerticalCollapse` = not `LeaveRemnant`. Its `CollapseFX` starts the first dust cloud and plays `DeathSound`
     (`Collapse_Cue`, with `Att_Collapse` and `SC_Collapse`);
   - starts a looping timer on `DustPulse`.
2. **`DustPulse`**, every `DustPulseInterval` seconds until the actor is gone: one more dust cloud at a random spot of
   the footprint (`DeathVfxOffset` +- `DustSpreadX/Y`, local space).
3. After `HeapDustDelay`: **`DustAtHeaps`**, a dust cloud on every spot where a heap will stand.
4. `HeapDustLead` later: **`SpawnDeathHeaps`** spawns `BP_BerlinCoverHeap_Collapse_S` for every transform in
   `DeathHeapsSmall` and `BP_BerlinCoverHeap_Collapse_L` for every one in `DeathHeapsLarge` (actor-relative, composed
   with `DeathStartTransform`), inside that dust.
5. At `SinkTime`:
   - **small piece:** the C++ destroys the actor. The last dust pulses outlive it by about 2 s; when they thin, the
     heap is what is left.
   - **remnant:** the C++ stops its sink timer and calls `BP_OnVerticalDestructionComplete`, which clears the
     `DustPulse` timer. The actor stays where the sink ended.
   The footprint skirt (an instanced mesh of the map) is not touched.

Nothing ticks before the death. During it there is the C++ sink timer, one Blueprint timer and two delays. They end
with the actor or, on a remnant, at the end of the sink: a remnant has no tick and no timer.

### Large and small (`REMNANTS`)

| Piece | Side | Height | In the ground | Left standing | Tilt (pitch/roll) | Yaw | Nav boxes kept |
|---|---|---|---|---|---|---|---|
| E1_CornerTower (hero) | large | 20.8 m | 62 % = 12.9 m | 7.9 m | 2-4 deg | up to 2 deg | 7 of 11 |
| E2_Courtyard (hero) | large | 16.0 m | 60 % = 9.6 m | 6.4 m | 2-4 deg | up to 2 deg | 10 of 14 |
| E3_CoreRow (wall core) | large | 18.4 m | 60 % = 11.0 m | 7.3 m | 2-4 deg | up to 2 deg | 2 of 4 |
| B7_WallCore (wall core) | large | 11.5 m | 55 % = 6.3 m | 5.2 m | 2-4 deg | up to 2 deg | 1 of 1 |
| B3_Facade4St (lone slab) | large | 15.5 m | 65 % = 10.1 m | 5.4 m | 3-5 deg | up to 3 deg | 4 of 4 |
| B4_Firewall (lone slab) | large | 16.7 m | 65 % = 10.9 m | 5.9 m | 3-5 deg | up to 3 deg | 4 of 4 |
| B5_CornerL (slab corner) | large | 11.1 m | 60 % = 6.7 m | 4.4 m | 3-5 deg | up to 3 deg | 2 of 3 |
| B1, B2 (one and two storeys, 5.0-8.4 m), B6 stubs, B8 bed | small | | all of it | nothing | class value | | none |
| E4 stub rings, E5 shells (4.3-4.6 m), E6 heap fins (6 m), E7 half houses (8 m) | small | | all of it | nothing | class value | | none |

- **Where the line is:** every piece 11 m or taller is large. That is the heroes and wall cores the owner named, plus
  the three tall slabs: a sunken, tilted stump of a four-storey facade or firewall reads well (see the B3 pictures of
  the test). The E5 shells and E7 half houses were left small: 60 % of an 8 m half house leaves a 3 m rim that is
  mostly inside its own collapse heap. To move a piece, add or remove its line in `REMNANTS` and re-run the builder.
- **Tilt:** the C++ sink picks the size in the range, a random direction and the yaw sign per death, so a row of dead
  ruins does not lean alike (two runs of the same three pieces: B3 pitch 3.6 / roll -2.8, then 1.7 / 2.5; E3 2.4 / 0.8,
  then -1.3 / 3.3; E1 -3.0 / -0.4, then 2.6 / 2.0). It is random, not derived
  from the position; a deterministic lean would need a C++ change or per-actor values.
- **What a remnant is for the game:**
  - **Dead.** `IsUnitAlive` is false: more damage does nothing but lower `Health` further. Tested: 10,000 more damage,
    point damage and `TriggerDestroyActor` gave no second death, no new heap or dust, no hole, no movement. Hits are
    not forwarded to the hole component any more, so a remnant takes no new shell holes (the old ones stay).
  - **Still solid.** The mesh keeps its collision: it blocks shells, weapon traces and line of sight where it now
    stands (sunken and tilted, exactly the visible mesh).
  - **Still blocks movement, by its wall lines.** The kept nav boxes stand at ground level under the walls that still
    stick out; boxes of low hulls that went under are destroyed (a kept box is one whose hulls still stand 1.5 m or
    more after the sink). They are the upright footprints from before the death: the few degrees of tilt are not
    followed, which moves a wall top by 0.3-0.5 m at most. Test: 0 of 19 / 22 / 49 points in the kept boxes of B3 /
    E3 / E1 were on the navmesh, and the path across each remnant was as long as before the death.
  - **No cover sockets** (the ruins never had any). The heap inside or beside it is the cover.
  - **Still accepts an attack order.** `AttackActor` on a remnant returns no error (the command does not ask whether
    an environment actor is alive), so a player can waste shells on it. Refusing it is a C++ change in the attack
    command; switching the weapon trace channels off on the mesh would stop it too, but then shots would fly through
    the walls that are still visible.
- **Heap and remnant:** the heap spots of the table lie inside the wall ring (E1, E2, B5) or along the middle of the
  row (E3, B7), so the heap fills the shell and the walls stand around it. No offset was changed. On E3 and B7 the
  heap's foot is wider than the row is deep and spills 0.5-1 m through the foot of the front wall; it reads as rubble
  that ran out through the ground floor (picture `death_E3_CoreRow_after.png`). Seen for all six large pieces with a
  heap: E1, E2 and B5 hold their heap inside the walls; E3's two heaps fill the shell. **B7 is the weak one:** it is a
  single wall line, and its heap (scale 0.9, centred on the piece) lies astride it, so the 5 m wall stump stands up
  through the middle of the mound (`death_B7_WallCore_remnant_plus10s.png`). It reads as a wall in its own rubble, not
  as a heap inside a wall, but if it should lie in front, give B7 its own heap offset in the builder (about +350 cm
  in x, towards the street) instead of the table's centre; the heap then no longer sits centred on the plot's skirt.
- **Cost of remnants:** a remnant is the dead actor with its mesh, the hole component (idle) and the kept boxes; no
  tick, no timer. It costs what the living ruin cost to draw, with less of it on screen.

### The dust

`NS_RTSDebrisExplosion_Huge`: a one-shot grey-brown cloud with a few flying stones, about 6.5 m across at scale 1,
gone after about 2.5 s. It is the only effect; the size of the event comes from its scale, how far the pulses are
spread and how long the sink lasts.

- The white `NS_DestructionSmoke` / `_Large` (what the crushed cover pieces use) reads as steam on these ruins; it was
  tried first and dropped.
- The `NS_RTS_GroundDust*`, `NS_RTS_DustSmoke_07` and `NS_RTS_StaticDust_22` systems loop: spawned once they never
  end. Do not put a looping system in `DeathDustVfx`.
- Contact sheet of the candidates: `ruin_death_run.sh <log> 240 fx` (`ruin_death_fx_probe.py`), pictures
  `Saved/Berlin/ruin_death_test/fx_<group>_<t>s.png`.

### Per class and per piece

| Class | Sink time | Tilt / yaw | Heaps appear at | Dust pulse every | Dust scale (min-max) |
|---|---|---|---|---|---|
| hero | 6.0 s | 1-2.5 / 1 deg | 1.8 s | 0.45 s | 1.2-2.2 |
| core | 5.0 s | 1-3 / 1 deg | 1.5 s | 0.45 s | 1.2-2.2 |
| slab | 4.0 s | 2-4 / 2 deg | 1.2 s | 0.5 s | 0.9-1.6 |
| filler | 3.5 s | 2-5 / 2 deg | 1.05 s | 0.5 s | 0.9-1.5 |
| stub | 2.5 s | 2-6 / 3 deg | 0.9 s | 0.5 s | 0.7-1.1 |
| rubble (B8) | 2.5 s | 0-2 / 0 deg | - | 0.5 s | 0.7-1.2 |

- **Sink depth of a small piece:** (mesh top + 80 cm) x 1.15, so a filler placed at scale 1.15 still goes under:
  224 cm (E4 stub ring) to 1063 cm (B2). The depth is not scaled with the actor. Large pieces: see the table above;
  they keep their class's sink time, so they sink slower.
- **Dust scale:** a cloud about 75 % of the piece's shorter ground side, clamped per class. Pulses are scattered over
  +-32 % of each ground side. E1: scale 2.14, about 13 clouds alive at once; a stub: 0.7-1.1, about 6.
- **Heaps** (from the `dead` entries of `footprint_table.json`; x, y in cm in the piece's local space, yaw added to the
  piece yaw, uniform scale; the dust on a heap spot is 1.6 x scale for S and 2.3 x scale for L):

| Piece | Heaps |
|---|---|
| E1_CornerTower | L at (150, -150), yaw 180 |
| E2_Courtyard | L at (620, 0), yaw 270; S at (-650, 560), yaw 0 |
| E3_CoreRow | L at (0, 750), yaw 270; L at (0, -750), yaw 270 |
| B7_WallCore | L at (0, 0), yaw 270, scale 0.9 |
| B5_CornerL | S at (0, 0), yaw 180 |
| E4_StubRing_A/B, E5_Shell1St_A/B, E6_HeapFin_A/B | S at (0, 0), yaw 180 |
| E7_HalfHouse_A/B | S at (0, 0), yaw 180, scale 0.8 |
| B1, B2, B3, B4 (lone slabs), B6 stubs, B8 debris bed | none |

- **Pieces without a heap** (B1-B4, B6, B8) only sink in their dust; B1, B2, B6 and B8 are gone, B3 and B4 leave
  their stump. On a full plot they stand on a B8
  bed inside a footprint skirt, so the plot keeps its rubble; the bed has its own health and dies separately. The
  table's `B_PLOT` / `B_PLOT_STUB` heaps belong to a whole plot, not to one actor, so no single piece spawns them: a
  plot whose slabs and bed are all shot away ends as its skirt. If that should leave a heap, give the B8 bed the
  plot's heap (it is the plot's anchor in the table); the builder would need to know which bed stands on a stub plot.

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
  (`RemoveSocketCoverOfDestroyedActors`). The sockets are projected onto the navmesh at registration; the ruin's own
  nav boxes are gone by then (step 1), but the navmesh tiles may still be rebuilding, so the call should use the retry
  that `RefreshCoverSocketsOfActor` has for points that are off the navmesh.
- No Blueprint-only detour was built (for example child heaps with an `RTSCoverProviderComponent` holding copies of
  the socket points): it would double the heap Blueprints for something one function fixes.

### Other limits

- **Heap tint:** the map's heaps get the lot's tint as custom primitive data. A spawned heap has the material's
  default tint; Blueprint cannot read the ruin's custom primitive data to pass it on.
- **The sink is a lift.** The mesh goes down in one piece. The dust hides most of it from the game camera, the
  thrown debris helps, but a close look shows a building sliding into the ground.
- **Collision during the sink:** the sinking mesh still blocks shells and traces until the actor is gone (a
  remnant's keeps blocking).
- **Pure-node loops:** `K2_GetComponentsByClass` is const, so its Blueprint node is pure and a `ForEach` re-evaluates
  it on every pass. Destroying components in such a loop skips every second one. The graph copies the boxes into
  `DeathNavBoxes` first.

### Changing the death

- **Values** (times, scales, heaps): edit `DEATH` and the constants around it in `ruin_blueprints_build.py`, re-run
  `run()`. No graph work.
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

## Death test (`ruin_death_headless_test.py`, 2026-11-13)

- **Run:** `Tools/BerlinMap/ruinkit/ruin_death_run.sh <logname> [timeout_s]`: a throwaway editor on `RTS_ToCopySmall`
  (never Ger_Berlin), log, `result.json` and pictures in `Saved/Berlin/ruin_death_test/`. About 6 minutes.
- **Placed before play** (yaw 90, then `RebuildNavigation`): E1_CornerTower, E3_CoreRow and B3_Facade4St (large),
  E7_HalfHouse_A (small, heap at 0.8), E4_StubRing_A (small, heap; killed by a player Pz IV G's shells), B2_Facade2St_B
  dressed through `ambient_place.py` (curtain, lamp, four crows), and a control `BP_BerlinCoverHeap_Collapse_S` placed
  like a map heap.
- **Per death** (`apply_damage`, the TakeDamage path; the stub by real shells): samples every tick on the world clock
  (mesh z, tilt, nav boxes, dressing, live dust systems, heap actors), pictures before and 0.5 / 2 / 5 s after, once
  the dust has thinned, and for a remnant 10 s after that. On a remnant: more damage, point damage,
  `TriggerDestroyActor` and an attack order.
- **Afterwards:** heap transforms against `footprint_table.json` and the transform before the death, navmesh points
  and paths, cover points at the heap sockets.
- **Fresh-load check:** `ruin_death_compile_check.py` (commandlet) loads and compiles all 23 Blueprints from disk.
- **Reading the navmesh numbers:** a point count only means something when the two open-ground path ends read True
  and the box points of the living ruin read False in the same run (they do in the result below). The first two runs
  of the day used a projection call that does not exist in Python and reported every point as blocked.

### Result (run6, from disk after the install, `Saved/Berlin/ruin_death_test/run6.log`, pictures also in `run6_final/`)

| Piece | Death by | Sink (cm / s) | Ends | Tilt pitch / roll / yaw | Nav boxes at death | Heaps (at s) | Dust clouds at once |
|---|---|---|---|---|---|---|---|
| B3_Facade4St | damage | 1010 / 4.0 | remnant, z -1010 cm | 1.7 / 2.5 / 0.7 | 4 -> 4 kept | none | 8 |
| E7_HalfHouse_A | damage | 1007 / 3.5 | destroyed at 3.87 s | 1.9 / 3.2 (last sample) | 3 -> 0 at 0.13 s | S (1.41, set 1.05) | 9 |
| E3_CoreRow | damage | 1102 / 5.0 | remnant, z -1102 cm | -1.3 / 3.3 / -1.8 | 4 -> 2 kept | L, L (1.54, set 1.50) | 11 |
| E1_CornerTower | damage | 1289 / 6.0 | remnant, z -1289 cm | 2.6 / 2.0 / 0.6 | 11 -> 7 kept | L (1.86, set 1.80) | 15 |
| B2_Facade2St_B | damage | 1063 / 4.0 | destroyed at 4.01 s | -1.5 / 2.3 (last sample) | 4 -> 0 at 0.03 s | none | 8 |
| E4_StubRing_A | tank shells | 224 / 2.5 | destroyed at 2.52 s | 2.8 / 1.8 (last sample) | 7 -> 0 at 0.02 s | S (0.92, set 0.90) | 7 |

- **Sink:** the mesh z falls linearly to the set depth (samples in `result.json`). A small piece's actor is invalid at
  the sink time; a remnant's z did not change in any later sample and its rotation is the tilt in the table.
- **Effect and sound:** the first dust system was alive in the first sample after the death on every piece. That
  sample came 0.02-0.03 s after the death on three pieces and 0.13-0.40 s on B3 (the first death of the run), E7 and
  E1: the death frame itself is a hitch in this editor (in three runs E1 gave 0.02, 0.25 and 0.28 s; it throws 120
  debris pieces). Not profiled in a packaged game. `ListWaves` 0.25 s after each death listed `Collapse01` (the cue's wave) on 6 of 6 deaths (`Collapse01` or `Collapse02`, the cue picks one).
- **Heaps:** every expected heap was found with 0.0 cm location error, 0.0 degrees yaw error and the table's scale
  (E7 0.8), 0.02-0.36 s after the set time (frame steps and the hitch above). Each has its 5-6 nav boxes.
- **Remnants (E1, E3, B3):** actor valid, mesh visible with query-and-physics collision, actor tick off, no ticking
  component, no dust system left. After 10,000 damage, point damage and `TriggerDestroyActor`: same z, same hole
  count, no new heap, no dust; only `Health` went further down. The attack order returned no error (see "What a
  remnant is for the game").
- **Navigation** (Character navmesh, HeavyTank in brackets; path from 4 m in front to 4 m behind):
  - B3_Facade4St: alive 0/19 of its box points on the navmesh, path 2156 (2683) against 1360 straight. After: 0 box points free, 19 under heap or remnant boxes, 0 still blocked for no reason; heap and remnant box points on the navmesh 0/19; path 2156 (2683); open ends on.
  - E7_HalfHouse_A: alive 0/17 of its box points on the navmesh, path 1829 (2251) against 1455 straight. After: 6 box points free, 11 under heap or remnant boxes, 0 still blocked for no reason; heap and remnant box points on the navmesh 0/23; path 1753 (2261); open ends on.
  - E3_CoreRow: alive 0/28 of its box points on the navmesh, path 4729 (5333) against 2335 straight. After: 0 box points free, 28 under heap or remnant boxes, 0 still blocked for no reason; heap and remnant box points on the navmesh 0/84; path 4729 (5333); open ends on.
  - E1_CornerTower: alive 0/79 of its box points on the navmesh, path 3993 (4504) against 2901 straight. After: 15 box points free, 64 under heap or remnant boxes, 0 still blocked for no reason; heap and remnant box points on the navmesh 0/80; path 3993 (4504); open ends on.
  - B2_Facade2St_B: alive 0/17 of its box points on the navmesh, path 2155 (2678) against 1360 straight. After: 17 box points free, 0 under heap or remnant boxes, 0 still blocked for no reason; heap and remnant box points on the navmesh 0/0; path 1360 (1360); open ends on.
  - E4_StubRing_A: alive 0/37 of its box points on the navmesh, path 2360 (2881) against 1750 straight. After: 21 box points free, 16 under heap or remnant boxes, 0 still blocked for no reason; heap and remnant box points on the navmesh 0/28; path 2094 (2642); open ends on.
- **Cover:** control heap 8 of 8 sockets published at the start and at the end; spawned heaps 0 of 8 each (see
  "Cover of the spawned heaps").
- **Dressing:** 3 of 3 tagged components visible before the death of B2, all hidden in the first sample (0.03 s).
- **Small pieces gone:** no `BP_BerlinRuin_*` actor was left except the three remnants.
- **Holes still work:** `ruin_holes_run.sh run7_after_death` after the change: 19 direct shell hits; holes after 36 s of fire E1 2, E3 2, B3 5, B7 3, E5 1 (3 after debug hits), slot 0 swapped to `MI_BerlinRuin_Wall_Holes` on each; its kills left E3 and B7 as remnants that still block (path 47.2 m and 32.0 m, as alive) and removed E5 and B8 (B8's 24 of 24 box points back on the navmesh; E5 31 of 42, the rest under its new heap).
- **Pictures** (`Saved/Berlin/ruin_death_test/`): `death_<piece>_before`, `_0_5s`, `_2_0s`, `_5_0s`, `_after`, and
  `_remnant_plus10s` for E1, E3 and B3. Earlier looks are kept in `run4_vanish_all/` (everything vanishing, before the
  remnant rule) and `run5_inmemory_remnant/`; `fx_*.png` is the effect contact sheet.
- **How it looks:** a grey-brown cloud grows over the whole footprint within half a second and is fed for the length
  of the sink, with stones flying out of it; the building goes down inside it. From the game camera a hero is fully
  hidden for 5-6 s. When the dust thins (2-3 s after the sink) a small piece has left its heap; a large one stands as
  a low, leaning shell with the heap inside it. The heaps were never seen appearing: they spawn under the dust.

### Not covered, or weaker than asked

- Tested on the unit-test map only: flat ground, no footprint skirt under the heaps, orange light. Not on Ger_Berlin.
- The other four large pieces were killed in a second, shorter run (`ruin_death_run.sh run7_large2 500 large2`,
  `result_large2.json`, pictures `death_<piece>_*`): B7 z -630, E2 -962, B5 -666, B4 -1089 cm, tilts 3.5 / 3.2 / 4.2 /
  4.2 degrees, kept boxes 1 of 1, 10 of 14, 2 of 3, 4 of 4 with 0 of 12 / 77 / 10 / 17 points on the navmesh, heaps
  at 0.0 cm error, each remnant still and dead afterwards. The small B and E variants not named above were built and
  compile but were not killed in a test.
- The remnant tilt is random per death, not derived from the actor position.
- A remnant can still be given an attack order; spawned heaps publish no cover and have no lot tint (all above).
- A ruin placed scaled up (fillers, up to 1.15) spawns its heap scaled with it; no test killed a scaled ruin.
- The death of a ruin while the game is paused, or two neighbours dying into each other's heaps, was not tried.

## Known weak spots

- **Death:** since 2026-11-13 the ruin sinks in dust and leaves its collapse heap (see "Death"). Open: the spawned
  heaps publish no cover points until the cover finder gets a registration call for actors spawned at runtime (C++),
  and they do not take the lot's tint.
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
