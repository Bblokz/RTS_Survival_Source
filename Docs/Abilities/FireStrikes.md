# Fire strikes (Feuersturm commander)

Two Tier III global abilities that leave the ground burning. Both burn through the **existing** ground fire of the
fire subsystem (`URTSFireSubsystem::RequestGroundFire`, `ARTSFireManager`): pooled `RTS_GroundFire_0` flames, one
fire-damage overlap per burning area per second, `ERTSDamageType::Fire`, shield source Flamethrower. No new burning
actor, Niagara system or damage type was added. Code: `GlobalAbilitySystem/GlobalAbilities/FireStrikes/`.

| Key | Class | What happens |
|---|---|---|
| `GA_GerIncendiaryJu87` | `UGA_IncendiaryAirStrike` (new, a `UGA_StripFlight`) | A Ju 87 (`Ger_Aircraft_StukaJu87_Airborne`) flies the strip the player draws and drops 6 incendiary canisters (`AIncendiaryCanister`) along it, enemies or not. Each bursts into a fireball and leaves a burning area. The aim preview shows the 6 spots (`GetStripDropPoints`, fixed seed). Shot down = the rest is not dropped. |
| `GA_GerFireBarrage` | `UGA_Barrage` (existing class, data only) | An off-map battery fires 8 incendiary shells into the circle. The shells carry `FWeaponData::GroundFire`, so every impact (ground or hull) leaves a burning area through `AProjectile::TryRequestGroundFire`. Every surface's impact effect is the molotov fireball (`NS_MolotovImpact`, x1.8) instead of a dirt explosion. |

The ability ends (`OnGlobalAbilityEnded`) when the aircraft has left / the last shell is launched; the fires keep
burning after that on their own timers.

## Tuning (authored by `Source/Tools/FireStrikes/author_fire_strikes.py`)

| | Incendiary Ju 87 | Fire Barrage |
|---|---|---|
| Tier / cooldown | III / 210 s | III / 240 s |
| Cost | 450 radixite | 700 radixite |
| Payload | 6 canisters, 1.6 s fall, released 9 m before their spot | 8 shells in bursts of 2, 0.5-0.9 s apart, radius 10 m |
| Burst / shell | 40 fire damage in 4 m (fills the infantry fire buffer) | 160 HEAT damage, 60 g TNT, 35 shrapnel (plain barrage: 250 / 80 / 50) |
| Burning area | radius 4 m, 18 s, 25 dps, flame scale 3 | radius 4 m, 20 s, 25 dps, flame scale 3 |
| Burst look | `NS_MolotovImpact` x1.8 + `Cue_MolotovImpact` | `NS_MolotovImpact` x1.8 + the barrage's impact sounds |
| Strip / circle | 20-45 m long, 4.5 m half width | crosshair 14 m |
| Icon (placeholder) | `T_GlobalAbility_3xJu87Strafing` | `T_GlobalAbility_Barrage` |

Damage by target type comes from two existing rules, not from per-type numbers:

- Rear-armor window of the ground fire: full damage up to 20 rear armor, falling off (exponent 2) to none at 60. Actors
  without `UArmorCalculation` (infantry, most buildings) always take the full 25 dps.
- The fire buffer of each target (`DeveloperSettings` `UnitResistances`): fire first fills a buffer (10 % of max HP
  for infantry, 5 % for buildings, 15-33 % for armour) that recovers every second (infantry and buildings 5/s, armoured
  cars 15/s, light armour 20/s, medium 25/s, heavy 35/s). Once full, every hit lands in full.

Result per second in a fire: basic infantry (200 HP) about 25 damage from the first tick after a canister burst, so
about 8 s to die; buildings ~25 (a 1400 HP T3 bunker loses ~30 % in one 18-20 s fire); armoured cars and light tanks
a few points once their buffer is full; medium and heavy tanks nothing (their buffer only fills).

## Limits

- The fire subsystem caps burning areas at `MaxActiveGroundFires` = 8 (pool size 8) and merges areas of one player
  closer than 0.75 x radius (3 m). At the cap a new fire replaces the one closest to burning out. A full Ju 87 run
  (6) plus a barrage (up to 8) therefore never burn 14 areas at once; flame units' incendiary weapons share the cap.
  Raising it is a project setting (`[/Script/RTS_Survival.RTSFirePoolSettings]` in `DefaultGame.ini`).
- Barrage shells land at random in the circle, so 5-8 separate areas burn (merges; 6 and 7 in the test runs).
- Look: `ARTSFireManager::StartGroundFireVfx` scales the pooled `RTS_GroundFire_0` component by radius / 100 in X and Y
  only, which smears its flame sprites into flat horizontal streaks at radius 4 m (the same for every incendiary
  weapon's ground fire). A uniform scale or a spawn-radius user parameter on the system would fix it; that file is not
  part of this work. The fireball burst covers the first seconds.

## Test

`-FireStrikesValidate` (`FireStrikes/Tests/FireStrikesTestSubsystem`, not in shipping), run by
`Source/Tools/FireStrikes/run_fire_strikes_test.ps1` on `UnitTests/RTS_ToCopySmall`. For each ability it spawns an
enemy Mosin squad and a T-34 in the target area, fires the template, then checks: the template (class, Tier III,
ground fire on), one fire per canister on the planned spots (Ju 87) or within radius + fire radius (barrage), every
fire on the landscape, a fresh enemy squad sent into a fire loses health over 5 s, a fresh T-34 parked in a fire
loses at most 25 % (0 % in the runs; the victim T-34 under the barrage is only logged, shells hit it), the
fires go out after their duration, the flames stop, the ability ends once and the aircraft is gone. Screenshots go
to `Saved/FireStrikesTest/`. Result 2026-11-05: PASS 42/42 (anchor -12145,-22000). Shots before the first burst are
black on that map, probably the fog of war until the aircraft or the fire reveals the target; the canister in
flight (and whether `SM_Bomb_250` falls nose first with `MeshRotationOffset` 0) has not been seen yet.
