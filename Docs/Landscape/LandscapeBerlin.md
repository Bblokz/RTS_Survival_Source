# M_RTS_Landscape_Berlin: the Ger_Berlin ground

Berlin 30 years after the 1945 strike: rubble, ash, broken cobbles, vitrified blast glass, dead park scrub and river mud. `ls_berlin_build.py` in this folder builds everything; don't hand-edit the master, because the next run replaces its whole graph.

| Asset | Path (`/Game/RTS_Survival/Materials/ground/Landscape/Berlin/`) |
|---|---|
| Master | `M_RTS_Landscape_Berlin` |
| Instance on the map | `MI_RTS_Landscape_Berlin` (`LS_TankTrails` on, `TankTrailStrength` 1) |
| Layer infos (weight-blended, no phys material, like DryCastle's) | `DATA/<Layer>_LayerInfo` |
| Textures (made by `Source/Tools/BerlinMap/berlin_textures_gen.py`) | `Textures/T_Berlin_<Layer>_D` (sRGB albedo, A = linear roughness), `Textures/T_Berlin_<Layer>_N` (normal map) |

## Layers

Each texture tile covers about 5 m.

- **Texture names:** `<Layer>` is the full layer name (`T_Berlin_BerlinRubble_D`). The builder also accepts the short form (`T_Berlin_Rubble_D`).
- **Luma mid:** the median linear luminance of the generated albedo (measured on the PNGs on 2026-11-12). The blend height centres on it, because ground albedo sits far below 0.5.
- **Height contrast:** about 0.25 / the luminance standard deviation, higher on the crisp hard layers.

| Layer | Ground | Tank ruts | Height offset / contrast / luma mid |
|---|---|---|---|
| `BerlinRubble` | base, soft | yes, at 60 % | 0.05 / 6 / 0.156 |
| `BerlinAsh` | soft | yes | -0.10 / 3 / 0.265 |
| `BerlinCobble` | hard | no | 0.15 / 8 / 0.160 |
| `BerlinGlass` | hard, glossy slag | no | 0.10 / 6 / 0.030 |
| `BerlinScrub` | soft, dead grass earth | yes | 0.00 / 4 / 0.065 |
| `BerlinMud` | soft, wet | yes | -0.15 / 8 / 0.045 |

If the textures are regenerated with a different brightness, update the luma mid defaults in `LAYERS`, or override `<Layer>_HeightLumaMid` in the instance.

## Pipeline

```
per layer: albedo (tile sample + large-scale sample mixed by macro noise) x tint, roughness = albedo A x scale,
           normal x (strength, strength, 1), blend height from albedo luminance
  -> LandscapeLayerBlend (LB_HeightBlend on all six)
  -> MF_RTS_ColorVariation_Landscape (InNoiseTillling 4, InNoiseStrength 0.5)   <- RVT output reads here
  -> MF_RTS_LS_TankTrails [LS_TankTrails], SoftGround = saturate(Rubble*0.6 + Ash + Scrub + Mud)
  -> BreakMaterialAttributes -> BaseColor, Metallic, Specular, Roughness, Normal, Emissive, AO
```

- **Height blend:** height = saturate((luma - `_HeightLumaMid`) x `_HeightContrast` + 0.5 + `_HeightOffset` + macro-noise jitter). The engine moves each layer's weight by its height, so cobbles and glass poke through at a transition and ash and mud fill the hollows, instead of an airbrushed fade.
- **Repetition:** each layer samples its albedo a second time at `Berlin_MacroScale` (0.23 x, offset per layer). One sample of `T_RTSNoise_MacroVariation` (the colour variation's texture) mixes the two between `Berlin_MacroBlendMin` and `Berlin_MacroBlendMax`. Normals use the tile sample only. `MF_RTS_TillingVariation` (engine TextureVariation: extra fetches plus gradient sampling) was left out for cost.
- **Samplers:** every sample uses the shared wrap sampler (`SSM_Wrap_WorldGroupSettings`), so the six layers' 12 textures cost no sampler slots. The sampler type follows each texture's own settings, so a different import (non-sRGB, masks) doesn't break the compile.
- **Runtime virtual texture:** the output is the same as DryCastle and DesertPaintable:
  - From the colour-variation result: BaseColor, Specular, Roughness, and Normal through Transform (tangent to world).
  - WorldHeight: absolute world Z.
  - The RVT caches its pages, so the tank trails come after it.
- **Tank trails:** `TankTrailStrength` is `MF_RTS_LS_TankTrails`' own parameter. Above 0 on the landscape material, `UTankTrailSubsystem` draws trails on the map (`Environment/TankTrails.md`).
- **Shared functions:** `MF_RTS_ColorVariation_Landscape` and `MF_RTS_LS_TankTrails` are only called, never rebuilt.

## Parameters (defaults)

| Group | Parameter | Default |
|---|---|---|
| Berlin Global | `Berlin_MacroScale` | 0.23 (large-scale sample UV = tile UV x this) |
| | `Berlin_MacroBlendMin` / `Berlin_MacroBlendMax` | 0.2 / 0.6 (share of the large-scale albedo, picked by the noise) |
| | `Berlin_NoiseSize` | 4000 cm (macro-noise tile) |
| | `Berlin_HeightNoise` | 0.3 (noise jitter on the blend heights) |
| per layer `<Layer>` | `<Layer>_TileSize` | 5 (metres per texture tile) |
| | `<Layer>_Tint` | (1, 1, 1) |
| | `<Layer>_RoughnessScale` | 1 |
| | `<Layer>_NormalStrength` | 1 |
| | `<Layer>_HeightContrast`, `<Layer>_HeightOffset`, `<Layer>_HeightLumaMid` | see the layer table |
| Glass | `BerlinGlass_RoughnessMin` | 0.2 (no mirror or sparkle from the RTS camera) |
| | `BerlinGlass_Specular` | 0.6 (other layers 0.5) |
| Mud | `BerlinMud_Wetness` | 0 (0 = dry) |
| | `BerlinMud_WetDarken` | 0.55 (albedo factor when fully wet) |
| | `BerlinMud_WetRoughness` | 0.25 |
| Tank Trails | `LS_TankTrails` (switch) | off in the master, on in the MI |
| | `TankTrailStrength` | 0 in the master, 1 in the MI |
| | `TankTrail_Tint`, `_Roughness`, `_NormalStrength`, `_RutLow`, `_RutHigh` | the function's defaults |

## Rebuilding

```python
import sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
import ls_berlin_build; ls_berlin_build.run()        # run(stats=False) skips the shader stats
```

Headless:

```bash
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<script calling run()> -unattended -nosplash -AllowCommandletRendering
```

- **What a run keeps:**
  - The master asset is kept. Its expressions are all deleted one by one and rebuilt.
  - The instance and layer infos are only created when missing, so their overrides survive. Tested: an instance override kept its value through a rebuild.
- **Textures:**
  - A missing texture falls back to `/Engine/EngineMaterials/DefaultDiffuse` or `DefaultNormal`. The run lists it in `fallback textures` and the log.
  - Run again after importing the real ones.
- **Report:** `run()` returns layer infos created, instance created, fallback textures, stats and failures. Any failure entry needs looking at; "save failed" means an editor holds the package.

## Shader stats (pixel instructions / samplers / estimated texture samples)

Measured on the instance with all six layers present: every preview weight is above 0, so this is the worst case. A landscape component only compiles the layers painted on it.

| Permutation (real textures, 2026-11-12) | Stats |
|---|---|
| `LS_TankTrails` off (the master's own shader map) | 237 / 2 / 41 |
| `LS_TankTrails` on (`MI_RTS_Landscape_Berlin` as shipped) | 236 / 2 / 41 |

- **Off versus on:** the trail sample sits in a uniform `[branch]`, so the two differ by the compiler's ±1.
- **Texture samples:** the engine's estimate is 41. The graph has 18 layer fetches, 1 macro noise and 3 in the colour variation, so 22 unique; the estimate seems to count code that the normal and the other attributes both emit.
- **Comparisons from the same machine:**
  - `MI_RTS_Landscape_DryCastle_Inst` measures 343 / 5.
  - `MI_RTS_Landscape_DesertPaintable` measures 334 / 4.
  - The sampler counts here read lower than `LandscapeMaster.md`'s table (334 / 6 there), so compare samplers on one machine only.
- **Not a landscape compile:** the stats compile the material on a plain mesh. The landscape's own per-component permutations (weightmap samplers) haven't been compiled yet; check them on the map first.

## Map side (Ger_Berlin)

- **Landscape actor:**
  - Material: `MI_RTS_Landscape_Berlin`.
  - Runtime virtual textures: `RVT_Environment` and `RVT_Height` (`/Game/RTS_Survival/Meshes/RVTs/`).
  - Virtual texture render pass: Always, as `berlin_map_build.py` / `kessel_map_build.py` already do.
- **Layer infos:**
  - Use the six `DATA/<Layer>_LayerInfo`; any order works.
  - `BerlinRubble` is the base fill.
- **No grass output:** there's no `LandscapeGrassOutput`; add one if the scrub should carry grass cards.
- **Ground surface bake:** the layers aren't listed in `URTSGroundSurfaceSettings` (`Environment/GroundSurface`), so the track-smoke surface bake counts them all as Dirt.
  - Cobble and Glass would want Concrete or Rock.
  - Ash and Mud would want Sand or Dirt.

## Gotchas (2026-11-12)

- **Layer info names:**
  - `LandscapeLayerInfoObject.LayerName` is read-only to `set_editor_property`, so the builder sets it with the console `set <object path> LayerName <name>` command.
  - The builder reads the name back and never saves a misnamed info.
- **Instance setters:** `set_material_instance_static_switch_parameter_value` and `_scalar_` return False on a just-created instance even when they apply, so the builder reads the values back.
- **Stats need a renderer:**
  - Under `-nullrhi`, `get_statistics` returns 0 / 0 for every material, and so does a plain commandlet without `-AllowCommandletRendering`.
  - Calibration: `MI_RTS_Landscape_DesertPaintable` gave 334 instructions, matching `LandscapeMaster.md`.
- **Stats per permutation:**
  - Toggling the switch on the saved instance, or on transient children of it, gave numbers that didn't follow the switch.
  - The builder reads the master (off) and the saved instance (on) instead.
- **Second rebuild warning:** a second `run()` in the same editor session logs `LogMaterial: Warning: M_RTS_Landscape_Berlin: Requesting an invalid TextureIndex! (1 / 0)` once, while the old graph is being deleted.
  - The rebuilt material compiles and measures the same afterwards.
  - A fresh process never logs it.
- **Exit code 3:** a headless script that called `get_*_parameter_names` and other stats after `run()` crashed in Python shutdown (exit code 3) after every save had finished. `run()` on its own exits 0.
