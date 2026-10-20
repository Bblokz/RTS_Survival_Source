# Aim ability crosshairs

The reticle shown on the ground while the player aims an ability (attack ground, barrages, grenades).

## How it works in game

- `ACPPController` spawns `BP_PlayerAimAbilityCursor` (`APlayerAimAbility`). While an aim ability is active it
  moves the actor to the cursor each tick and turns its +X axis from the primary selected unit towards the cursor
  (tilted to the ground normal, clamped to 20 degrees).
- `APlayerAimAbility::DetermineShowAimRadiusForAbility` / `ShowGlobalAbilityRadius` take an
  `EPlayerAimAbilityTypes` and a radius from the ability component, pick the material from the Blueprint's
  `MaterialPerAbilityAim` map and scale the actor by `Radius / UnitsPerScale`.
- The mesh is `/Engine/BasicShapes/Plane` (100 x 100 cm). Its UVs run along the actor's +X/+Y, so the texture
  turns with the direction of fire.

| Aim type | Typical use | Material instance | Texture |
| --- | --- | --- | --- |
| `SmallCrosshair` | attack ground (radius 200) | `MI_AimCrosshair_Small` | `T_AimCrosshair_Small` (512) |
| `MediumCrosshair` | mid barrages | `MI_AimCrosshair_Medium` | `T_AimCrosshair_Medium` (1024) |
| `LargeCrosshair` | artillery / rocket barrages | `MI_AimCrosshair_Large` | `T_AimCrosshair_Large` (1024) |
| `LargeComplexCrosshair` | heavy barrages | `MI_AimCrosshair_LargeComplex` | `T_AimCrosshair_LargeComplex` (1024) |
| `AntiInfantryGrenade` | frag grenades, molotovs | `MI_AimCrosshair_GrenadeAntiInfantry` | `T_AimCrosshair_GrenadeAntiInfantry` |
| `AntiVehicleGrenade` | AT grenades, bundle charges | `MI_AimCrosshair_GrenadeAntiVehicle` | `T_AimCrosshair_GrenadeAntiVehicle` |
| (not wired) | alternative Large look the user picked | `MI_AimCrosshair_LargeClassic` | `T_AimCrosshair_LargeClassic` |

`LargeClassic` is the first Large design with its warm look (pale orange, cream, soft brown contour, warm
vignette), kept unchanged on purpose; its instance carries those colours (`material_overrides` in
`crosshair_designs.py`). Put it on any aim type in the Blueprint map to use it.

State on 2026-10-19: the open editors ran a build without `LargeComplexCrosshair`, so `wire` mapped the other five
only, and the Blueprint save was blocked by a second editor holding the file. After a rebuild: close the other
editors, run `wire` again, save, and confirm with `run_capture.sh` without `WIRE`.

Assets live in `/Game/RTS_Survival/Materials/AimMaterials/Crosshairs/`. `LargeComplexCrosshair` was appended to
the enum; keep the enum append-only, ability Blueprints store its values.

### Layout contract (generator, material and Blueprint depend on it)

- Every design draws its area-of-effect ring centred at `AOE_RING_RADIUS = 0.88` of the texture half-size, so
  `UnitsPerScale = 50 * 0.88 = 44` on the Blueprint puts the ring exactly on the ability radius.
- The direction of fire is texture +X (the right-hand side of the image).
- Nothing is drawn beyond `SAFE_RADIUS = 0.975`; the generator refuses designs that do (textures clamp, a border
  texel would smear across the plane).

### Texture format

Channel-packed masks, not pictures: R primary strokes, G accent strokes, B dark outline grown around all strokes,
A the union (a plain alpha mask). Imported linear (sRGB off), BC7, clamp addressing, Effects LOD group, never
streamed (the reticle appears instantly and must be sharp on its first frame).

### Material `M_AimCrosshair`

Unlit, alpha-composite (premultiplied) translucency, two sided, depth test off so slopes and unit hulls never cut
holes into the reticle. One Custom node (`AimCrosshairShading`) does the compositing; parameters:

| Parameter | Use |
| --- | --- |
| `CrosshairMask` | the channel-packed texture |
| `PrimaryColor`, `PrimaryIntensity` | main strokes |
| `AccentColor`, `AccentIntensity` | secondary strokes |
| `OutlineColor`, `OutlineOpacity` | dark contour that keeps strokes readable on bright ground |
| `FillColor`, `FillOpacity`, `FillInnerFraction` | darkening vignette inside the ring, strongest at the ring |
| `RingRadiusUV` | 0.88, the layout contract above |
| `OcclusionToleranceCm`, `OcclusionFadeCm`, `OccludedOpacity` | where the scene is more than the tolerance in front of the plane (a unit), the reticle fades to `OccludedOpacity`; terrain bumps stay below the tolerance |
| `PulseAmount`, `PulseSpeed` | optional brightness pulse (off by default) |
| `Opacity` | overall fade |

The emissive is multiplied by `EyeAdaptationInverse`, so intensity 1 looks the same in bright and dark scenes.
Colours are linear: the default primary (1.0, 0.045, 0.008) shows as red-orange after the tonemapper; sRGB-looking
values such as (1.0, 0.3, 0.1) come out pale orange and vanish on desert sand. Primary and accent are blended by
weight, not added, so their anti-aliased edges never form bright seams.

## Making or changing crosshairs

Generator: `D:\UE5Projects\RTS_Survival\PythonAutomation\TargetCrosshairs` (numpy + Pillow).

- `crosshair_sdf.py`: signed distance primitives (rings, arcs, dashed rings, bars, chevrons, polygons) with
  analytic anti-aliasing, so edges are clean at any resolution and survive mip mapping.
- `crosshair_designs.py`: one builder per design plus the stroke weights; stroke widths are chosen for the size the
  reticle has on screen (the small one is drawn much bolder).
- `generate_crosshairs.py`: writes `Output/T_AimCrosshair_*.png` and `crosshairs_manifest.json`.
- `preview_crosshairs.py`: contact sheets that shade the masks like the material over four grounds, at game size,
  through a simulated mip chain.
- `unreal_setup_crosshairs.py`: `assets` imports the textures and (re)builds the material and instances,
  `wire` sets `MaterialPerAbilityAim` and `UnitsPerScale` on `BP_PlayerAimAbilityCursor`. The editor that runs
  `wire` must know `LargeComplexCrosshair` (rebuilt C++); otherwise that entry is skipped.

## Headless check

`UAimCrosshairCaptureSubsystem` (`Utils/RTSMapTests/AimCrosshairCapture`, compiled only with `RTS_WITH_MAP_TESTS`,
never in shipping) spawns the aim cursor Blueprint in an offscreen game run, shows each case through
`ShowGlobalAbilityRadius` and saves Close / Game / Far scene captures.

```
Source/Tools/AimCrosshairs/iterate.sh            # generate, preview, import, capture, contact sheets
Source/Tools/AimCrosshairs/run_capture.sh [out]  # capture only
```

Switches: `-AimCrosshairCapture`, `-AimCrosshairCases=Label=AimType:Radius[:/Game/Material]+...`,
`-AimCrosshairAt=X,Y;X,Y`, `-AimCrosshairYaw=`, `-AimCrosshairClass=`, `-AimCrosshairDir=`,
`-AimCrosshairWireUnitsPerScale=44` (puts the new materials on the spawned actor before the Blueprint itself is
wired). Runners use the DebugGame editor binary; results land in `Saved/AimCrosshairs`.

Gotchas:
- Freshly imported textures build on first use; the test shows every case once (warm-up) before taking pictures,
  or the first case comes out blank.
- The runner cannot save over packages the open editors hold (`BP_PlayerAimAbilityCursor`): wire it through the
  editor.
