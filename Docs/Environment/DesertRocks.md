# Desert rock kit (Bruchblock)

16 angular fractured desert rocks, 8 large and 8 medium, on one 4096 atlas. They are made for the orange
DryCastle desert. Tank fire cuts procedural shell holes into them (the wall hole system), and a lethal hit bursts
them into debris.

## Content
`/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/`

| Folder | Assets |
|---|---|
| Textures | `T_DesertRock_BC`, `_N`, `_ORM` (4x4 atlas, 1024 px cells) |
| Materials | `M_DesertRock` / `MI_DesertRock` (normal look), `M_DesertRock_WallHoles` / `MI_DesertRock_WallHoles` (masked, two-sided, the hole look) |
| Meshes | `SM_DesertRock_<L1..L8, M1..M8>_<Kind>` (2 LODs, UCX hulls, Nanite off), `SM_DesertRock_Debris_0..5` |
| Blueprints | `BP_DesertRock_Base` and 16 graph-less children `BP_DesertRock_<Rock>` |

Large: L1_Block, L2_Cracked, L3_Tumbled, L4_Pillar, L5_Pile, L6_Wedge, L7_Slab, L8_Leaning (3-7 m, about 2200 tris).
Medium: M1_Block, M2_Slab, M3_Tilted, M4_Cracked, M5_Pair, M6_Cube, M7_Long, M8_Wedge (1.8-2.8 m, about 900 tris).

## Colour
The baked albedo is neutral. All colour comes from `MI_DesertRock` (and the same values in the holes material):
`RockTint`, `DustColor` (on up-facing faces), `GroundColor` with `GroundFromRVT` (foot blend into the landscape
RVT) and `ContactHeight`. To retint for another desert, change those parameters, not the textures.

## Gameplay
`BP_DesertRock_Base` is a copy of `BP_OreCrushRock_Base` (parent `BP_NaturalCover` -> `ADestructableActorHealth`).

| Class | Health | Reduction | Hole radius | Max hole | Aim points | Crush | Nav box area |
|---|---|---|---|---|---|---|---|
| Medium | 2500 | 20 | 0.8 cm per mm calibre | 70 cm | 1 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |
| Large | 6000 | 25 | 1.0 cm per mm calibre | 110 cm | 3 | none | `NavArea_Null` |

- Medium rocks: medium and heavy tanks drive over them and crush them (the rock leans and sinks, as the Ore rocks);
  infantry and light vehicles path round.
- Large rocks: not crushable; no unit paths through a living rock. The area is free again after death.
- Holes: `WallHoleDamage` with `NoGrid`; `ADestructableActorHealth::TakeDamage` forwards each hit.
- Death: `HandleCrushableDeath` -> `CollapseIntoPieces` on Kinetic, FireOrLaser and Radiation deaths. The rock
  bursts into `SM_DesertRock_Debris_*`; the actor is destroyed after 3 s.
- Holes material: `ue_import.py` `matte_inside()` wires the Custom node's `Roughness` output into the holes
  library's rim blend (the library takes the node's default output, which is the colour here, and the damaged rock
  went glossy), and gives back faces (the inside of a hole) roughness 1, specular 0 and a flat normal.
- Trap: in the Ore base the death path is behind `Branch(Crushable)`. In `BP_DesertRock_Base` the Branch's false
  pin is also wired to the death switch, so these non-crushable rocks still die.

## Tools
Blender source (Blender 4.0): `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/DesertRockKit/Model`

- `build_rocks.py`: shapes from `Concepts/_tools/sets.py` set F, sculpt, bake, FBX + `out/rocks.json`.
  `blender -b --factory-startup -P build_rocks.py -- --samples 32` (all; about 25 min on GPU). `--only 0,11` limits.
- `compose_atlas.py`: cells -> `out/T_DesertRock_*.png`.

Unreal (`Source/Tools/DesertRocks`, headless, never the shared editor):

- `bash run_ue.sh "ue_import.py import" <log>`: textures, materials, meshes, debris, holes material.
- `bash run_ue.sh ue_blueprints.py <log> -nullrhi`: base + 16 Blueprints (idempotent).
- `DROCK_ONLY=<L rock>,<M rock> sh fire_test_run.sh <log> 560`: real Pz IV fire on RTS_ToCopySmall (holes, death,
  nav). Output in `Saved/DesertRocks/test`.
- `ROCK_SCRIPT=rock_crush_test.py sh fire_test_run.sh crush 600`: crush lanes (Pz IV crushes medium rocks, a Luchs
  and the large rock make vehicles path round) and lethal damage. Output in `Saved/DesertRocks/crush`.
- `ROCK_SCRIPT=rock_lineup_shot.py sh fire_test_run.sh lineup 420`: all 16 in two rows, pictures in
  `Saved/DesertRocks/lineup`.
