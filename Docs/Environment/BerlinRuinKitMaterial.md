# Berlin ruin kit material (`M_BerlinRuin`, 2026-11-12, material pass 2026-11-13)

One cheap shared master for every Berlin ruin kit piece (`KIT_CONTRACT.md` in
`D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\BerlinRuinKit\`), and its procedural shell-hole
variant. It replaces the DBK-based `MI_Berlin_Ruin_*` instances on the kit (those are left untouched).

Assets are in `/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/`:

| Asset | What |
|---|---|
| `M_BerlinRuin` | Opaque, default lit, one-sided master. Fully Rough, decal response Color. |
| `MI_BerlinRuin_Wall` | Slot 0 (`M_BerlinRuin_Wall`) on undamaged actors. `Ruin_IsRubble` off. |
| `MI_BerlinRuin_Rubble` | Slot 1 (`M_BerlinRuin_Rubble`), loose rubble heaps, street litter, cover heaps. `Ruin_IsRubble` on. |
| `MI_BerlinRuin_Rubble_Foot` | Child of the rubble instance for the footprint skirts and collapse heaps: darker, thinner dust (`RuinDustColor` (0.115, 0.102, 0.090), `RuinDustOpacity` 0.6). Built by the kit build since 2026-11-13; `berlin_map_build.foot_material()` writes the same two values. |
| `M_BerlinRuin_WallHoles` | Masked hole copy made by `Tools/WallChunks/make_wall_holes_material.masked_master_for(..., inner_tint=True)`. |
| `MI_BerlinRuin_Wall_Holes` | `make_holes_instance(MI_BerlinRuin_Wall, NoGrid, two_sided=True, inner_tint=True)`. The `UWallHoleDamageComponent` swaps slot 0 to this on the first hit (`DamagedMaterials`). |
| `T_BerlinRuin_Plaster_D`, `_Brick_D`, `_N`, `_Rubble_D` | 2048 px, BC7, LOD group World, 1 tile = 4 m. `_N` is only sampled with `Ruin_CloseUpDetail`. |

## Inputs

- **UV0**: the contract's world-scale box projection, 1 UV unit = 4 m. The tile scalar is applied in the vertex shader (a
  Vertex Interpolator; Customized UVs are not reachable from Python).
- **Vertex colour (linear)**:
  - R: soot (walls) / charred timber (rubble).
  - G: plaster loss, 0 = intact plaster, 1 = bare brick. Wall only.
  - B: dust and ground grime.
- **Custom primitive data 0-4** (the hole system owns 8-34 and never writes below 8: `WriteMask` in
  `WallHoleDamageComponent.cpp`; 5-7 are free). Unset CPD reads 0 in the shader, and the array is zero-filled when the
  hole component grows it on the first hit. So 0 means "unset" here:
  - 0, 1, 2: tint RGB multiplier. All three 0 uses `RuinTintFallback` (white). Write e.g. (1.05, 1.0, 0.9) for a warm
    building.
  - 3: soot amount. 0 uses `RuinSootAmountFallback` (1). To switch soot off, write a **negative** value (e.g. -1).
  - 4: rubble desaturation, 0..1 (1 = grey). 0 uses `RuinRubbleDesaturation` (0.20). For full texture colour write a
    **negative** value. Only the rubble branch reads it; on a kit piece it affects the piece's own rubble mounds.
  - Read by `RuinCPD_TintSoot` (vector, index 0) and `RuinCPD_Desaturation` (scalar, index 4), both in the vertex
    shader.
  - In Python: `comp.set_default_custom_primitive_data_float(index, value)` per value; `berlin_map_build._set_ruin_cpd`
    does that for a tuple (tint R, G, B, soot, desaturation).

## Texture packing (2 samples on the wall, 1 on rubble)

Both wall textures share one UV (`UV0 x RuinWallTile`, 4 m).

| Texture | RGB | A |
|---|---|---|
| `T_BerlinRuin_Plaster_D` | Weathered grey-beige lime plaster (sRGB). | Chip height, uniform 0..1. Plaster shows where A > vertex G, so the loss edge follows flakes and cracks and looks chipped. Also the breakup noise for soot and dust. |
| `T_BerlinRuin_Brick_D` | Dull red-brown brick, lime mortar, Blockverband. 52 courses (7.7 cm including the joint) x 16 stretchers per 4 m (sRGB). | Cavity: 1 = brick face, 0 = deep joint. Only read with `Ruin_CloseUpDetail`. |
| `T_BerlinRuin_N` | R, G: brick normal XY. B, A: plaster normal XY. Linear BC7, DirectX green, 0.5 = flat. | Only sampled with `Ruin_CloseUpDetail`. |
| `T_BerlinRuin_Rubble_D` | Brick, plaster and mortar rubble with charred timber, cavity shading baked in (sRGB). | Height, uniform 0..1 (soot / dust breakup). |

- **Brick tile.** The brick is unique over the full 4 m tile at the same texel density as the plaster (1.95 mm per px):
  one UV and less visible repetition.
- **Rubble** has no normal map. Cavity shading is baked into its albedo.
- **Rubble colour (2026-11-13).** Regenerated less red: brick fragments 43 % instead of 54 %, brick (0.442, 0.325, 0.274)
  instead of (0.455, 0.290, 0.235), plaster and mortar a step darker, 26 % dust tie-in instead of 18 %. Mean sRGB
  (0.430, 0.382, 0.345): luminance 0.39 as before, saturation 0.20 (was 0.26), luminance spread 0.087 (was 0.102).
  Height (A) is unchanged; plaster, brick and normal PNGs are byte-identical to 2026-11-12. The old PNG is kept as
  `Saved/Berlin/ruinkit_textures/old_T_BerlinRuin_Rubble_D_2026-11-12.png`.
- **Brick shimmer:** a course is 39.4 px, about 2.5 px at mip 4. The mortar is kept dirty and close to the brick mean (luminance 0.40 against 0.33). That halved the 52-cycle course energy, and the 12-cycle alias band that showed at mip 5 is gone (measured on the row-mean luminance spectrum).
- **Hole variant:** adds the library's `LowResBlurredNoise` sample, so the wall has 3 samples once it has holes.

## Shader

Vertex shader (through three vertex interpolators, 7 scalars):

```
uv        = UV0 * (RuinWallTile | RuinRubbleTile)
tint      = CPD 0-2 set ? CPD 0-2 : RuinTintFallback          (rubble: * RuinRubbleColor)
sootV     = vertex R * (CPD 3 set ? max(CPD 3, 0) : RuinSootAmountFallback)
threshold = (vertex G + RuinPlasterLoss) * (1 + 2w) - w        w = RuinPlasterEdgeWidth      (wall)
desat     = CPD 4 set ? saturate(CPD 4) : RuinRubbleDesaturation                              (rubble)
```

Pixel shader, wall:

```
d       = (Plaster.A - threshold) / w ;  plaster = saturate(d)
base    = lerp(Brick.rgb * RuinBrickColor, Plaster.rgb * RuinPlasterColor, plaster) * tint
noise   = Plaster.A
```

Pixel shader, rubble (`Ruin_IsRubble`):

```
c       = lerp(Rubble.rgb, luminance(Rubble.rgb), desat)
c       = c * RuinRubbleContrast + RuinRubblePivot * (1 - RuinRubbleContrast)
base    = c * tint ;  noise = Rubble.A
```

Shared:

```
soot    = saturate(sootV * (RuinSootStrength + RuinSootBreakup * (noise - 0.5)))
base    = base * (1 + soot * (RuinSootColor - 1))
dust    = saturate(B * (RuinDustStrength + RuinDustBreakup * (0.5 - noise))) * RuinDustOpacity
base    = lerp(base, RuinDustColor, dust)
```

No roughness pin (the master is Fully Rough) and no normal pin (the vertex normal). `Ruin_CloseUpDetail` (static switch,
off on every instance) brings back, for a camera that gets close: the normal map (brick / plaster blended by the plaster
mask), the bright broken plaster rim and the brick shadow under the plaster edge, and `lerp(Brick.A, Plaster.A, plaster)`
as breakup noise. It costs 20 instructions and one sample (312).

## Parameters (group `BerlinRuin`)

| Parameter | Default | Notes |
|---|---|---|
| `Ruin_IsRubble` (static switch) | false | true on `MI_BerlinRuin_Rubble` |
| `Ruin_CloseUpDetail` (static switch) | false | see above; set on every instance by the build, the hole instance included |
| `RuinWallTile` | 1.0 | UV0 multiplier for the wall textures (1.0 = 4 m) |
| `RuinRubbleTile` | 1.333 | 3 m rubble tile |
| `RuinPlasterLoss` | 0 | added to vertex G (per-instance "more / less plaster") |
| `RuinPlasterEdgeWidth` | 0.04 | transition width in chip-height units; smaller is sharper |
| `RuinSootAmountFallback` | 1.0 | used when CPD 3 = 0 |
| `RuinSootStrength` / `RuinSootBreakup` | 1.0 / 0.9 | |
| `RuinSootColor` | (0.075, 0.068, 0.064) | multiplier at full soot (keeps some texture) |
| `RuinDustStrength` / `RuinDustBreakup` / `RuinDustOpacity` | 1.0 / 0.8 / 0.75 | |
| `RuinDustColor` | (0.235, 0.212, 0.180) linear | |
| `RuinTintFallback` | (1, 1, 1) | used when CPD 0-2 are all 0 |
| `RuinPlasterColor` / `RuinBrickColor` | (1, 1, 1) | per-instance colour multipliers |
| **`RuinRubbleDesaturation`** | 0.20 | rubble only. 0 = texture colour, 1 = grey. Used when CPD 4 = 0 |
| **`RuinRubbleContrast`** | 0.90 | rubble only. 1 = texture contrast; lower flattens the fragments toward the pivot |
| **`RuinRubblePivot`** | 0.13 | linear grey the contrast turns about (about the texture's mean) |
| `RuinRubbleColor` | (1, 1, 1) | rubble colour / value multiplier, applied after desaturation and contrast |
| `RuinPlasterEdgeLight` / `RuinPlasterEdgeShadow` / `RuinNormalStrength` | 0.22 / 0.30 / 1.0 | `Ruin_CloseUpDetail` only |
| `RuinCPD_TintSoot`, `RuinCPD_Desaturation` | CPD 0-3, CPD 4 | not instance knobs |

Gone since 2026-11-13: `RuinPlasterRoughness`, `RuinBrickRoughFace`, `RuinBrickRoughJoint`, `RuinMatteRoughness`,
`RuinRubbleRoughness` (Fully Rough). If `fully_rough` is taken out of `MASTER_FLAGS` the build makes `RuinRoughness`
(0.9) and, in the close-up branch, the three cavity roughness parameters again.

The hole instance also has the library's `WallGrid*` parameters (NoGrid: `WallGridColumns` 0) and `WallHoleInnerDarken`
0.65 / `WallHoleInnerColor` (0.085, 0.06, 0.05).

Defaults live in `Source/Tools/BerlinMap/ruinkit/ruin_params.py`. Both the builder and the texture preview read them.

### Rubble look and the map's tints

Target: dusty grey-brown with the brick still recognisable, darker than the pale ash ground, not darker than the mud.
Measured in the look test (display sRGB luminance / saturation of a top-down picture; ash 0.37 / 0.05, landscape rubble
layer 0.26 / 0.16, mud 0.09 / 0.25):

| | 2026-11-12 | 2026-11-13 |
|---|---|---|
| loose heap, no primitive data | 0.18 / 0.30 | 0.19 / 0.16 |
| loose heaps, the four `RUBBLE_TINTS` | 0.11-0.15 / 0.22-0.31 | 0.17-0.20 / 0.07-0.20 |
| collapse heaps, the four `FOOT_TINTS` | 0.09-0.17 / 0.21-0.25 | 0.11-0.18 / 0.11-0.18 |

`berlin_map_build.py` holds the per-component sets as (tint R, G, B, soot, desaturation). The old sets were cool and dark
to fight the pink; on the new texture they turned rubble as dark as the mud, so they were retuned:

```
RUBBLE_TINTS  was (0.60,0.64,0.68,1.30) (0.70,0.70,0.70,1.10) (0.52,0.55,0.58,1.35) (0.78,0.74,0.70,0.95)
              now (0.90,0.90,0.90,1.20,0.35) (1.00,0.97,0.93,1.00,0.10) (0.80,0.81,0.82,1.30,0.45) (0.95,0.93,0.90,0.90,0.25)
FOOT_TINTS    was (0.78,0.78,0.78,1.1) (0.68,0.70,0.72,1.3) (0.85,0.82,0.79,1.0) (0.62,0.64,0.66,1.35)
              now (0.95,0.95,0.95,0.80,0.20) (0.85,0.86,0.87,0.90,0.35) (1.00,0.97,0.94,0.60,0.10) (0.80,0.81,0.82,0.90,0.30)
```

Kit pieces (`step_ruins`) still write CPD 0-3 only; their rubble mounds use the instance default (0.20).
The instanced path was rendered too: two hierarchical instanced components made by `_instance_actor` with the brickiest
and the greyest set differ as intended (`Saved/Berlin/ruinkit_look/after_instanced/game_25m_instanced.png`).

## Hole variant

- Built only by the shared library, with nothing edited afterwards:
  - `masked_master_for(M_BerlinRuin, inner_tint=True)`: a masked copy with the WallHoles Custom node. It reads CPD 8-34
    through the same parameters, and `RTS.WallChunks.FinishMaskedMaster` reports `masked RESULT DONE ... assumedOpaque=0`.
  - `make_holes_instance(MI_BerlinRuin_Wall, grid_axis=None, two_sided=True, inner_tint=True, name='MI_BerlinRuin_Wall_Holes')`.
- The builder then re-applies the wall instance's overrides and **every static switch**, so the holed wall matches the
  intact one.
- **NoGrid:** one instance serves every kit mesh, as on the PostApo ruins. The placeholder mesh passed for the unused
  grid bounds is `/Engine/BasicShapes/Cube`.
- **Two-sided:** a hole shows the inside of the solid wall. Back faces are darkened toward sooty brick (inner tint).
- **Rim:** the library's scorched rim (base x 0.3) is hardcoded. Its roughness term does nothing here: the master has no
  roughness pin.
- **Rendered in engine** (2026-11-13, `ruin_holes_headless_test.py`): ragged holes, scorched rim and dark interior show
  on E1, E3, B3, B7 and E5 after real tank fire and debug hits; pictures in `Saved/Berlin/ruinkit_test/holes_*.png`.
- **Contract check:** in the stats run, the `WallHole*`/`WallGrid*` scalar parameters are a superset of
  `M_RusBunker_Concrete_WallHoles`' (35 shared, plus `WallHoleInnerDarken`).
- **Rebuild:** each build deletes `M_BerlinRuin_WallHoles` and makes it again, so it never goes stale against the master.
  `MI_BerlinRuin_Wall_Holes` is kept and reparented, so Blueprint references stay valid. If the old hole master cannot
  be deleted the build stops (it used to fall back to deleting the instance). The master graph is rebuilt in place, so
  mesh slots and instances stay valid.

## Stats (PCD3D_SM6, default quality level, `MaterialEditingLibrary.get_statistics`)

Measured in a headless commandlet with a renderer (`ruin_cmd_run.sh`, `-AllowCommandletRendering`); the same call before
and after. Texture samples include the engine's own DBuffer decal reads (3 by default, 1 with decal response Color).

| Material | PS instr. 11-12 | PS instr. 11-13 | Texture samples | VS instr. | Interpolator scalars |
|---|---|---|---|---|---|
| `MI_BerlinRuin_Wall` | 367 (351 as saved) | **292** | 6 -> 3 | 296 -> 312 | 2 -> 7 |
| `MI_BerlinRuin_Wall_Holes` | 499 (483 as saved) | **423** | 7 -> 4 | 296 -> 312 | 2 -> 7 |
| `MI_BerlinRuin_Rubble` (and `_Foot`) | 331 (315 as saved) | **286** | 4 -> 2 | 296 -> 317 | 2 -> 7 |
| empty default-lit material | 284 | 284 | 3 | 284 | 0 |
| empty, Fully Rough + decal response Color | | 250 | 1 | 284 | 0 |
| `M_RusBunker_Concrete` / `_WallHoles` | 319 / 438 | | 4 / 5 | 554 | 0 |
| `MI_Berlin_Ruin_Brick` / `Cinderblock` / `Concrete` (DBK) | 392 / 408 / 400 | | 7 / 9 / 7 | 301-305 | 0 |

- **"As saved" and the 351 of the first version.** The 2026-11-12 master had `used_with_static_lighting` set. For such
  a material `get_statistics` reports the surface-lightmap base pass; for every other material (and the 284 baseline) it
  reports the volumetric-lightmap one, which is 16 instructions longer for the same graph (an empty material with the
  flag reads 269, not 284). 351 against 284 therefore understated the material's own cost (it was 83, not 67). The flag is off now (the project bakes no lighting; it only
  added permutations), and the first column gives the old graph measured the same way as the new one (flag switched off
  in memory, never saved). Neither pass is exactly the one drawn at run time without baked lighting, which the tool
  does not report; differences between materials carry over.
- **Vertex shader counts are a hint only** (they depend on usage flags). The pixel shader is what is paid per pixel.
- **Target 150-190: not reached and not reachable.** The engine's base pass for an empty default-lit material is 284
  here, 250 with the two flags this master sets and 244 with no decal response at all. The wall's own code is 42
  instructions and 2 samples (was 83 and 3), the rubble's 36 and 1 (was 47 and 1), the holes add 131 (was 132).

### Where the instructions went (`ruin_cost_probe.py`, one feature knocked out of the old graph at a time)

| Feature of the 2026-11-12 wall | Cost | Now |
|---|---|---|
| Soot | 15 | kept, rewritten (see below) |
| Dust | 11 | kept, rewritten |
| Normal map: sample, brick / plaster blend, scale | 10 | `Ruin_CloseUpDetail` only |
| CPD 0-3 fallback logic per pixel | 10 | vertex shader |
| Plaster rim light + brick shadow at the plaster edge | 8 | `Ruin_CloseUpDetail` only |
| Roughness by cavity and plaster, matte under soot / dust | 5 | gone (Fully Rough) |
| Brick sample (against using the plaster for both) | 5 | kept |
| Breakup noise `lerp(cavity, chip, plaster)` | 2 | chip height only |
| DBuffer decal normal + roughness response (engine) | 23, 2 samples | decal response Color |
| Specular / roughness path of the base pass (engine) | 12 | Fully Rough |

The graph cuts take 34, writing the threshold, soot and dust terms so that parameter-only maths folds into the
preshader takes 6 more (367 -> 327), the two flags 35 (-> 292).

### What was cut, and what it costs visually

Judged on before / after pictures from the game camera (`Saved/Berlin/ruinkit_look/compare_before_after/`), not on the
numbers. A 4x zoom of the B3 facade from 25 m (`facade_zoom.png`) shows plaster, brick courses, soot plumes and the
chipped plaster edge unchanged; the mean absolute difference on the facade is 1.4 % of full scale.

- **No normal map** on the wall: brick courses and plaster grain no longer catch the light. Not visible from 20-40 m;
  visible from a few metres (flat-shaded brick).
- **No rim light / edge shadow** at the plaster edge: the edge is a hard colour change. A few millimetres wide; the hole
  test's close-up at about 9 m still reads as chipped plaster.
- **Fully Rough**: no specular at all. The kit was 0.8-0.97 rough; under the overcast sun the sheen was not visible.
- **Decal response Color**: decals still colour walls, heaps and skirts (ruts, dirt, crater stains) but do not change
  their normal or roughness. Checked only roughly: a dirt-tracks decal laid across a collapse heap and the ground
  (`after_instanced/game_25m_instanced.png`) darkened both alike, but it rendered as a black square in the one-shot
  test scene, so the look of a real rut across a skirt has not been judged.
- `Ruin_CloseUpDetail` is compiled and measured but has not been rendered.
- **Soot / dust breakup** uses the plaster chip height on bare brick too (was the brick cavity): the mottling is
  blotchier and no longer follows the joints.
- **CPD fallbacks and the plaster threshold are interpolated per vertex**: identical result (they are constant per
  component, or linear in the vertex colour).

### Proposed, not done: the shared hole library

The hole Custom node comes from `Tools/WallChunks/make_wall_holes_material.py` and is shared with every other holed
material, so it was left alone. Measured on unsaved copies of the current wall (two-sided, 423):

| Library change | PS instr. | Notes |
|---|---|---|
| No grid code when the instance is NoGrid (`WallGridColumns` 0 is a parameter, so the grid maths stays compiled) | -31 | needs a second code variant or a static switch in the library |
| Mesh-space position through a vertex interpolator instead of a per-pixel transform | -20 | 3 more interpolator scalars, vertex shader +35; exact (the transform is affine) |
| Edge noise from a texture the material already samples (here `Plaster.A`) instead of `LowResBlurredNoise` | -5, 1 sample | changes the hole outline per material |
| All three | -55 | 423 becomes 368 |

The inner tint costs 7 and two-sided 13; both are needed for the dark interior.

## Rebuild

```
python Source/Tools/BerlinMap/ruinkit/ruin_textures_gen.py           # PNGs + Saved/Berlin/ruinkit_textures/ruinkit_preview.png
python Source/Tools/BerlinMap/ruinkit/ruin_textures_gen.py --only-rubble --no-preview
sh Source/Tools/BerlinMap/ruinkit/ruin_cmd_run.sh ruin_material_build.py <logname> RUIN_MODE=all    # build + stats
sh Source/Tools/BerlinMap/ruinkit/ruin_cmd_run.sh ruin_cost_probe.py <logname>                       # cost attribution, saves nothing
sh Source/Tools/BerlinMap/ruinkit/ruin_look_run.sh <outname>                                         # pictures + colour table
python Source/Tools/BerlinMap/ruinkit/ruin_look_compare.py <before> <after>                          # before | after sheets
sh Source/Tools/BerlinMap/ruinkit/ruin_holes_run.sh <logname>                                        # shell holes on the ruins
```

- Run Unreal headless only, never in the shared editor: clearing the graph of a master that a loaded level uses
  asserts there (`delete_all_material_expressions`).
- Don't wrap the commandlet in a pipe that closes early (`| Select-Object -First`): that kills it mid-run. Redirect to a
  file instead.
- `MaterialEditingLibrary.delete_all_material_expressions` removes only about half the nodes per call; the builder loops
  until the graph is empty.
- `Saved/Berlin/ruinkit_textures/ruinkit_stats.txt` (stats), `ruinkit_cost_probe.txt` (probe);
  `ruinkit_stats_2026-11-12.txt` is the first version's.

### Rebuild while editors are open

Every editor that has the kit loaded (the shared editor on `Ger_Berlin`, any throwaway editor that draws a ruin) keeps
the ten `.uasset` files open, and the build's first save then fails with "Error moving file" (nothing is written).

1. `powershell -ExecutionPolicy Bypass -File Source/Tools/BerlinMap/ruinkit/ruin_wait_unlocked.ps1 -Once` lists the
   holders; without `-Once` it waits until the files are free.
2. Throwaway editors: wait until they have quit.
3. The shared editor: one bridge call that saves exactly these ten packages unchanged
   (`EditorAssetLibrary.save_loaded_asset(asset, False)` for the four textures, two masters and four instances). Saving
   drops the editor's file handles; it does not take them again until it reloads the packages.
4. Run the build at once (another editor starting up takes the files again).
5. The shared editor still shows the old materials from memory. Reload exactly these packages there
   (`EditorLoadingAndSavingUtils.reload_packages`, masters and textures before instances) or restart it.

## Notes for meshes and integration

- **Vertex colour is most of the look.**
  - An unpainted mesh (white vertex colour) reads as fully sooty, bare, dusty brick.
  - Black vertex colour reads as clean plaster.
  - Paint G deliberately; a smooth gradient is enough, because the texture makes the edge.
- **UV:** keep box projection at 1 unit = 4 m. The normal map (close-up only) assumes V runs down the wall, as in
  Unreal's default projection.
- **Instanced meshes:** the master carries `used_with_instanced_static_meshes` (heaps, litter and skirts are
  hierarchical instanced meshes on the map).
- **Hole component:**
  - `DamagedMaterials[0] = MI_BerlinRuin_Wall_Holes`; slot 1 (rubble) gets none.
  - NoGrid; the PostApo ruin settings are a good start.
  - Debris pieces should use `MI_BerlinRuin_Wall_Holes`, which carries the Niagara mesh usage flag.
- **Per actor:** `SetCustomPrimitiveDataFloat` 0-4, or the Custom Primitive Data defaults on the mesh component; see
  the encoding under Inputs.
