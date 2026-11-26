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

## Presentation (2026-11-22)

**Dig-in intro (`DigInIntro`, `FSovietCounterattackDigInIntroSettings`).** At the mission start, before the prepare
countdown:

1. Four dig-in markers spawn on the two ridges (`MarkerLocations`; south ridge (6105,-3617) (7085,-3679), north ridge
   (7149,1386) (6351,1143), ground traced against world static). Each is a `BP_Hint_DigIn` (ghost sandbag wall,
   arrow, "Dig in a vehicle here") turned to face `MarkerFacingTarget` (the centre spawn), plus an `Objective` minimap
   icon. The hint Blueprint gives itself an 8 s life span on BeginPlay; the mission clears it, so the markers stay
   until the waves start. The fog of war is lifted `RevealRadius` around each marker for the tour, the whole prepare
   countdown and `RevealSecondsAfterTour` more, so the ridges are never black.
2. The Dig In field manual opens (`UInfoTakeoverSubsystem::QueueDigInTakeover`, the same clip takeover as the
   "Show Dig In Takeover" node). The game is paused while it is open.
3. When it closes, `PlayCameraTour` plays `TourShots` (south ridge, north ridge, "Hold the Gap" between them; about
   16 s). Optional `VoiceLine` on `VoiceLinePortrait` starts `VoiceLineDelaySeconds` after the tour's first shot is
   on screen (a takeover stops a portrait already playing). On `DA_Intro_SovietCounterattack` it is
   `IntroMission_DigInPositions` (13.7 s, take 2 of five drafts, 2026-11-22): "Radio intercepts, Kommandant! Ze
   Soviets are coming back for zhis ridge, by rail and by air. Use ze time you have. Dig in your vehicles at ze marked
   positions and let zem walk into ze gap." ElevenLabs take (sent as "zis", which it reads correctly) -> 
   `make_mission_lines.py` (AnnouncerFilter + radio bookends) -> BlenderTanks `Announcer/AAAGerMissions/IntroMission`
   -> `Source/Tools/IntroMission/counterattack/import_digin_line.py` (SoundWave + DA) -> `new_voice_lines.py`
   (subtitle row in `DT_CinematicTranscripts`).
4. When the tour ends (`OnCameraTourFinishedNative`, also when it never showed) the "Prepare to defend" timer starts.
   The full prepare time is left for the player.

`SkipPreparation` ends the intro at once (a tour already on screen plays out; the arrival cinematic waits for it).

**Dig-in positions (`DigInIntro`, prepare phase).** Every 0.5 s the mission looks for player vehicles that stand dug
in (`UDigInComponent::GetIsDugIn`, the wall finished) within `PositionRadius` (8 m) of a marker; each one holds the
closest position in range.
- A held marker hides its ghost wall and arrow, its text reads `MarkerHeldText` ("Position held") and its minimap icon
  goes; all three come back when the vehicle breaks cover or dies.
- The prepare objective gets a line under its description, `PositionsObjectiveLine`: "Vehicles dug in at the marked
  positions: {Held}/{Total}", refreshed when the count changes. Once every position is held the line goes (user,
  2026-11-23); it comes back if a position is lost again before the waves.
- `FirstPositionHeldLine` (announcer) plays the first time a position is held.
- When the waves start with every position held, `AllPositionsHeldReward` is paid (default +300 radixite, +150 metal)
  and `AllPositionsHeldLine` plays. Then the markers go.
- Lines on `DA_Intro_SovietCounterattack` (2026-11-22, user picks A2/B2/C3 of three versions each, ElevenLabs take 1
  of 3, all whisper-checked, AnnouncerFilter + radio bookends, `import_announcer_lines.py`):
  `IntroMission_DigInFirstHeld` (8.0 s) "Zhat is how it is done, Kommandant. One position held. Fill ze others before
  ze Soviets arrive." and `IntroMission_DigInAllHeld` (6.1 s) "Both ridges are dug in. HQ approves, Kommandant.
  Supplies are on zheir way." The air-drop cue `IntroMission_ArrivalCue` (5.1 s) "Here zey come. By rail and from ze
  sky!" is on `LS_Intro_FirstWaveArrival`'s audio track at 2 s.

**First wave arrival cinematic (`ArrivalCinematic`, `FSovietCounterattackArrivalCinematicSettings`).** Wave 1 only:

- The mission picks the first landing-field pad whose unit is a vehicle (pad 0 when there is none) and holds back the
  first train.
- Every 0.1 s it measures how fast the closest enemy aircraft closes in on that pad. At `TrainPreRollSeconds` +
  `TrainShotSeconds` + `DropLeadSeconds` from the pad the train is released, and at `TrainShotSeconds` +
  `DropLeadSeconds` the cinematic starts.
- It is a normal mission cinematic takeover (`OnCinematicTakeOverWithSkipping` with `LS_Intro_FirstWaveArrival`, an
  empty level sequence whose clock is set to the planned length, `bPauseAtEnd` so a natural end reports
  `FinishedNaturally` and not `StoppedExternally`). The HUD hides (the mission also collapses its own timer widget,
  which the takeover does not hide), "Hold Space to skip" blinks bottom centre, the overlay counts down bottom right,
  and the game keeps running.
- The view is a runtime `ACameraActor` the mission moves:
  - `TrainShotSeconds` (2 s) from a fixed spot ahead of and beside the front flatcar, looking at it as the train rolls
    in;
  - then a cut to `DropCameraDistance`/`DropCameraHeight` (3000) from the pad along `DropCameraYaw`, looking
    `DropLookAtAircraftWeight` of the way from the pad to the closest enemy aircraft (re-picked every frame, so the
    aircraft that ends up over the pad is the one followed), never higher than `DropLookAtMaxHeight` above the pad
    (keeps the black sky above the map out of the picture); once the pad's unit is down it turns to that unit.
- The game runs at `TimeDilation` (0.75) while the cinematic plays, so the player loses less game time to it; the
  previous speed comes back when it ends or is skipped.
- An announcer cue can sit on the sequence's own audio track (`set_arrival_cue.py`, at 2 s = the cut to the landing
  field); the overlay subtitles it and the takeover does not mute it.
- The shot ends `SecondsAfterDelivery` after that pad's unit is on the ground, at the latest after
  `DropShotMaxSeconds`. Fog of war is lifted around the train and the pad for the cinematic.
- When it ends, or is skipped, the view goes back to the player's camera.
- If no aircraft comes near within `MaxWaitSeconds`, or anything is missing, there is no cinematic and the train goes.

- Skipping: hold Space. The prompt (`UW_HoldToSkip`) gets Space from a Slate input preprocessor (2026-11-23), so the
  skip does not depend on keyboard focus (before, it only heard Space while it had focus, so a mouse click or another
  widget taking focus could silence it; not observed in a test, the owner asked to make sure it is skippable). Alt-tab
  while holding cancels the hold.

**Base reinforcements (`BaseReinforcements`, `FSovietCounterattackBaseReinforcementSettings`, user 2026-11-23).** Once
the arrival cinematic is over (finished, skipped, timed out, or never played), `Arrival_OnOver` fires
`BP_OnArrivalCinematicOver(bWasShown, bWasSkipped)` and, `DelayAfterCinematicSeconds` (1.5 s) later,
`DropBaseReinforcements` (also BlueprintCallable; runs once):
- the announcer line `VoiceLine` = `IntroMission_BaseHeavyReinforcements` (8.4 s, ElevenLabs take 1 of 3,
  whisper-checked, AnnouncerFilter + radio bookends): "Kommandant, HQ provides us with more heavy tanks to help defend
  our base; they will be dropped inside your base." Set on the DA by `set_base_reinforcements_line.py`.
- `UPlayerAirDropSubsystem::DeliverPlayerUnitsByAir` at the player HQ + `LandingOffsetFromHQ` (-1200,0: the open
  concrete west of the HQ, away from the attack; `FallbackLandingPoint` (250,300) without an HQ), ground traced and
  projected onto the navmesh, with `CargoPlaneAbility` (`GA_GerPzIIIFlak41Drop`: Ju 390B, one aircraft per vehicle, 1000 apart).
  Every German tank drop template uses the same aircraft and hang offset (-250), so heavy and light tanks share it.
  Vehicles spawn from the training option = the regular (non-variant) unit.
  Building footprints (2026-11-26): every aircraft, the first one included, sets its vehicle down at least
  `BuildingClearanceRadius` (800) from any building, building expansion or placed building preview. This goes through
  `FPlayerAirDropPayload::BuildingClearanceRadius`, then `UGA_AircraftDrop::SetBuildingClearanceRadius`, then
  `FBuildingFreeDropSpots`, which overlaps the building placement channel and also checks nomadics that are still
  unfolding. A built-over offset point moves to the nearest clear navmesh spot (rings around it), and the minimap ping uses
  that same spot. The player's own aircraft drop abilities and enemy air drops leave the clearance at 0 and are unchanged.
  - Breakthrough Doctrine (any non-Strike faction): 2 x `Tank_TigerH1`, 3 x `Tank_PzII_Luchs`.
  - Strike Division: 2 x `Tank_PantherD`, 2 x `Tank_PzIII_AA`.
- a reinforcements ping on the minimap at the landing point for `MiniMapPingSeconds` (8 s).

Test: `Source/Tools/IntroMission/counterattack/counterattack_presentation_test.py`, env `PRESENT_MODE`:
- `cut` (default): the chain cut to this mission, vehicles only, four Pz IV F1 dig in on the positions (held markers,
  counter, reward), natural end of the cinematic.
- `chain`: BaseBuilding then this mission with its real trigger; the test completes BaseBuilding; mixed tank/squad
  pool, so the filmed pad need not be pad 0.
- `skipprep`: `SkipPreparation` two seconds into the tour; the cinematic must wait for the tour, then is skipped by
  holding the prompt.
Results and screenshots in `Saved/CounterattackPresentationTest/` (`result_<mode>.json`, one folder per mode).
`counterattack_headless_test.py` turns both features off.

Packaging: the marker class (`BP_Hint_DigIn`), `LS_Intro_FirstWaveArrival` and the announcer lines are set on
`DA_Intro_SovietCounterattack` itself (`set_counterattack_da_refs.py`), not as C++ defaults. A value equal to the class
default is never written into the asset, so a cook would not have seen them.

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
| Centre | Async-spawned on a grid (`SpawnSpacing` 8 m, rows trailing away from the route) around `CenterSpawnLocation` (15500,23900, z 0 = traced to the ground): the north map edge between the rail line and where the aircraft fly in (user, 2026-11-23; was on route point 1), facing the first route point; the group drives in from there. |
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
`UnitsAddedPerWave`, `TankTypes`, `SquadTypes`. Values (C++ defaults and the data asset; 2026-11-23 every attack
30 s sooner, user request: prepare and wave interval each 30 s shorter):

| Difficulty | Prepare | Waves x interval | First wave | +/wave | Tanks | Squads |
|---|---|---|---|---|---|---|
| New to RTS | 150 | 3 x 70 | 6-8 | 1 | BT-7, T-26, T-70 | Mosin |
| Normal | 150 | 4 x 60 | 9-12 | 1 | T-26, BT-7-4, T-70, T-34/76 | Mosin, Okhotnik |
| Hard | 120 | 5 x 60 | 10-13 | 2 | T-70, T-34/76, T-34/76 L, T-34E, SU-76 | Mosin, Okhotnik, Large PTRS |
| Brutal, Ironman | 90 | 6 x 50 | 12-15 | 2 | T-34/76 L, T-34E, T-34/85, SU-85, KV-1 | Red Hammer, Large PTRS, Okhotnik |

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
