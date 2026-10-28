# Ground detail on Ger_Trains (material-only)

Wet tank ruts and ridge-and-furrow field soil are drawn by the Ger_Trains landscape material itself. There are no meshes or decals and no extra draw calls; everything is shader work on landscape pixels.

| Part | Where |
|---|---|
| Builder | `Source/Docs/Landscape/ls_detail_build.py`: `build_furrows_function()` and `build_wet_ruts_function()` build `MF_RTS_LS_FieldFurrows` and `MF_RTS_LS_WetRuts`, and `write_fields()` writes the crop fields into the instance. The master is wired by `ls_master_build.run()` (`Landscape/LandscapeMaster.md`) |
| Switch | `LS_GroundDetail` in `M_RTS_Landscape_DryCastle` (off by default, on only in `MI_RTS_Landscape_DryCastle_Trains`); it sits after `LS_TankTrails` and before the main BreakMaterialAttributes, so the RVT output never sees it |
| Fields | `Field<i>_A–E`, `FieldBounds`, `FieldCount` instance parameters, from the level's `ACrushableCropFieldActor`s (4-point splines), up to 10 fields |
| Grass | `MF_RTS_LS_GrassWeights` keeps grass out of the fields (FieldMask) |

## What it draws

- **Wet ruts.** The raw rut depth from the trail sample (alpha channel) is masked three ways:
  - by the trail's soft-ground mask and `TankTrailStrength`;
  - by the damp mask;
  - by the GrassCastle layer, because the damp mask also covers every low-lying desert.

  Mud fills each rut from its centre line outwards. The mud level varies along the rut with 3.7 m noise, so the mud comes in tapered streaks. It is darkened, desaturated soil, with roughness never below 0.45.
- **Furrows.** One custom HLSL node tests each field's real outline, an oriented quad that follows the fence.
  - The edge is roughened by noise.
  - Furrow bottoms are desaturated and darkened (`Furrow_Darken` 0.75) and ridge tops are lit; the normal tilts with the furrow slope (`Furrow_Depth` 9).
  - Pixels outside `FieldBounds` exit at once. Inside, the unrolled loop tests each field's four fence edges at once: `write_fields()` stores them as inward unit normals (`Field<i>_C`, `_D`) and offsets (`_E`), so a field costs two vector multiply-adds and three mins. Move or add a crop field, then rerun `write_fields()`.

## Puddles were removed (2026-10-28)

Puddles in damp hollows and water in ruts were built, reviewed and then removed at the user's request. The damp mask also covered the whole low desert, so it filled with blue-grey splotches, and the water never read well from the RTS camera. The lessons are still useful if puddles come back:

- **Noise channel:** the B channel of `T_RTS_PackedNoise` is fine grain and averages to about 0.72 at the mips the camera samples, so it can't make puddle shapes. R keeps its spread.
- **Thresholds:** noise percentiles from the exported texture didn't match the game. Calibrate with instance overrides and headless captures.
- **Emissive:** emissive on this landscape never reached the `-game` capture (final LDR colour), not even at 10, so a reflection fake has to go through albedo. The master's emissive input stays wired to the main break; with nothing upstream it folds to 0.

## Rebuild

1. Run `ls_master_build.run()`; it rebuilds every landscape function and the master (`Landscape/LandscapeMaster.md`).
2. With Ger_Trains loaded, run `ls_detail_build.write_fields()` after you move or add a crop field.
3. Once shaders have compiled, run Build → Build Grass Maps Only and save `Ger_Trains`.

## Gotchas (2026-10-28)

- **The setter's return value lies.** `set_material_instance_scalar_parameter_value` returns False on this instance even when the value is set; read `scalar_parameter_values` back instead.
- **The editor bridge's Python sandbox can't open files outside the project.** Inline values into the script.

## Review rounds

Two adversarial critique rounds were applied:

- **Round 1:**
  - invisible wet ruts, and roughness below Lumen's trace limit;
  - grass-tinted field soil and blurry furrows;
  - outlines that didn't follow the fences;
  - a field loop running on every pixel.
- **Round 2:**
  - plank-like wet ruts that ignored the soft-ground mask;
  - doubly darkened furrows and a slow field loop;
  - ruler-straight field edges.

## Cost

The setup was 40 Panzer IVs at 1920×1080 (DebugGame). Two runs of `run_bench.sh <name> -TankTrailBenchProfileGPU`, one with the switch off and one with it on, were compared with `compare_profilegpu.py`. Both runs were taken with puddles still in the chain, so today's cost is a little lower.

| Run | Main-view base pass | Base-pass draws |
|---|---|---|
| `costDetailOff` | 0.270 ms | 348 |
| `costDetailOn` | 0.275 ms | 348 |

So the cost is about 5 µs, with no extra draws. Both runs ran under heavy machine load (GPU frame 13.8 and 14.5 ms, against 5.87 ms for the same setup in `costTop`). Compare passes, not frame totals.
