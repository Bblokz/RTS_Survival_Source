# Crushable Crop Fields (wheat, pumpkins, vineyards) and farm fences

Code: `Source/RTS_Survival/Environment/Splines/CrushableCropField/CrushableCropFieldActor.h/.cpp`
Shared crush rule: `Environment/DestructableEnvActor/CrushDestructionTypes/FRTSCrushGate.h/.cpp`
Art pipeline: `D:/Blender/BlenderProjects/BlenderTanks/FarmCrops/` (`build_atlas.py`, `build_pbr.py`, `build_meshes.py`, `render_preview.py`)

## Why not one spline piece per plant

The crushable pipes (`ADestructibleSplineActor`) give every piece its own `USplineMeshComponent` with a physics body
and overlap events. That is right for a fence (tens of pieces) but a wheat field has thousands of plants, so the crop
field keeps everything the pipes share and drops the per-piece bodies:

| Shared with the crushable pipes | Crop field specific |
|---|---|
| `ECrushDestructionType` + `FRTSCrushGate` (one implementation, the spline actor calls it too) | Plants are ISM instances without collision: one component per standing variant, one per distinct crushed mesh |
| `FCollapseFX` (VFX + SFX, new `SpawnVfxAtLocation` / `PlaySfxAtLocation`) | Every `VehicleScanInterval` (0.25 s) one overlap query over the field finds vehicles; the field owns no collision primitive, so cover-finder, guard-surface and AOE object queries (which include `ECC_Destructible`) never see it |
| Spline authoring (closed `FieldOutline`, rows along local X) | While a vehicle passing the crush rule is inside, its oriented footprint is sampled every `CrushSampleInterval` against a grid of plants |
| Vehicle object channels (`COLLISION_OBJ_PLAYER` / `_ENEMY`) | A crushed plant swaps to the variant's crushed mesh lying in the vehicle's heading, so tanks leave trails |

Fences around the fields ARE plain `ADestructibleSplineActor`s (`BP_DestSpline_FarmFence`).

Each plant gets its height from a world-static line trace (30 m above and below the actor). The trace uses the first hit that is not an `AVolume`, so the lids of PCG, trigger and post-process volumes are never treated as ground. Before this rule, the 2026-10-27 `PCG_GroundStones` volume lifted every field to its top face.

## Navigation (vineyards block light tanks)

`StandingPlantsNavArea` marks one nav cell per standing plant (plant spacing x row spacing) through an
`UApartmentFootprintNavModifier`; crushed plants drop their cell (refresh merged per 0.5 s, the map runs
`DynamicModifiersOnly`). Blocking per agent works through the per-agent query filters, not through the area itself:
`RTS_NavArea_BlockLight` is excluded only in `RTS_NavFilter_LightTanks`. So vineyards: light tanks path around,
medium/heavy tanks (crush type `MediumOrHeavyTank`) drive through and open the cells; infantry walks through.
Wheat and pumpkins leave the area empty and are crushed by any tank.

## Rendering

- One material for every crop and the fence: `M_FarmCrops` (masked, two-sided) on the 2048 atlas `T_FarmCrops_D`
  (alpha = opacity), `_N`, `_MAR` (R metal, G AO, B rough, A foliage transmission).
- Per-instance tint: the actor writes a 0..1 seed into per-instance custom data 0 on both the standing and the crushed
  instance; the material reads `PerInstanceCustomData[0]` (default 0.5 for non-instanced meshes such as the fence).
- Plants never get removed from their ISM (indices stay valid): a crushed standing instance is shrunk and dropped
  500 uu; render state is marked dirty once per crush batch. Crushed remains get a random 0..1.5 uu lift so the flat
  cards of neighbouring plants never z-fight.
- Wheat (v3 realism pass, 2026-10-27): muted tan-gold, nearly neutral per-plant tint (`InstanceTintLow/High`
  0.84 / 0.95). Canopy cards are flat, about 1.7x the plant spacing, and thin out through a noisy rim, so the stand
  reads as one carpet instead of puffs. `MissingPlantChance` is 0.004.
- Burned-out wheat (`BP_CropField_WheatBurned`, `SM_WheatBurnt_A/B/_Crushed`, `NS_FarmCrush_Ash`):
  - Each plant is a slightly domed scorched-ground card plus charred stubble cards. Crushed plants become lighter
    trampled ash, so trails show. Any tank crushes it, and its shadows are off.
  - It uses `MI_FarmCrops_Burnt`, a child of `M_FarmCrops` on the same atlas. That instance has no per-plant tint,
    grey field patches and no transmission. Overlapping ground cards show up as cells as soon as neighbours differ
    in tone, so the ash texture keeps only fine-scale contrast.
- Hay props (plain static meshes, same `MI_FarmCrops` / atlas, with collision):
  - Meshes: `SM_HayStack_A` (beehive with pole), `SM_HayStack_B` (rick), `SM_HayBale_Round_A/B` and
    `SM_HayBale_Round_Stack`.
  - Atlas regions: `hay_strip` (tiles along V), `hay_weathered` (stack caps), `bale_end` and `hay_fringe` (wisp
    cards, LOD0 only).
  - Stacks get 4 convex hulls, bales get boxes. `ue_place_farms.py` `place_hay()` puts them around the wheat.
- ISM, not HISM: with HISM only the first component created during the construction script rendered in the editor
  (2026-10-27); ISM is GPU-scene culled per instance and cheaper to update while tanks crush.

## Ger_Trains placement

The request was three farm fields in front of the city walls, each with a different plant, and each area split into
several fenced patches, some with distance between them. So the map has three farm areas, nine patches in total
(folder `Farm`, re-runnable `ue_place_farms.py`):

| Farm area | Patches | Distance to the nearest wall segment |
|---|---|---|
| Wheat | `Farm_Wheat_W1-3` (one rotated 90 deg) | 53-77 m |
| Pumpkins | `Farm_Pumpkin_P1-3` (rotated 90 / 5 deg) | 100-118 m |
| Vineyard | `Farm_Vineyard_V1-3`, east of the rail line | 109-146 m |

- `Farm_WheatBurned_W4` is an extra burned-out wheat patch west of W2. It is placed last in `PATCHES`, so the other
  patches keep their seeds.
- Each patch has its own `Farm_Fence_*` with a 4 m gate.
- Every patch lies on the +Y side of the Soviet wall line (`Rus_Building_Wall*` / `GateWired*`, y -11k..-22k). That
  is the open approach from the player start (y about +19k). The city is behind the walls at y < -11k. Overview:
  `Saved/FarmCrops/farms_vs_walls.png`.

## Sounds

The project has no foliage rustle / vegetable squash audio, so the crush cues are built from the closest existing
material (all in `FarmCrops/Sounds`, random without replacement + pitch modulator; `Att_Collapse` / `SC_Collapse`):

| Asset | Cue | Source waves |
|---|---|---|
| Wheat | `Cue_SpecialBullet_GrassHit` (existing) | grass bullet hit |
| Pumpkin | `Cue_FarmCrush_Pumpkin` | `WARFARE_SOUNDS` bullet_impact_mud_01-08, pitch 0.65-0.85 |
| Vineyard | `Cue_FarmCrush_Vine` | bullet_impact_wood_light_01-08, pitch 0.8-1.0 |
| Fence | `Cue_FarmCrush_Wood` | bullet_impact_wood_heavy_01-08, pitch 0.72-0.9 |

These are stand-ins: swap in dedicated crop-crush recordings by replacing the waves in the cues (the BPs reference the
cues). `CrushSoundCooldown` (0.45 s) keeps a tank driving through a field from stacking cues.

## Explosions

`TakeDamage` flattens plants within `ExplosionCrushRadiusFraction` of a radial damage event's outer radius, but only
for damage applied to the field directly (it has no collision for AOE sweeps to find). `CrushPlantsInRadius` is BlueprintCallable for scripted events.
