# Berlin furniture (32 crushable apartment pieces, 2026-11-13 and 2026-11-14)

Thirty-two wrecked 1940s apartment furniture pieces for `Ger_Berlin`: 01-16 from 2026-11-13, 17-32 added on
2026-11-14 ("Second set"). Any tank (light and above) drives through a piece and crushes it; infantry walk around
it. Shells cut holes into it and damage destroys it. All thirty-two share one texture atlas and one material.

Everything below was built and tested headless on `RTS_ToCopySmall`. This document does not track what is placed on
`Ger_Berlin`.

## What exists

Content is in `/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/`:

| Folder | Assets |
|---|---|
| `Textures/` | `T_BerlinFurniture_D` (sRGB), `T_BerlinFurniture_N` (normal map, DirectX green, not flipped), `T_BerlinFurniture_ORM` (linear masks: R ambient occlusion, G roughness, B metallic). One 2048 atlas, 4 x 4 cells of 512 px, one cell per piece. |
| `Materials/` | `M_BerlinFurniture` (opaque, default lit, one-sided, 3 texture samples, `FurnitureTint` colour multiplier), `MI_BerlinFurniture` (on every piece and on the debris), `M_BerlinFurniture_WallHoles` (masked hole copy), `MI_BerlinFurniture_Holes` (two-sided, inner tint; what a hit swaps in). |
| `Meshes/` | `SM_BerlinFurn_<NN>_<Name>`, NN 01-32 (168-496 triangles, 1-3 UCX box hulls, one slot, lightmap UV 1), `SM_BerlinFurn_Debris_0..5` (copies of the ruin kit debris with `MI_BerlinFurniture`). |
| `Blueprints/` | `BP_BerlinFurniture_Base` and 32 children `BP_BerlinFurn_<NN>_<Name>`. |
| `Sounds/` | 10 crush cues `Cue_BerlinFurn_Crush_<Group>` and their 30 waves `S_BerlinFurn_Crush_<Group>_<1..3>` (see "Crush sounds"). |

Source: `D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\BerlinFurniture\` (`FBX/`, `Textures/`,
`furniture_table.json`, `READY.txt`, `READY2.txt`, `Previews/`). Pivot: on the ground at the centre of the body footprint (the UCX
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
| `17_Toilet` | 57 x 40, 86 | 260 | falls over |
| `18_KitchenRange` | 65 x 94, 82 | 540 | falls over |
| `19_Sideboard` | 53 x 182, 91 | 220 | falls over |
| `20_RoundTableTipped` | 115 x 103, 102 | 150 | leans |
| `21_Pram` | 100 x 56, 94 | 150 | falls over |
| `22_SewingTable` | 44 x 100, 96 | 190 | falls over |
| `23_Gramophone` | 54 x 61, 164 | 150 | falls over |
| `24_TrunkAndSuitcases` | 70 x 107, 59 | 150 | leans |
| `25_CoatStand` | 36 x 88, 208 | 170 | falls over |
| `26_CellarShelf` | 42 x 105, 178 | 150 | falls over |
| `27_WashTubMangle` | 52 x 137, 109 | 190 | falls over |
| `28_Vitrine` | 48 x 98, 198 | 190 | falls over |
| `29_LampAndRug` | 159 x 98, 58 | 150 | leans |
| `30_BarrelAndCrates` | 51 x 108, 76 | 200 | falls over |
| `31_DoorLeafFallen` | 203 x 90, 24 | 150 | leans |
| `32_HandCart` | 108 x 71, 67 | 150 | leans |

The body is the box over the UCX hulls, centred on the pivot; it is also the footprint of the `NavBlock` box. The
whole mesh is larger where a piece has loose bits (`size_cm` in `furniture_table.json`).

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

- **Graph** (variables `CrushDestructionType` = AnyTank, `CrushTipSpeedScale`, `CrushSinkDeltaZ`, `CrushSinkTime`,
  `CrushSound`):
  - `BeginPlay` -> `SetupCrush`: `SetupComponentCollisions([MainMesh], [], true)` ->
    `ConfigureVerticalCollapse(MainMesh, CrushTipSpeedScale, CrushSound, Att_Collapse, SC_Collapse)` ->
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
  destroyed 3 s later. It plays the piece's crush cue once (`WallHoleDamage.CollapseSettings.CollapseSound`).

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

## Second set: pieces 17-32 (2026-11-14)

Sixteen more pieces, set up exactly like the first sixteen. Nothing of the first set was changed or saved: the base
Blueprint, the first 16 children, the material, the hole variant, the three textures and the debris were only read.

- **No new texture or material.** The new meshes map every face into islands that already exist in the atlas, so
  they use `MI_BerlinFurniture` and get their holes from the same `MI_BerlinFurniture_Holes`. Where the atlas has no
  such material a region stands in for it (no glass: the Vitrine's doors are empty frames; pine boards for the pram's
  wicker; plain wood for suitcase leather; the list is in `READY2.txt`).
- **Meshes:** 216-496 triangles (5508 in total), 1-3 UCX hulls. All 16 match the table on hull count, size and bounds.
- **Blueprints:** 16 new children of `BP_BerlinFurniture_Base`, graph-less. Same rules as before, applied by the same
  script: health `max(150, suggested_health x 1.7)`, `DamageReduction` 4, AnyTank crush with vertical collapse, the
  `NavBlock` box with `RTS_NavArea_BlockChar` over the body footprint, the inherited hole component.
- **Falls over or leans** is still decided by shape (body taller than 1.2 x its shorter footprint side), not by the
  table's new `tall` flag. `tall` means "indoor furniture" for placement and is a different thing: the Pram, the
  WashTubMangle and the BarrelAndCrates fall over when crushed but are not `tall`. Every `tall` piece falls over.
- **Crush sounds: the existing ten cues, none new.** `furniture_sound_table.py` has the 16 new rows. The modeller's
  `sound_group` was taken for 15 pieces. The one change: `31_DoorLeafFallen` plays `WoodSmall`, not the suggested
  `WoodCabinet`, because that cue has a hollow boom and breaking crockery and a flat door leaf has neither.

  | Cue | New pieces | Fit |
  |---|---|---|
  | `WoodCabinet` | 19 Sideboard, 20 RoundTableTipped, 23 Gramophone, 24 TrunkAndSuitcases, 25 CoatStand, 26 CellarShelf, 28 Vitrine, 30 BarrelAndCrates | good for 19, 25, 26, 28, 30; 20 is a heavy table with one plate, 23 has a brass horn with no sound of its own, 24 has no crockery |
  | `WoodSmall` | 21 Pram, 29 LampAndRug, 31 DoorLeafFallen, 32 HandCart | good for 31 and 32; weak for 21 (wicker on a light iron chassis) and 29 (brass pole, cloth shade) |
  | `Washstand` | 17 Toilet | good (ceramic bowl) |
  | `TiledStove` | 18 KitchenRange | good (tiled body) |
  | `IronBed` | 22 SewingTable | fair (cast-iron frame under a wooden top) |
  | `Bathtub` | 27 WashTubMangle | weak: a thin zinc tub and a wooden mangle get the cast-iron boom |

  Nobody has listened to these pairings. `sounds/furniture_sound_test.py` takes its piece list from the table, so it
  would now spawn all 32; it was not re-run with them (its groups and timing were written for 16).
- **`furniture_table.json` copy.** `Source/Tools/BerlinMap/furniture/furniture_table.json` is the modeller's extended
  table plus `tall` true/false on the first 16 entries (true for Wardrobe, TiledStove, Piano, KitchenDresser,
  Washstand), so all 32 entries carry the flag. `tall` is true for 01, 08, 09, 12, 14, 17, 18, 19, 22, 23, 25, 26, 28.
- **Scripts became incremental**, so one run covers all 32 and leaves the first 16 alone:
  - `furniture_import.py` builds the shared assets only when one is missing and imports a mesh only when it is
    missing or its FBX changed (SHA-1 kept as asset metadata `FurnSourceSha1`; the first 16 have none and are kept).
    Pieces 17-32 need `READY2.txt`.
  - `furniture_blueprints_build.py` compares each Blueprint with what it would set and skips it when equal; it
    leaves the base alone when it is already set up.
  - `-FurnForce` on either script restores the old behaviour (rebuild and save everything; needs every asset free).
  - Run on 2026-11-14 with the shared editor open on `Ger_Berlin` (it had the base, the first 16 and the material
    loaded): first run imported 16 meshes and created 16 Blueprints (32 new files, no existing file written); second
    run imported 0, changed 0 and wrote 0 files. No bridge call was needed.

### Test of the second set

```
sh Source/Tools/BerlinMap/furniture/furniture_run.sh <logname> 300 2       (about 4 minutes; the third argument is the batch)
```

Same test, with the pieces and their roles picked from `build_report.json`: the three sturdiest are shelled, two
upright and two low pieces are crushed, three get debug hits, two are killed. The crushed and killed pieces stand
together under the camera, and the run reads its own log for `New ActiveSound <cue>` at each piece's place.
Results: `Saved/Berlin/furniture/test_result_batch2.json` (`b2_test2`), `test_result_batch2_test1.json`,
`b2_test1.log`, `b2_test2.log`, close-ups `piece2_*.png` and `after2_*.png`.

Result of `b2_test2` (all checks passed) and `b2_test1` (navigation, crush, cues, debug hits and deaths passed; its
overall flag was false because that version of the test still required a hole from a real shell, see "Holes"):

- **Fresh load:** all 33 Blueprints (base + 32) compile in a headless load with no Blueprint error or warning.
- **Setup,** read from the 16 spawned actors: slot 0 `MI_BerlinFurniture`, nav area `RTS_NavArea_BlockChar`, AnyTank,
  hole material `MI_BerlinFurniture_Holes`.
- **Navigation,** a 10 m path across each of the 16: Characters filter 1009-1049 cm, 61-143 cm to the side (a detour
  on all 16); LightTanks filter 1000 cm, straight, on all 16.
- **Crush by a Pz II Luchs,** eight different pieces over the two runs, each driven through on the centre line
  (0-2 cm off), mesh visible until the actor was gone 2.2-2.4 s after the crush:

  | Piece | Run | Fall / lean | Cue that started at its place |
  |---|---|---|---|
  | 17_Toilet (upright) | test1 | 90 deg after 1.8 s | `Washstand` |
  | 18_KitchenRange (upright) | test1 | 90 deg after 1.7 s | `TiledStove` |
  | 21_Pram (upright) | test2 | 90 deg | `WoodSmall` |
  | 22_SewingTable (upright) | test2 | 90 deg | `IronBed` |
  | 20_RoundTableTipped (low) | both | 8.6-8.7 deg | `WoodCabinet` |
  | 24_TrunkAndSuitcases (low) | both | 8.6-8.9 deg | `WoodCabinet` |

- **Holes:**
  - debug hits (75, 37 and 88 mm) in test2: BarrelAndCrates 0 -> 2 holes, WashTubMangle 0 -> 2, Vitrine 0 -> 3, each
    with slot 0 swapped to `MI_BerlinFurniture_Holes`; in test1 WashTubMangle 2, Vitrine 3, CoatStand 2 (one of its
    three traces missed the thin stand);
  - real 75 mm shell in test2: KitchenRange health down to 258 (from 540, MG hits included), 1 hole, slot 0 swapped
    (`RTS_WALL_HOLES hit BP_BerlinFurn_18_KitchenRange ... radius=22 direct=1 removed=0.16 m2`);
  - the other shelled pieces got no hole from a shell: a 75 mm hit takes about 220 health, which kills a 150-260
    piece outright, and a killing hit collapses the piece instead of holing it (Sideboard in both runs, SewingTable in
    test1); the Toilet and the BarrelAndCrates only took MG hits. So among the second set only the KitchenRange can
    show a shell hole from a 75 mm gun.
- **Death by damage:** CoatStand and Gramophone (test2), Pram and Gramophone (test1): mesh hidden, NoCollision, life
  span 3.0 s, gone 6 s later, `RTS_WALL_HOLES collapse ... threw=14 hide=1`, and each one's own cue started at its
  place (`WoodCabinet`, `WoodCabinet`, `WoodSmall`).
- **First set after the script change** (`b1_regress`, batch 1): navigation, the four crushes, shells, debug hits and
  both deaths passed as before. Its cue check is informational: the row is 156 m long and the camera is not moved, so
  sounds beyond 60 m are out of range, and a cue of 1 s or less (`WoodSmall`) is then not started at all.
- **Close-up names can be one piece off:** the screenshot is taken a frame after the camera has moved on. The
  pictures show the new pieces textured from the atlas and the KitchenRange with its shell hole.

## Rebuild

```
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/furniture/furniture_import.py -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/furniture/furniture_blueprints_build.py -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
```

- **`furniture_import.py`** (textures, material, hole variant, meshes, debris; incremental since 2026-11-14, see
  "Second set"):
  - stops if `READY.txt` is missing or the table's normal-map convention is not DirectX;
  - imports the FBX with the legacy importer (Interchange drops UCX hulls);
  - checks every mesh against `furniture_table.json` (hull count, size and bounds minimum within 1.5 cm; a wrong
    mirror would show there) and the slot name `M_BerlinFurniture`;
  - report: `Saved/Berlin/furniture/import_report.json`.
- **`furniture_blueprints_build.py`** (component templates, class defaults, the 32 children; incremental, never
  deletes a Blueprint): report `Saved/Berlin/furniture/build_report.json`. It stops if the base Blueprint is missing
  or lacks its crush variables.
- **The base graph is not built by a script.** Headless Python cannot write Blueprint graphs. It was authored once
  through a UECP-enabled throwaway editor; the exact calls are in
  `Source/Tools/BerlinMap/furniture/base_graphs_uecp.json` (also for `BP_BerlinCover_Base`). Replay them only if the
  base lost its graph.
- Both scripts were run twice in a row on 2026-11-13 (second import: master rebuilt in place, "deleted stale
  M_BerlinFurniture_WallHoles, instance kept", 29 / 29 assets written, no errors); the test passed again afterwards
  (run5).
- A run that has to change an existing asset fails with "Error moving file" while an editor has that asset loaded.
  A run that only adds new pieces, or changes nothing, is safe with an editor open.
  `furniture_blueprints_build.py` takes `-FurnDryRun`: it then does everything in memory and saves nothing (report
  `build_report_dry_run.json`), which works while another editor holds the Blueprints.
- `furniture_blueprints_build.py` also sets the crush sound of every Blueprint; it needs the cues of "Crush sounds".

## Test

```
# Git Bash; throwaway editor on RTS_ToCopySmall, never Ger_Berlin
sh Source/Tools/BerlinMap/furniture/furniture_run.sh <logname> 260      (about 3 minutes)
```

`furniture_headless_test.py` spawns all 16 pieces in a row (1200 cm apart), rebuilds navigation, starts PIE and
checks navigation, crushing by a light tank, real shells, debug hits and death by damage. Results:
`Saved/Berlin/furniture/test_result.json` (the last run, run5), `run1.log`, `run4.log`, `run5.log`, close-ups
`piece_*.png` and `after_*.png`. Give the run script at least 260 s: a full run takes about 185 s.

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
- **Run5** (after re-running both build scripts): all checks passed again. Real shells holed all three targets
  (TiledStove, Bathtub 2 holes, Piano; five `direct=1` hit lines); the Piano and the TiledStove then died under fire
  and threw 14 pieces each.
- All 16 children, the base and the 18 cover Blueprints compile on a fresh headless load with no Blueprint error.

### How it looks

- The atlas reads as worn wood, enamel and iron from the RTS camera; normals and roughness come through
  (`piece_01_Wardrobe.png`).
- Holes are clean round bites with a dark inside (`after_02_IronBed.png`); on thin parts such as the bed's mattress a
  hole goes right through.

## Crush sounds (2026-11-13)

A crushed piece plays one short sound of its own material, not the tree cue. The same cue plays once when the piece
dies from damage.

**Nobody has listened to these yet.** They were built offline and checked by loudness numbers, spectrograms and a log
test. Listen first: `Saved/Berlin/furniture/sounds/preview/` (`all_in_a_row.wav` = every cue once in the table order
below, at in-game cue level; `all_variations_in_a_row.wav`; `row_of_chairs_simulation.wav` = ten chairs 0.45 s apart
with the cue's random rolls; one WAV and one spectrogram PNG per variation; two `reference_*.wav` of existing cues).

### Cues

Ten cues in `/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Sounds/`, three variations each. Wood, upholstery
and the two cabinets share a cue. Cast iron and ceramic pieces each have their own, because a bathtub, a radiator and
a bed frame do not sound alike, and the piano and the clock need a tone nothing else has.

| Cue `Cue_BerlinFurn_Crush_` | Pieces | What you should hear | Length |
|---|---|---|---|
| `WoodSmall` | 03 KitchenTable, 04 ChairFallen, 10 ChestOfDrawers, 11 BookshelfFallen, 13 Desk; the base's default | two wood cracks, a sharp splinter snap, bits clattering | 1.0 s |
| `WoodCabinet` | 01 Wardrobe, 12 KitchenDresser | hollow boom, long crack and creak, glass and crockery inside | 1.45 s |
| `Upholstery` | 05 Armchair, 06 Sofa | soft whump, frame cracking inside the padding, spring squeak, a faint twang | 1.05 s |
| `IronBed` | 02 IronBed | rattly tube-frame clang, spring mesh rattle and squeaks, muffled mattress | 1.3 s |
| `Bathtub` | 07 Bathtub | deep dull boom, hard cast-iron knock, dull enamel ring, scrape | 1.45 s |
| `Radiator` | 16 Radiator | hard clang, six ribs knocking in a run, pipe scrape | 1.3 s |
| `TiledStove` | 08 TiledStove | masonry crunch, tiles cracking, dull ceramic shatter, tiles sliding off | 1.55 s |
| `Washstand` | 14 Washstand | marble slab snapping, basin shattering, light wood crack | 1.25 s |
| `Piano` | 09 Piano | case boom and crack, strings ringing at once out of tune, two strings letting go | 1.6 s |
| `Clock` | 15 ClockFallen | thin case crack, door glass, two sagging chime strikes, movement running down | 1.5 s |

- **Cue shape:** Modulator (pitch 0.94-1.06, volume 0.88-1.0) -> Random without replacement -> three wave players.
  Cue volume 0.75 (as `Cue_Collapse_Tree`), sound class `RTS_SClass_Weapons` (as the cactus and small-metal collapse
  cues; the tree cue has none). No attenuation or concurrency on the cue itself.
- **Attenuation and concurrency** are the existing collapse assets, passed by the crush call and by the hole
  component: `Att_Collapse` (full level to 20 m, linear fall-off to silence at 60 m) and `SC_Collapse` (8 voices, stop
  farthest then oldest). They were kept: the new sounds last 1.0-1.6 s where the tree waves ran 4.7-6 s, so far
  fewer of them overlap when a tank column crosses a row of furniture (not measured in a battle).
- **Waves:** mono, 48 kHz, 16 bit. The longest is 1.6 s; 1.7 s at the lowest pitch roll.
- **Sources:** layers of waves already in the project, pitched, cut and filtered: `/Game/WARFARE_SOUNDS/` (bullet
  impacts on wood / glass / rock / brick / body, `Miscellaneous_Sounds/Metal`, `Glass`, `Doors`, wood footsteps, cloth
  and bone-snap foley), `S_SmallStoneObj_Collapse_*` and `Sound_Stone_Destroy`. Synthesised (nothing tonal like it in
  the project): the piano strings and snapping strings, the clock chime, the bathtub's enamel ring, the radiator's
  pipe knock and the spring twang. The exact layers of each wave are the recipes in `furniture_sounds_build.py`; the
  source list per wave is in `build_stats.json`.

### Loudness

Measured the same way for the new waves and the existing cues: K-weighted, loudest 400 ms (ffmpeg's `ebur128` reads
between 0.1 dB higher and 0.7 dB lower than this meter on the 30 waves), and K-weighted energy of the whole one-shot, which does not depend
on length. "In game" = wave x cue volume x the middle of the modulator's volume range.

| | Loudest 400 ms, in game | Loudest roll | Whole-sound energy, in game | Wave length |
|---|---|---|---|---|
| New furniture cues (all 30 waves) | -19.0 LUFS | -18.5 | -22.7 to -20.2 dB (TiledStove -20.2 to -20.9, WoodCabinet_1 -20.5, the rest -22.7 to -21.6) | 1.0-1.6 s |
| `Cue_FarmCrush_Wood` (existing small-prop crush) | -18.7 | -16.2 | -22.7 | 0.5-0.7 s |
| `Cue_Collapse_Tree` (what furniture played before) | -17.4 | -15.6 | -14.2 | 4.7-6.0 s |
| `Cue_MetalObj_Destruction` | -16.1 | -15.1 | -18.4 | 1.1-1.2 s |
| `Cue_SmallStoneObj_Collapse` | -15.8 | -14.5 | -17.2 | 1.7-3.3 s |

- Every wave is levelled to -16.0 LUFS (loudest 400 ms), peaks -4.6 to -1.0 dBFS; the loudest cue roll peaks at
  -3.5 dBFS.
- So the furniture is 0.3 dB under the farm wood crush and 1.6 dB under the tree cue on the 400 ms figure, and equal
  to or up to 2.5 dB above the farm wood crush in total energy (it lasts two to three times as long).
- Ten chairs in a row (simulation): loudest 400 ms -17.7 LUFS against -19.0 for one chair, peak -2.7 dBFS.

### Rebuild

```
# 1. export the source waves from the project (read-only; 414 WAVs into Saved/Berlin/furniture/sounds/src)
sh Source/Tools/BerlinMap/furniture/sounds/run_headless.sh furniture_sounds_export.py
# 2. build the 30 waves and the preview folder (plain Python 3 + numpy, ffmpeg on the PATH; about 1 minute)
python Source/Tools/BerlinMap/furniture/sounds/furniture_sounds_build.py
# 3. import the waves and make the cues (idempotent)
sh Source/Tools/BerlinMap/furniture/sounds/run_headless.sh furniture_sounds_import.py
# 4. put the cues on the Blueprints (needs the Blueprints free: no editor may have them loaded)
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/furniture/furniture_blueprints_build.py -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
```

- **One table:** `sounds/furniture_sound_table.py` says which piece plays which cue, the cue volume and the modulator
  ranges. The offline build, the import, the Blueprint build and the test all read it. To change a sound, edit its
  recipe in `furniture_sounds_build.py` (layers with start time and dB), then run steps 2 and 3.
- **Step 2 is deterministic** (fixed seeds). Step 3 keeps the SHA-1 of each source WAV as asset metadata and skips
  unchanged waves and cues, so an unchanged re-run saves nothing and is safe while an editor is open. A changed wave
  cannot be saved while an editor has it loaded.
- **Step 3 exits with code 1** and an ensure "Decoder for AudioFormat BINKA not found" on a first import: harmless (a
  commandlet has no audio decoders). The line to read is `FURN_SND_IMPORT done: ...`.
- **A new cue is a duplicate of `Cue_Cactus_Collapse`** retargeted to the furniture waves, because Python cannot create
  Sound Cue graph nodes. The cues therefore open normally in the Sound Cue editor.
- **Blueprint wiring.** `CrushSound` (SoundBase, category Crush) is a variable of the base; `SetupCrush` passes it to
  `ConfigureVerticalCollapse`. The two graph calls are recorded in `base_graphs_uecp.json`. Step 4 sets the class
  default of all 17 Blueprints and the death sound on each child's `WallHoleDamage` template. On 2026-11-13 the
  shared editor held the Blueprints (Ger_Berlin open), so the variable, the graph change and the same per-Blueprint
  statements went through that editor's bridge and each Blueprint was saved there; step 4 itself was only run with
  `-FurnDryRun`.
- **Death by damage plays the same cue at the same level.** A piano hit by a shell should still jangle, and many
  deaths come from MG fire with no explosion to carry them. A quieter version would need a second set of cues.
  `CollapseFX.CollapseSfx` in `HandleDeath` stays empty, so a crush is one sound, not two.

### Sound test

```
sh Source/Tools/BerlinMap/furniture/sounds/furniture_sound_run.sh <logname> 360      (about 3 minutes)
```

`furniture_sound_test.py` runs in a throwaway editor on `RTS_ToCopySmall` with `LogAudio VeryVerbose`: 16 pieces with a
Pz II Luchs in front of each, and a second row of the 16 pieces that takes lethal damage. Result:
`Saved/Berlin/furniture/sounds/sound_test_result.json`, log `sound_run1.log`.

Result of `sound_run1` (2026-11-13), all checks passed:

- **Defaults:** all 32 spawned pieces and all 16 class defaults hold the cue of the table, both in `CrushSound` and in
  the hole component's collapse sound. All 10 cues load as Modulator -> Random -> their three waves, volume 0.75.
- **Crush:** all 16 pieces were crushed (4.6-8.7 s after the order). For each one the audio device logged
  `New ActiveSound <the piece's cue> Comp: AudioComponent_NN Loc: <the piece's place>` exactly once.
- **Death by damage:** all 16 row-2 pieces logged their own cue once at their place (`Comp: NO COMPONENT`, the
  fire-and-forget call); 16 `RTS_WALL_HOLES collapse` lines.
- 32 furniture sound starts in total, none rejected by range or concurrency, none at a wrong place, and
  `Cue_Collapse_Tree` never started.
- **What this proves:** the right cue became an active sound at the right place on a real audio device, after the
  audible-range and concurrency checks. It does not prove how it sounds or that the mix is right in a battle.

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
- **Tall pieces** (`tall` in the table: 01, 08, 09, 12, 14, 17, 18, 19, 22, 23, 25, 26, 28) read as standing indoor
  furniture or fixed installations: put them in building plots, back (-X) to a wall line. The rest lie flat or
  belong in yards and streets (21 Pram, 24 TrunkAndSuitcases, 27 WashTubMangle, 30 BarrelAndCrates, 32 HandCart).
- **Largest footprints:** 31 DoorLeafFallen 203 x 90 cm (mesh 260 x 150 with its frame pieces), 19 Sideboard
  53 x 182, 29 LampAndRug 159 x 98, 32 HandCart 108 x 71 (mesh 234 long with the shaft and the loose wheel).
- **Very thin loose bits** (records, glass shards, a floor tile under 1 cm; rug and tablecloth about 4 cm) can sink
  into uneven ground: put 17, 20, 23, 28 and 29 on flat ground.
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
- **Sounds were not listened to by their author.** Each piece has its own crush cue since 2026-11-13 ("Crush
  sounds"); they were checked by numbers, spectrograms and a log test only. The sink itself has no smoke effect.
- **Debris** is the ruin kit's six brick-chunk shapes with the furniture atlas on them. Their UVs do not match the
  atlas, so a piece shows whatever atlas area its UVs land on. It reads as splinters at RTS distance; real furniture
  splinter meshes would be better.
- **Small targets.** A tank's main gun often misses the low pieces; most damage then comes from its MG, which makes
  no holes.
- **The crushed piece keeps its collision** for the 2.4 s it sinks; shells can still hit it.
- **Tall pieces fall through the vehicle.** The fall is a plain rotation away from the vehicle, without physics.
- **No LODs.** 168-496 triangles per piece (4336 for 01-16, 5508 for 17-32).
- **Second set:** most pieces die to one 75 mm hit without showing a hole first (health 150-260). The sound pairings
  for the Pram, the LampAndRug and the WashTubMangle are weak; a new cue each would fix that. The sound test was
  not re-run with 17-32 (they play the same ten cues).
- **Tip timer after the actor is gone (read from `FVerticalCollapseOnCrushed.cpp`, not measured).** The tip runs on a
  60 Hz timer that is only cleared when the tip ends. A leaning piece is destroyed before that (2.4 s against about
  7 s), so its timer keeps ticking as a no-op until the tip time is over. The crushable pipes behave the same;
  changing it needs C++.
- **The flying debris was not looked at** in a picture; only the thrown counts in the log were checked.
- **The base parents the C++ class** `ADestructableEnvActor`, not `BP_DestructableEnvActor` as the pipes and poles
  do, so it has no `RTSNavCollisionEnvObstacle` component; its only nav effect is the `NavBlock` box.
- **The base graph exists only in the .uasset** (and as calls in `base_graphs_uecp.json`). The whole
  `Content/RTS_Survival/Blueprints/Environment/Berlin/` folder was untracked in git on 2026-11-13.
