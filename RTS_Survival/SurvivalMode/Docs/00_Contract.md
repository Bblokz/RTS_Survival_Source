# SurvivalMode — shared contract

Everything for this mode lives in two folders:
- C++: `Source/RTS_Survival/SurvivalMode/`
- Content: `Content/RTS_Survival/SurvivalMode/` (`Maps/`, `Data/`, `UI/`, `PCG/`, `Blueprints/`)

A new survival level = duplicate a survival map, place one `ASurvivalModeDirector`, point it at a
`USurvivalModeSettings` data asset, place `ASurvivalStartLocation` actors (4–6) and `ASurvivalEdgeSpawnZone`
actors along the map edges. Nothing level-specific lives in C++.

## Flow (phases, `ESurvivalPhase` in `Core/SurvivalModeTypes.h`)

1. Main menu "Survival" → `ERTSPreGameSetupFlow::FactionAndDifficulty` with the survival map as destination and
   `SetAllowUnitVariantSelection(true)` → existing faction map: faction → variants (saved in the player profile) →
   difficulty → travel to the survival map. (No change to the faction map.)
2. `ArmySelection` — in the survival map. `UW_SurvivalArmySelection` lists the faction's units
   (training layout of `URTSGameInstance::GetPlayerFaction()`), each shown as its selected variant (profile
   `M_UnitVariantSelections` + `UUnitVariantDataAsset`), with a supply cost. The player drags units into a fixed
   number of slots (`USurvivalModeSettings::Army.SlotCount`, any count, grid wraps). Slot 0 is always the HQ
   (locked). Total supply ≤ `Army.SupplyBudget`.
3. `StartLocationSelection` — the player cycles/clicks the `ASurvivalStartLocation`s and confirms one.
4. `Deploying` — the director calls `ACPPController::StartGameAtLocation(loc, /*bDoNotLoadPlayerProfileUnits*/true)`
   (applies techs/resource cards/variants, spawns nothing), then `USurvivalDeploymentComponent` flies the army in:
   vehicles (tanks, armored cars, nomadics incl. the HQ, harvesters) hang under drop aircraft
   (`AAircraftMaster::OrderAircraftDrop`, payload Tank); infantry and team weapons jump with parachutes
   (`UGA_Paratroopers::ExecuteCinematicDrop`); player aircraft fly in themselves. HQ lands first.
   The chosen start location runs `GeneratePlayerStartContent()` (resource fields, wrecks, pits);
   every other start location gets a Soviet base (`USurvivalSovietBaseComponent::GenerateSovietBaseAt`).
5. `GracePeriod` — `Settings.GracePeriodSeconds` (default 180) countdown on screen.
6. `Waves` — `USurvivalWaveDirectorComponent::StartWaves()`; the survival clock runs; waves enter from
   `ASurvivalEdgeSpawnZone`s and attack `ASurvivalModeDirector::GetCurrentPlayerBaseLocation()` evaluated when
   each wave launches.
7. `Defeated` — HQ and all units lost (or HQ lost, per settings) → defeat screen with time survived and waves
   beaten; best time saved per map.

## C++ ownership (one writer per file; nobody builds except the integrator)

| Folder / file | Owner |
|---|---|
| `Core/`, `Data/SurvivalModeSettings.*`, `Director/`, `Deployment/`, `Army/SurvivalArmyTypes.*`, `Army/SurvivalArmyLibrary.*` | integrator (main session) |
| `UI/` (army selection + start-location picker + HUD widgets) | mission-UI agent |
| `StartLocations/` (`ASurvivalStartLocation`, start-content PCG nodes) | start-location agent |
| `Waves/` (`USurvivalWaveDirectorComponent`, `USurvivalWaveSettings`, `ASurvivalEdgeSpawnZone`, AI tree glue) | waves/AI agent |
| `SovietBases/` (`USurvivalSovietBaseComponent`, `USurvivalSovietBaseSettings`, base PCG nodes) | Soviet-base agent |
| `Docs/NN_*.md` | each agent writes its own doc |

Shared engine/game files outside `SurvivalMode/` are touched only by the integrator. If you need a change
there, write it in your doc under "Integration requests" with the exact diff.

Code style: match the project — `M_` private members, `bM_` private bools, `not`/`and`/`or`,
`RTSFunctionLibrary::ReportError`, `GetIsValidX()` guards, `/** @brief */` doc comments on public API,
UPROPERTY(EditAnywhere/EditDefaultsOnly) for designer data. Include paths are `RTS_Survival/...`.

## Key interfaces (see the headers; signatures are binding)

- `ASurvivalModeDirector` (`Director/SurvivalModeDirector.h`)
  - `GetSettings()`, `GetPhase()`, `OnPhaseChanged` (native multicast `void(ESurvivalPhase OldPhase, ESurvivalPhase NewPhase)`)
  - `GetStartLocations()`, `GetChosenStartLocation()`, `GetElapsedSurvivalSeconds()`, `GetGraceSecondsRemaining()`
  - `GetCurrentPlayerBaseLocation()` — HQ if alive, else centroid of player buildings, else of player units.
  - `GetDifficulty()` — `FRTSGameDifficulty` from the game instance.
  - UI entry points: `ConfirmArmy(const TArray<FSurvivalArmyPick>&)`, `ConfirmStartLocation(ASurvivalStartLocation*)`.
- `USurvivalModeSettings` (`Data/SurvivalModeSettings.h`) — `Army`, `Deployment`, `GracePeriodSeconds`,
  `WaveSettings` (→ `USurvivalWaveSettings`), `SovietBaseSettings` (→ `USurvivalSovietBaseSettings`),
  `StartContent` (→ `USurvivalStartContentSettings`).
- `FSurvivalArmyEntry` / `FSurvivalArmyPick` / `USurvivalArmyLibrary` (`Army/`) — available units with variant
  and supply cost; supply validation.
- `ASurvivalStartLocation` (`StartLocations/SurvivalStartLocation.h`) — `GetDeployCenter()`,
  `GetBuildAreaRadius()`, `GeneratePlayerStartContent(const USurvivalStartContentSettings*)`,
  `GetDefensePoints()`.
- `USurvivalWaveDirectorComponent` (`Waves/`) — `InitWaves(ASurvivalModeDirector*)`, `StartWaves()`, `StopWaves()`,
  `GetWavesLaunched()`.
- `USurvivalSovietBaseComponent` (`SovietBases/`) — `GenerateSovietBaseAt(ASurvivalStartLocation*, int32 BaseIndex)`.

## Engine systems to reuse (do not rewrite)

- Faction units: `UTrainingLayoutSettings::Get()->LoadTrainingLayout()->FactionLayouts` (see
  `AFactionPlayerController::GetFactionUnitsFromTrainingLayout`).
- Variants: `UUnitVariantSettings::Get()->LoadVariantDataAsset()`, `FRTS_Profile::LoadPlayerProfile()->M_UnitVariantSelections`,
  `UPlayerUnitVariantManager::GetSpawnerKey`.
- Costs/time: `DT_Training` rows via `UTrainingOptionLibrary::GetTrainingOptionName`.
- Spawning: `ARTSAsyncSpawner::AsyncSpawnOptionAtLocation`.
- Waves: `UEnemyWaveController::StartSingleAttackWave` / `StartNewAttackMoveWave` on `AEnemyController`;
  strategic AI: `Enemy/StrategicAI/` (stochastic decision tree); tech: `EnemyAITechLevel`.
- PCG nodes: `Procedural/CustomNodes/` (`PCGCreateRadixiteField`, `PCGCreateMetalField`, `PCGFindEnemyDefensePoints`,
  `EnemyBaseSetup/*`, `PCGSpawnLevelInstance`), maps `Maps/Campaign/EnemyObjProcedural/*`.
- Drops: `GA_AircraftDrop`, `GA_Paratroopers`, `AAircraftMaster::OrderAircraftDrop`.
- Desert ground: `Materials/ground/Landscape/DesertPaintable/MI_RTS_Landscape_DesertPaintable`.

## The editor

One live editor (UECP MCP `uecp-rts_survival`). Read-only queries are fine for everyone. Asset creation only
inside your own `Content/RTS_Survival/SurvivalMode/<YourArea>/` folder. Only the level-designer agent opens or
edits the survival map. Never copy `.umap`/`.uasset` files on disk; duplicate through the editor. Do not build
C++ or restart the editor; the integrator builds and sends you compile errors.
