# Tank lethality pass (2026-11-03)

Viewers of the Shorts and Reels found tank fights too slow and flanking pointless: a T-34 behind a Tiger needed six
hits, and an 88 needed three hits on a T-34. This pass makes tank fights shorter and makes side and rear hits count.
It has three parts; each can be rolled back on its own.

Not yet play-tested. All numbers below are calculated, not measured in game.

## What changed

### 1. Side and rear penetrations deal more damage

A kinetic shell that penetrates deals its full damage times a multiplier for the side it went through:
front ×1.0, side ×1.5, rear ×2.0. Turret plates count the same way: mantlet and cupola count as front, turret sides as
side, turret rear as rear, and the combined sides-and-rear turret plate as side (`Global_GetDamageTypeFromPlate` in
`RTSComponents/ArmorComponent/Armor.h`). Bounces are unchanged.

- Values: `PlateTypeToKineticPenDamageMlt` in `RTS_Survival/DeveloperSettings.h`
  (`GameBalance::Weapons::ArmorAndModules`).
- Applied in `AProjectile::GetKineticPenDamageMlt` (`Weapons/Projectile/Projectile.cpp`), called from
  `ArmorCalc_KineticProjectile`; before this pass that call site was `constexpr float DamageMlt = 1.f;`.
- This applies to anything with armor plates that a cannon or AT rocket penetrates, so flanking bunkers and other
  armored buildings also pays now.

### 2. Vehicles have 25% less health

`VehicleHealthMlt = 0.75` in `GameBalance::UnitHealth` scales the armored-vehicle bases on top of `OverallHealthMlt`
(0.8): `OneLight/Medium/HeavyTankShotHp`, `ArmoredCarHealthBase`, `LightTankHealthBase`, `LightMediumTankBase`,
`CommandVehicleHp`, `MediumTankHealthBase`, `T2HeavyTankBase`, `T3MediumTankBase`, `T3HeavyTankBase`,
`SuperHeavyTankBase`. Infantry, buildings, trains, aircraft, nomadic trucks and dig-in walls keep their health.

| Base | Before | After |
|---|---|---|
| Armored car / light tank | 360 | 270 |
| Light-medium | 480 | 360 |
| Medium (T-34/76, Pz IV H, Pz III M) | 720 | 540 |
| T2 heavy / T3 medium (KV-1, Panther G, Jagdpanther) | 900 | 675 |
| T3 heavy (Tiger, IS-1) | 1200 | 900 |
| Super-heavy (IS-3, E-100) | 1840 | 1380 |
| One light / medium / heavy shot step | 80 / 180 / 280 | 60 / 135 / 210 |

Other changes that go with it, in `Game/GameState/CPPGameState.cpp`:

- T-34/85 health is trimmed by `T34_85BreakpointTrim` (15, in `DeveloperSettings.h`) to 660, so three Pz IV 75mm
  hits (3 × 221.4 = 664) still kill it. Without the trim it would be 675 and need four.
- Four units derive their experience worth from `MediumTankHealthBase` (T-34/85, T-34/100 and the two units that set
  `ExperienceWorth = MediumTankHealthBase` / `* 2`). These now divide by `VehicleHealthMlt`, so their experience worth
  stays as before.
- Tanks that add a fixed amount to a base (for example Brumbär and Sturmtiger `+ 400`, Pz IV F1 `- 150`,
  Sd.Kfz. 251 `+ 150`) keep that fixed amount, so they lose a little less than 25%.
- Blueprints that read `BP_GetHealthGameBalanceValue` get the new values too.
- Anything that damages vehicles now kills them sooner: mines, AT infantry, artillery, aircraft rockets and bombs,
  fire, and radiation. Fire thresholds scale with max health, so vehicles also start burning from smaller fires.

### 3. Flanking hits can detonate the ammunition (Realistic only since 2026-11-04)

When a kinetic shell of 75 mm or more penetrates a tank's side or rear, there is a chance that the ammunition detonates
and the tank dies from that hit. It plays the existing cook-off death, including the turret toss and the cook-off
launch sound.

| Side penetrated | Detonation chance |
|---|---|
| Front | 0% |
| Side | 15% |
| Rear | 25% |

- Values: `PlateTypeToAmmoDetonationChance` and `AmmoDetonationMinCalibreMm` in `DeveloperSettings.h`
  (`ArmorAndModules`).
- Rolled in `AProjectile::GetKineticPenDamageMlt`. On a detonation it calls the new
  `ATankMaster::StartAmmoDetonation` (`Units/Tanks/TankMaster.h`, `TankMasterModules.cpp`), which starts the cook-off,
  and the hit deals twice the tank's max health. The kill still goes through normal damage, so the shooter gets the
  kill and experience.
- Tanks with ammo cook-offs turned off (`SetVehicleModuleFiresEnabled`) never detonate.
- This is separate from the slower ammo cook-off that module damage can start (`AmmoCookOff::GetChance` in
  `VehicleModuleBalance.h`), which is unchanged.

### 4. Module follow-up (2026-11-04)

Two changes in `RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h`:

| Value | Before | After |
|---|---|---|
| `AmmoCookOff::AmmoCookOffDamagedAmmoChance` | 0.10 | 0.15 |
| `AmmoCookOff::AmmoCookOffDestroyedAmmoChance` | 0.20 | 0.25 |
| `RuleSetProbabilities::Plate_Front::Engine` (hull front engine hit chance) | 1.00 | 0.10 |

The front engine chance of 1.00 was a test value; the main front plate now threatens the engine less than either
glacis (upper 0.15, lower 0.35). `RTS.VehicleModules.AmmoCookOffChance` checks the cook-off numbers, and
`RTS.VehicleModules.FrontPlateModuleRules` checks the front plate stays below both glacis plates.

### 5. Arcade or realistic vehicle balance (2026-11-04)

Players choose the vehicle balance in a column beside the difficulty buttons (both the mission picker and the faction
campaign picker). **Realistic** is everything above plus overmatch kills (part 6). **Arcade** (the default) plays like
before this pass, except for the side/rear damage multipliers and the front engine fix, which apply in both:

| | Arcade | Realistic |
|---|---|---|
| Armored vehicle health | as before (`VehicleHealthMlt` undone) | ×0.75 |
| Side ×1.5 / rear ×2 penetration damage | yes | yes |
| Flank ammo detonation (part 3) | no | yes |
| Module ammo cook-off (damaged / destroyed rack) | 10% / 20% | 15% / 25% |
| Overmatch kills (part 6) | no | yes |

- The choice lives on the game instance (`URTSGameInstance::Get/SetVehicleBalance`); it survives map loads and
  restarts but not a game relaunch. For PIE, set `Vehicle Balance` in Project Settings > Game > RTS PIE Startup.
- The tank data maps are built for Realistic. `ACPPGameState::SetVehicleBalance` rescales `MaxHealth` and the fire
  threshold of every entry with `FTankData::bScalesWithVehicleBalance` (trains are excluded). It runs from
  `PostInitializeComponents`, before level actors begin play. When the in-mission picker changes it after placed
  vehicles began play, those vehicles get `SetMaxHealth` and keep their health percentage.
- Fixed amounts a vehicle adds to its base (`FixedVehicleHealthOffsets` in `DeveloperSettings.h`, e.g. Brumbär
  +400) are kept out of the rescale, and Arcade drops the T-34/85 trim, so Arcade values match the old ones
  (T-34/85 900, Tiger 1200, Brumbär 1120). Entries rounded after a multiplier (e.g. SU-76) can differ by a few HP.
- A new offset on a vehicle base must be added to `FixedVehicleHealthOffsets` and
  `VehicleBalanceHealth::GetFixedHealthOffset` in `CPPGameState.cpp`, or Arcade scales it too.
- Gameplay code asks `ACPPGameState::GetIsRealisticVehicleBalance(WorldContext)`; without a game state it says Arcade.

### 6. Overmatch: big guns crush the armor (Realistic only, 2026-11-04)

A kinetic penetration destroys the vehicle in one hit and shows **ARMOR DESTROYED!!** (the red of "HE destroyed
armor!!") when both hold:

- pen at impact ≥ `OvermatchPenToEffectiveArmorRatio` (1.5) × the effective, angled armor, and
- shell damage ≥ `OvermatchMinDamageShareOfMaxHealth` (45%) × the target's max health.

The values live in `ArmorAndModules` in `DeveloperSettings.h`; the check is `AProjectile::GetIsArmorOvermatched`, called
from `GetKineticPenDamageMlt`. The pen ratio alone can't separate the Pz IV's long 75 (about 1.6 against a T-34 front)
from the Tiger's 88 (about 1.8); the damage share does, and it scales with vehicle class without a gun list:

| Shooter → target (front) | Damage / health | Overmatch |
|---|---|---|
| Tiger 88 → T-34/76 | 56% | yes |
| Tiger 88 → T-34/85 | 46% | yes |
| Panther L70 → T-34/76 | 57% | yes |
| Pz IV long 75 → T-34/76 | 41% | no |
| T-34/76 → Pz IV H | 43% | no |
| T-34/85 → Pz IV H | 48% | yes |
| Tiger 88 → Tiger | 33% | no |
| IS-2 122 → Tiger | 67% | yes |
| Any medium gun → light tank | 80%+ | yes |

The margins are a few percent (43% vs 46%), so a later gun damage or vehicle health change can flip a matchup;
`RTS.Balance.VehicleBalance.Overmatch` pins this table.

How the tank dies (`AProjectile::StartOvermatchDeath`): through a front plate, half the kills play the regular death
(`OvermatchRegularDeathChance`), a quarter an ammo detonation with turret toss (`ATankMaster::StartAmmoDetonation`) and
a quarter a fuel explosion (`ATankMaster::StartFuelDetonation`, which starts the engine fire). A tank with that effect
turned off plays the regular death. Side and rear overmatch kills always play the regular death.

## Expected results

Shots to kill, with time to kill in brackets (first round loaded, then one reload per shot). This assumes every shot
penetrates. Front hits by mediums on heavies will often bounce; plate thickness is set in each tank Blueprint.
Shell damage has no random spread (only penetration varies by ±10%), so these counts are exact. A side or rear
penetration from 75 mm or more can also end the fight in one hit through the detonation chance.

| Attacker → target | Before | After: front | After: side | After: rear |
|---|---|---|---|---|
| T-34/76 → Pz IV H | 4 (20.7 s) | 3 (13.8 s) | 2 (6.9 s) | 2 (6.9 s) |
| Pz IV H → T-34/76 | 4 (17.7 s) | 3 (11.8 s) | 2 (5.9 s) | 2 (5.9 s) |
| Pz IV H → T-34/85 | 5 (23.6 s) | 3 (11.8 s) | 2 (5.9 s) | 2 (5.9 s) |
| T-34/85 → Panther G | 4 (28.8 s) | 3 (19.2 s) | 2 (9.6 s) | 2 (9.6 s) |
| Panther G → T-34/85 | 3 (15.6 s) | 3 (15.6 s) | 2 (7.8 s) | 2 (7.8 s) |
| Tiger → T-34/76 | 3 (16 s) | 2 (8 s) | 2 (8 s) | 1 |
| T-34/76 → Tiger | 6 (34.5 s) | 4 (20.7 s) | 3 (13.8 s) | 2 (6.9 s) |
| Tiger vs Tiger | 4 (24 s) | 3 (16 s) | 2 (8 s) | 2 (8 s) |
| Tiger → IS-2 | 5 (32 s) | 4 (24 s) | 3 (16 s) | 2 (8 s) |
| IS-2 → Tiger | 2 (14 s) | 2 (14 s) | 1 | 1 |
| King Tiger → IS-2 | 4 (24 s) | 3 (16 s) | 2 (8 s) | 2 (8 s) |
| IS-2 → King Tiger | 3 (28 s) | 2 (14 s) | 2 (14 s) | 1 |

With the detonation chance, a T-34/76 behind a Tiger kills it in at most two rear penetrations, and about one time in
four the first one does it.

## Rolling back

Each part can be undone on its own. Rebuild the editor after any change; no Blueprint or asset was changed.

| To undo | Change |
|---|---|
| 1. Side/rear damage | In `DeveloperSettings.h`, set all three `PlateTypeToKineticPenDamageMlt` values to `1.f`. |
| 2. Vehicle health | Players can already pick Arcade beside the difficulty. To make it the only behaviour: in `DeveloperSettings.h`, set `VehicleHealthMlt` to `1.f` and `T34_85BreakpointTrim` to `0.f` (otherwise the T-34/85 has 885 instead of 900, and four Pz IV hits still kill it). With a multiplier of 1 the two balance options are identical. The experience-worth lines can stay: dividing by 1 changes nothing. |
| 3. Ammo detonation | In `DeveloperSettings.h`, set all three `PlateTypeToAmmoDetonationChance` values to `0.f`. |
| 4. Module follow-up | Arcade already uses the old 10% / 20% cook-off odds. To drop the raise from Realistic too, set `AmmoCookOffDamagedAmmoChance` / `AmmoCookOffDestroyedAmmoChance` in `VehicleModuleBalance.h` back to `0.1f` / `0.2f` and update `RTS.VehicleModules.AmmoCookOffChance`. Leave the front engine chance at `0.10f`: 1.00 was a test value. |
| 6. Overmatch | In `DeveloperSettings.h`, set `OvermatchMinDamageShareOfMaxHealth` to `100.f` (no shell deals a hundred health bars) and update `RTS.Balance.VehicleBalance.Overmatch`. |

For a full code revert instead, undo these edits:

- `RTS_Survival/DeveloperSettings.h`: the two new maps and `AmmoDetonationMinCalibreMm` in `ArmorAndModules`;
  `VehicleHealthMlt` and `VehicleHealthScale` in `UnitHealth`, and the vehicle bases going back to
  `OverallHealthMlt`.
- `RTS_Survival/Weapons/Projectile/Projectile.h` and `.cpp`: `GetKineticPenDamageMlt`, its call in
  `ArmorCalc_KineticProjectile` (back to `constexpr float DamageMlt = 1.f;`), and the `HealthComponent.h` and
  `TankMaster.h` includes.
- `RTS_Survival/Units/Tanks/TankMaster.h` and `TankMasterModules.cpp`: `StartAmmoDetonation`.
- `RTS_Survival/Game/GameState/CPPGameState.cpp`: the T-34/85 health block and the four `ExperienceWorth` lines that
  divide by `VehicleHealthMlt`.

## Tuning without a rollback

- Shot counts change in steps. Before moving a health value, check that it does not land just above a common hit
  total. For example, 3 × 233.4 = 700 against the old 720 cost the T-34 a whole reload against the Pz IV. Common
  damage per hit: KwK 40 75mm 221.4, F-34 76mm 233.4, S-53 85mm 260.4, KwK 36 88mm 301.8, KwK 42 75mm 306.0,
  KwK 43 88mm 377.3, D-25T 122mm 601.8.
- If flanking feels too strong, lower the detonation chances first. Lowering the rear multiplier to 1.75 is a
  bigger step than it looks: a T-34/76 then needs three rear hits on a Tiger instead of two (2 × 408 = 817, short of
  900).
- IS-2 side hits on a Tiger kill in one shot by only 2.7 HP (902.7 against 900); any Tiger health increase flips that
  to two.
- If fights are now too short overall, raise `VehicleHealthMlt` toward 0.85 rather than removing the side multipliers.
