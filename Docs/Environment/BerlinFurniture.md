# Berlin furniture (16 crushable apartment pieces, 2026-11-13)

Sixteen wrecked 1940s apartment furniture pieces for `Ger_Berlin`. Any tank (light and above) drives through a piece
and crushes it; infantry walk around it. Shells cut holes into it and damage destroys it. All sixteen share one
texture atlas and one material.

Not placed on `Ger_Berlin` yet. Everything below was built and tested headless on `RTS_ToCopySmall`.

## What exists

Content is in `/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/`:

| Folder | Assets |
|---|---|
| `Textures/` | `T_BerlinFurniture_D` (sRGB), `T_BerlinFurniture_N` (normal map, DirectX green, not flipped), `T_BerlinFurniture_ORM` (linear masks: R ambient occlusion, G roughness, B metallic). One 2048 atlas, 4 x 4 cells of 512 px, one cell per piece. |
| `Materials/` | `M_BerlinFurniture` (opaque, default lit, one-sided, 3 texture samples, `FurnitureTint` colour multiplier), `MI_BerlinFurniture` (on every piece and on the debris), `M_BerlinFurniture_WallHoles` (masked hole copy), `MI_BerlinFurniture_Holes` (two-sided, inner tint; what a hit swaps in). |
| `Meshes/` | `SM_BerlinFurn_<NN>_<Name>` (168-488 triangles, 1-3 UCX box hulls, one slot, lightmap UV 1), `SM_BerlinFurn_Debris_0..5` (copies of the ruin kit debris with `MI_BerlinFurniture`). |
| `Blueprints/` | `BP_BerlinFurniture_Base` and 16 children `BP_BerlinFurn_<NN>_<Name>`. |

Source: `D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\BerlinFurniture\` (`FBX/`, `Textures/`,
`furniture_table.json`, `READY.txt`, `Previews/`). Pivot: on the ground at the centre of the body footprint (the UCX
hulls), +X front. Loose bits beside a piece (an open door, a drawer, books) belong to the mesh but have no hull.

| Blueprint | Body (X x Y, top, cm) | Health | Crushed |
|---|---|---|---|
| `01_Wardrobe` | 61 x 130, 188 | 200 | falls over |
| `02_IronBed` | 205 x 100, 112 | 190 | leans |
| `03_KitchenTable` | 70 x 110, 71 | 150 | leans |
| `04_ChairFallen` | 80 x 102, 42 | 150 | leans |
| `05_Armchair` | 93 x 86, 92 | 150 | leans |
| `06_Sofa` | 92 x 190, 93 | 190 | leans |
| `07_Bathtub` | 84 x 179, 80 | 510 | leans |
| `08_TiledStove` | 62 x 92, 188 | 600 | falls over |
| `09_Piano` | 59 x 150, 127 | 340 | falls over |
| `10_ChestOfDrawers` | 53 x 102, 84 | 150 | falls over |
| `11_BookshelfFallen` | 180 x 90, 30 | 150 | leans |
| `12_KitchenDresser` | 57 x 116, 191 | 200 | falls over |
| `13_Desk` | 72 x 142, 76 | 190 | leans |
| `14_Washstand` | 51 x 84, 121 | 150 | falls over |
| `15_ClockFallen` | 222 x 96, 35 | 150 | leans |
| `16_Radiator` | 77 x 99, 41 | 420 | leans |

- **Health** is `max(150, suggested_health x 1.7)` rounded to 10. The table's `suggested_health` is a relative guess
  (chair 30 to tiled stove 350); the scale puts it into 150-600. `DamageReduction` is 4 on every piece. Tune
  `HEALTH_SCALE`, `HEALTH_MIN` and `DAMAGE_REDUCTION` in `furniture_blueprints_build.py`.

## How a piece works

`BP_BerlinFurniture_Base` (parent: C++ `ADestructableEnvActor`, so health is the actor's `Health` field) has three
components and the only graph. The children are graph-less.

| Component | Setup |
|---|---|
| `MainMesh` | The piece's mesh, Movable. Collision as `SetupComponentCollisions(.., bOverlapTanks true)` sets it at BeginPlay: object type Destructible; blocks Visibility, Projectile, TraceEnemy, TracePlayer; overlaps `COLLISION_OBJ_PLAYER` / `_ENEMY` (vehicle hulls) and building placement; ignores pawns; does not affect navigation. |
| `NavBlock` | Hidden box over the body footprint, at least 80 cm tall: dynamic obstacle with `RTS_NavArea_BlockChar`. |
| `WallHoleDamage` | `UWallHoleDamageComponent` on `MainMesh`: NoGrid, `DamagedMaterials[0] = MI_BerlinFurniture_Holes`, the six furniture debris meshes, 0.3 cm hole radius per mm of calibre (a 75 mm hit bites about 22 cm), max 40, min 6, 900 cm2 per debris piece, 3 pieces per hit, 14 death pieces at 35 cm spacing, `bCollapseIntoChunksOnDeath` off. |

- **Graph** (variables `CrushDestructionType` = AnyTank, `CrushTipSpeedScale`, `CrushSinkDeltaZ`, `CrushSinkTime`):
  - `BeginPlay` -> `SetupCrush`: `SetupComponentCollisions([MainMesh], [], true)` ->
    `ConfigureVerticalCollapse(MainMesh, CrushTipSpeedScale, Cue_Collapse_Tree, Att_Collapse, SC_Collapse)` ->
    `SetupCrushDestructionOverlap(MainMesh, CrushDestructionType, VerticalCollapse)`.
  - `BP_OnUnitDies(DeathType)` -> `HandleDeath`: `Scavenging` (what a crush passes) ->
    `VerticalDestruction(DeltaZ = CrushSinkDeltaZ, CollapseTime = CrushSinkTime, bDestroyPostVerticalCollapse)`;
    `Kinetic` / `FireOrLaser` / `Radiation` -> `WallHoleDamage.CollapseIntoPieces(true)`.
  - This is the setup of the existing vertical-collapse crushables (`BP_Dest_Crushable_RusPipes1`,
    `BP_Dest_SteelTower_Base`), plus the death-type branch `BP_WpoTree` and the crushable poles use.
- **Why the hole component's death flag is off.** With it on, the component hides the mesh on every death, a crush
  included, so the vertical collapse would never show. The graph calls the collapse for damage deaths instead.
- **Crush.** Any vehicle whose nav agent is not Character crushes on overlap (`AnyTank`). Upright pieces (taller than
  1.2 x their shorter footprint side) fall over away from the vehicle in 1.7 s (tip speed 0.5) while they sink; low
  and already-fallen pieces only lean (tip speed 0.12, about 10 degrees) while they sink. The actor is destroyed 2.4 s
  after the crush (`SINK_TIME`). The sink depth is 0.6 x the longest body side + 30 cm.
- **Navigation: tanks through, infantry around.** `RTS_NavArea_BlockChar` is built into the Character navmesh only
  and excluded by `RTS_NavFilter_Characters`. The tank navmeshes do not see a piece at all, so a tank's path goes
  straight through it. The model is the existing crushable natural cover, where the nav area decides who paths
  round (`BP_Cover_Log1`: `RTS_NavArea_BlockCharLight` for a MediumOrHeavy crush). For "infantry round, every tank
  through" on small AnyTank crushables there was no exact example to copy: the dead trees have a child actor named
  `BlockChar` and the pipes and poles a `RTSNavCollisionEnvObstacle`, but their nav areas were not read. Treat the
  choice of `BlockChar` as this build's decision; it is tested (see "Test").
- **Shell holes.** As on the ruin kit: the first hit swaps slot 0 to `MI_BerlinFurniture_Holes` and writes the hole
  into custom primitive data; up to six holes; splinters fly. Rifle and MG rounds make no hole (under the 6 cm
  minimum radius); they only take health.
- **Death by damage.** 13-14 debris pieces fly, the mesh hides and loses its collision at once, the actor is
  destroyed 3 s later.

## Material

`M_BerlinFurniture` is made by `furniture_import.py` (rebuilt in place, so instances and mesh slots stay valid):
base colour = `T_BerlinFurniture_D` x `FurnitureTint`, normal = `T_BerlinFurniture_N`, AO / roughness / metallic =
`T_BerlinFurniture_ORM` R / G / B. Three samples, nothing else. It has the Niagara mesh-particle usage flag, because
the debris copies fly as Niagara mesh particles with the opaque instance.

The hole variant is made exactly as the Berlin ruin kit's (`BerlinRuinKitMaterial.md`, `WallChunkDamage.md`):
`Tools/WallChunks/make_wall_holes_material.make_holes_instance(MI_BerlinFurniture, grid_axis=None, two_sided=True,
inner_tint=True)`. That builds `M_BerlinFurniture_WallHoles` with `masked_master_for` (a masked copy plus the hole
Custom node and one noise sample, finished by `RTS.WallChunks.FinishMaskedMaster`) and the instance
`MI_BerlinFurniture_Holes` (inner darken 0.75, charred-wood inner colour). The stale hole master is deleted and
re-made on every run; the instance asset is kept, so Blueprint references hold.

## Rebuild

```
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/furniture/furniture_import.py -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/furniture/furniture_blueprints_build.py -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
```

- **`furniture_import.py`** (textures, material, hole variant, meshes, debris; idempotent):
  - stops if `READY.txt` is missing or the table's normal-map convention is not DirectX;
  - imports the FBX with the legacy importer (Interchange drops UCX hulls);
  - checks every mesh against `furniture_table.json` (hull count, size and bounds minimum within 1.5 cm; a wrong
    mirror would show there) and the slot name `M_BerlinFurniture`;
  - report: `Saved/Berlin/furniture/import_report.json`.
- **`furniture_blueprints_build.py`** (component templates, class defaults, the 16 children; idempotent, never
  deletes a Blueprint): report `Saved/Berlin/furniture/build_report.json`. It stops if the base Blueprint is missing
  or lacks its crush variables.
- **The base graph is not built by a script.** Headless Python cannot write Blueprint graphs. It was authored once
  through a UECP-enabled throwaway editor; the exact calls are in
  `Source/Tools/BerlinMap/furniture/base_graphs_uecp.json` (also for `BP_BerlinCover_Base`). Replay them only if the
  base lost its graph.
- Both scripts were run twice in a row on 2026-11-13 (second import: master rebuilt in place, "deleted stale
  M_BerlinFurniture_WallHoles, instance kept", 29 / 29 assets written, no errors); the test passed again afterwards
  (run5).
- Do not run either script while an editor has these assets loaded: a save then fails with "Error moving file".

## Test

```
# Git Bash; throwaway editor on RTS_ToCopySmall, never Ger_Berlin
sh Source/Tools/BerlinMap/furniture/furniture_run.sh <logname> 260      (about 3 minutes)
```

`furniture_headless_test.py` spawns all 16 pieces in a row (1200 cm apart), rebuilds navigation, starts PIE and
checks navigation, crushing by a light tank, real shells, debug hits and death by damage. Results:
`Saved/Berlin/furniture/test_result.json`, `run4.log`, close-ups `piece_*.png` and `after_*.png`.

### Result (run4, all checks passed; run1 gave the same behaviour)

- **Import:** 16 / 16 meshes match the table (hulls, sizes, bounds); 29 / 29 assets written; hole master masked, 36
  hole and grid parameters.
- **Setup,** read from the spawned actors: slot 0 `MI_BerlinFurniture`, object type Destructible, vehicle channels
  Overlap, Projectile and TraceEnemy Block, Pawn Ignore, nav area `RTS_NavArea_BlockChar`, AnyTank, hole material
  `MI_BerlinFurniture_Holes` on all 16.
- **Navigation,** a 10 m path across every piece: with the Characters filter 1009-1062 cm, passing 62-162 cm to the
  side of the centre (detour on all 16); with the LightTanks filter on the LightTank navmesh 1000 cm, straight, on
  all 16. After a piece was gone the Character path was 1000 cm too.
- **Crush,** Pz II Luchs (LightTank agent) ordered 20 m through the piece:

  | Piece | Crush after the order | Lean / fall | Sunk at the last sample | Actor gone |
  |---|---|---|---|---|
  | 01_Wardrobe | 5.0 s, tank 2.3 m away | 90 deg after 1.8 s | 128 cm | 7.4 s |
  | 12_KitchenDresser | 5.0 s | 90 deg after 1.8 s | 129 cm | 7.4 s |
  | 14_Washstand | 5.0 s | 90 deg after 1.8 s | 102 cm | 7.4 s |
  | 11_BookshelfFallen | 4.9 s | 8.6 deg | 125 cm | 7.3 s |

  The tank crossed within 1 cm of the centre line each time and the mesh stayed visible until the actor was gone.
  Run1 crushed 09_Piano the same way.
- **Real shells,** Pz IV G with AttackActor for 50 s:
  - 07_Bathtub: first 75 mm hit at 7.0 s, health 494 -> 276, 1 hole, slot 0 `MI_BerlinFurniture_Holes`; a second
    hit killed it;
  - 08_TiledStove: first hit health -> 327, 1 hole, same swap; a later hit killed it;
  - 09_Piano: only MG hits in this run (health 340 -> 206, no hole);
  - log: `RTS_WALL_HOLES hit ... radius=20-22 direct=1 removed=0.07-0.16 m2` four times;
  - run1: Bathtub 2 holes, TiledStove 2 holes, Radiator 1 hole, five hit lines.
- **Debug hits** on 02_IronBed (75, 37 and 88 mm): 0 -> 3 holes, slot 0 swapped, 0.16 / 0.04 / 0.22 m2 removed
  (`after_02_IronBed.png`).
- **Death by damage:** 05_Armchair and 13_Desk took lethal damage: mesh hidden, NoCollision, life span 3.0 s, actor
  gone 6 s later; `RTS_WALL_HOLES collapse ... threw=14 hide=1`. The two pieces killed by shells threw 13 and 14.
- **Untouched pieces** were all still there at the end.
- All 16 children, the base and the 18 cover Blueprints compile on a fresh headless load with no Blueprint error.

### How it looks

- The atlas reads as worn wood, enamel and iron from the RTS camera; normals and roughness come through
  (`piece_01_Wardrobe.png`).
- Holes are clean round bites with a dark inside (`after_02_IronBed.png`); on thin parts such as the bed's mattress a
  hole goes right through.

## Placing on Ger_Berlin

- **Rebuild navigation after placing.** The boxes mark their area when the navmesh is built, as the cover pieces do.
- **Pieces may stand on vehicle lanes.** Every tank class paths through them and they are gone after the first one.
  They never close a lane for vehicles.
- **Infantry treat a piece as an obstacle** until it is destroyed. Keep at least 1 m of open ground on one side of a
  piece wherever squads must pass (Character radius 35 cm), and do not plug a doorway, a breach or a stair with one.
- **Cover sockets:** keep a piece's footprint at least 60 cm away from the cover sockets of ruin and cover pieces. A
  socket inside a `BlockChar` box is off the infantry navmesh and is not published (inferred from the cover
  subsystem's `on_navigation` check; not tested with furniture).
- **Spacing:** pieces may touch each other; the nav boxes simply merge. Most pieces have loose debris or an open door
  beyond the body: compare `size_cm` with `body_size_cm` in `furniture_table.json` and leave that room.
- **Scale and yaw:** yaw is free. Keep scale at 1.0 and do not mirror (the contract; hole spheres and the nav box are
  in mesh space).
- **Where each piece fits** (against a wall, free-standing): `where_it_fits` in `furniture_table.json`; the back of a
  piece is -X.
- Spawn the Blueprints as actors. An instanced static mesh of the same mesh has no crush, holes or health.

## Open points

- **Not tested:** on `Ger_Berlin`; enemy-owned vehicles (the mesh overlaps `COLLISION_OBJ_ENEMY` and the crush rule
  only reads the nav agent type, so it should behave the same); medium and heavy tanks on furniture (the AnyTank rule
  accepts every agent that is not Character); wheeled cars (their nav agent is LightTank, so they crush too); a cooked
  build.
- **Shader cost was not measured.** The headless import runs without a renderer, so the statistics read 0. By
  construction the intact material has 3 texture samples and the hole variant 4.
- **Sounds are stand-ins.** The crush plays `Cue_Collapse_Tree` (wood) on every piece, the bathtub and the radiator
  included. The sink itself has no smoke effect.
- **Debris** is the ruin kit's six brick-chunk shapes with the furniture atlas on them. Their UVs do not match the
  atlas, so a piece shows whatever atlas area its UVs land on. It reads as splinters at RTS distance; real furniture
  splinter meshes would be better.
- **Small targets.** A tank's main gun often misses the low pieces; most damage then comes from its MG, which makes
  no holes.
- **The crushed piece keeps its collision** for the 2.4 s it sinks; shells can still hit it.
- **Tall pieces fall through the vehicle.** The fall is a plain rotation away from the vehicle, without physics.
- **No LODs.** 168-488 triangles per piece (4336 in total).
- **Tip timer after the actor is gone (read from `FVerticalCollapseOnCrushed.cpp`, not measured).** The tip runs on a
  60 Hz timer that is only cleared when the tip ends. A leaning piece is destroyed before that (2.4 s against about
  7 s), so its timer keeps ticking as a no-op until the tip time is over. The crushable pipes behave the same;
  changing it needs C++.
- **The flying debris was not looked at** in a picture; only the thrown counts in the log were checked.
- **The base parents the C++ class** `ADestructableEnvActor`, not `BP_DestructableEnvActor` as the pipes and poles
  do, so it has no `RTSNavCollisionEnvObstacle` component; its only nav effect is the `NavBlock` box.
- **The base graph exists only in the .uasset** (and as calls in `base_graphs_uecp.json`). The whole
  `Content/RTS_Survival/Blueprints/Environment/Berlin/` folder was untracked in git on 2026-11-13.
