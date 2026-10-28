# Ground grass on Ger_Trains (landscape grass)

Top-down grass clumps grow on the Ger_Trains landscape as landscape grass. They were modelled for the RTS camera, which looks down at about −70° from 30–60 m. Instances are generated only around the camera, in batched instanced draws, and cast no shadows.

| Part | Where |
|---|---|
| Builder | `Source/Docs/Landscape/ls_grass_build.py`: `run()` builds the grass types and the switched output, `tune_look()` changes only the types and the card tint |
| Grass types | `GroundFoliage/Grass/LGT_Trains_Green` (4 varieties), `LGT_Trains_Flowers` (3), `LGT_Trains_Dry` (4); density scaling on, so the Foliage slider (`grass.DensityScale`) thins them |
| Cards | `GroundFoliage/MeshesTop/SM_GFT_*` (11 meshes, 8–24 tris) on `T_GroundFoliageTop_D/N` through `MI_GroundFoliageTop` (child of `M_GroundFoliage`, normal atlas on) |
| Card pipeline | `D:/Blender/BlenderProjects/BlenderTanks/GroundFoliage/`: `build_topdown_atlas.py` (Blender, renders 16 clumps top-down), `compose_topdown_atlas.py` (atlas + preview), `build_topdown_meshes.py` (Blender, octagon dome cards), `ue_import_topdown.py` (UE import + material) |
| Material | `LandscapeGrassOutput` in `M_RTS_Landscape_DryCastle`, each input behind `LS_GroundGrass` (off by default, on only in `MI_RTS_Landscape_DryCastle_Trains`) |

## Weights

- **Green tufts and clumps** follow the breakup chain's final grassiness (`LS_NoiseBreakup` target), not the painted layer, so they match the visible green edges.
- **Flowers** grow in about 25 m noise patches inside that green.
- **Dry tufts and litter** grow on the soft sand layers (Desert, RTSSand, GraySand) wherever the ground isn't green.
- **Water:** nothing grows below `Grass_MinHeight` (−300, fading over 80 cm). The lake bed is painted grass and the lake surface sits at about −310.

## Editor gotchas (2026-10-27)

- **Grass maps go stale.** The editor re-renders grass maps only in Landscape mode or through Build → Build Grass Maps Only. After any change to the master or the instance switch, run that build once the status bar shows no shader compile; otherwise it silently skips components. Then save the map: `-game` and cooked runs read the grass maps stored in the level.
- **Scripted cameras don't count.** The editor spawns grass around its real viewport cameras; a camera set from Python doesn't count. Judge the look in a game run (`run_bench.sh`), whose capture uses the RTS camera framing.
- **Instance counts read 0.** `get_instance_count()` on grass components reads 0 even when grass renders. Component bounds (`SystemLibrary.get_component_bounds`) show where instances are.
- **Don't toggle `grass.Enable` repeatedly in a run.** Repeated toggling in one run left both the grass and the tank ruts missing from the final picture; measure grass with two runs instead (`EXEC="grass.Enable 0" run_bench.sh ...`).
- **`PerInstanceFadeAmount` is 1 when fully visible** (and on plain meshes), dropping to 0 at the cull distance. The foliage material's `alpha × fade` is correct; `1 − fade` hides every card.

## Top-down cards (2026-10-27)

- **Why new cards.** The first atlas used cards built for eye-level views. From the RTS camera its upright tufts are seen nearly edge-on and its soft-edged ground clumps become blobs, so the grass read as olive blotches.
- **How a clump is made.** Each new clump is modelled in Blender: blades, broad leaves, clover and flower heads radiate from a dark soil core, and the clump is rendered straight down.
- **What gets baked.** The cell's colour is the render's base colour darkened by ambient occlusion, which gives the dark core; normals and coverage come from the same render.
- **Blade width.** Blades are about 2 cm wide. Thinner blades fall below a texel in the distant mips, and the clump then reads as a dot.
- **Cards and companions.** Cards are 8-triangle octagons that skip the empty cell corners. Several meshes carry companion clumps, which adds variety without adding grass varieties, so draw calls stay where they were.
- **Atlas cells:**
  - green rosettes A–D, meadow clumps A–B, broadleaf A–B and clover;
  - poppy, chamomile and cornflower clumps;
  - dry stars A–B, an olive transition clump and straw litter.

## Cost (40 Panzer IVs, 1920×1080, DebugGame)

| Run | GPU frame | Main-view base pass (landscape + tanks + grass) | Base-pass draws |
|---|---|---|---|
| `costGrassOn` (first cards) | 5.79 ms | 0.24 ms | 331 |
| `costTop` (top-down cards) | 5.87 ms | 0.27 ms | 349 |

Measured with `run_bench.sh <name> -TankTrailBenchProfileGPU` and compared by `compare_profilegpu.py`. Grass adds about 200 draw calls compared with no grass (133). A clean grass-off run is still missing: the earlier one ran under heavy machine load.

## Not done yet

Tufts standing in a tank rut aren't flattened: the grass doesn't read the trail texture yet. A trail sample at each instance's position in the foliage material could squash them.
