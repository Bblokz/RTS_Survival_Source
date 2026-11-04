# Soviet GAZ-MM 72-K (anti-aircraft truck)

The light GAZ-MM truck with a 25 mm 72-K automatic anti-aircraft gun on a full-traverse pedestal in the bed. It is
an unarmoured enemy armoured car (Soviet units are enemy-only) that fights aircraft and light vehicles with fast
7-round clips. The art is built in Blender; the Unreal Blueprints are still to be built.

| | |
|---|---|
| Subtype | `Tank_GAZMM_72K` ("Rus GAZ-MM 72-K"), appended to `ETankSubtype`; `Global_GetIsArmoredCar` is true |
| Weapon | `Gun72K_25MM` ("25 mm 72-K (GAZ-MM AA)", short name "72-K"), appended to `EWeaponName` |
| Weapon stats | The Flak 38 entry with 25 mm calibre, 1.2x damage and penetration (54 / 37 mm), 7-round clip, 0.3 s cooldown, 4 s reload (about 1.4x a Flak 38's sustained damage), light cannon range |
| Unit stats | 0.8x armoured car health (as the Zil trucks), armoured car resistances and vision, 60 / 20 km/h, mount turn rate 50 |
| Cost | 110 radixite, 40 vehicle parts (`DeveloperSettings::GameBalance::UnitCosts::ArmoredCars::GAZMM72K_*`) |
| Enemy training | 10 training points (`EnemyTrainingHelpers`). Not yet in the `LightTanks` tech list or the survival waves: added once the Blueprint and spawner row exist (planned: `LightTanks` beside the BTR-152A; waves `LightTanks`, role `AntiAir`, weight 0.6) |

## Where it is registered (C++)

- `Units/Enums/Enum_UnitType.h`: `ETankSubtype::Tank_GAZMM_72K`, `Global_GetIsArmoredCar`.
- `Weapons/WeaponData/WeaponSystems.h`: `EWeaponName::Gun72K_25MM`, display and enum-string switches.
- `Game/GameState/CPPGameState.cpp`: weapon data after the Flakvierling entries, tank data after the BM-13.
- `DeveloperSettings.h`: `GAZMM72K_Radixite` / `GAZMM72K_VehicleParts`.
- `GameUI/TrainingUI/TrainingMenuManager.cpp`: training option (light tank training time).
- `Enemy/TrainingAndUnitCreation/EnemyTrainingHelpers/EnemyTrainingHelpers.cpp`.
- `Utils/RTSBlueprintFunctionLibrary`: `GetArmoredCar_GAZMM72K()`.

## Blueprints (to be built)

| Asset | Path |
|---|---|
| Vehicle | `/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/GAZMM_72K/Rus_ArmoredCar_GAZMM72K` |
| Embedded turret | `/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/GAZMM_72K/Rus_EmbTurret_GAZMM72K` |

The turret sets up its weapon with the FName `Gun72K_25MM` and turns a full circle. Anti-aircraft targeting is set
up in the Blueprint as for the Flak 38 mounts; the weapon data carries no air flag. After the Blueprints and the
spawner row exist: add it to `EnemyAITechLevel.cpp` `LightTankOptions` and to `SurvivalWaveSettings.cpp` (+ `wave_defaults.json`,
`40_WavesAndAI.md`); the wave pool only takes subtypes that spawned in the unit roster test.
