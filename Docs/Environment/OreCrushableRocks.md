# Ore Mountains crushable rocks (2026-11-14)

Sixteen loose gneiss rocks for `Ger_2ndOreMountains`, made to sit beside the cliff kit. They are 8 small and 8 medium
rocks on one texture atlas, and vehicles crush them. Small rocks are crushed by every vehicle. Medium rocks are crushed
by medium and heavy tanks; light vehicles path around them.

Not placed on any map. Everything was tested in a throwaway editor on `Maps/UnitTests/RTS_ToCopySmall` (see "Test").

## What exists

Content is in `/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/`:

| Folder | Assets |
|---|---|
| `Textures/` | `T_OreCrushRock_BC` (sRGB), `T_OreCrushRock_N` (DirectX normal map), `T_OreCrushRock_ORM` (linear: R ambient occlusion, G roughness, B height). One 2048 atlas, 4 x 4 cells of 512 px, one cell per rock. |
| `Materials/` | `M_OreCrushRock`, `MI_OreCrushRock` (on every rock and on the debris). |
| `Meshes/` | `SM_OreRock_<S/M><NN>_<Kind>` (16 rocks) and `SM_OreRock_Debris_0..5` (death-burst chunks, 15-28 cm, no collision). |
| `Blueprints/` | `BP_OreCrushRock_Base` and 16 children `BP_OreRock_<S/M><NN>_<Kind>`. |

Source and scripts are in `D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\OreCrushableRocks\`.

| Rock | Size X x Y x Z (cm) | Tris (LOD0 / LOD1) | Crushed by | Nav area |
|---|---|---|---|---|
| S01_Block | 68 x 54 x 40 | 300 / 150 | AnyTank | `RTS_NavArea_BlockChar` |
| S02_Wedge | 83 x 51 x 44 | 300 / 150 | AnyTank | `RTS_NavArea_BlockChar` |
| S03_Slab | 95 x 69 x 22 | 280 / 140 | AnyTank | `RTS_NavArea_BlockChar` |
| S04_Chunk | 55 x 50 x 41 | 300 / 150 | AnyTank | `RTS_NavArea_BlockChar` |
| S05_Split | 97 x 62 x 34 | 358 / 178 | AnyTank | `RTS_NavArea_BlockChar` |
| S06_Shard | 84 x 39 x 34 | 280 / 140 | AnyTank | `RTS_NavArea_BlockChar` |
| S07_Low | 59 x 47 x 28 | 260 / 130 | AnyTank | `RTS_NavArea_BlockChar` |
| S08_Pair | 99 x 58 x 31 | 358 / 178 | AnyTank | `RTS_NavArea_BlockChar` |
| M01_Block | 156 x 117 x 109 | 520 / 260 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |
| M02_Wedge | 194 x 129 x 126 | 560 / 280 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |
| M03_Slab | 238 x 157 x 54 | 520 / 260 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |
| M04_Split | 239 x 150 x 106 | 648 / 324 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |
| M05_Stack | 153 x 116 x 141 | 648 / 324 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |
| M06_Chunk | 135 x 126 x 129 | 520 / 260 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |
| M07_Tilted | 186 x 98 x 85 | 518 / 258 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |
| M08_Long | 245 x 104 x 88 | 560 / 280 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` |

- **Shapes.** The rocks are convex blocks cut on the cliff kit's joint lattice (`ock_rock.py` `LATTICE`), so they break
  along the same three joint sets as the cliffs. Corners are cut off for a frost-shattered look and the underside is
  flat. Split, Stack, Pair and Tilted rocks have two parts in one mesh, with one UCX hull per part.
- **Meshes.** The pivot is at the bottom centre. Nanite is off and there is no lightmap UV. LOD1 has 50 % of the
  triangles at screen size 0.25. Small rocks have no distance field; medium rocks keep the project default.

## Material

`M_OreCrushRock` follows `M_OreCliff`, so a rock beside a cliff piece reads as the same stone:

- **Albedo.** The atlas colour is the cliff kit's own `T_OreCliff_BC`. It was baked in object space at the cliff's
  4 m tile and was not re-tinted (mean sRGB 0.63, the same as the cliff texture). The material multiplies it by the
  same parameters as the cliff: `RockTint` (0.74, 0.67, 0.58, the `MI_OreCliff` value), a world macro noise
  (`MacroAmount` 0.16, `MacroWorldSize` 5200) and the baked cavity (`CavityStrength` 0.85).
- **Dust.** `DustColor` (0.47, 0.41, 0.31) at `smoothstep(0.70, 0.96, world normal z) x DustAmount` (0.75, as on the
  cliff). It settles in the hollows first, using the baked height in ORM.B.
- **Ground contact.** Within `ContactHeight` (10 cm) of `RVT_Height`, the rock takes the landscape colour
  (`RVT_Environment` mixed with `GroundColor`, `GroundFromRVT` 0.45, as the cliff does). Ground stones blend the same
  way.
- **Per-rock variation.** A hash of the object position gives each rock a brightness and hue jitter (`ValueJitter`,
  `HueCool` / `HueWarm`). `PerInstanceRandom` is not used, because it is constant on actor-placed meshes.
- **Usage and cost.** Usage flags: instanced static meshes and Niagara mesh particles (for the debris). In the
  throwaway editor the material compiled to 581 pixel and 289 vertex instructions, 7 samplers and 8 texture samples.
  That is heavier than `M_GroundStone`, mostly from the RVT samples and the Custom node.

## How a rock works

`BP_OreCrushRock_Base` is a duplicate of `BP_BerlinCover_Base`. It was copied, not authored, because headless Python
cannot write Blueprint graphs. Its parent is `BP_NaturalCover`, whose native class is `ADestructableActorHealth`. It
carries that Blueprint's crush graph unchanged.

- **BeginPlay.** `BP_NaturalCover` calls `SetupComponentCollisions(MainMesh, Overlap Tanks)`. Then `SetupCrush` runs:
  `ConfigureVerticalCollapse(MainMesh, CrushTipSpeedScale, Collapse_Cue, Att_Collapse, SC_Collapse)`, then
  `SetupCrushDestructionOverlap(MainMesh, CrushDestructionType, VerticalCollapse)`.
- **Crushed (Scavenging death).** `VerticalDestruction` runs: the rock leans about 0.1 tip speed and sinks
  `CrushSinkDeltaZ` (its height + 20 % of its half length + 20 cm) in `CrushSinkTime` (1.6 s small, 2.6 s medium).
  `NS_DestructionSmoke` spawns 40 cm up, the collapse cue plays, and the actor is destroyed.
- **Killed by damage (Kinetic, FireOrLaser or Radiation).** `WallHoleDamage.CollapseIntoPieces` hides the mesh,
  removes its collision, throws 8 (small) or 18 (medium) `SM_OreRock_Debris_*` chunks with
  `Cue_SmallStoneObj_Collapse`, and destroys the actor 3 s later.
- **Components** added to the base by `ue_blueprints.py`:
  - `WallHoleDamage`: no `DamagedMaterials`, so no holes are drawn, as on the Berlin rubble heaps. A hit throws at
    most 2 chips.
  - `NavBlock`: a hidden dynamic-obstacle box over the hull footprint. It is padded by 5 cm and is at least 80 cm
    tall.
- **Per child** (class defaults and templates, no graph):
  - mesh, Movable;
  - collision: Destructible; blocks Visibility, Projectile and the weapon traces; overlaps the vehicle object
    channels; ignores pawns; does not affect navigation;
  - `HealthComp`: small 700 health, medium 1800, `DamageReduction` 30;
  - `Crushable` on, `Overlap Tanks` on, `CrushDestructionType`, the nav area of `NavBlock`, the sink settings and
    one aim point at half height.

### Who crushes what, and navigation

The pairs below are the existing ones from `FRTSCrushGate`, `BP_Cover_Log1` and the Berlin cover and furniture. The
per-agent query filters (`/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_*`) exclude the nav area:

| Class | Crushed by | Before the crush | After the crush |
|---|---|---|---|
| Small | `AnyTank`: every agent that is not Character, so all tanks, wheeled vehicles and half-tracks | Infantry path around it (`BlockChar`). Every vehicle paths straight through and crushes it. | The actor is gone and the nav area is gone; the navmesh is free. |
| Medium | `MediumOrHeavyTank` | Infantry and light vehicles (the LightTank agent, wheeled cars included) path around it (`BlockCharLight`). Medium and heavy tanks path through and crush it. | Free for everyone. |

- A rock blocks no navmesh outright. An unfiltered nav query walks through the box.
- Pawns ignore the rock's collision. A light tank that clips a medium rock does nothing to it.

## Placing

- **Place the Blueprints as single actors.** An instanced mesh of the same `SM_OreRock_*` has no crush, health or nav
  box. Use the instanced ground stones (`GroundStones.md`) for non-crushable scatter.
- **Rebuild navigation after placing.** The boxes are cut in at build time.
- **Yaw is free.** Keep scale at 0.85-1.2. The nav box and the mesh scale with the actor, but `CrushSinkDeltaZ` is a
  fixed class default in cm. A rock scaled past about 1.2 may not sink fully before it is destroyed; raise
  `CrushSinkDeltaZ` on that instance (it is instance-editable). A small rock scaled up past about 1.3 stops looking
  small but still uses the AnyTank rule.
- **The underside is flat** (90-100 % of each footprint lies within 3 cm of the base). On slopes, sink a rock a few cm
  so its uphill edge does not float.
- **Leave infantry a way round.** Medium rocks also stop light vehicles. Keep about 1 m of open ground beside any rock
  on an infantry route, and keep medium rocks off lanes that light vehicles must use unless a detour is wanted.
- **Place them in the full editor** on the shared map, not through a commandlet.

## Rebuild

```
# Blender 4.0 (D:/Blender/BlenderSoftware/4.0/blender.exe), in the source folder; about 25 min on the CPU.
# Use --cpu while Unreal editors hold the GPU: a mid-run GPU out-of-memory fallback left bad passes on that rock.
# rocks.json is written after every rock. Check out/atlas_stats.json (coverage, ao_mean ~0.9-1, albedo ~0.63) before
# importing.
blender -b --factory-startup -P build_rocks.py -- --cpu [--only 0,3]
python compose_atlas.py
blender -b --factory-startup -P render_preview.py           # renders/overview.png, closeup.png
# Unreal, headless (Git Bash, in the source folder):
bash run_ue.sh ue_import.py import                           # textures, material, meshes, debris
bash run_ue.sh ue_blueprints.py blueprints -nullrhi          # base (duplicated once) + 16 children
bash run_ue.sh "ue_import.py material" mat                   # material / textures only
sh run_engine_test.sh <logname> 600                          # test and pictures, about 5 min
```

- **`ue_import.py`** deletes and reimports each mesh, because a reimport keeps the old import settings. That clears
  the Blueprints' mesh references, so always run `ue_blueprints.py` after it. It rebuilds the material in place and
  reports to `Saved/OreMountains/rocks/ue_import_report.json`.
- **`ue_blueprints.py`** is idempotent and never deletes a Blueprint. It duplicates `BP_BerlinCover_Base` only if
  `BP_OreCrushRock_Base` is missing. It reports to `Saved/OreMountains/rocks/blueprints_report.json`.
- **Bake lessons** (`build_rocks.py`):
  - The Principled DIFFUSE colour pass of Blender 4.0 came out about 3x too dark, and the AO pass was shadowed by the
    enveloping low mesh. All maps except the normal map are therefore baked as emission. The AO uses the AO node with
    `only_local`.
  - The cliff texture's alpha is height, so it is loaded `CHANNEL_PACKED`. A premultiplied alpha darkened the colour.
  - Every pass is baked into Non-Color images. `compose_atlas.py` converts the colour to sRGB.
  - The level cut of the underside must happen before the first `fit()`. The first build cut after it, which removed
    nothing: the rocks stood on one corner of the lattice-tilted bottom face and hovered in the engine.

## Test (`rock_engine_test.py`, run3 on 2026-11-14, all passed)

The test runs in a throwaway `UnrealEditor.exe` with `-DisablePlugins=Fab,BpGeneratorUltimate` and is never saved.

**Map.** It runs on `/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall`, not on `Maps/ToCopy/RTS_ToCopySmall`. Run1 on
the `ToCopy` copy failed: every vehicle drove 5-15 m to the side of its lane (cause not looked into), and fog of war
darkened the pictures. The unit-test copy is the one the Berlin crush tests use. Run2, on the first build, gave the
same results as run3.

Results are in `Saved/OreMountains/rocks/test/result.json` and `run3.log`.

- **Setup**, read from the spawned actors: slot `MI_OreCrushRock`, Movable, vehicle channels Overlap, Projectile
  Block, Pawn Ignore, one nav box with the right area, the crush type, health 700 / 1800.
- **Paths** across a rock, 12 m long, each agent on its own navmesh with its own filter (cm, side offset):

  | Rock | LightTank | MediumTank / HeavyTank | Character |
  |---|---|---|---|
  | S03_Slab, S05_Split, S04_Chunk (small) | 1200, straight | 1200, straight | 1209-1216, 70-99 to the side |
  | M01_Block, M04_Split, M06_Chunk (medium) | 1288-1314, 230-268 to the side | 1200, straight | 1223-1239, 116-154 to the side |

- **Crush lanes.** Each vehicle started 24 m before its rock and was ordered 24 m past it:

  | Lane | Rock | Vehicle | Result |
  |---|---|---|---|
  | A | S03_Slab (22 cm, the lowest) | Pz II Luchs | drove through 1 cm off the line; crush 5.4 s after the order; actor gone 3.0 s later |
  | B | S05_Split | Pz IV G | drove through 5 cm off the line; crush at 5.1 s; gone 3.0 s later |
  | C | M01_Block | Pz II Luchs | pathed round 2.2 m to the side; rock untouched |
  | D | M04_Split | Pz IV G | drove through 4 cm off the line; crush at 4.9 s; sank 79 cm in 1.1 s; gone 4.0 s later |

  After each crush, all four agents' paths across the spot were 1200 cm and straight. The sink pauses while a
  screenshot is taken, because the sink runs on wall-clock time.
- **Death by damage** (`apply_damage` on M06_Chunk and S04_Chunk):
  - the mesh hid, collision went off, life span 3 s, and the debris flew (`kill_burst.png`);
  - both actors were gone 12 s later, and every agent's path across the spot was straight.
- **A non-lethal hit** of 400 on M02_Wedge left 1430 health (1800 - (400 - 30)). The picture was taken in fog of war,
  so whether a health bar shows on a hit rock is **not verified**.
- **Material:** compiled in the editor to 581 pixel / 289 vertex instructions, not 0.

### Pictures (`D:\UE5Projects\RTS_Survival\Saved\OreMountains\rocks\`)

- `look_game_view.png`, `look_game_view_far.png`: all 16 rocks, two cliff pieces and a parked Luchs for scale, from
  the steep RTS angle.
- `look_close_small.png`, `look_close_medium.png`: close-ups of the two rows.
- `look_close_cliff.png`: the comparison shot.
  - `SM_OreCliff_Block2` (the cube) with `M01_Block` in front of it.
  - `SM_OreCliff_Chunk3` (left) with `M06_Chunk` beside it.
  - Both cliff pieces are lifted onto the ground by their bounds, because their pivot is at the centre.
- `crush_B_small_medium.png`, `crush_D_medium_medium.png`: 0.6 s into a crush.
- `kill_burst.png`: the debris burst.
- `hit_health_bar.png`: dark, in fog of war.
- Blender previews: `renders/overview.png`, `renders/closeup.png` in the source folder.

What they show:
- **Colour.** Mean RGB of a lit top face in `look_close_cliff.png` (10 x 10 px):
  - cliff `Block2` (188, 173, 123) against rock `M01` (179, 165, 117): about 5 % darker, same hue (r/g 1.09 / 1.08,
    b/g 0.71 / 0.71);
  - cliff `Chunk3` (157, 139, 95) against rock `M06` (143, 118, 78): a little warmer and darker.
  - Shaded faces of both read about (45-55, 20-27, 8-13).
  - So the rocks match the cliff kit in this light. The test map is orange desert sand under warm light, not the
    lighting of `Ger_2ndOreMountains`, which was not opened.
- No atlas seams are visible at close-up distance.
- The two size classes read apart from the game camera.
- Since the underside fix, the rocks sit on the ground (`look_close_small.png`). Run2's pictures, from the first
  build, showed them hovering.

## Open points (user decisions)

- **The crush puff is `NS_DestructionSmoke`.** It is white and sized for buildings, so it is large on a 50 cm rock
  (`crush_D_medium_medium.png`).
- **The crush sound is `StarterContent/Audio/Collapse_Cue`.** It is a 3.9 s rubble collapse: stone, but long. A
  death by damage already plays the shorter `Cue_SmallStoneObj_Collapse`.
- **Changing either needs a graph edit.** Both are literals in the duplicated graph of `BP_OreCrushRock_Base`, the
  `CollapseFX` make node and the `ConfigureVerticalCollapse` call. A Blueprint editor or a UECP-enabled editor can make
  that edit; headless Python cannot.
- **Nobody has listened to the crush or watched the puff in motion**; only the log and stills were checked.
- **Natural-cover parent.** Through `BP_NaturalCover` the rocks are cover actors without sockets, so they publish no
  cover points. They also carry `HealthComp`; whether a health bar shows on a hit rock is unverified. If infantry
  should take cover behind medium rocks, add sockets to the medium meshes (see `BerlinRubbleCover.md`).
- **Rocks block shells** (Projectile Block, health and `DamageReduction` 30). A main-gun round may stop on a knee-high
  rock. Turn the Projectile response off on the small rocks if that feels wrong.
- **Not tested:** heavy tanks (the rule includes them), wheeled vehicles, enemy-owned vehicles, a cooked build, and
  `Ger_2ndOreMountains` itself.
