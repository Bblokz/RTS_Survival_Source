# Ger_IntroMission (first campaign map)

The map's `BP_MissionManager` runs one chain of `UMissionBase` Blueprint children from
`/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/`. Each mission is the instanced `NextMission` of the
one before it (the instance's outer is the previous mission).

| # | Mission | What happens | Ends when |
|---|---|---|---|
| 1 | `Mission_Intro_MoveTo` | Camera-controls takeover, then the advisor lines. Move the Vultures to the road point. On the takeover's close it also starts the fuel convoy. | The Vultures reach trigger 1. |
| 2 | `Mission_Intro_SovietPatrols` | Cover takeover, combat music and the "Soviet infantry spotted" line. Three Mosin squads are tracked. | All three squads are dead, or the player crosses the sandbag line (trigger 2). |
| 3 | `Mission_Intro_HangarT26s` | Three T-26s leave the hangar mouths with attack-move orders. Four spaced-armour panzers arrive (Pz II F for the Strike Division, Luchs otherwise). | The T-26s are destroyed. |
| 4 | `Mission_Intro_BaseBuilding` | The base-building tutorial (below). | The barracks and forge are deployed. |

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
   - `DeliverPlayerHQByMammutGlider` brings the HQ to the hilltop concrete: build at (1450, 300), touchdown at (-4500, 300).
   - The minimap shows the reinforcements ping, and the poll timer `PollBaseStep` starts (1 s, looping, switch on `Stage`).
2. **HQ deployed** (`GetIsNomadicDeployedAsBuilding`):
   - `ACPPController::MissionOnHqSpawned` adds 1000 radixite and 400 metal. It also hooks up the glider HQ as the player
     HQ. Call it only once.
   - The `IntroMission_HQOperational` line plays.
   - The harvester count is stored as the baseline for the next step.
3. **Harvester trained** (count above the baseline): `BP_Hint_HarvestRadixite` on two crystals and `BP_Hint_HarvestMetal`
   on the barrels, with the text "Right click to harvest" and a 45 s lifespan. Ten seconds later
   `Step_UnlockGamma` allows the Gamma facility and plays `IntroMission_TrainGamma`.
4. **Gamma truck trained** (`GetPlayerNomadicsOfType`): `IntroMission_ExpandGamma`.
5. **Gamma deployed:** the refinery is allowed and `IntroMission_TrainRefinery` plays.
6. **Refinery trained:** the objective changes to "deploy it".
7. **Refinery deployed:** `IntroMission_BuildConverter` plays.
8. **Converter built** (`GetPlayerBuiltBuildingExpansionCountOfTypes(BTX_RefConverter)`):
   - `IntroMission_ConversionToggle` plays.
   - The barracks and forge are allowed on the HQ.
   - Two cargo planes drop the barracks truck (-3300, -1200) and the forge truck (4350, -250) with
     `bBuildOnLanding = false`, and `IntroMission_BarracksForgeDrop` plays.
9. **Both deployed:** `ClearPlayerTrainingRestriction`, then `OnMissionComplete`.

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

- `Source/Tools/IntroMission/intro_mission_headless_test.py` covers missions 1 to 3 and the convoy.
- `Source/Tools/IntroMission/base_building_headless_test.py` covers mission 4. It moves the base mission to the front of
  the chain in the editor world, without saving, and plays the player's part through the UI code paths, using
  `UMissionMapTestLibrary` (`Missions/Tests/`, map-test builds only):
  - it trains the harvester, Gamma and refinery by clicking the HQ's training menu items. Before Gamma is unlocked, a
    click on it must come back as locked by the mission;
  - it orders the trained trucks to deploy, 5 s after training, as a player would;
  - it builds the converter by its option click and placement click, then checks the conversion toggle on its
    command card;
  - it orders the dropped barracks and forge trucks to deploy;
  - it checks that each of the seven mission voice lines is heard playing, in order.
  - Result on 2026-11-12: 23/23 PASS.
- Run both in a throwaway editor (`UnrealEditor.exe`, or the DebugGame build when its DLL is current), never in the
  shared one.
