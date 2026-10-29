# Old macadam road (BP_GroundPathSpline_Road)

A worn two-lane road for the ground path splines, modelled on the grey Sudden Strike roads: dark tyre-polished wheel
paths, speckled stone aggregate, a few patched repairs and potholes, and pale gravel shoulders that fade into the
landscape. Cracks are drawn by the shader in world space, so no two stretches of road look the same.

| Part | Where |
|---|---|
| Blueprint | `/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Road/BP_GroundPathSpline_Road` (child of `BP_GroundPathSpline`) |
| Material | `M_GroundPath_Road`, instances `MI_GroundPath_Road` (default) and `MI_GroundPath_Road_WarTorn` (more cracks) |
| Textures | `T_GroundPath_Road_BC` / `_N` / `_M`, crack field `T_GroundPath_RoadCracks` |
| Meshes | `SM_GroundPath_Road_S00..S15`: 16 ribbon slices, each covering 1/16 of the texture's V range, Nanite off |
| Texture generator | `D:/Blender/BlenderProjects/BlenderTanks/GroundPath/gen_road.py <out dir>` (numpy, ~50 s, writes a preview) |
| UE builder | `Source/Docs/Landscape/path_road_build.py`: `run()` imports, rebuilds the material, the instances, the slices and the Blueprint; `place_demo()` re-places the demo and saves the level |
| Demo | `GroundPathDemo_Road` on Ger_Trains (folder GroundPaths), 118 m east-west with three bends, X -16000..-6600 around Y -16000 |

## Using it

1. Drag `BP_GroundPathSpline_Road` into the level and edit its spline like any ground path.
   - Every 2.2 m tile end is snapped to the ground (20 cm above it, `GroundOffset`) by a trace down from the spline, so **keep spline points above the ground**. A point below a hill makes the trace miss, and the road dives under the hill.
   - Each tile is rigid across its 9 m width. Route roads over gentle ground: where a hill falls away steeply sideways, the uphill edge dips into the slope. That's the same limit a real road has without grading.
2. Width is `PathWidth` (900 cm: 6.6 m of asphalt plus gravel shoulders). Keep `TileLengthRatio` at 3.6 so the texture keeps its proportions; the texture repeats every 32.4 m, split over 16 tiles of 2.2 m (the parent makes each tile `PathWidth × TileLengthRatio / slice count` long).
3. To swap the look, set `PathMaterial` to `MI_GroundPath_Road_WarTorn`, or make another instance of `M_GroundPath_Road`.
4. After placing roads, rerun the stone exclusion tagging (`GroundStones/ue_tag_exclusions.py`) and regenerate `PCG_GroundStones`, or the old stones stay on the road. The exclusion band is ±3.5 m, so a few pebbles may still land on the outer gravel shoulder. That looks natural there.

## How the shader works

The material is physically based: Default Lit, linear albedo 0.06–0.26 on asphalt and 0.09–0.45 on gravel, roughness 0.6–0.95 (lower in the polished wheel paths and oil drips), ambient occlusion, a tangent-space normal map, metallic 0, default specular. It is **Masked**: the road writes depth, so tank ruts, landscape grass and ground detail under it stay hidden. Only the ragged shoulder is dithered out (`DitherTemporalAA`) into the real landscape. In that band, the colour also blends toward the landscape colour read from `RVT_Environment`. The normal is built in world space as the landscape normal (also from `RVT_Environment`) plus the road's detail normal, so the slightly differently tilted 2 m tiles shade as one surface. Tiles cast no shadows and have no collision; the parent Blueprint sets that.

Two approaches were tried and dropped:
- **Moving vertices onto `RVT_Height` in the vertex shader.** Virtual-texture pages only become resident when pixels request them, so tiles that had sunk never fetched their real height.
- **Fading edges by height above `RVT_Height`.** On Ger_Trains that texture reads hilltops too low, which faded whole tiles out. The Blueprint's per-tile ground snap makes both unnecessary.

Everything is in one custom node, `RoadShade`:

- **Cracks.** `T_GroundPath_RoadCracks` holds a domain-warped Voronoi crack network, sampled in world space every `CrackScale` (12 m). For each pixel it stores the distance to the nearest crack (R) and a random value per crack (G). A crack shows only where its random value is below a threshold. World noise, `CrackAmount` and `EdgeCrackBoost` raise that threshold (more cracks near the crumbling edges). A crack also widens the further it is past the threshold. So cracks appear, grow and vanish along the road instead of repeating with the texture.
- **Alligator cracking.** A second, smaller, rotated sample of the same field (`AlligatorScale` 4.2 m, `AlligatorAmount`) adds block cracking in patches.
- **Hairlines.** The B channel holds a finer network, shown where the medium noise is high (`HairAmount`).
- **Weeds.** Some cracks get dry grass instead of dark grit (`WeedAmount`, `WeedColor`).
- **Visible at RTS zoom.** No crack is drawn thinner than about 1.2 screen pixels (`fwidth` of the distance field). A thinner one is widened to that and its contrast lowered to match, so from the gameplay camera cracks stay readable as fine dark lines instead of vanishing or shimmering.
- **Variation over large distances.** Dusty stretches (`DustAmount`, `DustTint`) and damp darker stretches (`DampAmount`) follow 60 m world noise, so neighbouring texture tiles never match.

Main parameters: `CrackAmount` 0.62 and `CrackWidth` 0.095 on `MI_GroundPath_Road` (master defaults 0.45 / 0.07), `EdgeCrackBoost`, `AlligatorAmount`, `HairAmount`, `WeedAmount`, `DustAmount`, `DampAmount`, `GroundBlend` (shoulder toward landscape colour), `CrackColor`.

## Texture contents

All painted procedurally by `gen_road.py`. Across the road (U), 1024 px span 900 cm; along it (V), 4096 px span 3240 cm.

- **BC.** Asphalt base with sun-bleached and damp mottling. Four wandering wheel paths (two lanes), darker and with fewer light grains. Long tyre streaks, light quartz and dark basalt aggregate grains. A faint centre joint, three small repair patches, oil drips in the lane centres and four ragged potholes. A crumbling asphalt edge with broken chunks, gravel shoulders with light, dark and warm pebbles, dust blown onto the edges and dirt smeared out of the verge. Alpha is the coverage.
- **N.** Normal map from a height field: grains, pebbles, the asphalt step, patch rims, potholes.
- **M.** R roughness, G ambient occlusion (cavity), B height, A asphalt mask (where the shader may crack).

## Cost (2026-10-28, Development build, headless Ger_Trains run with `ProfileGPU` and per-material draw events)

- **View:** about 140 m of road, 65 tiles in view, 1920x1080. Each tile is 32 triangles.
- **Depth prepass:** 65 draws, rounding to 0.00 ms.
- **Base pass:** 65 draws totalling 0.01–0.02 ms, out of a 0.34–0.37 ms base pass and a 5.7–6.2 ms GPU frame.
- **No shadow or Nanite passes:** tiles cast no shadows, and the slices are not Nanite.
- **Draw calls:** the real cost is one draw per 2.2 m tile per pass, so about 90 draws per 100 m of visible road. Spline meshes don't instance.
  - For kilometres of road on screen at once, use 8 slices (4.4 m tiles, half the draws) at the price of less terrain-following, or merge distant road into static meshes.
- **Textures:** about 20 MB with mips: BC 1024x4096 (DXT5), N 1024x4096 (BC5), M 1024x4096 (DXT5), cracks 1024x1024 (BC7).

Measure again with `BIN=Development EXEC="r.EmitMeshDrawEvents 1, r.ShowMaterialDrawEvents 1" Source/Tools/TankTrails/run_bench.sh road -TankTrailBenchAt=-11000,-16000 -TankTrailBenchProfileGPU`, then sum the `MI_GroundPath_Road` lines in the ProfileGPU dumps.
