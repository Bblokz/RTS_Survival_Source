# Berlin mission: mission Blueprints (scripting design)

Status: design, 2026-11-22. It turns the flow in [BerlinMissionFlow.md](BerlinMissionFlow.md) into `UMissionBase`
Blueprint stages for the `BP_MissionManager` on `Ger_Berlin`.

**Owner decisions, 2026-11-22:**
- the array order and build order in section 6, as proposed;
- the Forge stage gets the reshape in section 5;
- the runner plate stays open with a count (4.5);
- G1-G4 in section 3 are approved.

## 1. Rules (owner, 2026-11-22)

1. **One Blueprint per stage**, each a child of `UMissionBase` (or of a C++ mission class), in
   `/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/`, named `Mission_Berlin_<Stage>`.
2. **A flat `Missions` array.** Every stage has its own index in the manager's `Missions` array and is started by its
   own `MissionTrigger`. Usually that trigger is "Mission Class Completed"
   (`UMissionClassCompletedMissionTrigger`), watching the stage before it. No `NextMission` nesting and no
   `TriggerableMissions`, as on `Ger_IntroMission` (see `GerIntroMission.md`).
3. **The event graph shows the flow.** Every event, and every wait, is in the event graph, so the mission reads from
   left to right.
4. **Steps that always run together are Blueprint functions** on that stage (for example `SetUpSabotageTargets`), called
   from the event graph. A function is a synchronous step: it never waits and never schedules anything.
5. **Waits use the mission scheduler.** Use `ScheduleSingleCallback(Callback, DelaySeconds)` with a Create Event node
   (`FMissionScheduledCallback`, no parameters) that points at a custom event in the same graph, not a Delay node. For a
   repeated check, use `ScheduleRepeatedCallback`. Delays are whole seconds (`int32`). Keep the returned task id in a
   variable when the wait may have to be cancelled (`CancelScheduledCallback`).

## 2. Framework facts the design is built on

Read from `MissionBase.h/.cpp`, `MissionManager.cpp` and `MissionTrigger/`:

- The manager starts every `Missions[]` entry at begin play. A stage with a trigger waits, and the manager ticks its
  trigger.
- `BP_OnMissionStart` fires only **once the trigger has fired**, together with the plate. Nothing in a Blueprint runs
  before that, because there is no Blueprint event at load time. Setup that must happen earlier (enemy forces) therefore
  belongs to an earlier stage. Here that is `Arrival`.
- **Every started mission shows a plate** (MAJOR or BONUS). There is no plate-less mission. A text-only mission
  (`bIsTextOnlyMission`) shows a plate that completes itself after `TextOnlyMissionDuration`, with no sound.
- `OnMissionComplete` frees the plate, removes the stage's trigger areas and plays the complete sound. It also records
  the class, which is what "Mission Class Completed" triggers check.
- `OnMissionFailed` frees the plate and plays the fail sound. It does **not** record the class, so a failed stage never
  starts a class-completed trigger.
- A class-completed trigger watches **one exact class**: point it at the Blueprint class, not at a C++ parent. It
  cannot express "A or B".
- A Blueprint **cannot ask** whether another stage has completed (`GetHasCompletedMissionClassExact` is C++ only), and
  is **not told** when another stage ends.
- Trigger areas (`CreateTriggerAreaSphere` / `Rectangle`) are invisible runtime actors owned by the stage that created
  them. They report to that stage's `BP_OnTriggerAreaCallback` with a `TriggerID`.

## 3. Small generic C++ additions (go-ahead)

These make the graph-only rule work for Berlin and are reusable on every map. Each is a few lines on `UMissionBase`, the
manager or the trigger.

| Id | Addition | Why Berlin needs it |
|---|---|---|
| G1 | `GetHasMissionClassCompleted(Class)` and `GetHasMissionClassFailed(Class)`, both BlueprintPure, on `UMissionBase` | `ResponseForce` asks whether the rail yard was sabotaged; lines pick a variant |
| G2 | Event `BP_OnOtherMissionEnded(MissionClass, bCompleted)` sent by the manager to every active stage | Bonuses react to the alarm in their own graph: the rail yard fails, the runner plate closes, Forge crews react |
| G3 | `bAlsoWhenFailed` on `UMissionClassCompletedMissionTrigger` (the manager also records failed classes) | `ResponseForce` starts when `StayUndetected` **fails** (the alarm) |
| G4 | Generic `UPlayerUnitsNearMissionTrigger` (an actor tag or a location, plus a radius), made from the Forge station's `UForgeStationSightedTrigger` | Bonus plates appear when discovered: rail yard, Reichstag, cellar, Demeter pod, reaching the Leitturm |

**Built 2026-11-22** (compiled in the DebugGame editor target; not yet run in a game; the Development DLL is not
rebuilt):
- **G1:** `UMissionBase::GetHasMissionClassCompleted` / `GetHasMissionClassFailed`, which use the new
  `AMissionManager::GetHasFailedMissionClassExact` and `M_FailedMissionClassPaths`.
- **G2:** `BP_OnOtherMissionEnded(EndedMissionClass, bCompleted)` and the native twin `OnOtherMissionEndedNative`.
  The manager sends them from `NotifyActiveMissionsOfEndedMission` over a snapshot of the active missions. A stage only
  receives them after its trigger has fired.
- **G3:** `UMissionClassCompletedMissionTrigger::bAlsoTriggerWhenFailed`.
- **G4:** `UPlayerUnitsNearMissionTrigger` (`Missions/MissionTrigger/PlayerUnitsNearTrigger/`), with `CentreActorTag` or
  `CentreLocation`, `Radius` and `CheckIntervalSeconds`. The centre actor is found once by tag.

- **G5** (added 2026-11-22 at the Forge session's request, same rules):
  - `bCompleteMissionWhenTrackingDone` (default true);
  - `BP_OnTrackingReachedMaxCount` and its native twin `OnTrackingReachedMaxCountNative`.

  `TrackingFunction` / `TrackActorsWithMiniMapProgress` used to complete the mission by themselves when the count was
  full, so a graph's "line, then wait, then complete" never ran. The RailYard and RaidReichstag stages set the flag to
  false and continue from the event.

Without G1-G3 the alarm cannot reach other stages without polling C++ state. Without G4 every bonus plate is up from
the start (7 plates at once).

Berlin-specific C++ that the flow already needs (BerlinMissionFlow.md section 7):
- **Fleeing system.** A unit enters or leaves the Fleeing state, with a BlueprintPure `GetIsFleeing(Actor)` and a
  trigger `UBerlinUnitFleeingTrigger` (the first unit starts to flee).
- **`BP_BerlinDetectionZone` marker actor.** A visible ring with a radius, placed and moved in the editor. Stages find
  these actors by class and put a trigger area on each.
- **A count query**, for example `CountUnitsInRadius(Location, Radius, Owner, Kind)`, for the relay hold, unless one
  already exists.

## 4. The stages

The **Plate** column gives the objective type (MAJOR or BONUS) and the plate text.

| Stage | Parent | Trigger (watched class) | Plate | Ends with |
|---|---|---|---|---|
| `Mission_Berlin_Arrival` | `UMissionBase` | none (starts at begin play) | MAJOR: Set up in the Friedrichshain ruins | Complete: HQ deployed |
| `Mission_Berlin_StayUndetected` | `UMissionBase` | Class completed: `Arrival` | BONUS: Stay undetected | **Fail** = the alarm |
| `Mission_Berlin_ReachLeitturm` | `UMissionBase` | Class completed: `Arrival` | MAJOR: Reach the Leitturm | Complete: player unit at the Zoo tower |
| `Mission_Berlin_KeepHQ` | `UDontLoseHQMission` | Class completed: `Arrival` | MAJOR: Keep the HQ alive (collapsed) | Defeat when the HQ dies |
| `Mission_Berlin_StopRunners` | `UMissionBase` | `UBerlinUnitFleeingTrigger` | BONUS: Stop the runners (count) | Fail on the alarm (G2) |
| `Mission_Berlin_CellarPeople` | `UMissionBase` | Player units near the brewery (G4) | BONUS: The cellar people | Complete: zones revealed |
| `Mission_Berlin_RaidReichstag` | `UMissionBase` | Player units near the Reichstag (G4) | BONUS: Raid the Reichstag | Complete: factory dead |
| `Mission_Berlin_ForgeStation` | `UMission_BerlinForgeStation` (C++, built) | `UForgeStationSightedTrigger` | BONUS: The Forge survey station | Complete: 3 targets dead |
| `Mission_Berlin_RailYard` | `UMissionBase` | Player units near the yard (G4) | BONUS: Sabotage the rail yard | Complete: both targets dead; fail on the alarm |
| `Mission_Berlin_DemeterPod` | `UMissionBase`, text-only | Player units near the pod (G4) | Text plate "Found: Project Demeter", auto-completes | Completes by itself |
| `Mission_Berlin_ResponseForce` | `UMissionBase` | Class **failed**: `StayUndetected` (G3) | MAJOR: The response force is coming | Never (the relay or the HQ ends the game) |
| `Mission_Berlin_HoldRelay` | `UMissionBase` | Class completed: `ReachLeitturm` | MAJOR: Hold the relay bunker | Complete: transfer done, then victory |

### 4.1 `Mission_Berlin_Arrival`

- **Event graph:** `BP_OnMissionStart` → `SetUpEnemyForces` → `StartLocationIntro` → `BP_OnLocationIntroFinished` →
  `PlayPortrait(Crater)` → `ScheduleSingleCallback(33 s)` → `OnCraterLineDone`: start the Leitturm camera sequence
  (cinematic take-over) with `PlayPortrait(Leitturm)` → sequence finished →
  `PlayPortrait(StayQuiet)` → `RegisterCallbackOnNomadicVehicle(PlayerHQ, bOnBuildingConversion)` →
  `BP_OnCallBackNomadicVehicle` → `OnMissionComplete`.
- **Functions:**
  - `SetUpEnemyForces`: load the Berlin forces data asset for the difficulty, start the patrols (wave spawners), start
    the Reichstag factory training, enable the strongpoints.
  - `StartLocationIntro`: Berlin texts.
  - `GetPlayerHQ`.

### 4.2 `Mission_Berlin_StayUndetected` (owns the alarm)

All three alarm sources sit in this one graph, so no other stage has to raise the alarm.

- **Event graph:** `BP_OnMissionStart` → `CreateDetectionZoneTriggers` → `CreateForecourtTrigger` →
  `CreateRelayBunkerTrigger`.
- `BP_OnTriggerAreaCallback`, switching on `TriggerID`:
  - **Zone:** the actor is a player unit → `RaiseAlarm`.
  - **Forecourt:** the actor is an enemy and `GetIsFleeing` is true → `ScheduleSingleCallback(grace seconds)` →
    `OnGraceOver`: still alive and in the forecourt → `RaiseAlarm`.
  - **Bunker:** the actor is player infantry → `RaiseAlarm` (C2).
- **Functions:**
  - `CreateDetectionZoneTriggers`: one sphere per `BP_BerlinDetectionZone`, at the zone's radius, with player overlap
    logic.
  - `CreateForecourtTrigger`: by the ruin, about (8100, -15500), enemy overlap logic.
  - `CreateRelayBunkerTrigger`.
  - `RaiseAlarm`: a guard flag, then `OnMissionFailed`. The fail sound is the alarm sting.

### 4.3 `Mission_Berlin_ResponseForce`

- **Event graph:** `BP_OnMissionStart` → `PlayAlarmLine`, which plays `Berlin_RelayTransferStart` when
  `GetHasMissionClassCompleted(ReachLeitturm)` is true (G1) and `Berlin_AlarmRaised` otherwise.
  → `GetRailDelaySeconds` → `SpawnMissionTimerWidget("Rail arrival", RailSeconds + Delay)`
  → `ScheduleSingleCallback(RailSeconds + Delay)` → `OnRailArrives` → `SpawnRailWave`
  → `ScheduleSingleCallback(RoadSeconds)` → `OnRoadConvoy` → `StartRoadConvoy` → `PlayPortrait(RoadConvoy)`.
  Running in parallel: `StartShieldWaves`.
- **Functions:**
  - `GetRailDelaySeconds`: G1 on `RailYard`; the delay value comes from the mission node.
  - `SpawnRailWave`: wave spawner at the yard, with an attack-move on the HQ.
  - `StartRoadConvoy`: `AEnemyConvoyRoute`.
  - `StartShieldWaves`: Reichstag waves toward the player's western forces.
  - `ShowResponseOnMiniMap`.
- The response-force numbers (owner) are variables on this Blueprint, one set per difficulty.

### 4.4 `Mission_Berlin_ReachLeitturm` and `Mission_Berlin_HoldRelay`

- **ReachLeitturm event graph:** `BP_OnMissionStart` → `CreateZooApproachTrigger` → `BP_OnTriggerAreaCallback` →
  `PlayPortrait(LeitturmReached)` → `OnMissionComplete`.
- **HoldRelay event graph:** `BP_OnMissionStart` → `CreateBunkerTrigger` → the first player infantry inside →
  `StartTransfer` → `ScheduleRepeatedCallback(every 1 s, repeat forever)` → `OnTransferTick` → `UpdateTransfer`.
  When progress is full: `CancelScheduledCallback` → `PlayPortrait(DataSecured)` → `ScheduleSingleCallback(4 s)` →
  `OnMissionComplete` → `TriggerVictory`.
- **Functions:**
  - `StartTransfer`: show the bar.
  - `UpdateTransfer`: count player and enemy infantry in the bunker radius, then add progress, pause it or drain it
    (contested rule, flow 4.4).
  - `SetTransferProgressText`.

### 4.5 Bonus stages

- **StopRunners:**
  - On start: `PlayPortrait(RunnerFleeing)`, then `TrackFleeingGroup` (minimap markers, count text).
  - A repeated 1 s callback runs `RefreshRunnerCount`. The count rises with new runners and falls with kills; at 0 the
    text changes to "No runners. Stay quiet." and the plate collapses.
  - `BP_OnOtherMissionEnded(StayUndetected, failed)` → `OnMissionFailed`.
  - This **replaces the "completes and re-opens" rule** in flow 4.3: one stage instance cannot complete twice. See
    question 3.
- **CellarPeople:** on start → `PlayPortrait(CellarPeople)` → `RevealDetectionZonesAndPatrols` (minimap icons on every
  zone marker and patrol route) → `ScheduleSingleCallback(3 s)` → `OnMissionComplete`.
- **RaidReichstag:** on start → `RegisterFactoryCallbacks` (`RegisterCallbackOnBXPDies` / building death) → factory
  dead → `StopFactoryTraining` → `PlayPortrait(ReichstagRaided)` → `OnMissionComplete`.
- **RailYard:**
  - On start → `SetUpSabotageTargets` (switch tower + ramp, tracked with `TrackActorsWithMiniMapProgress`;
    `bCompleteMissionWhenTrackingDone` = false).
  - `BP_OnTrackingReachedMaxCount` → `PlayPortrait(RailYardSabotaged)` → `ScheduleSingleCallback(3 s)` →
    `OnMissionComplete`.
  - `BP_OnOtherMissionEnded(StayUndetected, failed)` before that → `OnMissionFailed`.
- **ForgeStation:** see section 5.
- **DemeterPod:** a text-only plate. On start → `AddArchiveItem(Demeter)` → `PlayPortrait(DemeterPod)`. The plate
  completes itself. (The flow doc said "no plate"; a short "found" plate is the graph-only way. A truly silent stage
  would need a hidden objective type in C++.)

## 5. The Forge station stage (already built in C++)

`UMission_BerlinForgeStation` (Source `bee57956`) runs its own sequence in C++. It sets up the garrison in its native
`OnMissionStart` (before the trigger), counts the targets in `TickMission`, and runs the intel and reinforcement waits
on `FTimerHandle`s. That works, but the flow cannot be seen in a graph, as these rules require.

Proposal: keep the C++ for the parts that are not flow, and move the order of events into the Blueprint child
`Mission_Berlin_ForgeStation`:
- **C++ keeps** the pre-trigger garrison setup and target tracking. A Blueprint cannot run before the trigger (section
  2).
- **C++ exposes** BlueprintCallable steps (`GiveIntel`, `LaunchReinforcements`) and one event,
  `BP_OnAllStationTargetsDestroyed`.
- **The event graph:** `BP_OnAllStationTargetsDestroyed` → `ScheduleSingleCallback(1 s)` → `GiveIntel` →
  `ScheduleSingleCallback(8 s)` → `PlayPortrait(Reinforcements)` + `LaunchReinforcements` → `OnMissionComplete`.
  The two timers and the tick-driven completion go.

This changes another session's committed work, so it waits for the owner (question 2). If approved, it goes to the Forge
station session.

## 6. Order

There are two orders:
- **Array order:** the index of each stage in `Missions`. With class-completed triggers it only affects readability.
- **Build order:** which stages are made first, so the mission can grow a step at a time and stay playable.

Proposed array order (index: stage, what it waits on):

0. `Arrival`: nothing.
1. `ReachLeitturm`: Arrival.
2. `HoldRelay`: ReachLeitturm.
3. `KeepHQ`: Arrival.
4. `StayUndetected`: Arrival.
5. `ResponseForce`: StayUndetected failed.
6. `StopRunners`: the first fleeing unit.
7. `CellarPeople`: player near the brewery.
8. `RailYard`: player near the yard.
9. `ForgeStation`: player near the station.
10. `RaidReichstag`: player near the Reichstag.
11. `DemeterPod`: player near the pod.

Proposed build order:
1. `Arrival`.
2. `ReachLeitturm` and `HoldRelay`: the mission can be won.
3. `KeepHQ`.
4. G1-G4.
5. `StayUndetected`, `ResponseForce` and the detection-zone marker.
6. `ForgeStation` (reshape), `RaidReichstag`, `RailYard`, `CellarPeople` and `DemeterPod`.
7. `StopRunners`, once the fleeing system exists.

The owner accepted both orders as proposed on 2026-11-22.

## 7. Build status (2026-11-22, evening)

**Everything in sections 4 and 5 is built and wired into `Ger_Berlin`.** Done:
- all stage Blueprints compile in the editor and after a fresh load from disk (`Saved/BerlinStages/compile_stages.py`);
- the map was read back from disk (`Source/Tools/BerlinMap/mission/verify_wire_berlin_stages.py`).

Not done:
- **no stage has run in a game** (the owner tests);
- **nothing is committed**;
- the Development DLL was older than the C++ in 7.4 at first; it has been rebuilt since (7.6).

Built with the editor bridge (`synthesize_graph` for functions, `build_blueprint_graph` for event graphs) in throwaway
DebugGame editors. Assets are in `/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/`.

**Waits.** A custom event's red delegate pin (`OutputDelegate`) is wired straight into the `Callback` pin of
`ScheduleSingleCallback` / `ScheduleRepeatedCallback`. That works like a Create Event node and needs no extra node.

### 7.1 `Ger_Berlin` `BP_MissionManager.Missions` (65.83 MB; navmesh and the 16 `RY_*` actors kept)

| # | Stage | Trigger | Plate |
|---|---|---|---|
| 0 | Arrival | none | MAJOR: Set up in the Friedrichshain ruins |
| 1 | ReachLeitturm | after Arrival | MAJOR: Reach the Leitturm |
| 2 | HoldRelay | after ReachLeitturm | MAJOR: Hold the relay bunker (then victory) |
| 3 | KeepHQ | after Arrival | MAJOR: Keep the HQ alive (collapsed) |
| 4 | StayUndetected | after Arrival | BONUS: Stay undetected (fails = the alarm) |
| 5 | ResponseForce | after StayUndetected **or failed** | MAJOR: The alarm is raised |
| 6 | StopRunners | Enemy Fleeing (first unit flees) | BONUS: Stop the runners |
| 7 | CellarPeople | player units near the brewery, 30 m | BONUS: The cellar people |
| 8 | RailYard | player units near the yard, 90 m | BONUS: Sabotage the rail yard |
| 9 | ForgeStation | Forge sighted trigger (Forge session) | BONUS: The Forge survey station |
| 10 | RaidReichstag | player units near the Reichstag, 90 m | BONUS: Raid the Reichstag |
| 11 | DemeterPod | player units near the pod, 25 m | Text plate: Found: Project Demeter |
| 12 | Counterattack | after ResponseForce | MAJOR: Hold out against the response force |

Index 12 is new: the response force needed a second stage (7.3).

Wiring tool: `Source/Tools/BerlinMap/mission/wire_berlin_stages.py`. It can be re-run: it keeps existing instances and
only replaces triggers that differ. Run it in an editor with the map open, then save.

**Trap:** a stage instance put on the map **before** its class defaults are set keeps the old values as overrides.
DemeterPod showed "Mission Title" and was replaced. Set the defaults first, then wire.

Pre-edit backup of the map: `Saved/BerlinStages/backup/Ger_Berlin_2026-11-22_1837.umap`.

### 7.2 Map additions

`BP_BerlinDetectionZone` (`Blueprints/Missions/GerCampaign/Berlin`) is a placed marker with an instance-editable
`Radius` and `ZoneName`, and a red editor-only sphere (no collision, hidden in game). Four are placed (folder
`Berlin/DetectionZones`):

| Zone | Position | Radius |
|---|---|---|
| `DZ_Reichstag` | (8100, -15500), the built ruin | 60 m |
| `DZ_RailYard` | (542, 19444) | 40 m |
| `DZ_AlexPatrol` | (7583, 12222) | 35 m |
| `DZ_MoltkeBridge` | (19500, -15000) | 30 m |

The Forge station has no zone on purpose (BerlinForgeStation.md). Move or resize the zones freely; StayUndetected reads
them at start.

### 7.3 The stages

- **Arrival:**
  - Flow: location intro → crater line → wait 33 s → Zoo tower camera tour (two shots, the Leitturm line as its
    `StartSound`) → stay-quiet line.
  - Then it finds the profile-spawned HQ (retrying every second) and completes when the HQ is deployed.
  - Placeholders: "Berlin / Friedrichshain ruins / 1976" and the camera yaws.
  - `SetUpEnemyForces` is not built (no patrol data yet).
- **ReachLeitturm:** 45 m circle at (-13542, -40556); any player unit enters → line → complete.
- **HoldRelay:**
  - Trigger at the bunker (-10500, -43000), 15 m. The first player squad unit starts the transfer.
  - Progress: +1 s per uncontested second (player infantry inside and no enemy infantry); `TransferSeconds` = 150.
  - Then line → wait 4 s → complete → `TriggerVictory`.
- **KeepHQ:** `UDontLoseHQMission` child, no graph.
- **StayUndetected (owns the alarm):**
  - Its triggers:
    - every detection zone (ids 100+, any player unit);
    - the relay bunker (id 1, player infantry);
    - the Reichstag forecourt (id 2, 20 m round the rally point, a **fleeing** enemy).
  - It also starts the enemy fallback watch (7.4) with `FallbackSettings`: rally (2600, -15994), 20 s of combat,
    half the men lost, squads of 2+, squads within 40 m of the rally stay and fight.
  - Alarm → stop the fallback watch → `OnMissionFailed`.
- **ResponseForce:**
  - The alarm line: `RelayAlarmLine` if ReachLeitturm is done, otherwise `AlarmLine`.
  - Then it holds `BaseHoldSeconds` (8), plus `SabotageDelaySeconds` (90) when RailYard completed before the alarm,
    then completes.
- **Counterattack:**
  - A child of `USovietCounterattackMission` with `DA_Berlin_ResponseForce`, a copy of the Intro asset.
  - Train: runs along `RY_Rail_Main` (136 path points, 3 m apart) and stops at the ramp (-1842, 12950); its units come
    out at (-2800, 14200).
  - Road group: spawns at the south-east edge (-16000, 47500).
  - No air drops, no dig-in intro, no arrival cinematic. Route via (-12000, 45500) and (2000, 44500) to the HQ.
  - Its own countdown ("Response force arrives") follows ResponseForce.
  - Wave sizes and timings are still the Intro's per-difficulty values. **They are the owner's to tune.**
- **StopRunners:**
  - On start: line, then a refresh every second. The refresh counts the living fleeing units, marks them on the
    minimap (`TrackUnitGroupOnMiniMap`) and updates the text: "Runners left: N", or "No runners. Stay quiet." at zero.
  - The stopped line plays when the count drops to zero. The stage fails on the alarm (`BP_OnOtherMissionEnded`).
- **CellarPeople:** line → every detection zone marked on the minimap (a `Warning` icon labelled "Detection zone") →
  one `Squad_Ger_Burrowers` (subtype 61) at (-7600, 36400) with a reinforcements ping → complete after 4 s.
  Patrol routes join the reveal once patrols exist.
- **RailYard:**
  - Registers `RY_Target_Ramp` and `RY_Target_SwitchTower` for the scavenge callback.
  - Each one scavenged counts down. With both done: line → wait 3 s → complete.
  - The alarm before that → fail.
- **RaidReichstag:**
  - Tracks every actor tagged `BerlinReichstagFactory` (`bCompleteMissionWhenTrackingDone` off; line → wait →
    complete).
  - **No such actor is on the map yet:** the Shield base buildings are not placed, so the plate says so.
- **DemeterPod:** text-only plate (10 s). Adds the Lore entry "Project Demeter" (`AddCustomArchiveItem`) and plays the
  line.
- **ForgeStation:** see BerlinForgeStation.md section 10.

Unrecorded lines are empty slots that play nothing:
- `LeitturmReachedLine`, `TransferStartLine`, `DataSecuredLine`;
- `AlarmLine`, `RelayAlarmLine`, `SabotagedDelayLine`;
- `RunnerFleeingLine`, `RunnersStoppedLine`;
- `CellarLine`, `SabotagedLine`, `RaidedLine`, `DemeterLine`.

### 7.4 C++ added for these stages

- **G1-G5:** section 3.
- **G6:** `UMissionBase::AddCustomArchiveItem(FArchiveCustomContent, SortingPriority, NotificationTime)`.
  - It is marked `BlueprintPure=false`: a `const` BlueprintCallable turns into a pure node, which never runs.
- **Enemy fallback:**
  - `UEnemyFallbackSubsystem` (`Enemy/Fallback/`):
    - control: `StartFallbackWatch(FEnemyFallbackSettings)`, `StopFallbackWatch`, `ForceUnitToFallBack`;
    - queries: `GetIsFleeing`, `GetFleeingCount`, `GetFleeingUnits`, `GetTotalFledCount`, `GetRallyLocation`.
  - Every second it checks the watched player's squads (`OwningPlayerToWatch`, 2):
    - it remembers each squad's full strength and the time of its first loss;
    - after `MinCombatSeconds` of fighting, with `LossFractionToFallBack` of the men lost, the squad gives up;
    - it then runs to the rally point through the new `AEnemyController::RetreatUnitToRallyPoint`. That function
      takes the squad out of its formation (`RemoveUnitsFromAnyFormation`) and starts a retreat-controller retreat
      with `RemoveUnits`, so the unit disappears on arrival.
  - `ForceUnitToFallBack` is for later patrol scripting: infantry whose vehicles died.
- **`UEnemyFleeingMissionTrigger`:** a new `EMissionTriggerType::EnemyFleeing`; fires once any unit has fled.

### 7.5 Still to do (not built)

- **The Reichstag Shield base:**
  - the factory, barracks and pits (concept B2) placed in the ruin;
  - the factory tagged `BerlinReichstagFactory`;
  - training all mission long (flow 4.6);
  - the Shield waves after the alarm.
- **Patrols near the start**, from the Berlin forces data asset (`UBerlinMissionForcesData` holds only the Forge
  station so far). Then:
  - Arrival's `SetUpEnemyForces`;
  - the "vehicles dead → infantry run" rule (`ForceUnitToFallBack`);
  - patrol routes in the cellar reveal.
- **The player start force:** whether it includes a German HQ truck. Arrival waits for one.
- **Recording** the voice lines listed in 7.3.

### 7.6 Readable graphs and review fixes (2026-11-22, late)

Owner rule: graphs must be human-readable, compact, without large gaps between nodes.

- **Event graphs (the flow):** every stage's EventGraph is laid out with explicit coordinates by
  `Source/Tools/BerlinMap/mission/layout_eventgraphs.py` (`build_blueprint_graph`, `clear_before_build`).
  - One row per flow, read left to right.
  - A scheduled wait's custom event sits right after its `Schedule…` node on the same row, so the red delegate wire is
    short.
  - Getters sit just under the pin they feed, and branch arms on a short row below.
  - A comment box per flow.
  - Re-run it after changing a stage's event graph; never run the auto-arranger on an event graph.
- **Functions:**
  - Reroute knots are spliced out and the graph is auto-arranged tightly (`tidy_graphs.py`, spacing 30/20). The
    arranger puts knots back, so splice once more after it.
  - The two sprawling functions were restructured with locals and small helpers:
    - `HoldRelay`: `IsInfantryInBunker(bPlayerUnits)` + `UpdateTransfer` (37 → 25 nodes);
    - `StopRunners`: `GetRunnerInfo`, `ShowRunners`, `ShowNoRunners` + `RefreshRunners` (52 → 17 nodes).
- **Logic check:** the logic read-back (`get_graph_intent`) of every graph was compared before and after. The only
  differences are the intended changes listed here.
- **Arrival:** `HandleMissingHQ`. If no HQ is found after `MaxHQSearchAttempts` (15, one per second), HQ is flown in by
  Mammut glider at `HQGliderBuildLocation` (13900, 43300). This covers a map that does not load the profile units.
- **RailYard:** if its plate only appears after the alarm, it fails at once (`BP_OnOtherMissionEnded` only reaches
  stages that have already started).
- **`UDontLoseHQMission` (C++):** it bound to the HQ only in `OnMissionStart` when already triggered, so a DontLoseHQ
  stage with a trigger (KeepHQ) never tracked the HQ. It now binds in `OnMissionStartedNative`, which runs after the
  trigger and also on the no-trigger path.
- **`FEnemyFallbackSettings.OwningPlayerToWatch`** defaults to `RTSPlayers::FirstAIPlayer` (2).
- **`DA_Berlin_ResponseForce` `SecondsUntilSovietsArrive`:**

  | NewToRTS | Normal | Hard | Brutal | Ironman |
  |---|---|---|---|---|
  | 60 | 45 | 35 | 25 | 25 |

  The Intro's 120-180 s meant the 150 s transfer always finished before any Soviet arrived. These are starting values;
  the owner tunes them.
- **Known simplification:** `SabotageDelaySeconds` delays the whole response (train and road), not only the train.

**Editor DLLs:** both the Development (20:06) and DebugGame DLLs include all of this C++. A Development build at 20:4x
was up to date.
