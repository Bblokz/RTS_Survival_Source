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
- **Sound:** a fire loop on the aircraft and an impact churn loop at the zone; see [Strafe sound](#strafe-sound).
- **Air-to-air** targets (an `AAircraftMaster`) keep the regular converging multi-trace fire.

Damage per round, rate of fire, ammo and armour penetration are the weapon's own. Hit chance now comes from the
footprint under the beaten zone instead of the end-of-range cone, so damage dealt changes; the strafe test logs it in
both modes.

## Area damage (2026-11-22)

Direct hits alone left infantry standing in the middle of a churned band: a soldier's footprint is about 1 m², so a
round had a 1-2 % chance to land on him. The band itself now does the damage.

- **Near misses (machine guns, no explosive filler):** every impact drawn on the ground, the round and each of its 3
  extra spurts, hits the **nearest** enemy soldier (`ASquadUnit`) within 90 cm x (calibre / 7.92)^0.6 for one round's
  damage (MG 15: 12, with the weapon's flux). Nearest only, so one spurt never hurts a packed squad at once.
  - The soldiers come from the volley's existing unit search (no new queries); positions are read when the tracer arrives.
  - Skipped: dead soldiers, soldiers seated in cargo (attached to a building or vehicle, which takes the hits),
    soldiers with collision off (not deployed yet), and soldiers more than 3 m above or below the impact. Not hidden
    ones: fog of war hides enemy soldiers that the strafe can still hit.
  - Every multi-socket aircraft MG is a strafe gun, so this holds for any MG ground attack, also enemy aircraft on
    the player's infantry. Cover and trenches give no reduction yet.
  - A round that hit a unit directly does no near-miss damage (it draws no spurts either).
  - Without the strafe impact system the spurts are not drawn, so only the round itself counts.
  - Vehicles never take near-miss damage: the armour check stays on direct hits.
- **Cannon splash (explosive filler, so the MK 108):** every round that lands deals rear-armour area damage
  (`FRTS_AOE::DealDamageVsRearArmorInRadiusAsync`, hostiles of the owner, destructibles too) around its impact:
  radius 90 cm x (calibre / 7.92)^0.6 (2 m for 30 mm), half the round's damage at the centre (MK 108: 40), falloff
  from the shrapnel particles, measured to each victim's nearest collision (`EAoeFalloffDistance::ClosestCollision`:
  to the actor origin, about 1 m above the ground for a soldier, a shell at his feet did a quarter of its damage),
  full damage up to the shrapnel pen (15 mm) and none from 1.5x it. A directly hit unit
  is left out, as it took the round. The cannon's spurts stay visual: no near misses. The weapon data's own
  `ShrapnelRange` (60 cm) and `ShrapnelDamage` (12.5) were too small to read as a 30 mm mine shell, so they are not used.
- Tuning: `AircraftStrafeGunSettings` in `WeaponStateAircraftStrafeGun.h` (`NearMiss*`, `CannonSplash*`).

Results 2026-11-22, `-AircraftStrafeTargets=Infantry` (two Mosin squads, 12 soldiers of 160 HP = 1,920 HP), one
30 s strafe window each:

| | Rounds landed | Area hits | Damage | Soldiers killed |
| --- | --- | --- | --- | --- |
| 2x Bf 110 (MG 15) | 411 (3,428 impacts drawn) | 88 near misses | 1,050 | 2 |
| 2x Ho 229 (MK 108) | 50 | 48 splashes | 1,024 | 3 |

Before, two Bf 110s did 0-49 damage to two BTRs in the same window, and an infantry estimate was a few hits per
soldier per pass. The Ho 229's first run measured to the actor origin: 53 damage from 42 splashes.
- Legacy guns and air-to-air fire are unchanged.

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

## Strafe sound

The aircraft MGs fire RandomBurst volleys: every gun fires once per volley, many volleys a second, and every round draws
4 spurts. Before 2026-11-18 a volley played the Blueprint's single Puma MG shot and each gun played one sand thud per
0.12 s. That gave a few thin shots and a few thuds under a dense band of spurts.

`UAircraftGunAudioPlayer` (`Audio/AircraftGuns/`) now holds two looping voices per gun, each baked at the real rate of
fire:

- **Fire loop:** attached to the aircraft. It starts on the first volley and stays on while volleys keep coming. The
  player measures the volley interval while firing. When no volley comes for 2.5 intervals (clamped to 0.14 - 0.45 s),
  the loop fades out in 0.06 s and a fire tail plays: the last round and its echo. Each burst starts the loop at a random
  point and a random pitch (0.96 - 1.04).
- **Impact loop:** starts at the first round that lands. It follows the zone, and every arriving round pulls it 30 % of
  the way to that round. It ends when no round has arrived for the fire loop's gap plus one volley interval, with an
  impact tail: the last hits thinning out, then dirt pattering down. Stone ground switches to the hard loop, with more ricochets and less thud.
- **Unit hits:** a clang cluster (3 - 6 clangs and a ricochet), at most one per 0.12 s per gun. It plays on top of the
  regular per-round metal impact of the trace path.
- **Guns:** a weapon with 3 or more fire sockets (Bf 110) uses the "many guns" loops, at twice the density.

The player replaces the per-volley launch sound and the throttled impact sound. The muzzle flashes stay. Air-to-air fire
keeps the fire loop. `-AircraftStrafeLegacyGuns` keeps every old sound.

**20 mm cannons:** these are `SetupMultiProjectileWeapon` weapons in the 15 - 25 mm range: the MG 151/20 of the Me 410
and the Fw 189. They get the cannon fire loop (2 guns at 720 rpm). `Cue_Aircraft20mm_Impact` replaces their sand and
stone impact sound, so every projectile that lands plays a Minengeschoss crack-pop with dirt and a thud. The MK 108
(30 mm, Ho 229) is a strafe gun (below); the BK 5 and the bombs keep their Blueprint sounds.

**30 mm strafe cannon:** the Ho 229's Blueprint sets up its MK 108s with `SetupMultiProjectileWeapon`.
`UAircraftWeapon::SetupMultiProjectileWeapon` turns every weapon that `UWeaponStateAircraftStrafeGun::GetIsStrafeCannon`
lists into a strafe gun instead, with the same sockets, VFX, burst settings and weapon data. So the cannon gets the
beaten zone, the impacts timed to its tracers, the dirt spurts and the pocks (both scale with calibre), and the steep
strafe dives. Since 2026-11-22 every round that lands also splashes (see [Area damage](#area-damage-2026-11-22)); the
projectile MK 108 did none (projectile shell splash starts at 45 mm).
`-AircraftStrafeLegacyGuns` brings the projectiles back. Strafe guns of 28 mm and up use the HeavyCannon sound set:
version A "Hammer" from `build_30mm_versions.py`, 600 rpm a cannon, with an impact churn of heavy HE bursts at about
13 a second. It plays 3.5 dB above the MG set.

Which aircraft get what. All strafe MGs use the MG 15 weapon data, so the number of fire sockets picks the loop. The
gun sounds are made when the weapon is set up, and the strafe QA spawns every aircraft Blueprint and lists them
(`AIRCRAFT_GUN_AUDIO` lines, 2026-11-18):

| Aircraft | Gun | Sound set |
| --- | --- | --- |
| Bf 110, Bf 109 | MG 15, 4 sockets | MG, 4-gun loops |
| Ju 87, Ju 87 (airborne), Ju 87 G | MG 15, 2 sockets | MG, 2-gun loops |
| Me 410 (all three), Fw 189 | MG 151/20 | Cannon + 20 mm impacts; their BK 5 keeps its own |
| Ho 229 | MK 108 (30 mm), fired as a strafe gun | 30 mm set ("Hammer") |
| Il-2, Pe-2, Pe-8, Ju 390 | MG 15 | MG set by the same code; their Blueprints add no weapon at spawn, so the QA could not list them |

| Asset (`/Game/RTS_Survival/Sounds/WeaponSounds/AircraftGuns`) | What |
| --- | --- |
| `DA_AircraftGunAudio` | `UAircraftGunAudioDataAsset`: MG set, cannon set, cannon calibre range, attenuation and concurrency. Set in RTS Audio Settings (`AircraftGunAudioData`) |
| `S_AircraftMG_FireLoop_Pair` / `_Quad` | 2 / 4 guns at 1050 rpm, 2.4 s seamless loops |
| `S_AircraftMG_ImpactLoop_Soft/Hard_Pair/Quad` | 26 / 46 hits a second plus ricochets, thuds and a sand churn bed, 3 s loops |
| `S_AircraftMG_FireTail_1-3`, `S_AircraftMG_ImpactTail_1-3`, `S_AircraftMG_MetalCluster_1-6` | burst ends, unit hits |
| `S_Aircraft20mm_FireLoop_Pair`, `S_Aircraft20mm_FireTail_1-3`, `Cue_Aircraft20mm_Impact` (8 waves) | 20 mm |
| `S_Aircraft30mm_FireLoop`, `_FireTail_1-3`, `_ImpactLoop`, `_ImpactTail_1-3` | 30 mm strafe cannon (unit hits use the MG clang clusters) |
| `Att_AircraftGun_Fire` / `_Impact` | spatialized, linear: full volume to 30 m / 25 m, silent at 150 m / 105 m |
| `SC_AircraftGun_Fire` / `_Impact` | 10 voices each, stop quietest |

Rebuild:

1. `python Source/Tools/AircraftStrafeAudio/build_aircraft_gun_sounds.py [preview]` renders every wave offline from the
   project's raw sound library (`D:/UE_Assets/Sounds`, through `_SfxBuilders/sfxlib.py`). The loops are built on a
   circular buffer, so they are seamless without a crossfade. With `preview`, it also writes
   `Saved/AircraftGunAudio/preview_*.wav`: a strafe pass run through the player's rules, next to the old sound.
2. `bash Source/Tools/AircraftStrafeAudio/run_import.sh` imports the waves in a DebugGame commandlet and makes the cue,
   the attenuations, the concurrencies and the data asset. A re-run skips unchanged waves but resets the data asset's
   values.

The strafe QA run checks the audio by counts, because a `-nosound` run cannot listen. The burst logic runs without an
audio device. The `STRAFE_AUDIO` line and its checks:
- one fire loop per burst, not per volley;
- every burst ends with a tail;
- the impact loop gets every round;
- a spawned Me 410's MG 151/20 has the cannon set and the 20 mm ground impact.

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

Area damage checks (`STRAFE_AREA_DAMAGE` line: near-miss hits and kills, cannon splashes):
- MG strafe: no cannon splash; Ho 229 run: splashes and no near misses.
- `-AircraftStrafeTargets=Infantry` puts two enemy Mosin squads (`Rus_Squad_Mosin`) on the target spots instead of
  the BTRs; their soldiers must take damage (`strafe_infantry_takes_damage`).

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
