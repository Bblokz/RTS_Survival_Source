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

- **`BP_BerlinRuin_Base`** is the shared parent: the C++ `ADestructableEnvActor` with a Movable `DefaultSceneRoot`. It has
  no graphs; it exists so that a death graph can be added once for the whole kit (see "Death").
- **`BP_BerlinRuin_<suffix>`**, one per mesh. The suffix is the mesh name after `SM_BerlinRuin_`, for example
  `BP_BerlinRuin_E1_CornerTower`. Each one has these components, all its own:
  - `StaticMesh` (Movable, like the PostApo ruins and the Gate): the kit mesh with its slot materials
    (`MI_BerlinRuin_Wall`, `MI_BerlinRuin_Rubble`).
    - Its collision is set on the template. It is what `SetupComponentCollisions` would set at BeginPlay: object type
      Destructible; blocks Visibility, Projectile, TraceEnemy and TracePlayer; overlaps BuildingPlacement; ignores
      pawns; no decals.
    - It does not affect navigation.
  - `WallHoleDamage` on `StaticMesh`: NoGrid; `DamagedMaterials[0] = MI_BerlinRuin_Wall_Holes`, and the rubble slot has
    none; debris `Debris/SM_BerlinRuin_Debris_0..5`; `bCollapseIntoChunksOnDeath` true.
  - `NavBlock_NN`: navigation boxes (see "Navigation").
- **Class defaults:** `Health`, `DamageReduction`, and `AimOffsetPoints`. The aim points sit in the tallest wall hulls,
  at about 40 % of the wall's height, at most 6.5 m, and at least 1.2 m above any rubble heap they stand on.
- **No graphs.** Headless Python cannot author Blueprint graphs, and the UECP tools may not be used on the shared editor.
  This is why the collision is on the template rather than coming from a BeginPlay call, and why the death is the
  component's own.
- **`B8_DebrisBed`** is rubble only. It uses the same destructible setup with empty `DamagedMaterials`:
  - hits never show a hole, but they throw a few chips;
  - on death it collapses and frees the navmesh.

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
  - A death removes them when the actor is destroyed, 4.5 s after the death. They are runtime modifiers, so only the
    tiles under the ruin are rebuilt.
    - In the test the navmesh was free when checked 9 s after the death.
    - A 2 s poll first saw it free 2-4 s after the death. That poll's clock is the script's summed tick time, which
      runs behind game time when the editor is slow, so read it as "within about 5 s".
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

## Death

`bCollapseIntoChunksOnDeath` (no graph):
- the remaining surface flies off as up to 25-120 pooled Niagara pieces;
- the mesh hides and loses its collision at once, rubble mound included;
- the collapse sound plays;
- the actor is destroyed 4.5 s later.

This is the cheap Soviet-wall death, not the PostApo sink.

**To get the sink**, add it once in `BP_BerlinRuin_Base` (needs the editor's graph tools), then set
`bCollapseIntoChunksOnDeath = false` in `setup_holes_template` and re-run `run()`. The graph:

1. `BP_OnUnitDies`.
2. `GetComponentByClass(WallHoleDamage).CollapseIntoPieces(false)`.
3. `VerticalDestruction` with `DeltaZ` of about the mesh height, about 4 s, yaw 0-3 degrees, pitch/roll 2-6 degrees,
   **`bDestroyPostVerticalCollapse` true**, and a `CollapseFX` of `NS_RTSDebrisExplosion_Huge`.

`bDestroyPostVerticalCollapse` must be true; PostApo HighRise2 has it false, which leaves a dead actor under the ground.
The nav boxes then sink with the actor; that is moving dynamic obstacles, so the tiles rebuild during the sink.

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
- **No geometry collections**, no `RTSNavCollisionEnvObstacle`, no Blueprint graphs.

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
  - takes more pictures, kills E3, E5, B7 and B8, and checks the navmesh where they stood.
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
  - **Death:**
    - The whole ruin, mound included, vanishes in one frame. 100-120 small pieces (at most 82 cm) burst outward and
      shrink away; there is no dust and nothing stays.
    - It reads as "popped" rather than "collapsed", worst on E3/B7, where a 3 m mound vanishes. The sink (see
      "Death") or a rubble decal or mesh left behind would fix it.

## Known weak spots

- **Death:** it pops, with no sink, dust or rubble left (see above). This is the main gap. It needs one graph in
  `BP_BerlinRuin_Base`.
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
