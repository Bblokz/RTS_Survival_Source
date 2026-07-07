Keeping track of state of the world like what turn it is and later what campaigns are done, what enemy objects are already attacked by the player etc will be in the WorldCampaign.

# World Campaign Notes

This AGENTS file applies to everything under `RTS_Survival/WorldCampaign/`.

## State Ownership

Persistent campaign/world state belongs in this WorldCampaign scope. Do not hide campaign runtime state in UI widgets, mission-only classes, or `URTSGameInstance` when the state describes the campaign map itself.

Use WorldCampaign-owned save/state types for things like:

- current turn,
- completed or failed campaign content,
- attacked enemy world objects,
- completed missions,
- chapter or campaign progression,
- dynamic event timers,
- world division save state,
- revealed or interactable map state.

Prefer stable `FGuid` anchor keys when storing per-map-object state. Actors can be destroyed, restored, regenerated, or pruned; anchor keys are the durable bridge between generated objects, save data, and future runtime state.

## World Data And Generated Map Data

There are two different meanings of "world data" in this folder:

- `AGeneratorWorldCampaign` owns the generated map layout and its transient placement state.
- `UWorldData` is designer-authored content that is loaded into generated objects after placement.

The generator's important runtime data is:

- `M_GenerationStep`: the current ordered generation step.
- `M_PlacementState`: seed used, cached anchors/connections, player HQ and enemy HQ anchors/keys, and maps from anchor key to enemy, neutral, and mission item type.
- `M_DerivedData`: rebuildable caches such as hop distances, connection degree data, chokepoint scores, and placed-count data used by placement rules.
- `M_StepTransactions` and step attempt maps: rollback/backtracking data for generation retries.
- placement rule structs and count/difficulty tuning: designer-facing constraints that decide what can be placed and where.

Saved campaign data lives in `FWorldCampaignState` / `USaveWorldCampaign`. It currently stores the current turn, generation step, HQ anchor keys, saved anchors, saved connections, saved map items, and saved world divisions. Add future persistent campaign fields there or in clearly named WorldCampaign save structs referenced from there.

`UWorldDataComponent` loads `UWorldData` into generated objects after placement. That data includes enemy primary reward pools, bonus objective definitions, base difficulty percentages, fortification strength reason text, strategic support reason text, and field division reason text. Keep this as content enrichment; do not make it the owner of generated topology or campaign progression.

## High-Level Generation Flow

`AWorldPlayerController::BeginPlay` is the current startup orchestrator:

1. Find the placed `AGeneratorWorldCampaign`.
2. Set up the world menu and read campaign settings, difficulty, and player faction from the game instance.
3. Initialize the generator with the controller and runtime settings.
4. Either generate a new campaign or restore a saved one.

For a new campaign, the generator runs the map generation pipeline:

1. Generate or gather anchor points inside the world boundary.
2. Create the connection graph between anchors.
3. Place the player HQ.
4. Place the enemy HQ.
5. Place the enemy wall.
6. Place enemy objects.
7. Place neutral objects.
8. Place missions.
9. Finish, promote objects, prune unused anchors, repair connectivity, load `WorldData`, initialize base fortification strength, cache save state, initialize divisions, then start systems that require live map objects such as country occupation and FOW.

The generator may use async placement and backtracking, but the high-level order above is the gameplay contract. Keep new placement logic consistent with that order unless the whole generation pipeline is intentionally redesigned.

For loaded campaigns, `UWorldStateAndSaveManager` loads `FWorldCampaignState`, then `AGeneratorWorldCampaign::RestoreWorldStateFromSave` rebuilds anchors, connections, map objects, and placement state from the saved anchor keys.

## High-Level Turn Flow

Turns are currently represented by `EWorldTurnType` with `Player`, `Enemy`, and `None`. The persistent turn number is stored on `FWorldCampaignState::CurrentTurn` and advanced by `UWorldStateAndSaveManager::AdvanceCurrentTurn`.

After initial world setup, the controller moves the camera to the player HQ and starts an enemy turn. That enemy turn advances the counter and then starts the first player turn, so the first playable player turn is based on the saved `CurrentTurn` after `AdvanceCurrentTurn`.

Current player turn flow:

1. `AWorldPlayerController::PlayerTurn` refreshes strategic support and field division difficulty reports.
2. It builds `FPlayerTurnContext` from the save manager's current turn.
3. It calls `BP_OnPlayerTurnStarted` so Blueprint/UI can present the player-facing turn state.

Current enemy turn flow:

1. Move player-owned world divisions for their committed orders.
2. Refresh strategic support and field division difficulty reports.
3. Move enemy-owned world divisions.
4. End the enemy turn, advance the current turn, update the turn counter UI, save campaign state, and start the next player turn.

As more campaign systems are added, keep turn-long effects and one-mission-equals-one-turn logic in WorldCampaign state/manager code. UI should display the state; it should not be the authority for whether a campaign item was completed, attacked, revealed, or timed out.
