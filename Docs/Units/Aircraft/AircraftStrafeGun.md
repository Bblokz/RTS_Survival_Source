# Aircraft strafe gun

Every multi-socket aircraft MG is a `UWeaponStateAircraftStrafeGun`
(`Weapons/AircraftWeapon/AircraftStrafeGun/`). That covers every `SetupMultiTraceWeapon` in the aircraft Blueprints:
Ju 87, Bf 109/110, Me 410, Fw 189, Ho 229, Ju 390, Il-2, Pe-2 and Pe-8. `UAircraftWeapon::SetupMultiTraceWeapon`
creates it, so no Blueprint changed. `-AircraftStrafeLegacyGuns` brings back the old `UWeaponStateMultiTrace` and the
timer-driven strafe aim point, for comparisons.

## What was wrong (QA 2026-11-17)

1. **Strafes from far away never fired.** When the aircraft spawned out of direct dive range, its first attack path
   was only an approach leg with no dive point. When that leg completed, the strafe code treated the run as done,
   switched to hunting and never dived. The Bf 110 strafe (`GA_2xBf110Strafing`) did this every time on `UT_Nomadics`.
   It now re-plans the run until it has dived (`FAircraftStrafeState::bM_HasDivedThisRun`); `TotalStrafeTime` still
   ends it. This changes every strafe ability, with old and new guns alike.
2. **Fire ignored the nose.** `UAircraftWeapon::GetFireDirection` aims from the first muzzle at the target with no
   limit. During a strafe the target slid from the clicked point to the run end on a fixed 2 s timer
   (`StrafePointTotalLerpTime`), unrelated to where the aircraft was.
3. **Rounds ended in the air.** The multi-trace weapon traces only `WeaponData.Range` (MG 15: 3520 cm). Dives start up
   to 70 m out, so many rounds ended in mid-air and drew `NS_SmallArms_AirImpact` puffs in the sky.
4. **The spread was a cone at the end of the range** (`TraceBulletAircraftAccuracy` 1200 cm x 0.7 for the MG 15), not
   a pattern on the ground.
5. **Impacts came before their tracers.** The impact is drawn when the trace resolves; the pooled tracer flies at 75 m/s.
6. **The Ho 229 has no MGs in play:** its `Setup4xMG` graph is never called; it only fires the MK 108s.

## What the strafe gun does, per volley (all guns firing once)

- **Nose cone:** the gun line is the aim direction clamped to 10 degrees yaw, 35 degrees below the nose and 4 degrees
  above it.
- **Walking aim point:** during a strafe run, `AAircraftMaster::Strafe_TickTargetFromFlightProgress` aims where a gun
  line meets the run line, and only lets that point move forward. The line looks 20 degrees down, or as far down as the
  nose when the dive is steeper.
- **Steep, long dives:** `AAircraftMaster::Strafe_ApplyDiveProfile` sets each strafe run's attack dive, on top of the
  template's settings:
  - The dive bottoms out 6.5 m above the run's ground.
  - It starts at most 36 m out.
  - Dive strength is at least 6, so the lowest point lies at 6/7 of the way to the run's end.
  - The pull-out is at least 25 m longer.

  A Bf 110 now dives at about -17 degrees and bottoms out around 3 m above the target, where it used to stay level at
  22 m. Legacy guns keep the template dive.
- **Beaten zone:** each gun's round gets a landing point on the aim plane. Across the flight the spread is
  340 cm x inaccuracy x (distance / 30 m), 1 sigma, at least 160 cm. Along the flight it is that times
  1/sin(dive angle), capped at 3x. Wing guns keep their spacing.
- **Ground:** one async probe per volley under the aim point (static world only) gives the ground plane, its normal
  and its surface. Every impact of the volley is placed on that plane.
- **Units:** one overlap per volley around the zone finds enemy units (victim object type of the owning player).
  A round whose landing point is inside a unit's footprint (its local bounds, X/Y) traces once to that unit, for the
  armour check; that hit goes through the regular trace damage path. Other rounds do no traces. If something else is
  in front of the unit (a wall, the ground), the round stays a ground impact.
- **Extra spurts:** every round that lands adds 3 purely visual spurts around itself (45 % of the zone's spread).
- **Timing:** the tracer starts at once from the projectile pool. The impact (and damage) waits until it gets there.
  One non-looping timer per gun checks arrivals at 60 Hz.
- **No sky puffs:** a round that never reaches the ground draws nothing.
- **Sound:** one impact sound per gun per 0.12 s.
- **Air-to-air** targets (an `AAircraftMaster`) keep the regular converging multi-trace fire.

Damage per round, rate of fire, ammo and armour penetration are the weapon's own. Hit chance now comes from the
footprint under the beaten zone instead of the end-of-range cone, so damage dealt changes; the strafe test logs it in
both modes.

## Strafe impact VFX

`UAircraftStrafeImpactBatcher` lives on `ASmallArmsProjectileManager`. It uses the same pattern as the trace impact
batcher: impacts of a frame go into two float4 user arrays (`StrafePositionAndCalibre`, `StrafeNormalAndSurface`),
plus a group offset and count, and are drawn by one system.

`NS_AircraftStrafeImpacts` (`/Game/RTS_Survival/ImpactSfxVfx/StrafeImpacts`, always cooked) is built headless by
`Source/Tools/AircraftStrafeQA/build_strafe_impact_vfx.sh` (C++ `RTS.StrafeImpactVfx.BuildNiagaraSystem` /
`PatchNiagaraSystem`, HLSL in `StrafeImpactHlsl/`). It uses the muzzle VFX materials.

| Emitter | Per impact | Look |
| --- | --- | --- |
| `Ground_Spurt` | 3 | Upright dirt column (custom-aligned sprites), thrown up 1-3 m, 0.9-1.7 s |
| `Ground_Debris` | 4 | Dark clods, ballistic, land around the hole |
| `Ground_Dust` | 1 | Haze growing to 2.3 m, drifts, 2-3.4 s |
| `Ground_Flash` | 1 | Bright additive kick at the spurt's base, 1-2 frames, on about half the impacts |
| `Unit_Spark` | 3 | Additive sparks, 1-2 frames, on unit hits |

**Bullet pocks** are deferred decals, not particles. `UAircraftStrafeImpactBatcher::AddBulletPock` keeps its own ring of
240 decals and recycles the oldest first, so a strafe never evicts the shell craters of `ULandscapeImpactDecalSubsystem`.
The pocks use the crater material's smallest band at 60 cm x (calibre / 7.92)^0.6. 60 % of the rounds that land leave
one; the extra spurts leave none. A pock holds for 14 s, then fades out over 8 s. The fade is the decal colour's
alpha, stepped by a 0.25 s timer. Two engine behaviours rule out the built-in fade:
- `UDecalComponent::SetFadeOut` sets a lifespan that destroys the pooled component.
- The crater material has no `DecalLifetimeOpacity` node, so an engine fade would do nothing.

The player's Shell Craters setting (`rts.ImpactDecals`) Off turns the pocks off.

Gotchas:
- **Sprite renderers must set their facing and alignment explicitly.** The UE 5.5 default is `Automatic`, which uses
  every declared `SpriteFacing` / `SpriteAlignment` attribute. With both declared as up, every quad collapsed to nothing.
- **Never remove an emitter handle by hand.** The system graph keeps stale emitter nodes and the system runs without
  emitters. Use `build_strafe_impact_vfx.sh rebuild` instead: it deletes the system and builds it fresh from the layer
  table.
- Impacts snap to `ALandscapeProxy::GetHeightAtLocation`. The ground probe hits the landscape's simple collision,
  which can sit up to a metre off the drawn surface.

Size and lifetime scale with calibre, (calibre / 7.92)^0.6. Dirt colour follows the ground surface: dirt, sand or stone.
Without the system (asset missing), ground rounds fall back to the regular trace impact.

## QA test

```
bash Source/Tools/AircraftStrafeQA/run_strafe_qa.sh both
```

This runs `-AircraftStrafeValidate -AircraftStrafeAbility=GA_2xBf110Strafing -AircraftStrafeClassMarker=BF110Airborne` on
`UT_Nomadics`, first with `-AircraftStrafeLegacyGuns`, then without. Each run places two enemy BTR-152As on the strafe
line, measures every MG round for 30 s after the first (`FAircraftStrafeShotRecorder`), logs a flight-state line per
aircraft every 2 s, takes screenshots from a camera over the target, and writes
`Saved/AircraftStrafeQA/<label>.csv` plus `strafe_qa_compare.png` (top-down and side-view plots).

Strafe gun checks:
- at least 100 rounds
- 90 % or more land on the ground or a unit
- mean impact-vs-tracer timing error of 0.05 s or less
- every impact emitter spawned
- the batcher drew at least one ground impact per round
- bullet pocks were placed

Two values are only logged as `STRAFE_NOTE`, because neither works as a pass/fail check:
- **Off-nose share:** it counts beaten-zone spread and hunting-phase rounds, while the gun cone itself holds by
  construction.
- **BTR damage:** the MG 15 mostly bounces off a BTR-152, with old and new guns alike.

Before the first round, a 4.5 s showcase draws two walking bands (7.92 mm and 20 mm) next to the map's units and
screenshots them. It gives way as soon as the real strafe fires. `RTS_STRAFE_POCK` lines log every 40th pock.

Results 2026-11-17, Bf 110:

| | Old guns | Strafe gun |
| --- | --- | --- |
| Rounds that hit nothing | 85 % | 4 % |
| Impact before the tracer arrives | 0.43 s | 0.017 s |

The strafe gun run also: 470 pocks placed, peak 1,868 live strafe particles, mean frame 17.7 ms in DebugGame.
