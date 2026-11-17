# Aircraft strafe gun

Every multi-socket aircraft MG (`SetupMultiTraceWeapon` in the Bf 110, Ho 229 and Ju 390 Blueprints, all `MG_15`) is
now a `UWeaponStateAircraftStrafeGun`
(`Weapons/AircraftWeapon/AircraftStrafeGun/`). `UAircraftWeapon::SetupMultiTraceWeapon` creates it, so no Blueprint
changed. `-AircraftStrafeLegacyGuns` brings back the old `UWeaponStateMultiTrace` and the timer-driven strafe aim point,
for comparisons.

## What was wrong (QA 2026-11-17)

1. **Fire ignored the nose.** `UAircraftWeapon::GetFireDirection` aims from the first muzzle at the target with no
   limit. During a strafe the target slid from the clicked point to the run end on a fixed 2 s timer
   (`StrafePointTotalLerpTime`), unrelated to where the aircraft was, while the aircraft dived at its attack point only
   5 m below cruise height. Rounds went steeply down, sideways, or back under the aircraft.
2. **Rounds ended in the air.** The multi-trace weapon traces only `WeaponData.Range` (MG 15: 3520 cm), without the
   `TraceSimulationRangeMlt` the single trace uses. Dives start up to 70 m out, so many rounds ended in mid-air and drew
   `NS_SmallArms_AirImpact` puffs in the sky.
3. **The spread was a cone at the end of the range** (`TraceBulletAircraftAccuracy` 1200 cm x 0.7 for the MG 15), not
   a pattern on the ground. On shallow dives the upper half of the cone missed the ground entirely.
4. **Impacts came before their tracers.** The impact is drawn when the async trace resolves (one frame), while the pooled
   tracer flies at 75 m/s (`SmallArmsProjectileSpeed`): about 0.5 s early at 40 m, so the dirt kicks up ahead of the
   bullets.

## What the strafe gun does

- **Nose cone:** the gun line is the aim direction clamped to 10 degrees yaw, 35 degrees below the nose and 4 degrees above it
  (`AircraftStrafeGunSettings`). Fire always leaves the front of the aircraft.
- **Walking aim point:** during a strafe run, `AAircraftMaster::Strafe_TickTargetFromFlightProgress` aims where a gun
  line about 20 degrees down from the aircraft meets the run line, and only lets that point move forward. The fire walks with
  the aircraft at its own speed.
- **Beaten zone on the ground:** each round lands at a sampled point in the ground plane around where the gun line meets
  it. Across the flight the spread is 340 cm x inaccuracy x (distance / 30 m), 1 sigma. Along the flight it is that
  times 1/sin(dive angle), capped at 3x. Wing guns keep their spacing, so each gun leaves its own line of spurts. The
  trace runs from the muzzle to that point and 6 m past it, so it still finds terrain and units.
- **Impacts on tracer arrival:** the tracer starts at once from the projectile pool, and the impact and damage wait
  until it gets there. One non-looping timer per gun checks arrivals at 60 Hz.
- **No sky puffs:** a round that hits nothing draws no impact.
- **Air-to-air** targets (an `AAircraftMaster`) keep the regular converging multi-trace fire.

Damage per round, rate of fire, ammo and armour penetration are unchanged.

## QA test

`-AircraftStrafeValidate` (on `UT_Nomadics`) now measures every MG round for 30 s after the first one
(`FAircraftStrafeShotRecorder`, map tests only). It logs `RTS_AIRCRAFT_TEST STRAFE_QA ...` and writes
`Saved/AircraftStrafeQA/<label>.csv`. Strafe gun checks: at least 100 rounds, 90% or more hit the ground or a unit, 2% or
fewer off the nose cone, and a mean impact-vs-tracer timing error of 0.05 s or less. With
`-AircraftStrafeLegacyGuns` the run is only measured, not judged.

`Source/Tools/AircraftStrafeQA/run_strafe_qa.sh` runs both and plots them with `plot_strafe_qa.py`.
