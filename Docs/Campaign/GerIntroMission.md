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
| 1 | `Mission_Intro_MoveTo` | Camera-controls takeover, then the advisor lines, queued on the `GermanAnnouncer` portrait: `HelloIsThisOn`, `IntroMission_AdvisorWelcome`, then the three briefing lines from `IntroMission/Briefing` (2026-11-15, user picks): `IntroMission_SovietStrongholds` ("Thirty years ze Soviets have hidden...", take 3, eleven_v3), `IntroMission_ClearStronghold` ("Our goal is to clear out a small soviet stronghold...", take 2, eleven_v4) and `IntroMission_TreadCarefully` ("Tread carefully kommandant...", take 3, eleven_v3), about 45 s together. Later mission lines queue behind them. Move the Vultures to the road point. On the takeover's close it also starts the fuel convoy. | The Vultures reach trigger 1. |
| 2 | `Mission_Intro_SovietPatrols` | Cover takeover. When it closes: combat music, the advisor's "Soviet infantry spotted" line, then (queued after it) the Mosin rifleman's Russian radio line `IntroMission_MosinGermansSpotted` on the `SovietMosin` portrait, with English subtitles (see `VoiceLines/AGENTS.md`). Three Mosin squads are tracked. | All three squads are dead, or the player crosses the sandbag line (trigger 2). |
| 3 | `Mission_Intro_HangarT26s` | Three T-26s leave the hangar mouths with attack-move orders. Four spaced-armour panzers arrive (Pz II F for the Strike Division, Luchs otherwise). As each spawns, `SendPanzerBehindVultures` attack-moves it to a line `PanzerBehindVulturesDistance` (1500) behind the Vulture soldiers' centre, away from the hangars, 500 apart, facing the hangars. The `IntroMission_LightPanzerSupport` line ("Kommandant soviet light panzers have been spotted!...", ElevenLabs take 3 of 3, 2026-11-12) plays. 5 s after the first panzer arrives (`OnPanzerSupportSpawned` -> `ShowSpacedArmourThenT26`: Delay 5 with the mission manager as world context) the spaced armour takeover opens; its On Closed calls `ShowT26InfoTakeover`, which opens the vehicle info takeover (library row `T26`) on the first living hangar T-26 (`TryShowT26InfoTakeover`). When no T-26 has spawned yet it sets `bT26TakeoverPending`, and the last hangar spawn opens it (`ShowT26TakeoverIfPending`). | The T-26s are destroyed. |
| 4 | `Mission_Intro_RidgeArmor` | Soviet armour comes out of the ridge gate, eight T-26s flank from the convoy's map edge, and German heavy reinforcements arrive from the west map edge (below). The `IntroMission_HeavyArmorApproaching` line plays (take 4, 16.7 s). | Both Soviet groups are destroyed: the ridge wave (5 on Normal, 6 on Hard, 7 on Brutal and Ironman) and the 8 T-26s. Each group shows as a survival-style division marker on the minimap until its last tank dies. |
| 5 | `Mission_Intro_ClearSovietRig` ("Secure the Line", a `USecureBaseMission`, 2026-11-16) | The ridge tanks are dead but the Soviet base on the ridge still stands. The fog lifts over the base for 60 s, destroy markers and minimap icons go on its 7 buildings (both cooling towers, both barracks, both MG guard towers, the long camo bunker), the base comes alive (4 Mosin squads and 1 PTRS squad marching, a Zil fuel truck driving up to the rear wall where a crane unloads it) and a 30 s camera tour shows each target with a caption. Then the objective counts the destroyed buildings. See below. | All 7 buildings are destroyed. |
| 6 | `Mission_Intro_BaseBuilding` | The base-building tutorial (below). Its trigger waits for ClearSovietRig, so the HQ glider and the "ridgeline is secured" line only come once the Soviet base is destroyed. | The barracks and forge are deployed. |

## Mission_Intro_ClearSovietRig: secure the line

`Mission_Intro_ClearSovietRig` is reparented (2026-11-16) to the C++ mission class `USecureBaseMission`
(`Source/Docs/Missions/MissionCameraTour.md`); its old placeholder graph (complete at once) is gone. All its values are
class defaults written by `Source/Tools/IntroMission/setup_secure_line_mission.py`, so the map was not re-saved:

- **Targets:** actors of `Rus_Building_CoolingTowers`, `Rus_Building_Barracks`, `Rus_BXP_GuardTowerMG` and
  `Rus_BXP_LongCamoBunker` (the placed ones are the `_Concrete` children) inside the base box (x -2750..4250, y -4400..5400):
  7 buildings. Trim `TargetClasses` (or the box) to change which ones count. `BP_Hint_DestroyTarget` ("Destroy") floats
  over each one and an attack icon sits on the minimap; both go when it dies.
- **Objective:** "Secure the Line": "Destroy the Soviet ridge base: its cooling towers, barracks, MG guard towers and
  the long camo bunker. N/7 destroyed." No voice line yet (`ObjectiveVoiceLine` is empty).
- **Reveal:** 9 vision bubbles of 24 m over the base and the rear slope, 60 s.
- **Base life:** Mosin squads march on four lanes (west of the barracks, between the barracks and the fuel storage,
  behind the west wall and gate, behind the north cooling towers) and a PTRS squad between cooling towers 2 and the rear
  wall. A `Rus_ArmoredCar_ZilFuel` spawns behind the crest south of the rear wall (4640,-5200) 17 s after the tour starts,
  drives straight north along the outside of the east wall and parks parallel to it by the long bunker (~27 s into the
  tour, during the bunker shot); `BP_MissionFuelCrane` (3650,-1850, inside the wall) then lifts its barrel pallets over
  the wall onto a stack (`Source/Docs/Missions/MissionFuelCrane.md`). The crane only reaches a truck parked parallel
  within about a metre of (4640,-1717); arriving from the east the truck stopped nose-in and out of reach.
- **Tour (6 shots, 34 s + 2 s back):** the base from the west, the MG guard towers, the barracks, cooling towers 1,
  cooling towers 2, the long bunker from the rear with the truck and crane, then back to the player's view. Captions name
  each target.
- **Done:** when the last of the 7 is dead the mission completes and `Mission_Intro_BaseBuilding` starts (HQ glider).
- The placed Soviet tanks inside the base (two MS-1, T-50, BT-7) and the marching squads are not targets; the player
  fights them on the way in.

### Assault response (ramp trigger, 2026-11-16)

`USecureBaseMission::AssaultResponse` (`FSecureBaseAssaultResponse`). Its values are written by
`Source/Tools/IntroMission/setup_assault_response.py`, kept separate from `setup_secure_line_mission.py`.

1. **Trigger:** the trigger arms when the camera tour ends. Every 0.5 s it polls for a living player-owned pawn in a
   box centred at (-4300, 0, 500) with half extent (900, 1900, 700). That box covers x -5200..-3400 and
   y -1900..1900: the top of the west ramp (ground about 140 at x -6800, rising to the 590 plateau at x -4400),
   20-30 m in front of the `Rus_BXP_53KSectorPit` (-2184, -506) and the dug-in `Rus_Vehicle_T34E` (-1200, 989).
   A unit already standing there when the tour ends fires it at once. The response fires once.
   `StartAssaultResponse()` fires it by hand.
2. **Alarm:** `IntroMission_MosinBaseUnderAttack` plays on the `SovietMosin` portrait. The voice is Vlasov speaking
   Russian through the Broken transmitter chain; the subtitle is the English text "Resistance forces are attacking the
   base; bring in reinforcements! DESTROY THEM ALL!!".
3. **Soviet air drops:** each one is an `AEnemyAirDrop` spawned at runtime (`InitRuntimeAirDrop`), with
   `bWarnPlayer` off so the generic paratrooper warning does not talk over the scripted lines.
   - Two `GA_SovietParatroopers` transports: one lands at (300, -1750) after 2 s, the other at (1900, 4450) after 4 s.
     Each drops a Mosin squad and a PTRS (Okhotnik) squad, plus one more Mosin squad on Hard and above.
   - One `GA_SovietT34Drop` cargo plane (PE-8) brings a T-26 **on ropes**: the new `bRopeSlungVehicles` option, which
     sets `UGA_AircraftDrop::bRopeSlungTanks`. It lowers the tank at (1950, -1550), between the barracks and the fuel
     storage.
   - The landing spots were chosen from an actor-bounds survey: each is at least 7 m clear of every structure taller
     than the floor slabs.
   - The landed units join the enemy AI's direct control.
4. **Air support:** after 7.5 s, `IntroMission_StukaSupport` plays on the advisor portrait ("Verdammt! Zeir position
   is tougher zan expected! HQ provides two Ju-87 ground attack aircraft zu aid in bringing zat base down!"). After 9 s
   a copy of `GA_Ju87Strafing` is flown at the base centre (800, 500). `SetMissionStrafeOverrides` sets 2 aircraft,
   60 s, and `bAirSupportStaysOnTarget`.
   - Without that last option the strafe ability makes one run and then hunts the nearest enemies anywhere. With it,
     the new opt-in `FStrafeAircraftSettings::bRepeatRunsOverTarget` (`AAircraftMaster::OnAttackMoveCompleted`) keeps
     every run over the target until the 60 s are up.

Audition takes are in BlenderTanks `Announcer/AAAGerMissions/IntroMission/Audition_RampAssault/`, takes A/B/C for
each line. The user picked take A of both (2026-11-16); B and C were deleted.

Test: `assault_response_headless_test.py`, run in a throwaway DebugGame editor. It cuts the chain to ClearSovietRig,
applies the defaults to the map instance, or uses the saved Blueprint values with `ASSAULT_TEST_USE_SAVED=1`. It
spawns a Pz IV F1 below the ramp with `MapTest_SpawnActor`, checks there is no response, then moves the tank onto the
ramp. Run of 2026-11-16 with repeat runs on:
- the trigger fired 0.27 s after the tank reached the ramp, and 3 Soviet transports flew in;
- the T-26 hung 12 m under the PE-8 and set down at z 534 inside the base; no sling errors in the log;
- 4-8 paratroopers stood near each landing point 30-50 s after the trigger;
- 2 Ju 87s passed over the base about every 20 s. Deep-base damage was 320 / 1356 / 244 in the 20 s windows from
  t+15, 6 Soviet actors died, and the aircraft stayed within 105 m of the target.

Distances in this section are Unreal units (cm) unless they say m.

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
  - `briefing_voice_lines_in_order`: `IntroMission_AdvisorWelcome`, then the three briefing lines, each heard at least
    5 s after the one before.
  - The Vultures parachute in (`DropVultures` global ability), so the test waits until every player soldier has stood
    on the ground for 3 s (60 s at most) before teleporting them to the move-to point.
  - 2026-11-15 Strike/Normal: 54/59. The 5 failures are the usual load ones (4 convoy checks and
    `panzers_drove_behind_vultures`).
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
- `Source/Tools/IntroMission/secure_line_headless_test.py` covers mission 5 ("Secure the line"). It cuts the chain in the
  editor world to [ClearSovietRig without its trigger, BaseBuilding with its trigger] (not saved) and checks: the tour
  (all 6 shots, the bars/caption overlay up during it and gone after, about 34-42 s wall clock, camera back to the
  player's rotation; position within 25 m because the unattended editor's cursor edge-scrolls the camera the moment
  control returns), 7 targets with 7 markers over them, the 9 reveal bubbles (gone by the end), 4 Mosin + 1 PTRS squads
  inside the walls with the march order, the fuel truck parked parallel within 2.5 m of (4640,-1717) during the bunker
  shot and the crane unloading it, then no glider for 15 s, then the targets killed one by one through
  `ApplyDamage` (marker count and destroyed count follow, no glider before the last), ClearSovietRig completes and the
  Mammut glider comes. It takes a `HighResShot` per tour shot (scene only: PIE screenshots leave out UMG).
  - It also checks the crane takes the truck's own pallets (visible `Cargo*` parts drop), the objective panel reads
    "Secure the Line" / "1/7 destroyed" after the first kill, and takes `shot showui` pictures on their own ticks (these
    do include UMG: bars + caption mid-tour, HUD + objective after).
  - 2026-11-16 (Development build, heavy machine load): 28/28 PASS; truck parked 25.9 s into the tour (bunker shot),
    parallel to the wall. The pose trace after the tour shows the edge-scroll drift (+X+Y every frame).
  - Known: the tour runs on real time, the truck's timer and drive on game time. When frames hitch badly (a run at
    ~2 fps) game time falls behind and the truck parks after the bunker shot; at normal frame rates they match.
  - Full chain (`intro_mission_headless_test.py`, Strike/Normal, 2026-11-16): 58/63, every ridge and secure-line check
    passes; the 5 FAILs are the usual load ones (4 convoy checks, `panzers_drove_behind_vultures`). A first attempt
    died of a GPU device-removed (VRAM full with other sessions' editors); `-dpcvars=r.Streaming.PoolSize=900` helped.
- `intro_mission_headless_test.py` (the whole chain) now expects ClearSovietRig to run its tour and wait after the
  ridge, kills its targets, and only then waits for the glider (`secure_line_running_after_ridge`,
  `secure_line_tour_played`, `no_glider_while_base_stands`, `clear_rig_completed`). Its convoy checks ignore the base's
  own fuel truck (x > 2500).
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
