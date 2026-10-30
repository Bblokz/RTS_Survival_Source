# SurvivalMode — integration and "new map" template

## How a match runs (code path)

1. `USurvivalModeLauncher::LaunchSurvivalMode(SurvivalMap)` (main menu button) → launch request
   `FactionAndDifficulty` + variant selection on → `RTS_FactionSelection` map → faction → variants → difficulty →
   the survival map opens.
2. The map's `ASurvivalModeDirector` (BeginPlay) gathers `ASurvivalStartLocation` / `ASurvivalEdgeSpawnZone`
   actors, waits for the main game UI, then shows `ArmySelectionWidgetClass`.
   `ACPPController::GetIsGameWithChoosingStartingLocation` returns true when a director exists, so the
   controller does not spawn the profile units/HQ on its own.
3. `ConfirmArmy` → `StartLocationPickerWidgetClass` → `ConfirmStartLocation` →
   `ACPPController::StartGameAtLocation(center, bDoNotLoadPlayerProfileUnits = true)` (techs, resource cards,
   variants) → chosen location `GeneratePlayerStartContent`, every other location
   `USurvivalSovietBaseComponent::GenerateSovietBaseAt` → `USurvivalDeploymentComponent::StartDeployment`.
4. Deployment: HQ first. Vehicles hang under `Deployment.VehicleDropAircraftClass`
   (`Ger_Aircraft_JU390B_AirborneUnarmed`; the `DropShips/BP_DropShip_*` Blueprints are "started as landed" and never
   take off when spawned in the air) via `AAircraftMaster::OrderAircraftDrop`; team weapons and walkers walk out of a
   landed aircraft; infantry jump with `GA_GerParatroopers` (copied from `DA_GlobalAbilitySystem`) unless the
   settings carry their own `ParatrooperTemplate`. Aircraft come from the map edge nearest the start.
5. HQ lands → `ACPPController::SetupPlayerHQReference` (drop-off + mission starting resources) → grace period
   (`GracePeriodSeconds`, 180) → `USurvivalWaveDirectorComponent::StartWaves` → defeat check every 2 s
   (`DefeatRule`) → `UW_SurvivalHud::ShowDefeat`; best time and waves survived per map+difficulty in save slot
   `SurvivalModeRecords`.
6. Waves are rounds (`USurvivalWaveSettings::Rounds`): a round sends groups from the edges for
   `FirstRoundActiveSeconds` (60) + `ActiveSecondsPerRound` (20) per round (max 420), then clears; it is survived
   once its own enemies are dead (or after `ClearTimeoutSeconds`). Round enemies are the units that appear at a
   just-launched group's spawn points (game unit manager add events, `AttributionRadius`/`AttributionWindowSeconds`);
   the HUD shows "N ENEMIES REMAINING" (alive + still spawning). The alive cap counts only wave units, not the
   Soviet base garrisons.
7. After every survived round: intermission (`USurvivalModeSettings::Rewards`, 40 s + 1 s per round, max 60) with
   `OptionsPerReward` (3) reinforcement sets in `UW_SurvivalRewardPicker` (code-built widget). Sets are rolled by
   `FSurvivalRewardLibrary` from the faction roster (selected variants, supply costs): themes Infantry / Armour /
   Support / Combined, worth `FirstRewardSupply` (16) + `RewardSupplyPerRound` (5) per round, max 70, at most
   `MaxUnitsPerSet`; designer sets in `AuthoredSets` can replace one option. The pick (or option 1 when time runs out)
   is flown in by a reinforcement-mode `USurvivalDeploymentComponent` next to the base; then the next round starts.

Shared files touched outside `SurvivalMode/`:
- `Player/CPPController.h/.cpp`: friend `ASurvivalModeDirector`; `GetIsGameWithChoosingStartingLocation` also checks
  for a director; `bM_IsHQDeliveredAfterStart` silences the "no HQ at profile load" error for air-dropped HQs.
- `Enemy/EnemyController/EnemyController.h/.cpp`: `OverrideStrategicDecisionTree` (survival decision tree template).
- `Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h/.cpp`: `SetStochasticDecisionTree`.
- `Units/Aircraft/AircraftMaster/AAircraftMaster.cpp`: a drop aircraft goes movement-idle when it lands (it ran the
  finished path every frame while parked, one error per frame).
- Content: `Blueprints/GameUI/StartGame/StartGameUI` has a SURVIVAL button (below Quit) calling
  `USurvivalModeLauncher::LaunchSurvivalMode`.
- `Config/DefaultGame.ini`: `DirectoriesToAlwaysCook` for `/Game/RTS_Survival/SurvivalMode` and for the folders whose
  Blueprints the start content and Soviet bases spawn (RusBuildings, RusBunkers, CaptureObjects, Resources/ChildBps,
  ScavengeObjects, CraterDirt decals, GroundFoliage meshes, DestructableEnvActor Military/SandBagWalls/Airport). The
  survival data assets hold those classes as C++ defaults, which are not serialized, so the cooker cannot follow them;
  a new preset or start-content entry from another folder needs its folder added here.

Testing: place the map in PIE with the director's `bDebugAutoStart`, or pass `-SurvivalAutoStart -SurvivalGrace=20` to
a build with `RTS_WITH_MAP_TESTS` (the Shipping test exe: add `-RTSMapSmokeTest -RTSMapSmokeTestSeconds=900` and the map
path; the plain game has no such switch): it deploys a debug army at the first start location and logs RTS errors instead of
showing a modal dialog per error. Progress is logged under `LogTemp: Survival ...`.

Full menu route without a mouse, in the same builds: start on the `StartGame` map with `-RTSSurvivalMenuFlow
-RTSMapSmokeTestSeconds=420 -SurvivalDriveSetup -SurvivalPickReward=1 -SurvivalGrace=20` (presses the SURVIVAL button,
goes through faction, variants and difficulty, confirms an army and a start location, picks reward 2).

## Making a new survival map

1. Duplicate `/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01` (or start from `Maps/ToCopy/RTS_ToCopyMap`)
   through the editor into `/Game/RTS_Survival/SurvivalMode/Maps/`.
2. Sculpt 4–6 start areas (see `10_LevelDesign_SM_Desert_01.md` for the recipe), place one
   `BP_SurvivalStartLocation` per area (set `BuildAreaRadius`, `DisplayName`, defense points).
3. Place 6–10 `BP_SurvivalEdgeSpawnZone` along the map edges.
4. Place one `BP_SurvivalModeDirector`; set `Settings` to a `USurvivalModeSettings` asset (duplicate
   `DA_SurvivalMode_Default` to change slots, supply, grace, waves, bases or start content for that map).
5. Set the World Settings GameMode override to `BP_SurvivalGameMode`.
6. Rebuild navigation and wait until it finishes before saving (a save while the editor still auto-rebuilds
   drops most navmesh tiles; the .umap is ~128 MB when complete), then add the map to `USurvivalModeLauncher::GetSurvivalMaps` and to the cook list.
