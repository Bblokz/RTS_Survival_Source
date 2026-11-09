# Shell craters (landscape impact decals)

Shells larger than 15 mm that hit a **landscape** leave a crater decal at the impact. Nothing else does: hits on
units, buildings, props, roads (spline meshes) and air bursts leave nothing. Works on every map with a landscape,
with no per-map setup.

## Code

| What | Where |
|---|---|
| Runtime pool | `Environment/ImpactDecals/LandscapeImpactDecalSubsystem.h/.cpp` (`ULandscapeImpactDecalSubsystem`, a world subsystem in every game world) |
| Tuning (bands, material, merge) | `Environment/ImpactDecals/RTSImpactDecalSettings.h/.cpp`: Project Settings -> Game -> RTS Impact Decals |
| User setting | `Game/UserSettings/GraphicsSettings/ImpactDecalsLevel.h` (Off / 50 / 100 / 200 / 400), Graphics -> "Shell Craters" combo (`M_ComboImpactDecals`, BindWidgetOptional) |
| Console variable | `rts.ImpactDecals` = most craters at once, 0 = off (removes the craters on the map and spawns none) |
| Benchmark | `Environment/ImpactDecals/Tests/ImpactDecalBenchmarkSubsystem.*`, runner `Source/Tools/ImpactDecals/run_bench.sh` |
| Art | `/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/` (one material, 4x4 atlas), authoring in `Source/Tools/ImpactDecals/` |

Hooks (all call `ULandscapeImpactDecalSubsystem::RequestImpactDecal(context, calibreMm, location, normal, hitActor)`):
- `AProjectile::SpawnExplosionHandleAOE`: every projectile impact (normal is the Z axis of the impact rotation).
- `AProjectile::HandleTimedExplosion`: `RequestImpactDecalBelowExplosion` traces down `MaxTimedExplosionGroundGapCm`
  (150) and leaves a crater when the landscape is that close. Arcing artillery is timed to land on its target, so its
  explosion often comes from the flight timer before a trace touched the ground; without this, StartGame's backdrop
  battles left 0 craters from 72 such shells. Higher air bursts still leave nothing.
- `UWeaponStateTrace::OnAsyncTraceHitValidActor`: trace weapons (autocannons) that hit.
- `ABombActor::OnAsyncTraceComplete` and `AICBMActor::HandleImpact`: bombs and rockets.

`RequestImpactDecal` rejects in this order, cheapest first: calibre <= `MinCalibreMm` (15), `rts.ImpactDecals` == 0,
hit actor is not an `ALandscapeProxy`. Only then does it touch the subsystem. It snaps the crater height onto
`ALandscapeProxy::GetHeightAtLocation` (one map lookup + one heightfield sample): traces hit the landscape's simple
collision, which on Ger_MarderStugMission sat up to 90 cm off the drawn surface, enough for a thin small-calibre
crater box to miss the ground.

## How a crater is placed

1. Band = first of the 8 bands whose `MaxCalibreMm` covers the calibre (last band takes everything larger):
   20 / 34 / 50 / 75 / 88 / 112 / 150 / 240 mm design calibres, 40 / 70 / 110 / 160 / 210 / 290 / 400 / 600 cm.
2. Size = band design diameter x clamp(calibre / design calibre, 0.8, 1.25) x random 0.92-1.08 x global multiplier.
   A calibre never borrows another band's art at a silly scale.
3. Projection along the landscape normal (thin box, 40-180 cm deep by band), random spin around it.
4. Decal Color picks the atlas cell: R = (cell + 0.5) / 16 (band b uses cells 2b, 2b+1), G = random variation, A = opacity.
5. Merge: a crater whose centre lies within 35 % of an existing same-or-larger crater's radius (and is not bigger than it)
   adds nothing, so a barrage on one spot does not stack overdraw or push out older craters elsewhere.

## Why it is cheap

- One shared material, no material instances or dynamic instances; per-crater variation is the Decal Color.
- A ring of `UDecalComponent`s on one transient owner actor, created once each up to the cap; when full the oldest is
  moved to the new impact (one render-state update at end of frame). No actor spawns, no garbage per crater.
- Cap change at runtime: shrinks to the newest N (destroys the rest); Off destroys all and the owner.

## Art

`MI_LandscapeImpactDecal` (parent `M_LandscapeImpactDecal`, Deferred Decal, DBuffer colour + normal + roughness):
2 textures, `T_LandscapeImpact_BCO` (albedo + opacity, BC3 sRGB) and `T_LandscapeImpact_NRS` (normal XY, roughness,
soot mask, BC3 linear), 2048², 10.7 MB together. 262 PS instructions (a constant DBuffer decal is 219), 2 samples.
Band b uses cells 2b (round crater) and 2b+1 (oblique hit / grazing furrow for 20-37 mm).
Material params on the MI: Tint, ScorchColor, ScorchStrength, Opacity, NormalStrength, RoughnessMul, SlopeFadeStart/End
(receiver steeper than ~60° fades out, so vertical hull sides and cliffs stay clean; flat hull tops inside the thin box
still catch a crater).
Made procedurally (`Source/Tools/ImpactDecals/gen_atlas.py`, previews `preview.py`, UE import/material build
`ue_build_material.py`) over three adversarial review rounds: `Source/Tools/ImpactDecals/REVIEW_LOG.md`.
Contact sheet: `Saved/ImpactDecals/final/contact_sheet_all_bands.png`.

Each band is generated with its own parameters (from `Saved/ImpactDecals/r3/atlas_meta.json`), not one crater rescaled:

| band | design calibre | kind | crater dia / box | depth / dia | rays (count, length cm) | clods (count, size cm) | cell B |
|---|---|---|---|---|---|---|---|
| 0 | 20 mm | scuff | 0.21 | 0.18 | 8, 15 | 12, 0.8-2.6 | furrow |
| 1 | 34 mm | pit | 0.29 | 0.25 | 9, 27 | 18, 1.2-4.6 | furrow |
| 2 | 50 mm | pit | 0.31 | 0.18 | 10, 38 | 22, 1.5-6.0 | oblique |
| 3 | 75 mm | crater | 0.36 | 0.14 | 12, 56 | 28, 2.0-8.0 | oblique |
| 4 | 88 mm | crater | 0.39 | 0.15 | 14, 88 | 38, 3.0-11.0 | oblique |
| 5 | 112 mm | crater | 0.41 | 0.20 | 16, 122 | 52, 4.0-15.0 | oblique |
| 6 | 150 mm | crater | 0.47 | 0.26 | 18, 170 | 90, 6.0-32.0 | oblique |
| 7 | 240 mm | crater | 0.53 | 0.27 | 22, 255 | 140, 8.0-46.0 | oblique |

## Measured (2026-11-09, DebugGame, Ger_Trains, 1920x1080, shared machine)

`run_bench.sh final1 -ImpactDecalBenchProfileGPU`: 100 craters over 10 s, hold, 100 more (recycling), 100 in one frame,
for the pool and for the old way (UGameplayStatics::SpawnDecalAtLocation per crater + DestroyComponent of the oldest).

Game + render thread per crater, including the end-of-frame render state update and a render thread flush
(one-frame micro steps, median of 8; immune to other programs' frame noise):

| per crater | pooled | legacy |
|---|---|---|
| create (ring not full yet) | 11.6 us | 10.7 us |
| replace the oldest (steady state in a battle) | **2.8 us** | 10.6 us |
| 100 craters over 10 s, steady state | 0.028 ms per second | 0.106 ms per second |

GPU (ProfileGPU, DBuffer decal pass with 100 craters in view at RTS zoom): 0.02-0.12 ms.

Same bench on the **Development** editor build (`BIN=Development run_bench.sh dev_bench -ImpactDecalBenchProfileGPU`):

| per crater (median of 8) | pooled | legacy |
|---|---|---|
| create (ring not full yet) | 10.3 us | 8.4 us |
| replace the oldest | **2.9 us** | 12.1 us |
| 100 craters over 10 s, steady state | 0.029 ms per second | 0.121 ms per second |
| game thread per call inside the 10 s fill (no flush) | 67.5 us create, 19.6 us recycle | 74.7 us create, 82.1 us recycle |

ProfileGPU decal pass, Development: 0.02 ms for 79-89 visible craters, 0.21 ms for 72, 0.91 ms on the one capture
taken while the pass was first compiling/streaming (100 visible). The hold step (100 craters standing) added no
measurable game/render/GPU time for the pool (deltas -1 to -2 ms, i.e. noise); legacy hold +5.2 ms game p50 is the
same noise, not a real cost.

Oldest-first removal (`-ImpactDecalBenchLive`, `RunEvictionCheck`, Ger_Trains): cap 10, ten craters at known points
-> count 10; an 11th -> count 10, the first (oldest) gone, craters 2-11 present; cap lowered 10 -> 5 -> count 5,
the newest five kept. Both checks PASS.
Legacy decals also keep the engine default FadeScreenSize 0.01, which hid 25 of the 100 craters (all 20-37 mm) at this
zoom; the pool uses 0 so small calibres stay visible.

Whole-frame deltas (wall/game/render/GPU per step, in the log and `ImpactDecalBench_Frames.csv`) were swamped by other
sessions' game runs and builds on the same GPU (render baselines 7-43 ms between steps); use the micro and ProfileGPU
lines. Per-call game thread cost inside the 10 s fill (no flush, cold caches): ~70 us create, ~18 us recycle.

Live game checks (`run_bench.sh <name> -ImpactDecalBenchLive [-ImpactDecalBenchNoStart]`): the map's own fighting for
150 game seconds, counting shells over 15 mm that reach the landscape check, then the real escape menu widget's Shell
Craters combo is set to Off, 200 and back through the settings subsystem's Apply (runner restores GameUserSettings.ini):

| map | landscape impacts | other impacts (units, air bursts) | craters + merged | craters off landscape | menu |
|---|---|---|---|---|---|
| Ger_Trains | 64 | 182 | 41 + 23 | 0 | Off -> 0 and craters removed, 200 -> 200, restored |
| StartGame (backdrop battles, after the timed-explosion fix) | 55 | 197 | 55 + 0 | 0 | same, PASS |
| Ger_FirstMission (micro + picture, no fighting in 150 s) | traced 400/400 landscape points | - | renders on the desert landscape; mesh road lanes get none | 0 | - |
| Ger_Trains, **Development** build | 81 | 163 | 67 + 14 | 0 | PASS |
| StartGame, **Development** build | 29 | 144 | 29 + 0 | 0 | PASS |
| Ger_FirstMission, spawned tank fight | 24 | 155 | 24 + 0 | 0 | PASS |
| Ger_MarderStugMission, spawned fight | 273 | 892 | 263 + 10 | 0 (2 of 4 runs before the height snap: 1 crater 90 cm off) | PASS |
| Ger_MarderStugMission, spawned fight, after the height snap (x2) | 367 / 367 | 1014 / 910 | 350 + 17 / 348 + 19 | 0 / 0 | PASS |
| Ger_FirstMission / RedLine / Ger_DestroyT35, **Development**, spawned fight | 21 / 77 / 36 | 195 / 205 / 265 | 20+1 / 77 / 36 | 0 | PASS |
| Ger_MarderStugMission, **final Development build** (height snap), spawned fight | 319 | 919 | 303 + 16 | 0 | PASS |
| RedLine, spawned fight | 71 | 253 | 70 + 1 | 0 | PASS |
| Ger_DestroyT35, spawned fight | 30 | 360 | 30 + 0 | 0 | PASS |
| Ger_Trains, map fighting + spawned fight, with the eviction check | 76 | 183 | 76 + 0 | 0 | PASS (eviction + menu) |
| MainMap, **Development**, spawned fight | 16 | 172 | 15 + 1 | 0 | PASS |
| DesignBaseMap, **Development**, spawned fight (map has **no landscape**, static mesh floor) | 0 | 108 | 0 | 0 | correct: no craters; the bench's "hit the landscape" expectation does not apply |

`-ImpactDecalBenchSpawnFight` spawns 3 Panzer IV H vs 3 T-34/76 every 30 s (centre falls back to the middle of the
map's landscape). The off-landscape test compares each crater with `ALandscapeProxy::GetHeightAtLocation` (75 cm);
a first version that traced from above counted craters under trees, trenches and new wrecks as off-landscape.
"Other" hits are units, buildings, trenches, trees, roads (`BP_RoadSpline`) and timed air bursts.

Improvements over the old decal path, all measured or structural:
- recycling instead of spawn + destroy: 3.8x less CPU per crater in the steady state, zero UObject garbage;
- one material for all craters (cell picked by Decal Color): no MID per crater, no material swaps on recycle;
- transform + colour folded into one render state recreate per recycle;
- merge of craters landing inside a same-or-larger crater (no stacked overdraw under barrages);
- cheap rejection order in RequestImpactDecal (calibre, setting, landscape) before any lookup.
