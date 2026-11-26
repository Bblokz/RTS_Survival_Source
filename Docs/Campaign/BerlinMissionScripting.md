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
  - `SetUpEnemyForces`: load the Berlin forces data asset for the difficulty, start the Reichstag factory training,
    enable the strongpoints. (Not built. The patrols got their own stage instead, see section 8.)
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
  - The tour lifts the fog 45 m around both shots (`RevealRadius` 4500), so the tower and the radio bunker show. The
    announcer's words run in the subtitle strip at the top (2026-11-24). Skippable (hold Space; prompt visible since
    2026-11-25, see 11.1). Nothing is stationed at the Zoo yet.
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
  - **Since 2026-11-25 every unit comes by train** (owner pick A): `Spawns.bAllUnitsByTrain`, so the road group at the
    map edge (-16000, 47500) no longer spawns. The station's groups take their own route (`bTrainUsesOwnRoute`, empty
    `TrainAttackRoute`): straight from the station to the player HQ (339 m on the tank navmesh).
  - Before: the train groups shared `AttackRoute` (-12000, 45500) → (2000, 44500) → HQ, laid out for the road group.
    They walked 335 m back north to the first point, next to where the train enters, and the groups stalled there (the
    owner's 2026-11-25 play test; a rifleman was still at that point 8 min after the alarm). The cause of the stall is
    not known; all legs are on the navmesh. `RTS_FORMATION_WAIT` warnings now name the units an attack-move group waits
    on after 60 s.
  - One train per wave; the next train leaves only after the previous one has driven out again, so later waves can
    come later than the wave timer.
  - No air drops, no dig-in intro, no arrival cinematic.
  - `BaseReinforcements` is still on (copied from the Intro): at the alarm four Strike Division tanks are flown into the
    player's base. **(open)** whether Berlin keeps that.
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
  - ~~the factory, barracks and pits (concept B2) placed in the ruin; the factory tagged `BerlinReichstagFactory`~~:
    placed 2026-11-24, see section 9;
  - training all mission long (flow 4.6);
  - the Shield waves after the alarm.
- ~~Patrols near the start~~: built 2026-11-24 as their own stage, see section 8 (not in Arrival's
  `SetUpEnemyForces`). Still open: patrol routes in the cellar reveal.
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

## 8. Early patrols (2026-11-24)

Owner ask: the first minutes felt empty. Patrols near the start, marked on the minimap, so the player can avoid them or
wipe them out; beaten squads run to the Reichstag.

### 8.1 What the player sees

- At mission start a **BONUS** plate, "Soviet patrols". Three patrols spawn near the start a few seconds in and loop
  their routes. Each one is a minimap marker that follows the group until its last unit is gone.
- Once the patrols are out, the plate counts the enemy units left. When none is left, the bonus completes.
- **Running:**
  - a patrol whose last tank dies sends its infantry to the Reichstag;
  - any enemy squad (patrol or garrison) that has fought for 20 s and lost half its men also runs (the fallback watch).
  - The first runner starts "Stop the runners". A runner that reaches the forecourt raises the alarm, but only once
    StayUndetected runs (after the HQ deploys).
- A runner that reaches the Reichstag disappears. That also counts as "gone" for the patrol plate, so the bonus
  completes even then; the alarm is the price. **(open)** whether escaped runners should fail the bonus instead.

### 8.2 Units per patrol (`DA_BerlinForces_*` → `Patrols`)

Every placed patrol spawns the same list. Tune it in the data asset; the placed spawners' own unit lists are a
preview only (the stage replaces them with `SetUnits`).

| Difficulty | PTRS squads (`Squad_Rus_Okhotnik`) | Mosin squads | BT-7 | T-34/76 | Units |
|---|---|---|---|---|---|
| NewToRTS | 1 | 1 | 2 | 0 | 4 |
| **Normal** (owner spec) | 2 | 0 | 2 | 1 | 5 |
| Hard | 2 | 1 | 2 | 1 | 6 |
| Brutal / Ironman | 2 | 1 | 1 | 2 | 6 |

- The owner asked for "BT-5"; there is no BT-5 in the game, so it is the **BT-7** (`Tank_BT7`).
- Every list keeps a tank, so the "tanks dead → infantry runs" rule always applies.
- `Patrols.Fallback` holds the fleeing thresholds and the rally point, the forecourt at (2600, -15994). Its values are
  the same as StayUndetected's `FallbackSettings`.

### 8.3 Placed spawners on `Ger_Berlin` (outliner folder `Mission/Patrols`)

| SpawnerId | Centre | Patrol loop (world) |
|---|---|---|
| `BerlinPatrol_Central` | (11000, 30000), ~150 m from the start | (13500, 30000), (9500, 32200), (9500, 27800) |
| `BerlinPatrol_North` | (18000, 30000) | (18000, 32500), (20208, 28890), (15800, 28500) |
| `BerlinPatrol_South` | (-3116, 33915) | (-616, 33915), (-4616, 36115), (-4616, 31715) |

- They are ordinary `AEnemyWaveSpawner` actors:
  - `PatrolWave`, 2 spawn points, `Timing.bStartOnBeginPlay` off, first wave after 2 s, `Repeat Once`;
  - `Minimap.bShowOnMinimap` and `PatrolSettings.bInfantryFleesWhenVehiclesLost` on.
- **Move them freely** in the editor (the actor, and the spawn-point and waypoint diamonds). The points are relative
  to the actor. Each patrol point was projected onto the default navmesh. Vehicle routing between the points is not
  checked: if a tank gets stuck, drag that point into a wider street.
- Placed by `Source/Tools/BerlinMap/mission/berlin_patrols.py` (`configure_forces()`, `place_spawners()`). Re-running
  `place_spawners()` moves the spawners back to the script's positions.

### 8.4 `Mission_Berlin_Patrols` (Missions[13], no trigger, BONUS)

- **Event graph:**
  - `BP_OnMissionStart` → `LoadPatrolForces` → (none set: `OnMissionFailed`) →
    `ScheduleSingleCallback(PatrolStartDelaySeconds = 3)` → `OnPatrolsDue` → `StartPatrols` →
    `ScheduleRepeatedCallback(PatrolCheckSeconds = 2)` (task id kept).
  - `OnPatrolCheck` → `RefreshPatrols` → all gone: `CancelScheduledCallback` → `OnMissionComplete`.
- **Functions:**
  - `LoadPatrolForces`: `UBerlinMissionForcesData::LoadForDifficulty(ForcesPerDifficulty, difficulty)` into
    `PatrolForces`.
  - `StartPatrols`:
    - `AddToWaveSupply`: units per patrol × number of spawners. Wave units cost enemy wave supply and refund it on
      death, so once the patrols are dead the enemy has that much more supply (on Normal 20 + 15 = 35).
    - `StartFallbackWatch(Patrols.Fallback)`. The fleeing watch now runs from mission start.
    - Per id: `FindEnemyWaveSpawner` → `SetUnits(BaseUnits = Patrols.PatrolUnits)` → `StartSpawner`.
  - `RefreshPatrols`:
    - sums `GetLivingSpawnedUnitCount` and `GetSpawnedWaveCount` over the spawners;
    - sets the plate text with the count;
    - all gone = at least one wave spawned and no living unit.
- **Instance values:**
  - `ForcesPerDifficulty` is set on the map instance, copied from the Forge station's instance by
    `wire_berlin_stages.py`. Python cannot build a map keyed by `ERTSGameDifficulty` (the struct `FRTSGameDifficulty`
    shadows its Python name), and a Blueprint map variable takes no literal default.
  - `PatrolSpawnerIds` is a class default.

### 8.5 Changes elsewhere

- **StayUndetected `StartEnemyFallback`:**
  - it starts the watch only when none runs (`GetIsWatching`), so the patrols' data asset settings are kept;
  - it puts the forecourt trigger at the subsystem's `GetRallyLocation`.
- **C++:**
  - `FEnemyWaveSpawnerPatrolSettings::bInfantryFleesWhenVehiclesLost`;
  - `AEnemyWaveSpawner`: `SetUnits`, `GetLivingSpawnedUnitCount`; it hands each spawned patrol to the fallback subsystem.
  - `UEnemyFallbackSubsystem`:
    - `WatchVehicleEscortedGroup`: once the group's last vehicle is dead, its living squads `ForceUnitToFallBack`.
      This is checked on the watch's timer, so it needs a running watch.
    - static `GetIsUnitActorAlive`: vehicles now count as dead once dying (`IsUnitAlive`), not only once destroyed.
  - `UBerlinMissionForcesData`: `Patrols` (`FBerlinPatrolForces`: `PatrolUnits`, `Fallback`), static
    `LoadForDifficulty`, `GetPatrolUnitCount`.
- **Builds:**
  - Development builds and links.
  - DebugGame compiles. Its link was blocked on 2026-11-24, first by another session's unfinished
    `UFuelDepotTestSubsystem`, then by a running DebugGame process holding the DLL, so that DLL is stale.
  - Do not open or save these stages or `DA_BerlinForces_*` in a DebugGame editor until it relinks, or the new fields
    are dropped.
- **Not play-tested:** the owner tests.

## 9. Reichstag base placed (2026-11-24)

The B2 "Kuppelwerk" layout is on `Ger_Berlin`. The buildings are the existing Soviet Blueprints, in the outliner folder
`Mission/ReichstagBase`, all tagged `BerlinReichstagBase`, owner player 2. Positions come from
`Source/Tools/ReichstagRuin/RT_Layout.json`, mapped through `BP_ReichstagRuin_B2`'s transform. The ruin is at
(8112, -15995), yaw 90, portico south.

| Actor | Blueprint | Where |
|---|---|---|
| `RT_Base_Rus_Building_Factory_1` | `Rus_Building_Factory` (tag `BerlinReichstagFactory`) | under the dome, (8231, -16268) |
| `RT_Base_Rus_Building_Barracks_1` / `_2` | `Rus_Building_Barracks` | north-west courtyard, (7209 / 9009, -18595) |
| `RT_Base_Rus_BXP_120MortarPit_1`, `RT_Base_Rus_BXP_DShKPit_1` | the pits | the other courtyard, (7112 / 9217, -13330) |
| `RT_Base_Rus_BXP_BS3Pit_1`, `RT_Base_Rus_BXP_53KPit_1` | the pits | forecourt in front of the portico, (1913, -18396 / -13599) |
| `RT_Base_Rus_Bxp_Megaphone_Propaganda1_1` | propaganda megaphone | forecourt, (3112, -17195) |

- **Ground:** the landscape under the whole base is a flat pad at z 0. Each actor stands on its pivot, as the game
  places it: mesh bottoms are 0 to -32 cm, the barracks foundation is the -32. No landscape sculpting was needed.
  - Checked by sampling the landscape every 1.5 m under each footprint.
  - Each building's lowest vertex was compared with the ground in 50 cm cells, and every ruin rubble heap was
    checked the same way: no gaps.
  - Low-angle editor shots of every building's base confirm it: sandbags and plinths rest on the cobbles.
- **Clipping:**
  - Every LOD0 vertex of the ruin's 130 piece instances was tested against each building's oriented boxes, and every
    building vertex against each piece's box, and the buildings against each other.
  - Two box hits remained, and the surface checks cleared both. No dome vertex is inside the factory; its top is at
    12.8 m, and the lowest dome geometry above it at 18.0 m. The DShK pit only overlaps a rubble heap's bounding box,
    not the heap's surface.
- **Factory exit (owner pick A):** the factory's gate faces a hall wall 2.3 m away, so its `AITrainingExit` sat 0.8 m
  outside the wall, off the navmesh for every tank size. On this instance the exit is moved to the open west end of
  the hall: (6412, -15995), 5.2 m from the factory, facing the portico. It is on all four navmeshes, with a 64 m path
  to the forecourt. Trained tanks appear there, not out of the gate.
- **Navigation** (rebuilt with the buildings): all exits reach the forecourt for infantry; both barracks exits and the
  factory exit for all tank sizes.
- **Not placed:** the CSD banner on the pediment, because no asset exists.
- **Not built yet:** what the base trains and how fast (flow 4.6), the patrol refill from it, and the Shield waves.
  "Raid the Reichstag" now has its target.
- **Tools:** `Source/Tools/BerlinMap/mission/berlin_reichstag_base.py` (`place()`, `set_factory_exit()`,
  `clip_report()`, `ground_report()`, and `flatten()` / `seat()` for uneven ground), `shots_reichstag_base.py`,
  `verify_reichstag_base.py` (read-only, from disk). Map backup:
  `Saved/BerlinReichstagBase/backup/Ger_Berlin_before_base_2026-11-24.umap`.

## 10. Reichstag base training per difficulty (2026-11-24)

Owner picks (2026-11-24):
- the unit ladder, option A;
- a data asset of its own holding the ladder and the pace;
- pace option A and cap option A;
- "Some patrol around the Reichstag, some go to the player base after the player base is found". "Found" is read as
  the alarm. This replaces the flow doc's Shield waves toward the west (flow 4.1 / 4.6).

### 10.1 Data: `DA_Berlin_ReichstagTraining`

The asset is in `/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets/` (`UBerlinReichstagTrainingData`). It has one
block per difficulty; Ironman uses Brutal.

| | NewToRTS | Normal | Hard | Brutal / Ironman |
|---|---|---|---|---|
| Squads | Mosin, PTRS (`Squad_Rus_Okhotnik`) | same | same | same |
| Tanks | none | BT-7 | BT-7, T-34/76 | BT-7, T-34/76, T-34/85 |
| Training points per minute (start 0) | 2 | 4 | 6 | 9 |
| Cap of living base-trained units | 6 | 8 | 10 | 12 |
| Patrol group / most patrols out | 2 / 1 | 2 / 1 | 3 / 2 | 3 / 2 |
| Attack group / share of groups after the alarm | 2 / 0.5 | 3 / 0.5 | 3 / 0.5 | 4 / 0.6 |

- Unit costs: Mosin 3, PTRS 4, BT-7 10, T-34/76 20, T-34/85 28. One unit per batch.
- The shared `PatrolSettings` (guarding at each point), `AttackSettings` and formation width are in the same asset.
- The routing numbers (groups, share) are starting values for the owner to tune.

### 10.2 How it runs

- **Mission_Berlin_Arrival**, first step of its event graph: `SetUpReichstagTraining` →
  `UBerlinReichstagTrainingSubsystem::StartReichstagTraining(ReichstagTraining, difficulty)`.
  - `AEnemyController::ApplyMissionTraining` writes the level into the running AI: units, points per minute, start
    points, batch, cap.
  - The listed units become trainable at the factory (tanks) and barracks (squads). The map files T-34s under tier 2,
    which only a research centre unlocks.
- **Every unit the base trains** first goes to its stand spot, as before (`OnBuildingTrainedUnitReady`).
  - Before the alarm: once `PatrolGroupSize` units wait and fewer than `MaxReichstagPatrols` patrols are out, they
    leave as a patrol on an `ABerlinReichstagPatrolRoute`. The routes are used in turn. The rest guard the base.
- **Mission_Berlin_ResponseForce** (the alarm), second step of its event graph: `StartReichstagAttacks` →
  `StartAttacksOnPlayerBase(player HQ)`.
  - From then on the groups alternate by `AttackShareAfterAlarm`: some keep the patrols full, the others attack-move
    to the HQ. Once the patrols are full, every group attacks.
  - Attack groups walk in legs of at most 250 m, each projected onto the navmesh. Ger_Berlin path queries over about
    600 m fail on the node limit.
- **Cap** (`FEnemyAIMissionSettings::MaxAliveBuildingTrainedUnits`, generic): while that many units trained at the
  AI's buildings are alive, both training and training-point income pause, so no bank builds up. A unit already
  queued still finishes, so the count can overshoot by one.
- **Claimed units** leave the AI's idle pool (`DeregisterDirectControlUnits`) before their patrol or attack order.

### 10.3 Map (`Ger_Berlin`)

- **Patrol routes** (outliner folder `Mission/ReichstagBase`), points projected onto the navmesh. Move them and drag
  their points freely.
  - `RT_PatrolRoute_Ring`: 6 points round the ruin.
  - `RT_PatrolRoute_Gate`: 4 points from the forecourt to the Brandenburg Gate.
- **The enemy controller:**
  - It got its own copy of the survival decision tree; without one the AI trains nothing.
  - It starts with training off until Arrival applies the difficulty.
  - Direct control stays off, so the tree sends no attacks of its own.

### 10.4 Checked (headless `-game` run, Normal, `smoke_reichstag_training.py`)

- **Trained units:** only the Normal units (BT-7, PTRS, Mosin), at about 4 points a minute (first BT-7 at 168 s).
  The factory's units come out at the moved exit in the hall's west end.
- **Patrol:** the first two units left as a patrol on `RT_PatrolRoute_Ring`, and the AI's stand-spot orders let go of
  them.
- **After the attack phase:** groups of three attack-moved toward the target via 2-3 waypoints.
- **Arrival's hook** ran by itself at mission start. It applied Brutal, because a bare `-game` launch loads no
  difficulty.
- **Not seen:** the cap actually pausing training. In the run the attack groups died on the way, so fewer than 8 units
  were alive at any time.
- **Not play-tested** with a real game start.

### 10.5 Also changed

- **Data folder:** all Berlin data assets now live in `/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets/`, next
  to the map. That is `DA_BerlinForces_*` (4), `DA_Berlin_ResponseForce` and `DA_Berlin_ReichstagTraining`. The old
  folder's redirectors are fixed up and deleted. Paths are updated in `ForgeStationTestSubsystem.cpp`,
  `forge_assets_build.py`, `forge_place.py`, `berlin_patrols.py`, `wire_berlin_stages.py` and the Ore session's
  `make_counterattack_settings.py`.
- **C++ files:**
  - `Missions/Berlin/Reichstag/` (`BerlinReichstagTrainingData`, `BerlinReichstagPatrolRoute`,
    `BerlinReichstagTrainingSubsystem`);
  - `EnemyAIBehaviour.h` (`FEnemyMissionTrainingOverride`, `MaxAliveBuildingTrainedUnits`);
  - `EnemyController::ApplyMissionTraining`;
  - `EnemyStrategicAIComponent` (cap, `OnBuildingTrainedUnitReady`, `GetAliveBuildingTrainedUnitCount`).
- **Builds:** Development links. The DebugGame DLL is stale, because another session's editor held it: do not save
  these assets from a DebugGame editor until it relinks.
- **Tools** in `Source/Tools/BerlinMap/mission/`:
  - `berlin_reichstag_training.py` (asset, folder move, routes, controller);
  - `build_reichstag_hooks.py`;
  - `smoke_reichstag_training.py`;
  - `probe_berlin_enemy_ai.py`.

## 11. Supply point bonus (2026-11-24)

Owner ask:
- a bonus objective for the supply point, triggered after 140 s;
- complete at once if the point is already held;
- reward: three armoured cars "of the 223 type", delivered inside the base;
- the introduction is a skippable takeover cinematic in which the announcer explains supply points.

### 11.1 The stage: `Mission_Berlin_SupplyPoint` (Missions[14], BONUS "Take the supply point")

- **Trigger:** the new generic `UTimeElapsedMissionTrigger` (`Seconds` = 140, game time since the map started;
  `EMissionTriggerType::TimeElapsed`).
- **Event graph:**
  - `BP_OnMissionStart` → `FindSupplyPoint` → `IsSupplyPointHeld`.
  - Already held: `GiveSupplyPointReward` → `OnMissionComplete`.
  - Not held: `PlaySupplyPointTour` → `ScheduleRepeatedCallback(CaptureCheckSeconds = 1)`.
  - `OnCaptureCheck` → `IsSupplyPointHeld` → held: `CancelScheduledCallback` → `GiveSupplyPointReward` →
    `OnMissionComplete`.
- **Functions:**
  - `FindSupplyPoint`: the map's first `ASupplyPoint`.
  - `IsSupplyPointHeld`: `GetOwningPlayer` = 1.
  - `PlaySupplyPointTour`: `PlayCameraTour` with `bAllowSkipping` and `StartSound` = `IntroLine`. Three shots of
    9.5 + 9 + 10 s, each with a 35 m fog reveal and a caption: "Supply point", "The supply truck", "Keep the road
    safe".
  - `GiveSupplyPointReward`: `UPlayerAirDropSubsystem::DeliverPlayerUnitsByAir` (cargo plane) at the HQ +
    `RewardLandingOffset` (18 m), plus `ShowReinforcementsOnMiniMap`.
- **Reward:** `RewardPayload` = three `Tank_Sdkfz_221`. **There is no Sd.Kfz. 223 in the game.** The 221 (20 mm) is
  the closest: the 223 is the radio version of the same four-wheel chassis. Swap the payload once a 223 exists.
- **Skip (fixed 2026-11-25):** hold Space for 1 s. The prompt was there but hidden: it is anchored at the bottom of
  the screen and was added under the tour widget (z-order 0 against 6), so the bottom cinema bar covered it. Camera
  tours now add it at z-order 8 (`MissionCameraTourSession.cpp`), which also fixes the Zoo tower tour.
- **Captured during the tour:** the check runs from the start, so the bonus completes then. Completing the mission
  cuts the tour short.

### 11.2 The Advisor's line: `Berlin_Cinematic_SupplyPoint` (28.3 s)

> Kommandant, zhis is a supply point. Move any of your units into ze area und hold it until our flag is raised. Zen
> our HQ sends a supply truck. Ze crane loads it, und vhen ze truck is back at ze HQ, ve receive Radixite, metal und
> vehicle parts. Ze trucks cannot be commanded, so keep ze road to ze HQ safe. As long as ve hold ze point, ze trucks
> keep coming. Take it, und I vill send you three armoured cars.

- **Made with:** `elevenlabs_lines.py` (announcer voice, eleven_v4, three takes), then `make_mission_lines.py`
  (settings.json + AnnouncerFilter, radio bookends).
- **Take:** take 2 (`Recordings/EL_Berlin_Cinematic_SupplyPoint_2.wav`). Takes 2 and 3 were whisper-exact; take 1
  said "the supply point". Take 3 is the spare.
- **Files:** `BlenderTanks/VoiceLines/Announcer/AAAGerMissions/Berlin/`, imported into
  `.../Anouncer/AAAGerMissions/Berlin/` (`RTS_SClass_CinematicAndTransmissions`, by `ue_import_berlin_lines.py`).
- **Subtitle:** in `DT_CinematicTranscripts` (`new_voice_lines.py`, English with the accent spellings).

### 11.3 Map

- The supply point is the 2026-11-17 recording's `BP_SupplyPoint_TowerCrane` at (-13555, 39306). It is now labelled
  `SupplyPoint_Crane`, in the outliner folder `Mission/SupplyPoint`.
- Its settings are unchanged:
  - enemy-held at the start; only the player captures;
  - 12 s to capture; supplies ready 45 s after capture;
  - 150 Radixite + 100 metal + 25 vehicle parts per truck, repeated while held.

### 11.4 Not tested

The stage and its tour have not run in a game. The time trigger, the tour framing (yaw and focus offsets are first
guesses), the cargo-plane drop of three vehicles at the HQ and the subtitle timing are for the owner's play test.

## 12. Alarm response: Soviet radio + patrols converge (2026-11-24)

Owner's picks (2026-11-24): one Soviet rifleman line per alarm cause; every patrol goes for the spot where the player was
found, fights 20 s, then returns to its route. Plus three Forge crew lines (BerlinForgeStation.md section 7).

- **C++ parent:** `Mission_Berlin_StayUndetected` is reparented to `UMission_BerlinStayUndetected`
  (`Missions/Berlin/Alarm/`). Its graph is unchanged. Natively:
  - `OnTriggerAreaCallback` records the last alarm candidate before the Blueprint sees it: zone ids 100+ =
    `Spotted` (the player unit's location), id 1 = `RelayBunker` (the infantry's location), id 2 = `RunnerArrived`
    (the player unit closest to the runner).
  - `OnMissionFailed` (the alarm) plays the cause's line on the `SovietMosin` portrait before the stage fails, so it
    queues ahead of ResponseForce's advisor line, then calls `AEnemyWaveSpawner::ConvergeAllPatrolsOnAlarm(location,
    PatrolCombatSecondsBeforeReturn = 20)`. Once only.
- **Lines** (Vlasov, Russian, "Broken transmitter", `mosin_lines.py`; `AlarmLines` on the class default):

  | Line | Cause | English subtitle |
  |---|---|---|
  | `Berlin_Alarm_Spotted` | detection zone | "Patrol to all posts! Germans in the ruins, here, in our sector! I see them. All patrols, converge on me!" |
  | `Berlin_Alarm_RunnerArrived` | runner at the forecourt | "Reichstag, open up! It's us, from the post, the Germans are in the city! Raise the alarm, everyone out!" |
  | `Berlin_Alarm_RelayBunker` | relay bunker | "Zoo tower, alarm! Germans in the relay bunker! They are at the equipment! All patrols, to the tower!" |

- **Patrols** (`UEnemyFormationController`, generic): every living, non-fleeing unit of every patrol-wave spawner
  attack-moves to the spot. A group's first combat starts its 20 s; then it breaks off and walks to its nearest patrol
  point and patrols on. No combat: back 30 s after all arrived, or after 60 s + distance / 2.5 m/s. Checked on the
  3.25 s formation tick, so "20 s" is 20-23 s.
- **Edge:** the forecourt raises the alarm after a grace wait; a zone entry in that window makes the line the zone's.
- **Changed 2026-11-25 (owner):** the patrols attack-move to the **player HQ** (`bPatrolsAttackPlayerHQ`; the
  controller's HQ, else the first German HQ nomadic, else the alarm spot), not to where the player was seen; in the
  play test they went for a unit by the start crater instead. The 20 s rule starts only **80 s after the alarm**
  (`PatrolCombatCheckDelaySeconds`, generic `CombatCheckDelaySeconds` on the converge chain): fights on the way do not
  turn a group back. After 80 s: 20 s after the next fight it returns; without a fight, 30 s after all arrived, or at
  80 + 60 s + distance / 2.5 m/s.
- **Not tested in a game run.** The Development DLL needs a rebuild before the user editor has the new class.
