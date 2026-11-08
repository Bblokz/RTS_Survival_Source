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

## Ground fire look and cap (round 3c, 2026-11-05)

Every burning area of the fire subsystem (these strikes, flamethrowers, napalm, incendiary rockets) now gets:

- the old low carpet: pooled `RTS_GroundFire_0` (type `GroundFire`), spread over the radius in X/Y. On its own it is
  only a few centimetres of flame and reads as flat streaks from the RTS camera;
- upright flames (new pool type `ERTSFireType::GroundFireFlame` = `2_Nbonefire_pt`) at uniform scale, one in the
  centre and the rest on a ring at 0.55 x radius (turned by the area's location, each dropped onto the landscape):
  one per 1.8 m cover circle, at least 1, at most 4 (1 m: 1, 3 m: 3, 3.5 m and up: 4). Size = VfxScale x 0.6,
  clamped 1-2.2, +-15 % jitter. They stop with the area. All numbers are `URTSFirePoolSettings` config.

Cap: `MaxActiveGroundFires` 8 -> 24, `GroundFire` pool 8 -> 24, `GroundFireFlame` pool 96 (24 x 4). A full Ju 87 run
(6) + a barrage (up to 8) + six flame units' fires burn together with headroom; at the cap the fire closest to burning
out is still replaced. Measured with `-FireVfxStudy` (DebugGame, 1280x720, a shared, busy machine, so GPU numbers are
+-1 ms noise): 24 areas of radius 4 m cost +0.4 ms game thread with the carpet only and +1.4 ms with the upright
flames (96 CPU emitters), GPU and render thread within noise. Development/Shipping builds run that CPU work faster.

Merging is unchanged: areas of one player closer than 0.75 x the larger radius refresh each other.

- Barrage shells land at random in the circle, so 5-8 separate areas burn (merges; 6-8 in the test runs). The
  template starts its shells 25-35 m up (`GA_Barrage` starts some at 5.9 m, and such a flat shell once landed 45 m
  off).

## Test

`-FireStrikesValidate` (`FireStrikes/Tests/FireStrikesTestSubsystem`, not in shipping), run by
`Source/Tools/FireStrikes/run_fire_strikes_test.ps1` on `UnitTests/RTS_ToCopySmall`. For each ability it spawns an
enemy Mosin squad and a T-34 in the target area, fires the template, then checks: the template (class, Tier III,
ground fire on), one fire per canister on the planned spots (Ju 87) or within radius + fire radius (barrage), every
fire on the landscape, a fresh enemy squad sent into a fire loses health over 5 s, a fresh T-34 parked in a fire
loses at most 25 % (0 % in the runs; the victim T-34 under the barrage is only logged, shells hit it), the
fires go out after their duration, the flames stop, the ability ends once and the aircraft is gone. Screenshots go
to `Saved/FireStrikesTest/`. A third scenario, `BothAtOnce`, fires both strikes side by side while six flame-unit
stand-ins (3 m, 7 s, fed every 5 s) burn between them, and checks that every strike fire burns its full duration and
that all areas burned at the same time (peak areas = fires seen). Result 2026-11-05 10:49: PASS 67/67 (anchor
-12145,-22000; BothAtOnce 18 areas at once, cap 24). The fog post process is switched off for the pictures.

`-FireVfxStudy` (`FireStrikes/Tests/FireVfxStudySubsystem`) lines up pool fires of the weapons' radii and candidate
fire systems, photographs each from the RTS camera and from the ground, and measures the cost of the full cap.
Before/after pictures: `Saved/FireVfxStudy/Before` (run with
`-ini:Game:[/Script/RTS_Survival.RTSFirePoolSettings]:MaxGroundFireFlamesPerArea=0`) and `Saved/FireVfxStudy/After`. The canister in flight (and whether `SM_Bomb_250` falls nose first with `MeshRotationOffset` 0) has not been seen
yet.
