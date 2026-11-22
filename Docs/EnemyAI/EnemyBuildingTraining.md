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
   A mission can keep the AI to a few units with `RestrictTrainingToSquads` / `RestrictTrainingToTanks` on the enemy
   controller's `M_EnemyAIMissionSettings` (struct `FEnemyAIMissionSettings`). With both lists empty the AI trains
   anything its buildings unlock. Otherwise it only picks units from the lists, still only those its buildings unlock,
   and an empty list next to a filled one means none of that kind. A pressure that no listed unit fits no longer skips
   the think step: the listed units answer it. `Ger_IntroMission` uses `Squad_Rus_Mosin` only (2026-11-21), so its
   two barracks train Mosin squads.

   A placed enemy controller needs a decision tree (`M_StochasticDecisionTree`) for any of this: training pressure
   comes from the tree's actions, and without one the think loop reports an uninitialised variable every 6 s and
   nothing is trained. Survival copies `DA_SurvivalWaveSettings`' tree in at runtime; a mission map gets its own
   saved copy with `AEnemyController::CopyStrategicDecisionTreeFrom` (Blueprint-callable, run from editor python, see
   `Source/Tools/IntroMission/intro_ai_mosin_setup.py`). With `bAllowDirectControlStochasticDecisionTree` off, the
   tree only feeds training pressure and sends no attacks. The AI earns `TrainingPointsPerMinute` (a float, default 3: one
   Mosin squad a minute; fractions carry over) on top of `TrainingPointsStart` (default 10). Ger_IntroMission uses 1.5
   points a minute from 0 start points, so it trains one Mosin squad (3 points) every 2 minutes.
3. `CreateTrainingBatch` hands every paid unit to the least busy standing building of its kind
   (`GetQueuedSecondsRemaining`). Equally busy buildings (within 0.1 s) take turns: the one that took a unit longest
   ago (`GetLastEnqueuedWorldSeconds`) goes first. A unit no building can take gets its points back.
   - Legacy path: a level whose enemy `UTrainerComponent`s are on the blackboard (`InitTrainerAsEnemyTrainerComponent`)
     still spawns at that trainer at once. No Soviet building uses that path.
   - The old base-cluster fallback is gone. It projected the base point with a 2 cm extent and in practice trained
     nothing.
4. The building trains its queue one unit at a time. For each unit:
   - `OnTrainingStarted` fires, and `OnTrainingActiveChanged(true)` fires once per busy stretch.
   - When the time is up, the async spawner creates the unit at the exit for the building's owner.
   - `UEnemyStrategicAIComponent::OnBuildingTrainerUnitSpawned` runs the same path as every trained unit:
     `RegisterAsBlackboardIdle`, which puts the unit in the controller's direct-control pool.
   - Then it walks to a stand spot near its exit (see Fixes, 3) and waits there 45-60 s
     (`StandSpotWaitSeconds` + seeded `StandSpotWaitJitterSeconds`). Log: `AI_TRAIN stand_spot_wait`.
   - `Training_ReleaseStandSpotWaiters` (every strategic think, 6 s) then sends it to a random base defense position
     (`AI_TRAIN stand_spot_release`). It keeps waiting while it is not idle (walking, fighting) or while the AI has no
     defense position. A unit the AI picked from the idle pool for a mission meanwhile is left to that mission
     (`AI_TRAIN stand_spot_picked`).
   - No stand spot found: straight to a defense position, as before (`AI_TRAIN defense_position`).
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
| `-SpawnQA` | two units per building and no test move orders; exit-view frames `SpawnQA_<Building>_job<N>_<stage>.png` just before the spawn and 0 / 1 / 3 / 8 / 20 s after, plus one `RTS_EBT spawnqa` line per frame (ground offset, building overlap, stacking, exit lanes, fog, facing, movement, active command). A human vision bubble over the first building's exit is the fog control: its units must show, all others stay hidden. It fails on a teleport or drop in the first 2.5 s, a unit seen through the fog (or a control unit that is not shown), overlapping units at +20 s, or a body in any exit lane or within 2.5 m of its exit at +20 s. It skips the think and cancel phases |
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

## Spawn QA (2026-11-19)

The first `-SpawnQA` run is in `Saved/EnemyBuildingTrainer/spawn_qa1`, with the contact sheets `SpawnQA_Infantry.png` and
`SpawnQA_Vehicles.png`.

What is fine:
- every exit faces away from its building;
- no unit appears inside building collision;
- units stand on the ground (2-8 cm) and tanks face out of the exit.

What was not fine (the first three fixed the same day, see below):
- **Squads jump and drop:** about 1 s after they appear, every soldier snaps about 2.1 m along world +X+Y and drops
  from 1.5 m.
  - Cause: `SquadController.cpp` `FindIdealSpawnLocation() + (0, 0, 150)`. The comma operator makes this `+ 150` on
    X, Y and Z.
  - The async spawner's 1 s `TimeNotSelectable` delays the teleport.
  - Player squads share this code.
- **Squads then walk back to the spawn point and end facing the building.**
- **Units stay at the exit when the AI has no base defense position** (the test map has none):
  - the second squad ends inside the first (3-5 of 6 soldiers overlapping at 8 s);
  - the second tank is pushed 9.7 m aside by `MoveClearOfVehiclesWhenSpawned`.
- **Trained units are visible through the fog of war for about 4 s.** `UFowComp` starts participating 4 s after the
  unit starts (`TimeRemainingStartAsyncLoading + 2`), a delay meant for the player trainer's hidden pre-spawn.
- **Debug draw in non-Shipping builds:** a green 1 m sphere with "trained : X" at every trained unit. The ungated
  `DebugTrainedUnit` draws it.
- **Tanks: the first rendered frame is grey and has no tracks**, fine by +1 s. The AI spawns the unit and shows it in
  the same frame; the player trainer has it ticking hidden first.
- **Barracks:** the squad formation reaches into the sandbag piles beside the door, which don't block pawns.

### Fixes (user: "1,2,3", 2026-11-19)

1. **Squad spawn placement** (shared by every squad the async spawner or a trainer places):
   - `SquadController.cpp` lifts each grid point by the soldier's capsule half height + 10 cm, instead of the comma
     `(0, 0, 150)`.
   - `ASquadUnit::OnSquadUnitSpawned_SetEnabled` places a soldier that already stands within 6 m of its spot in the
     same frame it shows. Only its selection waits for `TimeNotSelectable`.
   - A soldier far away (the player trainer's pre-spawn at the spawner) still moves when the delay ends, as before.
   - The parachute paths cancel the pending placement as before. The at-once placement only changes where in its
     2.5 m grid a soldier stands before the drop takes over.
2. **Fog:**
   - `UFowComp::StartFowNowHiddenUntilSeen` joins the fog now, the owner hidden until the next fog tick (0.1 s) shows
     it.
   - Called before `StartFow`, the next start goes that way. Without a fog manager it does nothing.
   - The strategic AI calls it for every building-trained unit and its soldiers: at spawn, and again once a squad is
     loaded.
3. **Stand spots** (`EnemyTrainedUnitStandSpots`):
   - Every building-trained unit is sent to a stand spot first (user, 2026-11-19: "wait near their building and after
     some time go to a defensive position"). Before that, only units without a base defense position used one.
   - **Where spots are tried:** rings around the exit, nearest first, the ground in front of the exit before the
     building's sides, up to 50 m and 120 degrees off the exit axis.
   - **What a spot must pass:**
     - its whole footprint on the unit's own navmesh (tanks use their agent's);
     - a straight navmesh line from the exit, when any spot has one;
     - outside every trainer's exit lane: 18 m long, 3 m half-width for infantry and 5 m for vehicles;
     - clear of the side's soldiers and tanks, and of the spots other units were sent to, by both radii + 1 m.
   - **Facing:** the unit faces away from the building.
   - **Footprint radius:** squads `max(250, 130 * sqrt(members))`; other units their colliding local bounds, clamped
     to 250-800 cm.
   - **Reservations:** a spot stays reserved for 60 s, then for as long as its unit stands on it.
   - **Log:** `AI_TRAIN stand_spot`.
   - **Then the defense position:** after the wait (see Flow, 4).
   - **Cover keep-out:** every trainer registers its exit with `URTSCoverFinderWorldSubsystem::AddCoverKeepOutZone`
     (refreshed by the 1 s owner check, removed at EndPlay).
     - The zone is the exit lane plus 3 m behind the exit toward the door
       (`EnemyAISettings::UnitTraining::ExitCoverKeepOutBehindCm`).
     - That player's soldiers take no cover in it while they look for cover without a target (idle). Before this,
       idle squads auto-covered at the sandbags flanking the barracks door, 2 m from where the next squad appears.
     - Combat and stress-retreat cover still use every point, so defenders under fire keep the door's sandbags.

After the fixes (final run `spawn_qa6`, Development build; sheets `SpawnQA_Infantry_after.png` /
`SpawnQA_Vehicles_after.png`; the plain run `integration_final` passes):
- **Squads** appear on the ground with no jump or drop.
- **Fog:** units stay hidden through the fog, and the control building's units show.
- **Where units go:** squads walk to spots 8.4 m off the exit and tanks drive to spots 10 m off it. Second units take
  the other side of the lane.
- **Nothing in the way:** no body stands in an exit lane, and the nearest soldier to an exit is 3.2 m away
  (2.06 m before the cover keep-out).
- **One overlap (open):** in `spawn_qa6` two soldiers stood on the same spot at +20 s. `spawn_qa7` (which passes)
  logged why: two soldiers at exactly (11200, -22200), each holding a different cover point id (state 4).
  - So the cover finder has two cover points at the same location, and each can be reserved.
  - It is not the stand-spot search and not a keep-out fallback. It was not fixed.

Real survival match after the fixes (`survival_watch2`: SM_Desert_01, `-SovietBaseWatch=480`, nothing forced):
- 10 units trained. All 10 took the defense-position branch (`AI_TRAIN defense_position`) and none used a stand spot.
- The defense positions were 51 m to 448 m from the unit's building, often at another base.
- So in matches the stand spots only matter while the AI has no defense position. A match always gave one here.

Things to know:
- **Idle Soviet squads then spread into cover** within `M_EnemyAutomaticCoverSearchRadius` (25 m, by design), so
  soldiers end up along the building's sandbags and walls. The exit keep-out zones keep that cover off the door's
  flanks. The QA checks every body, not the squad's centre.
- **The optimizer's 0.5 s ticks:** `URTSSquadUnitOptimizer` moves soldiers outside the player's view in 0.5 s
  movement ticks, about 250 cm steps at walking speed. The QA forces per-frame movement while it samples, as
  `SquadMarchTestScenario` does; otherwise those steps read as teleports.
- **Still open (not asked for):**
  - the green `DebugTrainedUnit` sphere;
  - the grey, trackless first tank frame;
  - the barracks sandbags.


## Wait at the stand spot, then defend (user, 2026-11-19)

Every building-trained unit now waits at its stand spot for 45-60 s, then goes to a base defense position (see Flow, 4).
Checked in a real match (`survival_watch3`: SM_Desert_01, `-SovietBaseWatch=480`, Development build, nothing forced):

- 4 units were trained (Mosin squad, T-26, BT-7, T-70). All 4 went to a stand spot 8-11 m from their exit
  (`AI_TRAIN stand_spot_wait`), although the AI had defense positions the whole match.
- The Mosin squad and the T-26 left after 54-56 s for defense positions 28 m and 80 m from their exits
  (`stand_spot_release`). The release runs on the 6 s strategic think, so a unit leaves up to 6 s after its time.
- The BT-7 and T-70 were taken out of the idle pool by something else just before or at their release
  (`stand_spot_picked`), and were left to that order. The decision tree does not log, so which mission took them
  was not checked.
- Not checked: how the units looked while they waited (no captures), and the Shipping build.
