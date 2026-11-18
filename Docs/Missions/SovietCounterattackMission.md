# Soviet counterattack mission (`USovietCounterattackMission`)

A defend-the-base mission, first used as mission 7 of `Ger_IntroMission` (`Mission_Intro_SovietCounterattack`, after
the base-building tutorial). C++: `Source/RTS_Survival/Missions/MissionClasses/SovietCounterattackMission/`.

## Flow

1. **Prepare.** The objective reads "Prepare to Defend" and a mission timer (`W_BP_MissionTimer`, top of the HUD) counts
   down "Prepare to defend" for `SecondsUntilSovietsArrive`.
2. **Survive.** When it runs out the same widget switches to "Survive" for `WaveCount x SecondsBetweenWaves` seconds.
   Wave 1 comes at once, then one wave every `SecondsBetweenWaves`. Both countdowns and the waves run on the world
   timer manager, so they stay in step (and pause with the game).
3. **Kill the remaining Soviet forces.** When the Survive timer runs out the timer is removed. The objective changes to
   "Kill remaining Soviet forces N", where N counts the living units plus the ones still on their way. Every living
   unit gets an `AttackTarget` icon on the minimap (moved every 0.5 s, removed when it dies). When none is left and
   nothing is still coming, the mission completes.

## Waves

- **Size.** Wave n (from 1) rolls a random count in `[MinUnitsFirstWave, MaxUnitsFirstWave] + (n - 1) x UnitsAddedPerWave`.
- **Units.** Each unit picks one entry of the difficulty's `TankTypes` and `SquadTypes` together, uniformly; list an
  entry twice to make it more common.
- **Split.** The units are shuffled and split as evenly as possible over the three spawns: 12 gives 4/4/4; leftovers
  go to random spawns. Each spawn's share is its own group and attack-moves on its own.
- **Route.** `AttackRoute` (8400,-1500,296) -> (4137,-1752,532) -> the player HQ, read when the group sets off
  (`FallbackHQLocation` (1450,300) without an HQ). Each spawn adds its own XY offset to every point
  (`CenterRouteOffset` 0,0; `TrainRouteOffset` +700,+900; `AirDropRouteOffset` -700,-900), and every point is projected
  onto the navmesh. Groups move with `AEnemyController::MoveAttackMoveFormationToLocation`; they move on from a
  waypoint after 25 s of fighting there (`AttackMoveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint`).
- **Order delay.** A group gets its order `OrderDelaySeconds` (3 s) after its last unit appeared, so fresh squads do not
  drop it.
- **Minimap during the waves.** Each group shows as a survival-style division marker (`TrackUnitGroupOnMiniMap`), removed
  when the remaining phase starts (`bMarkAttackGroupsOnMiniMap`).

## The three spawns

| Spawn | How the units arrive |
|---|---|
| Centre | Async-spawned on a grid (`SpawnSpacing` 8 m, rows trailing away from the route) around `CenterSpawnLocation` (8400,-1500,296), facing route point 2. |
| Landing field | One runtime `AEnemyAirDrop` per pad (`LandingPadCenters`, the 4 Pe-8 pads of the intro landing field; z 0 = ground traced at runtime), one unit each, cargo plane delivery (`GA_SovietT34Drop` copy): vehicles hang on ropes and are lowered (`bRopeSlungVehicles`), squads come in a Pe-8 that lands. A batch is 4 units (one per pad), queued across waves. The batch's units attack together once all four are on the ground (or their aircraft was shot down); the next batch only takes off when every aircraft of this one has left. Only the first aircraft of a batch triggers the announcer warning. |
| Train | A `BP_SovietCargoTrain` (`ASovietCargoTrain`, see `SovietCargoTrain.md`) drives from the north map edge along `Train.PathPoints` (`REAR_Rail_Main` to the junction, then `REAR_Rail_Siding`) to the station, one flatcar with a static tank mesh per vehicle (meshes from `CargoTankMeshes` in turn) and one goods wagon when squads ride along. At the stop the meshes hide after `UnloadDelaySeconds`, the units spawn on a grid around `StationSpawnLocation` (19953,15593,943), and after `DepartDelaySeconds` the empty train drives back out and destroys itself. One train at a time: a wave's train waits while the previous one is still on the rails. |

The landing field units are kept out of the enemy AI's direct control: `FEnemyAirDropDeliverySettings::bHandUnitsToEnemyAI`
(new, default true) sets `UGA_AircraftDrop::SetHandPayloadToEnemyAI` -> `FAircraftDropRequest::bHandPayloadToEnemyAI`.
Released units come back through `UGA_AircraftDrop::OnUnitReleased` -> `AEnemyAirDrop::OnUnitDelivered`; squads use the
new `AAircraftMaster::OnAircraftDropSquadReleased` (tanks the existing `OnAircraftDropPayloadReleased`). A squad whose
aircraft is already gone when it spawns goes to the enemy AI after all.

## Data (`USovietCounterattackSettings`, `DA_Intro_SovietCounterattack`)

Per difficulty (`NewToRTS`, `Normal`, `Hard`, `Brutal`, `Ironman`), all whole numbers:
`SecondsUntilSovietsArrive`, `WaveCount`, `SecondsBetweenWaves`, `MinUnitsFirstWave`, `MaxUnitsFirstWave`,
`UnitsAddedPerWave`, `TankTypes`, `SquadTypes`. The C++ defaults are starting values to tune:

| Difficulty | Prepare | Waves x interval | First wave | +/wave | Tanks | Squads |
|---|---|---|---|---|---|---|
| New to RTS | 240 | 3 x 100 | 6-8 | 1 | BT-7, T-26, T-70 | Mosin |
| Normal | 180 | 4 x 90 | 9-12 | 1 | T-26, BT-7-4, T-70, T-34/76 | Mosin, Okhotnik |
| Hard | 150 | 5 x 90 | 10-13 | 2 | T-70, T-34/76, T-34/76 L, T-34E, SU-76 | Mosin, Okhotnik, Large PTRS |
| Brutal, Ironman | 120 | 6 x 80 | 12-15 | 2 | T-34/76 L, T-34E, T-34/85, SU-85, KV-1 | Red Hammer, Large PTRS, Okhotnik |

Also: the route and offsets, spawn points, pads, air drop and train settings, timer widget class, the timer and
objective texts, and the remaining-unit minimap icon.

## Tools and tests

- `Source/Tools/IntroMission/counterattack/setup_counterattack_assets.py` creates or updates the data asset (map fields:
  timer widget, train class, path from `train_path.json`, cargo meshes) and the mission Blueprint.
- `Source/Tools/IntroMission/rewire_ridge_chain.py` adds `Missions[3]` with a "Mission Class Completed" trigger on
  `Mission_Intro_BaseBuilding` when the Blueprint exists.
- `Source/Tools/IntroMission/counterattack/counterattack_headless_test.py`: throwaway DebugGame editor, cuts the chain to
  the counterattack, shortens every difficulty in memory (prepare 15 s, 2 waves 70 s apart, 6 then 9 units), checks
  the timers and objective texts, where each unit appeared (even split), the train, the groups driving to the HQ, the
  timer's removal and the completion once every Soviet is dead.
- Debug: `SkipPreparation()` starts the waves at once; `GetPhase()`, `GetSpawnedWaveCount()`,
  `GetLivingSovietUnits()`, `GetLivingSovietUnitLocations()`, `GetUndeliveredUnitCount()`.
