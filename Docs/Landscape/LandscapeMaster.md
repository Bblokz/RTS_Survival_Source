# M_RTS_Landscape_DryCastle: layout and material functions

The DryCastle landscape master is shared by Ger_Trains, Ger_DestroyT35 and Ger_MarderStugMission. Its effects live in six reusable material functions. The master itself holds only:

- the landscape layers,
- the layer weights,
- the static switches,
- the grass output,
- comment frames that explain each section.

The scripts in this folder build everything; don't hand-edit the generated nodes, because the next run replaces them.

## Pipeline

```
LandscapeLayerBlend_0
  -> MF_RTS_LS_GroundBreakup          [LS_NoiseBreakup]
  -> MF_RTS_LS_RadixiteBlight         [LS_RadixiteBlight]
  -> MF_RTS_ColorVariation_Landscape  (the RVT output reads here)
  -> MF_RTS_LS_TankTrails             [LS_TankTrails]
  -> MF_RTS_LS_FieldFurrows -> MF_RTS_LS_WetRuts   [LS_GroundDetail]
  -> BreakMaterialAttributes_2 -> material outputs
MF_RTS_LS_GrassWeights -> [LS_GroundGrass] -> LandscapeGrassOutput
```

- **Switches:** every static switch defaults to off, and an off switch passes its input straight through.
  - `MI_RTS_Landscape_DryCastle_Inst` turns on `LS_TankTrails`.
  - `MI_RTS_Landscape_DryCastle_Trains` (Ger_Trains, a child of `_Inst`) turns them all on.
- **DesertPaintable:** `M_RTS_Landscape_DesertPaintable` calls the tank trails and the radixite blight too (before its colour variation); `ls_master_build.run()` wires its blight (`wire_desert_paintable()`).
- **RVT:** the runtime virtual texture caches its pages, so it reads the ground before the tank trails and ground detail. Anything that changes every frame must stay after the colour variation.

## Functions

All six are in `/Game/RTS_Survival/Materials/ground/Landscape/Functions`, under the library category "RTS Landscape". They take and return Material Attributes, so any landscape or ground material can chain them. Their parameter names are the ones the instances already use, so existing overrides keep working.

| Function | Inputs | Outputs | Builder |
|---|---|---|---|
| `MF_RTS_LS_GroundBreakup` | LayerBlend, GrassLayer, GrassWeight, SandWeight, SoilColor, SoilNormal | Attributes, Grassiness, Damp, DirtColor | `ls_breakup_build.py` |
| `MF_RTS_LS_TankTrails` | Attributes, SoftGround | Attributes, RutCentre | `ls_trails_build.py` |
| `MF_RTS_LS_FieldFurrows` | Attributes, SoilColor | Attributes, FieldMask, DetailNoise | `ls_detail_build.py` |
| `MF_RTS_LS_WetRuts` | Attributes, RutCentre, Damp, GrassWeight, SoilColor, DetailNoise (optional) | Attributes | `ls_detail_build.py` |
| `MF_RTS_LS_GrassWeights` | Grassiness, SandWeight, FieldMask, BlightMask (optional) | Green, Flowers, Dry | `ls_grass_build.py` |
| `MF_RTS_LS_RadixiteBlight` | Attributes, DirtColor (optional) | Attributes, Blight | `ls_blight_build.py` |

What each function does:

- **GroundBreakup:** noise-broken grass edges, dirt patches, dry grass, damp hollows and value jitter.
  - SoilColor and SoilNormal reuse the master's own boulder-layer samples, so it adds no texture fetch.
  - Grassiness, Damp and DirtColor feed the effects after it, so the map keeps one soil palette.
- **TankTrails:** ruts drawn into `RT_TankTrails` by `UTankTrailSubsystem` (`Environment/TankTrails.md`).
  - It works on any master. `M_RTS_Landscape_DesertPaintable` (Ger_FirstMission and others) calls it too since 2026-10-28, wired by `ls_trails_build.run_desert_paintable()`; its SoftGround is saturate(Sand + Weedy + DesertGravel + ScorchSandyStone).
- **FieldFurrows and WetRuts:** see `Environment/GroundDetail.md`.
  - DetailNoise is optional on WetRuts. Connecting FieldFurrows' DetailNoise to it saves a texture fetch.
- **GrassWeights:** see `Environment/GroundGrass.md`.
- **RadixiteBlight:** dead ground around radixite crystals, see `Environment/RadixiteBlight.md`.

The master supplies the layer weights:

- **GrassWeight:** the `GrassCastle` layer.
- **SandWeight:** saturate(Desert + RTSSand + GraySand).
- **SoftGround:** saturate(grass + sand), for the trails.

## Rebuilding

```python
import sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
import ls_master_build; ls_master_build.run()        # functions, master, switches in both instances
import ls_detail_build; ls_detail_build.write_fields() # with Ger_Trains loaded: the crop fields
```

1. `run()` rebuilds the six functions in place and replaces the master's tagged nodes (desc `LSMaster`).
   - A rebuild keeps each function's input and output nodes (matched by name), so every material that calls the function stays connected; a single function can be rebuilt on its own (`build_function()`).
   - The original hand-made layer graph is only moved and framed.
   - Older per-builder tags (`LSBreakup`, `LSTankTrails`, `LSGroundDetail`, `LSGroundGrass`) are removed too.
2. Once shaders have compiled, run Build → Build Grass Maps Only and save Ger_Trains; `-game` reads the grass maps stored in the level.
3. Comment frames need their sizes, and the output node needs moving next to `BreakMaterialAttributes_2`. Python can't set either; only `URTSMaterialGraphLibrary` (`Utils/MaterialGraph`) can.
   - If the editor that ran `run()` was built before that library existed, the comments come out small and the output node stays where it was.
   - The builders record each comment's size in its desc (`size=WxH`).
   - Once that editor has saved the assets, apply the layout from a DebugGame build, then reload the packages in the editor (Asset → Reload, or `EditorLoadingAndSavingUtils.reload_packages`):

```bash
UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script="<this folder>/ls_editor_layout.py"
```

## Gotchas (2026-10-28)

- **Don't use `delete_all_material_expressions_in_function`.** It removes only every other node per call; one pass once left duplicate function inputs, and the duplicated input pins broke the compile. `ls_graph.function_asset()` deletes the nodes one by one and keeps the inputs and outputs.
- **Callers bind to a function input's id.** Recreating an input unhooks every material that calls the function, which is why rebuilds keep them. `unreal.SystemLibrary.is_valid` filters deleted expressions that the object iterator still returns before garbage collection.
- **Reload a just-created asset first.** A function just created by `create_asset` rejects new nodes until `load_asset` returns it again.
- **Custom nodes are never shared.** Two identical custom nodes compile to two functions, and any texture fetch that depends on them happens twice. Identical plain math nodes are shared, but across function calls even those weren't, so the detail noise is passed between the functions explicitly.
- **The shader complexity view counts instructions, not what runs.** Branches that skip work far from fields or crystals don't lower the count; an unrolled loop counts once per iteration. Keep per-iteration work small (precompute on the CPU what you can).
- **Check every permutation, not just the default.** `get_statistics` on a child instance with each switch combination, compared before and after a change, catches a broken permutation. A broken one shows 0 instructions and samplers −1.

## Shader stats (pixel instructions / texture samples)

| Switches on | Flat graph | Functions | Plus blight, optimised (2026-10-28) |
|---|---|---|---|
| none | 236 / 6 | 236 / 6 | 236 / 6 |
| LS_NoiseBreakup | 404 / 39 | 404 / 39 | 404 / 39 |
| LS_TankTrails | 237 / 6 | 237 / 6 | 237 / 6 |
| LS_GroundDetail | 698 / 9 | 702 / 9 | 542 / 9 |
| LS_GroundGrass | 236 / 16 | 236 / 16 | 236 / 16 |
| LS_RadixiteBlight | | | 343 / 8 |
| all on (Ger_Trains) | 852 / 50 | 856 / 50 | 780 / 50 (952 before optimising) |
| DesertPaintable trails + blight | | 237 / 6 | 334 / 6 |

- **Field test:** the fence edges are precomputed as unit normals and offsets (`Field<i>_C`, `_D`, `_E`). That took the field test from about 30 to about 10 instructions per field.
- **Blight:** compares squared distances scaled by 1 / radius² with one square root at the end.
- **What stays the same:** the detail noise UV is plain math instead of a custom node (+4 instructions), and moving the furrow maths out of the loop by hand made it 4 instructions worse, because the compiler already shares it.
