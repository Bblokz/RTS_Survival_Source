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

## Dig in

The truck digs in behind its own closed ring of sandbags around all four sides. Regular tank walls are a single wall in front.
- `EDigInType::RusAATruckRingWall`: 900 HP and 14 s to build (`AATruckRingDigInWallHealth`, `AATruckRingDigInWallBuildingTime` in `DeveloperSettings.h`). The light tank wall has 400 HP and the medium 700.
- Wall Blueprint: `/Game/RTS_Survival/Blueprints/GroundVehicles/DigIn/BP_DiginWall_RusAATruck_GAZMM72K`.
  - Meshes: `.../Meshes/Props/SandBags/TankWalls/RusAATruck/SM_RusDigIn_GAZMM72K_Wall`, plus `_WallAt66` and `_WallAt33` for damage.
  - Collision: one box per wall run.
  - The `DigIn` component spawns it at the actor origin (`DigInOffset` 0).
- The wall is 125 cm high. The 72-K fires over it at 0° elevation in every direction.
- The Blueprints use `RusMediumTankWall` until this C++ is built. The post-build job `Saved/AgentScripts/GAZMM72KUE/job_digin_postbuild.py` then switches them to the ring type.

## Crew

Three Soviet soldiers (`SK_RussianSoldier`, the Mosin squads' mesh) ride the traverse on the gun socket `CrewFrame`. They are the `Crew1..3` components of `Rus_Turret_GAZMM72K`, driven by `PitCrew` (`UGunPitCrewComponent`):

| Component | Role | Spot (gun frame, cm) | Animations (`GAZMM_72K/Crew`) |
|---|---|---|---|
| `Crew1` | Layer, left seat, cranks the elevation handwheel | (-50, -48, 0) | `AN_GAZMM72K_Layer_Idle` |
| `Crew2` | Gunner, right seat, cranks the traverse handwheel | (-50, 48, 0) | `AN_GAZMM72K_Gunner_Idle` |
| `Crew3` | Loader, stands on the bed between the seat backs | (-82, 0, -34) | `AN_GAZMM72K_Loader_Idle`, `Montage_GAZMM72K_Loader_Reload` |

Every reload, the loader drops a 7-round clip into the feed. The clip is `SM_GAZMM72K_Clip`, carried as `Clip1` / `Clip2` through `PitCrew` `ShellCarry`:
- The loader is both handler and loader (crew index 2).
- At 0.45 of the drill the carried clip disappears into the feed.
- At 0.80 the next clip is in his hands at his belt.

The drill is authored at 15 degrees elevation, so at high elevation the hands are about 15 cm off the feed.

Wiring:
- `BP_PostInit` calls `InitPitCrew(Turret)`.
- `BP_PlayWeaponAnimation` calls `OnGunFired` (flinch 0.25).
- `ReloadWeapon` calls `OnReloadStarted` (`MinReloadTimeForLoadingSequence` 2 s; the weapon reloads in 4 s).

Sources:
- Animations: `CustomMapItems/InfantryAnimations/VehicleCrew/gen_gazmm72k.py`. Validated by `vcval.py` with `specs/gazmm_*.json` at full traverse and 0 to 85 degrees elevation, plus the crew-against-crew check `GAZMM72K/check_crew_overlap.py`.
- Unreal jobs: `Saved/AgentScripts/GAZMM72KUE/job_crew_*.py`.
