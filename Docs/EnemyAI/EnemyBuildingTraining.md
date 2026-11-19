# Enemy building training

The enemy strategic AI trains the units it picks at its own Soviet buildings. A building takes time over each unit
and shows that it is working while it trains, like the cranes on the player's buildings. Nothing appears at once.

## Flow

1. `UEnemyStrategicAIComponent::Training_ThinkStep` refreshes the trainers. These are the buildings of the controller's
   own player (`UGameUnitManager::GetPlayerBxps`) that carry a `UEnemyBuildingTrainerComponent`.
2. Picking is unchanged (tech unlocks, pressure buckets, random batch, reservation, points). One filter is new:
   squads are only picked while a barracks or tent stands, and tanks only while a vehicle building stands. A
   reservation whose kind has no building left is dropped instead of blocking the AI. Units the async spawner has no
   class for are never picked. `Squad_Rus_HazmatBazooka` had none in a survival match on 2026-11-19 (its tent trained it
   for 26 s, then refunded it). Fixed the same day:
   - a `SetupRusSquads` row was added to `RTS_BP_RTSAsyncSpawner`;
   - `Rus_Squad_HazmatBazooka`'s construction script set its own subtype to `Squad_Rus_HazmatEngineers`, a copy
     leftover, and now sets `Squad_Rus_HazmatBazooka`.
3. `CreateTrainingBatch` hands every paid unit to the least busy standing building of its kind
   (`GetQueuedSecondsRemaining`). A unit no building can take gets its points back.
   - Legacy path: a level whose enemy `UTrainerComponent`s are on the blackboard (`InitTrainerAsEnemyTrainerComponent`)
     still spawns at that trainer at once. No Soviet building uses that path.
   - The old base-cluster fallback is gone. It projected the base point with a 2 cm extent and in practice trained
     nothing.
4. The building trains its queue one unit at a time. For each unit:
   - `OnTrainingStarted` fires, and `OnTrainingActiveChanged(true)` fires once per busy stretch.
   - When the time is up, the async spawner creates the unit at the exit for the building's owner.
   - `UEnemyStrategicAIComponent::OnBuildingTrainerUnitSpawned` runs the same path as every trained unit:
     `RegisterAsBlackboardIdle`, which puts the unit in the controller's direct-control pool, then a move to a base
     defense position.
5. Building destroyed, packed up or removed mid-training:
   - The queue is dropped and `OnTrainingCancelled` fires per unit.
   - Units still waiting refund their points. The unit in training is lost.

## Training time

`20 s + 1.5 s per training point`, capped at 120 s (`EnemyAISettings::UnitTraining::BuildingTraining*`). Each
building can scale it with `TrainingTimeMlt`.

| Unit | Points | Time |
|---|---|---|
| Mosin squad | 3 | 25 s |
| T-70 | 12 | 38 s |
| T-34/76 | 20 | 50 s |
| KV-1 | 28 | 62 s |
| IS-2 | 55 | 103 s |

## Setting up a building (Blueprint only)

Add a `UEnemyBuildingTrainerComponent` ("Enemy Building Trainer") to the building Blueprint:

| Property | |
|---|---|
| `TrainerKind` | `Infantry` (barracks, tents) or `Vehicles` (factory, assembly yard, platform factory) |
| `TrainingTimeMlt` | 1 unless the user asks otherwise |
| `ExitPointComponentName` | default `AITrainingExit`: add a Scene component with that name where the unit leaves, with +X pointing out. It must be on navmesh within 5 m (25 m at most), outside the building's collision. Without one, the unit leaves 4 m in front of the bounds. |
| `IdleAnimation` | `FinishCycle` (default: cranes finish their loop, then rest), `ReturnToStart`, `PauseInPlace` |
| `IdleRestSeconds` | where in the loop the cranes rest and where training starts (default 0 = frame 0); pick a moment with nothing hanging in the air. Equal-length loops stay in step. |

Activity comes from component tags, with no Blueprint graph needed:

| Tag | Component | While training | Idle |
|---|---|---|---|
| `AITrainingActivity` | skeletal mesh, single-node animation (`AnimationSingleNode`) | plays looping from `IdleRestSeconds`; all tagged meshes start on the same call, so equal-length loops stay in step | per `IdleAnimation` |
| `AITrainingActivity` | light | visible | hidden |
| `AITrainingActivity` | Niagara / Cascade | activated | deactivated |
| `AITrainingActivity` | audio | plays | fades out (`AudioFadeOutSeconds`) |
| `AITrainingActivity` | any other scene component | visible (with children) | hidden |
| `AITrainingHideWhileTraining` | scene component | hidden | visible |
| `AITrainingGlow` | static or skeletal mesh | material scalar `AITrainingActive` = 1 | 0 |

- While idle, the component puts the tagged components back to the idle look once a second. A Blueprint that shows its
  machinery again after construction cannot leave lights on or cranes running. Call `RefreshActivityState` after such
  a change for an immediate update.
- Anything else, such as doors on a timeline, montages or anim-Blueprint meshes, binds `OnTrainingActiveChanged`,
  `OnTrainingStarted`, `OnTrainingFinished` or `OnTrainingCancelled`.
- Pure getters: `GetIsTraining`, `GetActiveTrainingProgress` (0..1), `GetActiveTrainingOption`, `GetQueueLength`.

## Buildings that train (user, 2026-11-18)

| Blueprint | Kind | Activity |
|---|---|---|
| `Rus_Building_Barracks` (+ `_Concrete` child) | Infantry | window, door and gable glow, door bulb and doorstep light, chimney smoke (now off when idle); `_Concrete` also has a 9-man drill squad and sergeant on the Appellplatz. Script: `_AssetSource/BarracksTraining/ue_build_barracks_training.py` |
| `Rus_Building_LargeTent` (+ `_Soviet` child, inherits) | Infantry | lit door seams and windows, door lamp and light pool, stove-pipe smoke. Script: `Source/Tools/EnemyBuildingTrainer/LargeTent/ue_largetent_ai_training.py` |
| `Rus_Building_Factory` (A2 crane) | Vehicles | A2 crane loop only while training (frame 0 = doors closed, jib parked), gate work light, roof weld light, sparks. Script: `Source/Tools/RusFactoryA2/ue_rusfactory_a2_trainer.py` |
| `AssemblyYard/Rus_Building_AssemblyYard` | Vehicles | four cranes in step only while training, six floodlights, a T-34 hull in the bay while training; exit 7 m past the ramp foot |
| `Rus_Building_PlatformFactory` | Vehicles | floods, weld light, sparks; trolley on the fixed beam (user pick B, 2026-11-19) |

- Cooling towers, Research Center and Admin Bunker only unlock tech. They do not train.
- Tech unlocks are unchanged: an assembly yard or platform factory unlocks no tier by itself. With only one of them
  and no factory or cooling towers, the AI has no light tanks. It trains Tier 2 tanks there once a research center
  stands.

## Test

`Source/Tools/EnemyBuildingTrainer/run_enemy_building_trainer_test.ps1` runs `-EnemyBuildingTrainerTest` headless on
RTS_ToCopySmall, for player 2's map controller. It spawns the buildings on a 60 m grid of navigable slots around the vehicle duel bench field
and routes one unit per building through the strategic AI: a Mosin squad or a BT-7. The first building also gets a
second unit. It checks:

- the building is active at once;
- no unit appears before its time or more than 15 s after;
- the unit appears within 15 m of the exit, is owned by player 2 and is in player 2's direct-control pool;
- tanks can drive 8 m away from the exit;
- the second unit starts straight after the first, with the activity on the whole time;
- the building is idle afterwards;
- one real `Training_ThinkStep` spends points and queues at buildings, with no unit within 12 s;
- a building destroyed mid-training drops 2 queued units, refunds the waiting one and spawns nothing.

| Switch | |
|---|---|
| `-Buildings Factory,AssemblyYard` | subset of the buildings |
| `-Capture` | PNG frames per building: `0_idle`, `1_training30`, `2_training70`, `3_after`, `4_rest` (33 s after: crane cycle finished) |
| `-Exposure <bias>` | exposure bias for the captures |
| `-AddMissing` | adds a runtime trainer to Blueprints without one (core check only) |
| `-SpawnerAudit` | no buildings: spawns every squad and tank of every tech level through the async spawner and checks each reports the ordered type and subtype (`NO_SPAWNER_ROW`, `NO_SPAWN`, `SUBTYPE_MISMATCH`) |
| `-Squad <ESquadSubtype>` / `-Tank <ETankSubtype>` | trains that unit instead of `Squad_Rus_Mosin` / `Tank_BT7`. Every spawned unit must report the ordered type and subtype on its RTS component, which catches wrong spawner rows and copied Blueprints. |

Captures turn off texture streaming and occlusion queries, and refresh skeletal poses before each shot. Before these
fixes, meshes dropped out of frames and cranes showed the previous capture's pose. Second and later tanks of one
building drive to their own spot, 9 m to the side.

The output folder is `Saved/EnemyBuildingTrainer/<time>/`, with `summary.md` and `trainer.log`. A building without tagged
components logs `WARNING no tagged activity components`.

## Verified in a real match (2026-11-19)

The run was SM_Desert_01 with `-SurvivalAutoStart -SurvivalGrace=20 -SovietBaseWatch=900`; nothing was suspended or
forced. Log: `Saved/EnemyBuildingTrainer/survival_watch/survival.log`, lines `AI_TRAIN`.

- Player 2's AI trained squads at its 2 barracks and 2 tents from t = 104 s on: 13 squads in 15 min.
- Each squad left at its exit after its training time.
- The match's 4 bases had no vehicle building, so no tank was picked. The tank route was checked in the headless test's
  real think step.

## Behaviour changes to know

- The Rus Factory A2 crane, the assembly yard cranes and floodlights, and the platform factory trolley and lights are
  still wherever no AI trains: menu battles, the showcase map, hand-placed mission maps.
- Survival presets only place the Rus Factory. The assembly yard and platform factory exist only in hand-placed maps
  (Ger_Trains, the tutorial, RTS_Showcase_SovietArmy, UT_EnemyAI). They unlock no tier by themselves.
- Not verified:
  - brightness on daylight maps (everything was tuned on the dark red test map);
  - hiding during construction and pack-up (checked by reading the graphs, not by a run);
  - the factory's welding sparks, which never showed in a capture.

## Spawner audit (2026-11-19)

The first `-SpawnerAudit` passed 39 of 48 AI-trainable units. After the fixes, 45 of 48 pass
(`Saved/EnemyBuildingTrainer/spawner_audit2`).

- **Copied Blueprints that still set the subtype of the unit they were copied from**, retargeted with UECP:
  - Hazmat Bazooka (said HazmatEngineers);
  - Dyakonov (said Mosin);
  - Large PTRS (said Okhotnik);
  - Toxic Guard (said Red Hammer);
  - T-70 Flame (said T-70, in both `SetUnitSubtype` and `BP_GetTankDataOfPlayer`);
  - IS-3 Flame (said IS-3, in both nodes);
  - SU-122 (its `MySubType` variable said SU-85).
- **Spawner rows added:** Hazmat Bazooka and Large Mix Basic.
- **Left as is: no Blueprint of their own exists.**
  - `Squad_Rus_LargeMixT2` and `Tank_SU_85_L` have no row, so the AI never picks them.
  - `Squad_Rus_GhostsOfStalingrad` is mapped to `Rus_Squad_MosinPTRS` (the Okhotnik squad). Survival waves spawn it
    (`SurvivalWaveSettings`), so the row stays until a real Ghosts squad exists.
- `Rus_Squad_MosinPTRS` is the Okhotnik squad: it declares `Squad_Rus_Okhotnik`.

