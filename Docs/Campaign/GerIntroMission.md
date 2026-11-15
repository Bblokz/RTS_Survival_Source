# Ger_IntroMission (first campaign map)

The map's `BP_MissionManager` runs `UMissionBase` Blueprint children from
`/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/`:

- **`Missions[0]`:** `MoveTo` -> `SovietPatrols` -> `HangarT26s` -> `RidgeArmor`. Each one is the instanced `NextMission`
  of the one before it (the instance's outer is the previous mission).
- **`Missions[1]`:** `ClearSovietRig`, with a `MissionTrigger` of type "Mission Class Completed"
  (`UMissionClassCompletedMissionTrigger`) watching `Mission_Intro_RidgeArmor`.
- **`Missions[2]`:** `BaseBuilding`, with the same trigger type watching `Mission_Intro_ClearSovietRig`.

The manager activates every `Missions[]` entry at begin play. A mission with a trigger then waits, and the manager ticks
its trigger until the watched class has completed (`AMissionManager::GetHasCompletedMissionClassExact`), so a finished
mission hands on without one more level of `NextMission` nesting under index 0 (user request, 2026-11-12).
`TriggerableMissions` + `TriggerMissionFromArray` would still nest the instance under the mission before it. Both new
mission BPs also call `TriggerMissionFromArray(0)` from `BP_OnMissionComplete` when their `TriggerableMissions` is not
empty; it is empty on this map. `Source/Tools/IntroMission/rewire_ridge_chain.py` builds this layout and can be run
again (`rewire()` then save the map; `describe()` prints the layout).

| # | Mission | What happens | Ends when |
|---|---|---|---|
| 1 | `Mission_Intro_MoveTo` | Camera-controls takeover, then the advisor lines. Move the Vultures to the road point. On the takeover's close it also starts the fuel convoy. | The Vultures reach trigger 1. |
| 2 | `Mission_Intro_SovietPatrols` | Cover takeover. When it closes: combat music, the advisor's "Soviet infantry spotted" line, then (queued after it) the Mosin rifleman's Russian radio line `IntroMission_MosinGermansSpotted` on the `SovietMosin` portrait, with English subtitles (see `VoiceLines/AGENTS.md`). Three Mosin squads are tracked. | All three squads are dead, or the player crosses the sandbag line (trigger 2). |
| 3 | `Mission_Intro_HangarT26s` | Three T-26s leave the hangar mouths with attack-move orders. Four spaced-armour panzers arrive (Pz II F for the Strike Division, Luchs otherwise). As each spawns, `SendPanzerBehindVultures` attack-moves it to a line `PanzerBehindVulturesDistance` (1500) behind the Vulture soldiers' centre, away from the hangars, 500 apart, facing the hangars. The `IntroMission_LightPanzerSupport` line ("Kommandant soviet light panzers have been spotted!...", ElevenLabs take 3 of 3, 2026-11-12) plays. 5 s after the first panzer arrives (`OnPanzerSupportSpawned` -> `ShowSpacedArmourThenT26`: Delay 5 with the mission manager as world context) the spaced armour takeover opens; its On Closed calls `ShowT26InfoTakeover`, which opens the vehicle info takeover (library row `T26`) on the first living hangar T-26 (`TryShowT26InfoTakeover`). When no T-26 has spawned yet it sets `bT26TakeoverPending`, and the last hangar spawn opens it (`ShowT26TakeoverIfPending`). | The T-26s are destroyed. |
| 4 | `Mission_Intro_RidgeArmor` | Soviet armour comes out of the ridge gate, eight T-26s flank from the convoy's map edge, and German heavy reinforcements arrive from the west map edge (below). The `IntroMission_HeavyArmorApproaching` line plays (take 4, 16.7 s). | Both Soviet groups are destroyed: the ridge wave (5 on Normal, 6 on Hard, 7 on Brutal and Ironman) and the 8 T-26s. Each group shows as a survival-style division marker on the minimap until its last tank dies. |
| 5 | `Mission_Intro_ClearSovietRig` | Placeholder for the user's ridge cinematic: it completes at once. A comment box in its graph marks where the cinematic goes (`PlayClearSovietRigCinematic`, finishing in `OnClearSovietRigCinematicFinished`). | At once, for now. |
| 6 | `Mission_Intro_BaseBuilding` | The base-building tutorial (below). | The barracks and forge are deployed. |

## Mission_Intro_RidgeArmor

At `BP_OnMissionStart`:

1. **Targets** (both once):
   - `ComputePlayersCentre` averages every `Ger_Inf_Hazmat_Vulture`, `Ger_Vehicle_PanzerII_F_SpacedArmor` and
     `Ger_Vehicle_PanzerII_Luchs_SpacedArmor` actor (`PlayerUnitClasses`) into `PlayersCentre`. With none left it uses
     the reinforcement point.
   - `ComputePanzerCentre` averages the two Panzer II classes (`PanzerUnitClasses`) into `PanzerCentre`. With none
     left it uses `PlayersCentre`.
2. **Voice line:** `PlayHeavyArmorApproachingLine` plays `IntroMission_HeavyArmorApproaching` on the `GermanAnnouncer`
   portrait (the same `PlayPortrait` call as the T-26 mission's line).
3. **Soviet wave** (`SpawnSovietAttackers`), spawned with "Spawn Vehicle Variant", yaw 180, on the 599 cm plateau in front
   of (west of, facing the player) `BP_Dest_PostApo_ConcreteFenceGate` at (-646, -437) and its twin at (-646, -95):

   | Slot | Unit | Location (z 660) | When |
   |---|---|---|---|
   | 0, 1 | `Rus_Vehicle_T3476` | (-1650, -366), (-1650, 434) | Always |
   | 2, 3, 4 | `Rus_Vehicle_BT-7` | (-2400, -766), (-2400, 34), (-2400, 834) | Always |
   | 5 | `Rus_Vehicle_T3476` | (-1650, 1234) | `GetIsGameDifficultyHardOrHigher` |
   | 6 | `Rus_Vehicle_T3485` | (-3150, 1234) | `GetIsGameDifficultyBrutalOrHigher` |

   - Rows are 750 cm apart in X; tanks side by side are 800 cm apart in Y. At 600 cm the spawn collision handling
     pushed every tank after the first of a row 3.5-6.6 m sideways.
   - On 2026-11-12 every slot was on the LightTank and MediumTank navmeshes (all but slot 5 also on HeavyTank), with no
     actor in an 8 x 6 m box.
   - The slots stay clear of the diagonal sandbag wall south-west of the gate, the auto-cannon bunker and 53-K pit at
     x -3600 to -4000, and the plateau's north edge at y ~1500.
   - As each tank spawns (`OnSovietSpawned`), `SendSovietToPlayers` attack-moves it to `PlayersCentre` plus its offset
     from the wave's centre, facing away from the gate.
4. **Flanking T-26s** (`SpawnFlankingT26s`, called at the end of `SpawnSovietAttackers`): eight `Rus_Vehicle_T26` at the
   fuel convoy's start, the map-edge end of the two-track (Y -24760). They spawn 2 wide (x -6180 and -5380, either side
   of the track at x -5780) and 4 deep (y -24300, -23500, -22700, -21900), 800 cm apart, z 210, facing `PanzerCentre`.
   On 2026-11-12 all eight slots were on all three tank navmeshes with no actor in a 9 x 6 m box.
   - As each spawns, `SendT26ToPanzers` attack-moves it to `PanzerCentre` plus half its offset from the column's
     centre (±200 cm across, ±600 cm along).
   - They come from the south (-Y), which is the player's left while the player faces the ridge (+X).
5. **Two division markers instead of objective text** (user request, 2026-11-12): each group gets a survival-style
   minimap marker through `TrackUnitGroupOnMiniMap`, a chevron along its heading plus the upright badge at the centroid
   of its living units.
   - The ridge wave goes into `SpawnedSoviets` (group `RidgeMainForce`, first heading at `PlayersCentre`). The T-26s go
     into `SpawnedFlankingT26s` (group `FlankingT26s`, first heading at `PanzerCentre`).
   - Each group is tracked once its last spawn has reported (`PendingSovietSpawns` / `PendingFlankSpawns`; failed
     spawns count down too).
   - A marker goes with its group's last unit. `BP_OnUnitGroupDefeated` counts `DefeatedUnitGroups`, and at 2 the
     mission completes, so no T-26 is left roaming during the ClearSovietRig cinematic.
   - No "Soviet tanks destroyed" text: the mission title stays "Ridge Armor" and no text is drawn over the tanks.
6. **Reinforcements** (`SpawnGermanReinforcements`) in an 800 cm square around (-30984, 3366, 173), at the west map edge
   near the player's start, so they drive about 65 m to the player (user request, 2026-11-12). They face the gate and
   spawn 130 cm up (part of the square is on the 175 cm airport road). The slots form a hollow square, in order:
   front row (+800, -800 / 0 / +800), middle sides (0, -800) and (0, +800), rear corners (-800, -800) and (-800, +800).
   With a heavy tank in the centre slot, the spawn collision handling pushed the Panther D 11 m out of the square in
   two runs (at the first spot, the road crossing). At the west edge every slot is on all three tank navmeshes. The
   front-right slot touches a crushable lamp post (`BP_Civilian_Dest_CrushablePoleLight7`), which did not push spawns
   at the first spot. The six German tanks the user had placed there (Pz III J x3, Pz III AA Flak 41 x2, Wespe) are no
   longer in the map (gone from the 17:46 save).

   | Player faction (`GetPlayerFaction()`) | Front row x3 | Middle sides x2 | Rear left | Rear right |
   |---|---|---|---|---|
   | Strike Division (2) | `Ger_Vehicle_PanzerIII_J` | `Ger_Vehicle_PanzerIII_AA_Flak41` | `Ger_Vehicle_Wespe` | `Ger_Vehicle_PantherD` |
   | Any other | `Ger_Vehicle_PanzerIV_F1` | `Ger_Vehicle_Sdkfz7Flak_Flak36` | `Ger_Vehicle_Brummbar` | `Ger_Vehicle_TigerH1` |

   - The user asked for "Ger_Vehicle_PanzerIII_Js" and "Ger_Vehicle_PanzerIII_AA_Flak42". No such Blueprints exist, so
     they are the Pz III J and the Flak 41 Pz III AA, the only Pz III AA with a FlaK 4x gun.
   - As each one spawns, `SendReinforcementToPlayers` attack-moves it to `PlayersCentre` plus its square offset, facing
     the gate.
   - The first spawn shows the reinforcements ping on the minimap for 10 s.

The spawn slots, unit classes and target classes are class defaults of the Blueprint (categories Soviets,
FlankingT26s, Reinforcements, Players).

## Fuel convoy

`Mission_Intro_MoveTo.StartFuelConvoy` runs `AConvoy::StartConvoy`:

- **Definition:** `DA_Convoy_IntroFuel`, one `Rus_ArmoredCar_ZilFuel` with one Mosin squad at the rear.
  `TruckSpeedFraction` is 0.22, and the convoy has no minimap mark.
- **Route:** `UConvoyRouteLibrary::BuildNavigableRoute` over `IntroConvoyRoutePoints`, which is
  `BP_GroundPathSpline_TwoTrack` sampled every 15 m. It runs from the map edge at Y -24760, across the main road at the
  move-to point (30.1 m along the spline), to the hilltop exit, where the convoy despawns.
- **Timing:** at 0.22 the truck crosses the road about 128 s after the camera takeover closes, which is roughly when a
  player finishes the patrols. Change `TruckSpeedFraction` to move it.

## Mission_Intro_BaseBuilding

1. **Start:**
   - `RestrictPlayerTrainingTo` limits training to the harvester and the scavengers.
   - `DeliverPlayerHQByMammutGlider` brings the HQ toward the hilltop concrete, with `bBuildOnLanding = false`: build
     spot (1450, 300), touchdown at (-4500, 300). The truck drives out of the glider's nose and waits there.
   - The `IntroMission_HQGliderInbound` line plays ("Sehr gut! Now zhat the ridgeline ist sequred...").
   - The minimap shows the reinforcements ping, and the poll timer `PollBaseStep` starts (1 s, looping, switch on `Stage`).
2. **HQ released by the glider** (`BP_OnAirDeliveredNomadicLanded`, ID 0): `UMissionBase` registers it as the player
   HQ at once (`ACPPController::RegisterPlayerHQ`). Converting the HQ truck skips the build radius only for the
   registered player HQ, so without this the player could not place it.
3. **HQ out of the glider** (`BP_OnAirDeliveredHQOutOfGlider`: the truck is idle after its drive out):
   - The nomadic conversion takeover opens (game paused).
   - From its On Closed: the `IntroMission_ConvertHQ` line plays ("Kommandant, select ze nomadic HQ and convert it into a
     static building."), `BP_Hint_ExpandHQ` is placed on the build spot with its text set to "Deploy HQ", and the
     objective asks the player to deploy the HQ.
4. **HQ deployed** (`GetIsNomadicDeployedAsBuilding`), converted by the player:
   - The "Deploy HQ" marker is removed.
   - `ACPPController::MissionOnHqSpawned` adds 1000 radixite and 400 metal. Call it only once.
   - The `IntroMission_HQOperational` line plays.
   - The harvester count is stored as the baseline for the next step.
5. **Harvester trained** (count above the baseline): `BP_Hint_HarvestRadixite` on two crystals and `BP_Hint_HarvestMetal`
   on the barrels, with the text "Right click to harvest" and a 45 s lifespan. Ten seconds later
   `Step_UnlockGamma` allows the Gamma facility and plays `IntroMission_TrainGamma`.
6. **Gamma truck trained** (`GetPlayerNomadicsOfType`): `IntroMission_ExpandGamma`.
7. **Gamma deployed:** the refinery is allowed and `IntroMission_TrainRefinery` plays.
8. **Refinery trained:** the objective changes to "deploy it".
9. **Refinery deployed:** the building expansion pack-up takeover opens (`ShowBxpPackUpThenConverter`); from its On Closed `PlayConverterInstructions` plays `IntroMission_BuildConverter` and sets the objective (2026-11-12 text: "Kommandant, now zhat ze refinery is expanded you will see a new building panel appear on ze bottom of ze screen. In zhere ze building expansion slots are listed. Click one of ze slots and select the radixite converter from ze economic expansion panel."; ElevenLabs eleven_v3 take, 17 s).
10. **Converter built** (`GetPlayerBuiltBuildingExpansionCountOfTypes(BTX_RefConverter)`):
   - `IntroMission_ConversionToggle` plays.
   - The barracks and forge are allowed on the HQ.
   - Two cargo planes drop the barracks truck (-3300, -1200) and the forge truck (4350, -250) with
     `bBuildOnLanding = false`, and `IntroMission_BarracksForgeDrop` plays.
11. **Both deployed:** `ClearPlayerTrainingRestriction`, then `OnMissionComplete`.

The objective text changes at every step through `SetObjective`, a wrapper around `SetMissionObjectiveText`.

## Mission API added for it (UMissionBase)

| Function | Notes |
|---|---|
| `RestrictPlayerTrainingTo(Options)`, `AllowPlayerTrainingOptions(Options)`, `ClearPlayerTrainingRestriction()` | `UMissionTrainingRestrictionSubsystem` (`Missions/TrainingRestriction/`). While a restriction is on, the training menu shows every option outside the allow-list as `TS_LockedByMission`, and clicks on it do nothing. Field defences count as options. The requirement tooltip reads "Locked: follow the mission objective". The log category is `LogMissionTrainingRestriction`. The restriction ends with the world. |
| `GetPlayerNomadicsOfType(Subtype)` | Player trucks and buildings of the subtype, trained ones included. |
| `GetIsNomadicDeployedAsBuilding(Nomadic)` | True when the status is `ENomadStatus::Building`, so a truck that is still unfolding returns false. |
| `GetPlayerBuiltBuildingExpansionCountOfTypes(Types)` | Counts only `BXS_Built`. `DoesPlayerHaveAnyBuildingExpansionsOfType` also counts placement and construction. |
| `SetMissionObjectiveText(Title, Description)` | Sets the widget state and refreshes the mission widget. In synthesized graphs, wire the text pins (they are const refs). |
| `DeliverPlayerNomadicByCargoPlane(..., bBuildOnLanding)` | `false` leaves the truck where it was set down, so the player deploys it. |
| `DeliverPlayerHQByMammutGlider(..., bBuildOnLanding)` | Default `true` (Ger_Trains). `false` leaves the HQ truck where it drove out of the glider; `BP_OnAirDeliveredHQOutOfGlider` fires once it is idle (at most 30 s after the release). Either way the HQ is registered as the player HQ (`ACPPController::RegisterPlayerHQ`) the moment the glider releases it. |
| `TrackUnitGroupOnMiniMap(GroupId, Units, HeadingTarget)`, `BP_OnUnitGroupDefeated(GroupId)`, `GetTrackedUnitGroupCount()`, `GetUnitGroupMarkerLocation(GroupId)` | `Missions/MissionClasses/MissionBase/MissionUnitGroupMarkers.cpp`. Shows a unit group on the minimap like a survival wave: a chevron along the group's heading plus the upright badge, at the centroid of its living units, kept inside the minimap edge, refreshed every 0.25 s. The first heading points at `HeadingTarget`. When the last unit is gone the icons go and `BP_OnUnitGroupDefeated` fires, so a mission can complete on it without objective text. Tracking the same ID again replaces the group; markers are cleared when the mission completes, fails or is cleaned up. The icon code is shared with `USurvivalWaveMarkerTracker` in `GameUI/MiniMap/CustomIcons/UnitGroupMiniMapMarker.h` (namespace `RTSUnitGroupMiniMapMarker`). |
| `UMissionMapTestLibrary::MapTest_ConvertNomadicAt(Truck, Location)` | Test only: selects the truck, presses convert (`ConstructBuilding`) and clicks the preview at Location. Returns whether the truck took the order and whether the preview demanded a build radius (false for the registered player HQ). |

## Fixes found while building it (2026-11-12)

- **The HQ truck crawled and never reached its build spot.** `Ger_HQ_Truck`'s class defaults had saved a
  `DeployAnimationMesh` static mesh component, created when `InitNomadicVehicle` ran on the CDO. Every spawned HQ
  truck took it as its root, so the simulating `Chasis` could no longer move the actor. The truck crept forward in
  50 cm nudges until the build order gave up ("failed to move to its building construction location twice").
  - The object was moved out of the CDO and the Blueprint resaved.
  - `ANomadicVehicle::InitNomadicVehicle` now creates the deploy mesh `RF_Transient` and never on a CDO or archetype.
- **An off-screen nomadic never finished deploying.** Far outside the camera's view, `URTSOptimizer` turns off
  skeletal mesh ticks (`OnlyTickPoseWhenRendered`), so the truck montage of a deploy or pack-up froze until the
  player looked at it. `ANomadicVehicle::SetConversionKeepsAnimating` now pauses the unit's optimiser from
  `StartBuildingConstruction` or the pack-up start until it is a building or truck again (every cancel path included),
  through the new `ATankMaster::SetUnitOptimizationEnabled`.
  - Edge case: a conversion that ends while `UGameUnitManager::SetAllUnitOptimizationEnabled(false)` is active
    re-enables that one unit's optimiser.

## Tests

- `Source/Tools/IntroMission/intro_mission_headless_test.py` covers missions 1 to 5, the start of 6 and the convoy.
  - `chain_saved` checks the saved layout: the `Missions[0]` chain and the two triggered entries.
  - Env `INTRO_TEST_DIFFICULTY` (`normal`, `hard`, `brutal`, `ironman`) sets the PIE startup difficulty on the
    `URTSGameInstancePIEDeveloperSettings` defaults before PIE. Unset, the ini value (Hard) applies.
  - Ridge checks:
    - the Soviet wave per difficulty (class counts) and the two tracked groups (`SpawnedSoviets` = the wave,
      `SpawnedFlankingT26s` = 8);
    - the division markers (`ridge_markers_*`): two markers, each within 8 m of its group's centroid, while both groups
      live; after the test kills the ridge wave, only the T-26 marker is left; after it kills the T-26s, none (checked
      only while the completed mission object still exists);
    - every Soviet tank first seen within 4 m of a slot west of the gate, and no two spawns closer than 5 m;
    - the 8 flanking T-26s first seen within 4 m of their slots at the convoy start, no two closer than 5 m, all south
      (-Y, the player's left) of the player centre by 150 m or more;
    - the reinforcement kit per `INTRO_TEST_FACTION` (`none` accepts either whole kit), in the 800 cm square at the west
      edge, with no overlaps;
    - `PlayersCentre` and `PanzerCentre` equal the test's own averages;
    - the Soviets and the reinforcements drive towards `PlayersCentre`, the T-26s towards `PanzerCentre`: the best
      progress of each tank counts, so a tank killed in the fight keeps what it drove. A reinforcement that has not
      moved passes this step only if an enemy came within 60 m of it (attack-move stops to fight), and the test logs
      that enemy;
    - at the end of the run, every surviving reinforcement stands within 15 m of `PlayersCentre` plus its square
      offset, or has an enemy within 60 m (in combat) (`ridge_reinforcements_arrived`);
    - the heavy-armor line plays after the ridge mission starts;
    - no Mammut glider until the last Soviet tank is gone.
  - The test then kills the ridge wave only and checks the mission keeps running and no glider comes while the T-26s
    live (`ridge_waits_for_flanking_t26s`). It then kills the T-26s: the ridge mission and ClearSovietRig complete, and
    BaseBuilding's glider appears.
  - Mission completion is recorded the tick it happens (`note_completions`). In PIE the manager empties `Missions`
    after activating them, so a completed mission nothing references is garbage collected within seconds, and a later
    `GetIsMissionComplete` finds no object.
  - Tanks of the reinforcement classes that are already on the map when PIE starts are ignored.
  - Results on 2026-11-12 (Development `UnrealEditor.exe`, kill path), first layout (reinforcements at the road
    crossing, no T-26s):
    - Strike, Normal: 5 Soviet tanks, 44/45 (the one FAIL was the unchanged T-26 mission's
      `panzers_drove_behind_vultures`, under load).
    - Breakthrough, Brutal: 7 Soviet tanks, 45/45. `none`, ini Hard: 6 Soviet tanks, Strike kit, 45/45.
    - A later Strike/Normal run under heavy load: 2 of 7 reinforcements had not arrived after 281 s. Pz III J #0 was
      still fighting the placed `BP_Solo_Rus_Mosin` / `BP_Solo_Rus_PTRS` soldiers at the sandbag line (x ~ -12920),
      20 m from that square: attack-move doing its job, not a spawn bug. The convoy and
      `panzers_drove_behind_vultures` also failed in that run, from the load.
  - Results with the west-edge reinforcements and the 8 flanking T-26s:
    - Strike / Normal and Breakthrough / Hard, final build (Development, division markers): 51/55 each. Every ridge,
      T-26, reinforcement, arrival and marker check passed. Markers sat 14-41 cm from their group's centroid.
    - The 4 FAILs in both runs come from machine load: about 7-10 fps against about 19 fps in the earlier runs.
      - The convoy checks fail because the truck ran at about 40% speed from t = 25 s, before the ridge mission exists.
      - `panzers_drove_behind_vultures` (the unchanged T-26 mission) also fails.
    - An earlier Breakthrough/Hard run is void: someone stopped and restarted its PIE by hand in the test editor's
      window. Throwaway test editors always run with `-DisablePlugins=Fab,BpGeneratorUltimate` (UECP off).
- `Source/Tools/IntroMission/base_building_headless_test.py` covers mission 6. It finds the base mission in any
  `Missions[]` entry, drops its trigger and makes it the only mission in the editor world, without saving. It plays the player's part through the UI code paths, using
  `UMissionMapTestLibrary` (`Missions/Tests/`, map-test builds only):
  - it closes the nomadic conversion takeover, checks the convert line comes after it and the "Deploy HQ" marker is
    on the build spot, then converts the HQ with `MapTest_ConvertNomadicAt`; the preview must not ask for a build
    radius (proof that the glider HQ is the registered player HQ);
  - it trains the harvester, Gamma and refinery by clicking the HQ's training menu items. Before Gamma is unlocked, a
    click on it must come back as locked by the mission;
  - it orders the trained trucks to deploy, 5 s after training, as a player would;
  - it builds the converter by its option click and placement click, then checks the conversion toggle on its
    command card;
  - it orders the dropped barracks and forge trucks to deploy;
  - it checks that each of the nine mission voice lines is heard playing, in order.
  - Result on 2026-11-12: 23/23 PASS; after the glider HQ flow (HQ converted by the player), 32/32 PASS; with the pack-up takeover, 34/34 PASS; on the ridge chain layout (BaseBuilding as triggered `Missions[2]`), 34/34 PASS.
- Run both in a throwaway editor (`UnrealEditor.exe`, or the DebugGame build when its DLL is current), never in the
  shared one.


## Ridge mission panel (2026-11-14)

`Mission_Intro_RidgeArmor` sets its objective at start (`SetRidgeObjective`: "Destroy the Soviet Armour" + description). Once both Soviet groups have spawned, `StartSovietKillCounter` runs `TrackingFunction` (no per-tank minimap text) over the ridge wave + flanking T-26s: "Soviet tanks destroyed 0/13" (14 Hard, 15 Brutal+), which completes the mission. The two division markers stay; `BP_OnUnitGroupDefeated` only completes when the counter has not already (it checks `GetIsMissionComplete`). The map's mission instance keeps its own placeholder `MissionWidgetState`, so the texts are set at runtime. Intro test 2026-11-14 (Strike/Normal): 53/58, every ridge, panel and takeover check passes; the 5 failures are the load-sensitive convoy and panzer-rally checks.
