# Berlin rubble cover (natural cover with shell holes, 2026-11-13)

Ten low pieces of city wreckage for `Ger_Berlin` that infantry take cover behind. They are natural cover, like the farm
tractors and sandbag walls, and they take the Berlin ruin kit's procedural shell holes. They share the kit's material
(`MI_BerlinRuin_Wall` holed, `MI_BerlinRuin_Rubble` never holed).

Content is in `/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/`:
- `Meshes/SM_BerlinCover_<NN>_<Name>` holds the mesh, its UCX hulls and its cover sockets.
- `BP_BerlinCover_<NN>_<Name>` is one Blueprint per piece. Its parent is `BP_BerlinCover_Base`, a graph-less child of
  `BP_NaturalCover`.

## Pieces

+X (local) is the threat side of the one-sided pieces. That side has the plastered street face; the sockets stand on
-X, facing +X. Sizes are the mesh bounds in cm (X by Y, top height).

| Blueprint | Size, top | Tris / hulls / nav boxes | Sockets | Protects | Health / reduction |
|---|---|---|---|---|---|
| `01_WallRun`: waist-high broken wall run with rubble at its front foot | 256 x 570, 132 | 368 / 2 / 2 | 4 crouch in a row | +X | 1200 / 8 |
| `02_CornerL`: L-shaped corner stub, 1.85 m at the corner | 544 x 629, 185 | 334 / 6 / 5 | 3 crouch facing +X, 2 crouch facing +Y | +X and +Y (corner) | 1400 / 8 |
| `03_HeapChunk`: collapsed-wall heap with a standing 2.3 m chunk (window hole) | 404 x 550, 228 | 473 / 2 / 2 | standing_right + standing at the two chunk edges | +X (tall) | 1800 / 8 |
| `04_ToppledSlab`: wall slab with a window, toppled 36 deg onto a heap | 312 x 500, 141 | 368 / 2 / 2 | 3 crouch under the leaning slab | +X | 1100 / 8 |
| `05_FallenChimney`: 95 cm brick stack lying askew, with its 1.9 m stump | 342 x 686, 192 | 341 / 3 / 2 | 3 crouch on -X, 2 crouch on +X, standing_right at the stump end | +X and -X (two-sided), plus the +Y end | 1300 / 8 |
| `06_Breastwork`: 120 deg crescent of piled bricks, 1.0-1.15 m | 308 x 594, 118 | 450 / 5 / 4 | 4 cargo (fire-over, like the sandbag walls), radial | +X arc | 1000 / 8 |
| `07_Doorway`: broken doorway (2 m door), 2.2 m jambs, stone stoop | 225 x 550, 206 | 314 / 5 / 5 | standing + standing_right at the jambs, crouch at both low ends | +X; the door stays open to walk through | 1400 / 8 |
| `08_FacadePanels`: three staggered fallen facade panels, 7.6 m | 270 x 760, 131 | 344 / 5 / 3 | 6 crouch, 1 prone behind the gap heap | +X (staggered line) | 1700 / 8 |
| `09_CellarLip`: cellar-wall lip round a rubble pit, 2.2 m breach on one long side | 660 x 614, 148 | 472 / 7 / 6 | 7 crouch inside, facing out on all four sides | all round | 1600 / 8 |
| `10_MasonryBlock`: 1.35 m thick masonry block with brick spill | 275 x 455, 144 | 339 / 2 / 1 | 2 crouch on -X, 2 on +X, 1 crouch on the -Y end, 1 prone behind the +Y spill | all round (small) | 1500 / 10 |

- **Walls.** Every covering part is slot-0 masonry: a closed two-sided solid 45-135 cm thick (the chimney stack 95 cm,
  the block 135 cm). The heaps are slot 1.
- **Covering heights.** Crouch and cargo cover is 0.93-1.45 m. Standing cover is at least 1.5 m: the 03 chunk is
  1.95-2.3 m, the 07 jambs about 2.1 m and the 05 stump 1.7-1.9 m.
- **Sockets** follow the existing natural cover:
  - feet at z = 0, 45 cm outside the masonry (47-62 cm under the toppled slab);
  - +X of the socket points at the cover;
  - standing points sit 22 cm inside the edge they peek past, and facing the cover a soldier's right is Up x Forward;
  - prone points are about 90 cm from low cover;
  - sockets of one type facing the same way are at least 100 cm apart (the cover system merges them under 90).
- **Vertex colour** (R soot, G plaster loss, B dust) is painted per piece:
  - 01, 04 and 08 keep mostly plastered street faces;
  - 02 and 09 are sooty inside;
  - 05 and 06 are bare brick;
  - 10 is part plaster.
- **Sheets.** Overview: `D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\BerlinRuinKit\Model\Cover\Previews\cover_overview.png`,
  seen from the socket side, with a figure per socket coloured by type. Also in `Previews/`: `v2_front`, `v2_vcol`,
  `v2_plan` and `v2_street`.

## How cover and holes live on one actor

- **Cover.** `BP_NaturalCover` is in `M_SocketCoverActorClasses`. Its native parent is `ADestructableActorHealth`, so
  every child registers the sockets of `MainMesh` as cover points at map start and drops them when the actor is
  destroyed.
- **Holes.** `ADestructableActorHealth::TakeDamage` applies the hit to `HealthComp`, then forwards it to the hole
  components (`ForwardDamageToHoleComponents`) before the death check. Its BeginPlay hands the max health to the hole
  component. No C++ change was needed.
- **Hole component** (`WallHoleDamage`), on `MainMesh` (`WallMeshComponentName = MainMesh`):
  - NoGrid, outset 0;
  - `DamagedMaterials[0] = MI_BerlinRuin_Wall_Holes`, nothing on the rubble slot;
  - kit debris `SM_BerlinRuin_Debris_0..5`;
  - 0.6 cm radius per mm of calibre, max 90, min 10, so a 75 mm hit bites about 45 cm, not the whole low wall;
  - 3000 cm² of face per debris piece, 3 pieces per hit, 45 death pieces.
- **Death by damage.** The hole component's `CollapseIntoPieces`, as `BP_BerlinRuin_*` (since 2026-11-13 called by
  the base graph for the ten crushable pieces, see "Crushing"; the heaps still use `bCollapseIntoChunksOnDeath`):
  - the pieces fly, the mesh hides and loses its collision at once;
  - the actor is destroyed 4.5 s later, which removes the cover points and the nav boxes.
- **Health** sits on the child's `HealthComp` override (MaxHealth = CurrentHealth, flat `DamageReduction`).
  `BP_NaturalCover`'s own templates are checked before and after every build and are never saved.
- **Collision.** `MainMesh` carries what `BP_NaturalCover`'s BeginPlay sets anyway
  (`SetupComponentCollisions(MainMesh)`, Overlap Tanks off):
  - object type Destructible;
  - blocks Projectile, TraceEnemy, TracePlayer and Visibility;
  - ignores pawns;
  - does not affect navigation.
- **Vehicles: crushed by the heavier classes, see "Crushing" below.** `NavBlock_NN` boxes are hidden dynamic obstacles,
  one per UCX hull over 40 cm tall. They are min-area rectangles, yawed for diagonal walls. Flat rubble beds stay
  walkable: the pit floor of 09 and the spill of 10. Until 2026-11-13 the boxes were `NavArea_Null` (everything
  pathed around); the heaps (`BP_BerlinCoverHeap_*`) still are.

## Crushing (2026-11-13)

The ten pieces are crushable, the heaps are not. The rule is per Blueprint (`CRUSH` in `cover_build.py`):

| Crushed by | Pieces | Nav area of the `NavBlock_NN` boxes | Who paths around |
|---|---|---|---|
| Medium and heavy tanks (`MediumOrHeavyTank`) | 01_WallRun, 02_CornerL, 04_ToppledSlab, 06_Breastwork, 08_FacadePanels | `RTS_NavArea_BlockCharLight` | infantry, light tanks |
| Heavy tanks (`HeavyTank`) | 03_HeapChunk, 05_FallenChimney, 07_Doorway, 09_CellarLip, 10_MasonryBlock | `RTS_NavArea_BlockCharLightMedium` | infantry, light and medium tanks |
| Nobody | `BP_BerlinCoverHeap_*` | `NavArea_Null` | everyone |

- **Why these nav areas.** Units query their navmesh with a per-agent filter
  (`/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_*`). `BlockCharLight` is excluded by the Characters and
  LightTanks filters only; `BlockCharLightMedium` by Characters, LightTanks and MediumTanks. Both existed already:
  `BP_Cover_Log1` (MediumOrHeavy) and `BP_Cover_DeadTreeLog_01` (HeavyTank) use them the same way. No nav asset was
  added. A box is now a nav *area*, not a hole: an unfiltered query walks straight through it.
- **The graph lives in `BP_BerlinCover_Base` only** (the ten children and the heaps stay graph-less). It was authored
  once through a UECP-enabled throwaway editor (`synthesize_graph`; the exact calls are kept in
  `Source/Tools/BerlinMap/furniture/base_graphs_uecp.json`); headless Python cannot write graphs and
  `cover_build.py` never touches it. Variables (category Crush): `Crushable` (bool, default **false** = the "none"
  rule, what the heaps keep), `CrushDestructionType` (`ECrushDestructionType`), `CrushTipSpeedScale`, `CrushSinkDeltaZ`,
  `CrushSinkTime`.
  - `BeginPlay` -> parent BeginPlay (BP_NaturalCover: `SetupComponentCollisions(MainMesh, Overlap Tanks)`) -> `SetupCrush`:
    if `Crushable`: `ConfigureVerticalCollapse(MainMesh, CrushTipSpeedScale, Collapse_Cue, Att_Collapse, SC_Collapse)`
    -> `SetupCrushDestructionOverlap(MainMesh, CrushDestructionType, VerticalCollapse)`.
  - `BP_OnUnitDies(DeathType)` -> `HandleCrushableDeath`: if `Crushable`: `Scavenging` (what a crush passes) ->
    `VerticalDestruction(DeltaZ = CrushSinkDeltaZ, CollapseTime = CrushSinkTime, bDestroyPostVerticalCollapse,
    NS_DestructionSmoke)`; `Kinetic` / `FireOrLaser` / `Radiation` -> `CollapseIntoDebris` =
    `WallHoleDamage.CollapseIntoPieces(true)`.
- **What `cover_build.py` sets per crushable child** (class defaults and templates, no graph):
  - `Crushable` true, `CrushDestructionType`, `Overlap Tanks` true (BP_NaturalCover's variable: MainMesh then overlaps
    the vehicle object channels `COLLISION_OBJ_PLAYER` / `_ENEMY` and raises overlap events);
  - `CrushTipSpeedScale` 0.1 and `CrushSinkTime` 3.0 (the values of `BP_Dest_Crushable_RusPipes1`: a slow lean while
    it sinks), `CrushSinkDeltaZ` = mesh top + 0.2 x half its longest side + 30 (208-313 cm);
  - the nav area on every `NavBlock_NN`;
  - `collapse_into_chunks_on_death` **off** on the hole component. With it on, the component hides the mesh on every
    death, a crush included, and the vertical collapse would never show. The graph now calls the collapse itself for
    damage deaths. The heaps keep it on.
- **Crushed look.** The piece leans away from the tank (about 11 degrees in 3 s) and sinks into the ground with a
  smoke puff and the collapse cue; the actor is destroyed 3 s after the crush. Cover points and nav boxes stay until
  then. A tank that may not crush a piece and clips it anyway (pawns do not collide with it) does nothing to it.
- **If the base loses its graph or variables**, `cover_build.py` reports it in `errors`, builds every piece not
  crushable (`NavArea_Null`, chunk death) and the game behaves as before 2026-11-13.

## Rebuild

```
D:\Blender\BlenderSoftware\5\Blender5.0.exe -b --factory-startup --python generate_cover.py   (Model/Cover: FBX, cover_sockets.json, build_info.json, .blend)
...                                             --python validate_cover.py                    (Validation_Report.json: OVERALL PASS)
...                                             --python render_cover.py -- <tag>              (Previews/)
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/ruinkit/cover_build.py -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
```

- **`cover_build.run()`** is idempotent:
  - re-imports the FBX with the legacy importer (UCX), assigns the kit instances and re-adds the sockets;
  - checks each socket (forward vector level, the facing ray meets a hull within 150 cm, at least 30 cm clear of every
    nav box);
  - creates or updates the Blueprints.
- **Report:** `Saved/Berlin/cover_bp/build_report.json` (`natural_cover_master_unchanged` must be true).
- **Tuning:** health in `PIECES`, holes in `HOLES`.

## Test

```
# Git Bash; throwaway editor on RTS_ToCopySmall, never Ger_Berlin
sh Source/Tools/BerlinMap/ruinkit/cover_run.sh <logname> 700
```

### What the test does (`cover_headless_test.py`)

- **Setup:** spawns all ten pieces in a row, yaw 90, so the threat side faces world +Y.
  - Six cover pieces get a player `Squad_Ger_Mauser` 15 m behind and an enemy `Rus_Squad_Mosin` 33 m in front.
  - Four shell pieces get a player Pz IV G 35 m in front.
  - Then it runs `RebuildNavigation` in the editor world and starts PIE.
- **Soldier health:** both sides' test soldiers get 50000 health. Without that, the squads died in the first 45 s of
  the firefight (runs 4-5).
- **Checks:**
  - every socket is a published cover point (`FindCoverPointsInRadius`, same type, within 40 cm);
  - nav-box centres are off the navmesh;
  - the player squads get `MoveToLocation` 2 m behind their piece's bounds and then take cover by themselves;
  - soldier positions are compared with the socket and exposed positions at 8, 15, 25 and 40 s; the paths into 09 and through 07, and across 01 on the HeavyTank navmesh, are queried;
  - the tanks get `AttackActor`, and the hole count and slot materials are read;
  - kills 01, 09, 04 and 08, then checks 9 s later that the actor is gone, no socket points remain and the nav boxes
    are back on the navmesh.

### Result (run6 and run7, `Saved/Berlin/cover_test/run6.log`, `run7.log`, `result.json`)

Run7 is the current geometry (2 m door, 2.2 m breach); run6 had a 1.2 m door and a 1.35 m breach.

- **Setup:** all ten pieces are right:
  - health and reduction as in the table, and `is_natural_cover` true;
  - `hole_mesh` MainMesh, the collision responses and the slot materials.
- **Sockets published:** 48/48 in both runs. Every socket logged `RTS_COVER_SOCKET ... on_navigation=1`, and none was
  skipped.
  - The navmesh offset was 0-7 cm; 10-12 cm on 04 and 10, and 18 cm on one 09 point.
- **Nav boxes:** all 32 box centres were off the navmesh while the pieces stood.
- **Paths** (run7, `find_path_to_location_synchronously`; none was partial). The Character navmesh is the default:

  | Path | Navmesh | Length | Straight line | Result |
  |---|---|---|---|---|
  | Into 09's pit from behind | Character | 1043 cm | 650 cm | goes round to the breach |
  | Into 09's pit from the breach side | Character | 650 cm | 650 cm | straight in |
  | Through 07's door | Character | 800 cm | 800 cm | straight through |
  | Across 01 | Character | 1128 cm | 900 cm | detours |
  | Across 01 | HeavyTank | 1487 cm | 900 cm | detours |
  | Through 07's door | HeavyTank | 1528 cm | 800 cm | detours: tanks don't fit the door |

- **Cover taken.** The cover AI chooses its own points, so this varies between runs.
  - `RTS_COVER_ALIGNMENT reached=entered` (LogRTSSquadUnitCover) plus soldier position against the socket, or for
    standing cover against its exposed peek spot (ExposedOffset).
  - **06 Breastwork:** one man in TrenchStandUp. Only the cargo sockets make that type, so this is certainly a socket.
    Positions matched cargo_1 at 0 cm (run7), and cargo_2/cargo_3 at 0-1 cm (run6).
  - **07 Doorway:** StandingLeft and StandingRight entered, one man each, peeking into the door. Crouch_1 at 0 cm (run6).
  - **03 HeapChunk:** StandingRight entered. The man exposed and returned to reload repeatedly with 0-1 cm alignment
    residual.
  - **01 WallRun:** all three men on crouch_2-4 at 0 cm (run6); one on crouch_1 at 0 cm (run7).
  - **09 CellarLip:** crouch entries inside the lip. Soldiers sat on crouch_4 / crouch_5 (7-36 cm), the socket on the
    -X face and the breach-side socket.
  - **05 FallenChimney:** in run7 the enemy squad's grenades and rifles killed it (6 holes, health 0) while the
    soldiers walked in. Their points were unpublished: `RTS_COVER_LEAVE ... reason=point no longer published`.
- **Real shells:**
  - after 36 s of AttackActor: run6 holes 04 3, 08 3, 10 4, 02 2; run7 holes 04 1, 08 3, 10 1, 02 3;
  - every holed piece swapped slot 0 to `MI_BerlinRuin_Wall_Holes` and kept `MI_BerlinRuin_Rubble` on slot 1;
  - grenades and rifle fire from the enemy squads also holed the cover pieces;
  - proof of real hits: the `RTS_WALL_HOLES hit ... direct=1` lines.
- **Death:**
  - at the kill the mesh hides with no collision and a 4.5 s life span;
  - 9 s later all four killed actors were gone, 0 socket points were left, and every nav-box centre was back on the
    navmesh;
  - the soldiers in cover there logged `RTS_COVER_LEAVE ... reason=point no longer published` and stayed standing where
    they were.

### How it looks

- **In the engine:** the kit material and vertex colours come through. Grey-green plaster patches on the street faces,
  red brick on the tops, ends and backs, dark soot inside 09 and 02.
- **Rubble:** heaps read pale and flat-shaded from the RTS camera, the same as the kit's B beds.
- **Holes:** dark, scorched bites. On 10 several hits merged into one big hole through the block. On low walls a hole
  often bites down from the top, so the wall reads as notched rather than holed.
- **Death:** like the kit, the piece pops: small debris bursts out and nothing stays behind.

### Crush test (`cover_crush_headless_test.py`, `cover_crush_run.sh`, 2026-11-13)

```
sh Source/Tools/BerlinMap/ruinkit/cover_crush_run.sh <logname> 560     (about 2 minutes)
```

Six lanes on `RTS_ToCopySmall`. Each vehicle starts 24 m in front of its piece and gets `MoveToLocation` 24 m behind
it, straight through the piece. Before the orders a 12 m path across every piece is queried on each agent's navmesh
with that agent's filter. Results: `Saved/Berlin/crush_test/result.json`, `run3.log`, `crush_C_tall_heavy.png`.

Run3 (all six lanes passed; run2 gave the same behaviour):

| Lane | Piece (rule) | Vehicle | Result |
|---|---|---|---|
| A | 06_Breastwork (MediumOrHeavy) | Pz IV G, medium | drove through at 1-2 cm off the centre line; crush 4.7 s after the order with the tank 2.8 m away; piece sank 191 cm in 2.1 s (mesh visible throughout), actor gone 2.4 s after the crush |
| B | 01_WallRun (MediumOrHeavy) | Pz II Luchs, light | pathed round, 3.9 m to the side; piece alive, not touched |
| C | 03_HeapChunk (Heavy) | Tiger H1, heavy | drove through; crush at 4.0 s; sank 307 cm, actor gone at 6.4 s |
| D | 10_MasonryBlock (Heavy) | Pz IV G, medium | pathed round, 2.4 m to the side; piece alive |
| E | BP_BerlinCoverHeap_Brick (none) | Tiger H1 | pathed round, 8.1 m to the side; heap alive (control) |
| F | 04_ToppledSlab (MediumOrHeavy) | Tiger H1 | drove through; crush at 4.2 s; actor gone at 6.6 s |

- **Paths before** (straight = 1200 cm): on A, B and F the LightTank path was 1471-1511 cm and the Character path
  1330-1376 cm, MediumTank and HeavyTank 1200 cm. On C the LightTank, MediumTank and Character paths were 1567, 1661
  and 1384 cm, HeavyTank 1200 cm. On D only HeavyTank was straight. On E every agent detoured (1638-2114 cm).
- **After the crush** every agent's path across A, C and F was 1200 cm, and their cover points were gone (4/4 -> 0/4,
  2/2 -> 0/2, 3/3 -> 0/3). B, D and E kept all their points (4/4, 6/6, 8/8).
- **Lean.** 0.2 degrees after 0.4 s in run2, as the 0.1 speed predicts. The 20-30 degrees in the run3 samples come
  from the screenshot hitch: the tip runs on wall-clock time.
- The queries log one engine ensure (`NavFilter_AIControllerDefault`: the querier is not an AI controller); the
  filter's own area list is still applied, as the detours show.

### Regression after the crush change (run8, `Saved/Berlin/cover_test/run8.log`, `result_run8_crushable.json`)

`cover_headless_test.py` now queries with the Characters filter (HeavyTanks for the heavy navmesh). Run8:

- sockets published 48/48; every nav-box centre is unwalkable for infantry (0/32) while the pieces stand;
- paths: across 01 Character 1128 cm (detour), HeavyTank 900 cm (straight: it would crush it); through 07's door
  Character 800 cm, HeavyTank 800 cm (crush); into 09's pit as before (1043 / 650 cm);
- infantry in cover at 40 s: 06 three men on cargo_2-4 at 0 cm, 01 two on crouch_2-3 at 0 cm, 09 on crouch_4-5,
  03 / 07 at standing points, 05 on crouch_2;
- real shells: holes 04 2, 08 3, 10 1, 02 2, slot 0 swapped to `MI_BerlinRuin_Wall_Holes`, 59 `RTS_WALL_HOLES hit`
  lines;
- death by damage: `RTS_WALL_HOLES collapse ... threw=11-19 hide=1` for seven pieces (three died under fire, four
  were killed); mesh hidden, no collision, life span 4.5 s; 9 s later actor gone, 0 socket points, nav boxes free.

## Scattering on Ger_Berlin

- **Rebuild navigation** after placing; the boxes are cut in at build time, as for the ruin kit.
- **Crushable pieces and lanes:** see "Crushing". Do not rely on 01, 02, 04, 06 or 08 to stop medium tanks, or on
  any of the ten to stop heavy tanks; use heaps or ruin pieces where a lane must stay closed.
- **Spacing:** keep at least 3 m of open ground between a piece's socket side and anything else that blocks
  navigation. Keep at least 1.5 m between pieces, so the sockets (45 cm out) stay on the navmesh and squads can walk
  in. Do not let another piece or ruin cover a socket side.
- **Scale and yaw:** yaw is free. Scale 1.0-1.15 only, as for the kit fillers: the walls must stay at least 45 cm
  thick, and mirroring breaks the holes.
- **Streets:** 01, 07 and 08 along kerbs, facing up or down the street. 04 and 10 as single blocks in the carriageway
  (10 is all-round). 05 lengthwise in side streets (two-sided).
- **Courtyards and squares:** 02 in corners, facing the entrance. 09 as a strongpoint (all-round). 06 facing an
  approach.
- **Blast zone, open rubble fields:** 03, 05, 09 and 10. These read as the wreckage of something larger; 03's chunk is
  the tallest silhouette at 2.3 m.

## Weak spots

- **Which socket a soldier takes is the cover AI's call.** It scores by distance and angle to the threat, and scanned
  cover near a piece competes with the sockets. In the runs, squads used some sockets of each piece, never all of
  them. Every socket is proven published and on the navmesh.
- **Pieces die fast to infantry.** One Mosin squad's grenades and rifles killed 05 (1300 health, reduction 8) within
  about a minute in run7, and 01 in run4. Raise `DamageReduction` in `PIECES` if cover should outlast a rifle squad.
  The ruin kit uses 5-25.
- **Death pops.** Nothing is left and there is no sink. That needs a graph in `BP_BerlinCover_Base`; see "Death" in
  `BerlinRuinKit.md`.
- **Cover after death.** The mesh hides at once and the points go when the actor is destroyed 4.5 s later. The
  soldiers then leave cover but stay standing where they were.
- **Crushing is by class, not by speed or direction.** A medium tank ordered through a heavy-only piece paths round
  it; 07's door is too narrow for tanks, so only heavies go "through" it (by crushing it).
- **Lanes.** A crushable piece on a lane no longer closes it for the classes that crush it, and it is gone after the
  first such tank. Infantry and lighter vehicles still need a way round.
- **The crushed piece keeps its collision while it sinks** (3 s): shells still hit it and its cover points are still
  published for that time.
- **Tip timer after the actor is gone (read from `FVerticalCollapseOnCrushed.cpp`, not measured).** The lean runs on
  a 60 Hz timer for 8.5 s (speed 0.1) and is only cleared when it ends; the actor is destroyed after 3 s, so the timer
  ticks as a no-op for the rest. `BP_Dest_Crushable_RusPipes1` behaves the same; changing it needs C++.
- **Hulls are approximate.** The heap hulls are low, and the toppled slab and the leaning panels use their convex
  hull, so a shell may bite air just behind them.
- **Rubble hits** spend a hole slot and show no hole, the same as the kit.
- **One hole setting for every piece.** A 150 mm hit (90 cm cap) can notch a 1.1 m wall to the ground.
- **The test map shoots back.** On `RTS_ToCopySmall` stray fire lands on every piece, and the test soldiers need extra
  health to survive the test.


## Rubble heaps as cover (2026-11-13, later)

The loose rubble heaps of Ger_Berlin are natural cover too. `ruinkit/heap_cover_build.py` (headless commandlet) builds
seven Blueprints `BP_BerlinCoverHeap_<Name>` (children of `BP_BerlinCover_Base`) on copies of the heap meshes
(`Cover/Meshes/SM_BerlinCoverHeap_<Name>`; the shared kit meshes are untouched):

| Name | Source mesh | Size, top (cm) | Prone sockets | Health / reduction |
|---|---|---|---|---|
| Brick | DBK_Brick_Rubble_A | 578 x 700, 97 | 8 | 900 / 8 |
| Block | DBK_Cinderblock_Rubble_A | 660 x 826, 112 | 7 | 900 / 8 |
| Concrete_A / _B / _C | DBK_Concrete_Rubble_Mid_A / _B / _C | 764 x 680, 146 / 484 x 446, 63 / 458 x 548, 89 | 4 / 5 / 6 | 1000 / 8 |
| Collapse_S | SM_BerlinFoot_CollapseHeap_S | 877 x 948, 203 | 8 | 1600 / 10 |
| Collapse_L | SM_BerlinFoot_CollapseHeap_L | 1278 x 1432, 324 | 8 (4 published) | 2200 / 10 |

- **Sockets:** `prone_<n>` all round the foot, 50 cm outside the heap body and its nav boxes, facing the heap, at least
  1.5 m apart. A heap is low cover from every side, so every point is a lying point.
- **Destructible, no shell holes:** health on `HealthComp`; the hole component only runs the death (collapse into kit
  debris, actor gone after 4.5 s, cover points and nav boxes freed). `DamagedMaterials` is empty: a heap has no wall to
  punch through. The hole counter still counts hits; nothing is drawn.
- **On the map:** `step_dress()` spawns a cover actor for every kit heap of scale 0.8 or more (`BER_HeapCover_*`, 211);
  smaller heaps stay instanced dressing. `step_feet()` spawns every collapse heap as a cover actor
  (`FOOT_HeapCover_*`, 175). Actors are scaled like the instance was, so socket distances scale with them.
- **Test** (`heap_cover_headless_test.py`, `heap_cover_run.sh`, results `Saved/Berlin/heap_cover_test/`): all seven are
  natural cover; sockets published 8/8, 4/4, 8/8, 4/8 (Collapse_L), 5/5, 7/7, 6/6; squads lay on prone points of Brick
  and Concrete_A under fire; killed Brick, Collapse_S and Block were gone 9 s later with 0 cover points left and their
  nav boxes back on the navmesh. Tested on the unit-test map, not on Ger_Berlin.
- **Open:** four corner sockets of Collapse_L are not published (cause not looked into); vehicles are blocked, not
  crushing (same as the other cover pieces).
