# Ore Mountains rockworks (2026-11-17)

Eight large gneiss rock formations for `Ger_2ndOreMountains`. They are the bigger siblings of the 16 crushable rocks
(`OreCrushableRocks.md`) and use the same stone as the cliff kit. No vehicle can crush them, they block every agent
while alive, and they stop shells. Tank fire cuts procedural shell holes into them (the wall-hole system). At zero
health a rockwork bursts into debris, the same way the desert rocks do (`DesertRocks.md`).

They are not placed on any map. Everything was tested in a throwaway editor on `Maps/UnitTests/RTS_ToCopySmall`.

## Content

`/Game/RTS_Survival/Blueprints/Environment/OreMountains/Rockworks/`

| Folder | Assets |
|---|---|
| `Textures/` | `T_OreRockwork_BC` (sRGB), `_N` (DirectX), `_ORM` (R ambient occlusion, G roughness, B height). One 4096 atlas shared by all eight. |
| `Materials/` | `M_OreRockwork` / `MI_OreRockwork` (the normal look); `M_OreRockwork_WallHoles` / `MI_OreRockwork_WallHoles` (masked, two-sided: the hole look). |
| `Meshes/` | `SM_OreRockwork_R01..R08_<Kind>` (2 LODs, one UCX hull per block, Nanite off, distance field on); `SM_OreRockwork_Debris_0..5` (0.5-0.85 m, 80 tris, no collision). |
| `Blueprints/` | `BP_OreRockwork_Base` and eight graph-less children `BP_OreRockwork_R<NN>_<Kind>`. |

| Rock | Size X x Y x Z (cm) | Tris LOD0 / LOD1 | Hulls | Health | Hole cap (cm) |
|---|---|---|---|---|---|
| R01_Outcrop | 796 x 543 x 367 | 3375 / 1687 | 4 | 10000 | 117 |
| R02_Stack | 589 x 548 x 473 | 3074 / 1536 | 3 | 8000 | 95 |
| R03_LeaningSlab | 648 x 518 x 381 | 2737 / 1368 | 2 | 8000 | 57 |
| R04_SplitTor | 673 x 548 x 448 | 3308 / 1654 | 2 | 8000 | 120 |
| R05_Rib | 964 x 367 x 257 | 2771 / 1386 | 3 | 10000 | 113 |
| R06_FallenBlock | 669 x 486 x 311 | 3107 / 1554 | 3 | 8000 | 120 |
| R07_Tower | 515 x 423 x 546 | 3153 / 1577 | 3 | 8000 | 120 |
| R08_Spur | 925 x 507 x 360 | 3565 / 1783 | 3 | 10000 | 120 |

- **Shapes.** Each formation is built from 2-4 convex blocks cut on the cliff kit's joint lattice
  (`ock_rock.py` `LATTICE`). All blocks of one formation share one lattice orientation, so their joint faces are
  parallel and stacked blocks sit face on face. Corner cuts give frost-shattered edges, and blocks on the ground are
  cut flat at z = 0.
- **Meshes.** The pivot is at the bottom centre. The faces that lie flat on the ground were removed, which frees atlas
  space. Build sizes are final: place the rocks at a scale of about 1, because scaling stretches the holes.
- **Atlas.** One smart UV project and pack covers all eight rocks together, so every rock has the same texel density:
  116-118 px/m. For comparison, the medium crushable rocks are 127-180 px/m and the cliff kit texture is 256 px/m
  tiled. One atlas was enough; a second one would bring the rockworks to about 165 px/m.

## Material

`M_OreRockwork` is the `M_OreCrushRock` shading (which follows `M_OreCliff`) with the same values: `RockTint`
(0.74, 0.67, 0.58), `DustColor`, `GroundColor`, `GroundFromRVT` 0.45, macro noise and per-rock jitter. It differs in
two values: `ContactHeight` is 15 cm instead of 10 (the rocks are bigger), and `NormalStrength` is 0.65 instead of 1.0
(at 1.0 the baked crack cells read as a wormy pattern up close). The albedo was baked from the cliff kit's
`T_OreCliff_BC` at its 4 m tile; the atlas mean is sRGB 0.63, the same as the crushable rocks.

`M_OreRockwork_WallHoles` is built by `Tools/WallChunks/make_wall_holes_material.py` (`make_holes_instance`, NoGrid,
two-sided, inner tint). After that, `ue_import.py` `matte_inside()` makes three changes:
- It wires the Custom node's `Roughness` output into the rim lerp (the desert fix against glossy damage).
- It gives back faces roughness 1, specular 0 and a flat normal.
- It adds an inner fill: back faces emit `WallHoleInnerFillColor` (0.15, 0.14, 0.125) x `WallHoleInnerFill` (0.25).
  This is rockworks only. A hole in a 3-4 m solid block looks into back faces that the rock shadows completely, so
  without the fill the hole read as a black void. At 2.0 it read white.

`WallHoleInnerColor` is (0.15, 0.14, 0.125) and `WallHoleInnerDarken` is 0.55: fresh broken gneiss, greyer and a
little darker than the weathered surface.

**Cost**, measured in the throwaway editor:

| Material | Pixel instructions | Vertex instructions | Samplers | Texture samples |
|---|---|---|---|---|
| `M_OreRockwork` | 581 | 289 | 7 | 8 |
| `M_OreRockwork_WallHoles` | 700 | 289 | 8 | 9 |

The hole material is only on a rock after its first hit. The same figures were read again after the inner fill was
added (fire2 run).

## How a rockwork works

`BP_OreRockwork_Base` is a duplicate of `BP_DesertRock_Base`, which is the copy of `BP_OreCrushRock_Base` whose
`Branch(Crushable)` false pin is also wired to the death switch. Without that wiring a non-crushable rock never dies.
Headless Python cannot write Blueprint graphs, which is why the base is duplicated rather than built.

The children set only class defaults and component templates:
- **MainMesh:** Destructible object type. It blocks Projectile, Visibility and both weapon traces, ignores Pawn and
  overlaps BuildingPlacement. `Overlap Tanks` is off, so at runtime `BP_NaturalCover` sets the vehicle object channels
  to Ignore.
- **HealthComp:** health 8000, or 10000 for rocks whose longest side is at least 7 m. `DamageReduction` is 30.
- **WallHoleDamage:**
  - NoGrid, 1.0 cm of hole radius per mm of calibre.
  - The hole cap is 55 % of the thinnest main block's hull width (at most 120 cm), so a hole never opens through a
    block. The leaning slab's cap is 57 cm.
  - A death burst throws up to 90 `SM_OreRockwork_Debris_*` and plays `Cue_SmallStoneObj_Collapse`. The actor is
    destroyed after 3 s.
- **NavBlock:** a dynamic-obstacle box over all hulls, padded 5 cm, with area `NavArea_Null`.
- **Aim points:** on the centroids of the biggest blocks (1-2 per rock).
- `Crushable` and `Overlap Tanks` are false.

**Why these numbers.** A Pz IV G main-gun hit took 191 health per hit at reduction 30 (measured: 8000 to 7809). That
is about 42 hits for an 8000 rock and 52 for a 10000 rock. The desert large rocks take about 30 hits (6000 health at
reduction 25). The reduction of 30 cancels small-arms fire.

**Units are stopped by the navmesh only.** The mesh ignores pawns, as the desert rocks do. A unit pushed against a
rock by physics could clip into it, but pathing never takes a unit through one.

## Placing

- Place the Blueprints as single actors. An instanced `SM_OreRockwork_*` has no health, no holes and no nav box.
- Rebuild navigation after placing: the boxes are cut into the navmesh at build time.
- Yaw is free. Keep the scale at 0.9-1.1, because holes stretch on scaled meshes.
- The nav box is one rectangle over all blocks. The gap of the split tor and the space under the leaning slab are
  blocked too.
- The underside is flat. On slopes, sink a rock a little.

## Rebuild

```
# Blender 4.0, in D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreRockworks
blender -b --factory-startup -P build_rocks.py -- --cpu --samples 16   # about 20 min; --nobake for shapes only (15 s)
python compose_atlas.py                                                # out/T_OreRockwork_*.png, out/atlas_stats.json
blender -b --factory-startup -P render_preview.py [-- --shape]         # renders/*.png
# Unreal, headless (Git Bash, in Source/Tools/OreRockworks)
bash run_ue.sh "ue_import.py import" import       # textures, materials, meshes, debris, holes material
bash run_ue.sh ue_blueprints.py bps -nullrhi      # base (duplicated once) + 8 children; idempotent
bash run_ue.sh "ue_import.py material" mat        # textures and materials only
sh fire_test_run.sh fire 660                      # real Pz IV fire on two rocks (ORW_ONLY=<rock>,<rock>)
ROCK_SCRIPT=rockwork_crush_lineup_test.py sh fire_test_run.sh lineup 660   # no-crush lanes + lineup pictures
```

- **Shapes are always rebuilt for all eight.** `build_rocks.py` builds and exports all eight every run, because the
  shared UV pack needs them all. `--only` limits only the bake.
- **Foliation trap.** In Blender 4 every `v.co` write marks the normals dirty, and reading `v.normal` per vertex after
  that recomputes all of them. The cost is quadratic: 10+ minutes per rock. `foliation()` reads the normals once with
  `foreach_get`.
- **Rerun the Blueprints after a mesh import.** `ue_import.py import` deletes and reimports the meshes, which clears
  the Blueprints' mesh references, so run `ue_blueprints.py` again afterwards.

## Tests (2026-11-17, ONE throwaway editor at a time on UnitTests/RTS_ToCopySmall, never saved)

Pictures are in `Saved/OreMountains/rockworks/`. Superseded pictures (taken before the final material values) are in
`superseded/`.

**Fire** (`fire/`, `logs/fire.log` and `logs/fire2.log`), on R03_LeaningSlab and R04_SplitTor, with a Pz IV G 35 m away
from each:
- In 36 s of fire each rock took 5-6 hits of 191 health. Health went 8000 to 6852 (run 1) and 8000 to 7043 (run 2).
  Each rock got 2-3 holes, and the slot switched to `MI_OreRockwork_WallHoles` on the first hit.
- A lethal hit hid the mesh, removed its collision and burst it into debris. Both actors were gone 9 s later.
- **Paths across each rock, all four agents, each on its own navmesh with its own filter:**
  - While alive, every agent detoured. R03: Character 306 cm to the side, LightTank 401, MediumTank 439,
    HeavyTank 477. R04: 331 / 426 / 464 / 505.
  - Points inside the box were 0/6 on the navmesh.
  - After death, every agent's path was straight and 6/6 points were on the navmesh.
- **Not verified:** the extra debug hit on the thin slab of R03 hit near its top end and removed nothing, so the 57 cm
  cap on the slab was not exercised. All real hits landed on the support block.

**No crush** (`lineup/lanes_result.json`, `logs/lineup.log`): ALL PASSED.
- A Pz IV G was ordered straight through R02_Stack. It drove round, 4.6 m to the side; the rock was untouched.
- A Tiger H1 was ordered straight through R06_FallenBlock. It drove round, 5.8 m to the side; untouched.
- All four agents' paths detoured before and after the drive.

**Lineup** (`lineup/`, `logs/lineup_pics.log`, final material):
- All eight rocks, a parked Pz IV G, the medium crushable `BP_OreRock_M04_Split` and the cliff piece
  `SM_OreCliff_Block2`, plus close-ups of R01, R03 and R04.
- The last picture shows R08 with two debug holes (75 mm and 88 mm).
- Colour of lit top faces in `compare_tank_crush_cliff.png` (12 x 12 px):

  | Sample | sRGB | r/g | b/g |
  |---|---|---|---|
  | R08 rockwork | (173, 157, 107) | 1.10 | 0.68 |
  | M04 crushable | (170, 155, 110) | 1.10 | 0.71 |
  | Cliff Block2 | (200, 188, 142) | 1.07 | 0.75 |

  The rockworks match the crushable rocks. The cliff piece is about 15 % brighter and a little cooler. The test map is
  orange desert in warm light, not the Ore map.

## Open points

- **Holes are hollow inside.** The inside of a hole is the far side's back faces: flat grey polygons with the inner
  fill. Where ground faces were removed, the sand can show through (R08).
  - `WallHoleInnerFill` is an emissive tuned only under this map's exposure. At 0 the inside is black, at 2.0 white,
    and at 0.25 a light cool grey, a little brighter than wanted.
  - It may glow in shade or at night. It is a parameter on `MI_OreRockwork_WallHoles`: 0.12-0.15 is probably closer.
- **Time to kill.** One Pz IV needs about 42-52 hits, roughly 4-5 minutes at the measured rate. Only 6 hole slots
  exist; later hits merge into existing holes.
- **Shapes** are blocky lattice blocks (by design, like the crushable rocks). R05 and R08 are 9.6 m and 9.3 m long,
  a little over 9 m.
- **The red grid with circles** in `lineup/holes_fill_R08_Spur.png` is a test-map or game overlay near that spot.
  Its cause was not looked into.
- **Not verified:**
  - `Ger_2ndOreMountains` lighting and the RVT foot blend there.
  - 88 or 105 mm real fire.
  - Infantry or light vehicles actually driving round.
  - LOD1 switching, a cooked build, and a health bar on a hit rock.
