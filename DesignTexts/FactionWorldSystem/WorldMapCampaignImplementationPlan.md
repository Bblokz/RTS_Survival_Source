# World Map Campaign Implementation Plan

This document compares the requested world map campaign design with the current `RTS_Survival/WorldCampaign` implementation and lists the remaining work in dependency order.

## Current state already implemented

### Campaign generation foundation

- `AWorldPlayerController::BeginPlay` already has a readable startup sequence: find the world generator, create the world menu, then generate or load the world.
- `AGeneratorWorldCampaign::InitializeWorldGenerator` already receives the world controller, campaign generation settings, and selected difficulty, then runs generation when `bNeedsToGenerateCampaign` is true.
- `AGeneratorWorldCampaign::ExecuteAllStepsWithBacktracking` already defines the core generation order: anchor points, connections, player HQ, enemy HQ, enemy wall, enemy objects, neutral objects, and missions.
- Generation already has editor-callable step functions, backtracking transactions, placement rules, generated anchors, connections, player HQ, enemy HQ, enemy wall, enemy items, neutral items, mission objects, and debug reporting.
- The core enum layer already exists for high-level item categories, enemy item subtypes, player item subtypes, neutral item subtypes, and mission subtypes.

### World map object actors

- The project already has anchor, connection, and object actors: `AAnchorPoint`, `AConnection`, `AWorldMapObject`, `AWorldEnemyObject`, `AWorldMissionObject`, `AWorldNeutralObject`, and `AWorldPlayerObject`.
- `AWorldMapObject` already stores its owning anchor and deterministic anchor key, so it can become the visual/runtime bridge for click selection, save data, and mission launch data.

### UI and player profile foundation

- `UWorldProfileAndUIManager` already creates `UW_WorldMenu`, initializes it, and fills it with default faction profile data for new campaigns.
- `UW_WorldMenu` already coordinates the major UI panels: world view, command perks, card menu, archive, and tech tree focus states.
- `FPlayerData` already groups perk, archive, and card save data for the world campaign profile UI.
- `UW_TurnCounter` exists and can display a turn number plus resource widgets, but it is not yet backed by a persistent world turn state.

### Faction-to-world generation flow

- The faction system already stores generation settings in `URTSGameInstance` and opens the campaign world from the faction selection flow.
- `FCampaignGenerationSettings` already carries generation seed, personality, and whether a campaign needs to be generated.

## Main architectural gap

The current code can generate and display a campaign map, but it does not yet have a persistent campaign runtime loop. The missing layer is a central world campaign state machine that owns the saved map state, turn counter, mission selection, player rewards, enemy turn simulation, event timers, and UI updates.

Implement this before adding individual chapter mechanics. Otherwise, each new feature will need temporary control flow and later rework.

## Required high-level runtime flow

Add one explicit orchestration function on a world campaign runtime/controller layer. Suggested shape:

```cpp
void AWorldPlayerController::BeginPlay_RunWorldCampaignStartup()
{
	BeginPlay_FindWorldGenerator();
	BeginPlay_InitWorldMenu();
	BeginPlay_LoadOrGenerateCampaignState();
	BeginPlay_BindWorldMapSelection();
	BeginPlay_RefreshWorldMapUI();
	BeginPlay_StartPlayerTurn();
}
```

Then keep the recurring turn loop equally obvious:

```cpp
void UWorldCampaignTurnManager::RunPostMissionCampaignSequence(const FWorldMissionResult& MissionResult)
{
	ApplyMissionResult(MissionResult);
	AdvanceTurnCounter();
	RunEnemyTurn();
	SaveCampaignSnapshot();
	StartPlayerTurn();
}
```

This gives the project the “main function that clearly calls things step by step” requested in the design, while keeping generation, UI, profile, and enemy simulation separated.

## Implementation sequence

### Phase 1: Persistent campaign state and map item data

**Already implemented:** Generation can place map objects and assign high-level item categories and raw subtype enums.

**Implement next:**

1. Create save-game data for the campaign snapshot.
   - `USaveWorldCampaign`
   - `FWorldCampaignState`
   - `FWorldMapPersistentItem`
   - `FWorldCampaignTurnState`
   - `FWorldCampaignChapterState`
   - `FWorldCampaignDynamicEventState`
2. Store every generated anchor/object as persistent data.
   - Anchor key.
   - Item type.
   - Raw subtype enum.
   - Base difficulty.
   - Current owner/faction if needed.
   - Dynamic difficulty modifiers or a way to derive them.
   - Revealed/visible state for fog of war.
   - Runtime state such as destroyed, completed, locked, or active.
3. Add conversion helpers for raw item enum decoding.
   - `GetRawItemAsMapMission`.
   - `GetRawItemAsMapEnemyItem`.
   - `GetRawItemAsMapPlayerItem`.
   - `GetRawItemAsMapNeutralObjectType`.
4. Add save/load entry points to the world controller or a dedicated save manager.
   - New campaign: generation output is converted to `FWorldCampaignState`.
   - Load campaign: actors are reconstructed or initialized from `FWorldCampaignState`.

**Dependencies:** This must come before turn logic, enemy turn logic, events, and mission result handling because all of those need a stable saved state.

### Phase 2: World campaign startup orchestration

**Already implemented:** `AWorldPlayerController::BeginPlay` calls separate setup functions, but generation and UI are still loosely coupled and saved loading is a TODO.

**Implement next:**

1. Rename/fix the typo `Beginplay_SetupWorldGenerator` to `BeginPlay_SetupWorldGenerator`.
2. Replace `BeginPlay_GenerateOrLoadWorld` with a clear pipeline:
   - validate world profile/UI manager.
   - validate game instance.
   - load selected campaign slot or detect new campaign.
   - initialize/generate the map.
   - build or load `FWorldCampaignState`.
   - initialize player profile snapshot/default faction data.
   - refresh UI.
   - start player turn.
3. Store `M_PlayerFaction` correctly; it is currently declared but not assigned in the header flow.
4. Require `GetIsValidWorldGenerator()` before calling `M_WorldGenerator->InitializeWorldGenerator`.

**Dependencies:** Requires Phase 1 data types so startup has something concrete to produce/load.

### Phase 3: Turn counter and turn manager

**Already implemented:** `UW_TurnCounter` can display a turn count, but there is no campaign state that increments one mission as one turn.

**Implement next:**

1. Add `UWorldCampaignTurnManager` or a controller-owned component.
2. Add `CurrentTurn` to `FWorldCampaignTurnState`.
3. Define one mission attempt as one turn.
4. Expose simple functions:
   - `StartPlayerTurn()`.
   - `CompleteMissionAndAdvanceTurn(const FWorldMissionResult&)`.
   - `RunEnemyTurn()`.
   - `FinishEnemyTurnAndStartPlayerTurn()`.
5. Connect `UW_TurnCounter` to persistent turn state.
6. Track duration-based effects and events in turns rather than timers.

**Dependencies:** Requires persistent campaign state. Enemy simulation and dynamic events depend on this.

### Phase 4: Player turn actions

**Already implemented:** Perk UI, card menu UI, archive UI, default profile data, and resource widgets exist.

**Implement next:**

1. Commander perk spending.
   - Perks from commander XP.
   - Unlock starting unit slots.
   - Unlock starting resource slots.
   - Unlock factory card slots.
   - Increase later slot costs to discourage focusing only one type.
2. Tech spending.
   - Spend radixite and blueprints.
   - Save unlocked technologies in the world campaign profile snapshot.
3. Allied base upgrades.
   - Airfield upgrades.
   - Factory upgrades.
   - Resource and construction blueprint costs.
4. Profile card selection UI.
   - Existing card profile save data should remain in the separate player profile file as requested.
   - Add world-specific UI for selecting which profile cards are active for the next mission.
5. Mission selection validation.
   - Show location difficulty percentage.
   - Show dialog for estimated enemy strength.
   - Warn if the mission is very difficult for the current commander level.

**Dependencies:** Needs turn manager and player profile snapshot/default profile. Mission selection needs map item persistent data.

### Phase 5: Mission launch and mission result ingestion

**Already implemented:** Mission enums exist and campaign generation places mission objects.

**Implement next:**

1. Add `FWorldMissionLaunchRequest`.
   - Mission enum.
   - Anchor key.
   - Difficulty percentage.
   - Enemy support modifiers.
   - Player selected cards.
   - Commander/profile data snapshot.
2. Add a map from `EMapMission` to mission level/mission setup data.
3. Store launch request in the game instance or a campaign transition save object before opening the mission level.
4. Add `FWorldMissionResult`.
   - Victory/defeat.
   - Gained cards.
   - Gained blueprints/radixite.
   - Gained commander XP.
   - Destroyed/changed map items.
5. On victory:
   - mark mission complete.
   - grant rewards.
   - apply any special mission effects.
6. On defeat:
   - increase area difficulty.
   - optionally remove XP/resources if desired.
   - keep the area active or mark failed depending on design.
7. After result processing, call the turn manager’s post-mission sequence.

**Dependencies:** Needs player turn mission selection, turn manager, persistent state, and game instance transition data.

### Phase 6: Enemy turn simulation

**Already implemented:** Enemy map item types exist and generation can place airfields, factories, barracks, supply depots, research facilities, fortified checkpoints, enemy HQ, and wall objects.

**Implement next:**

1. Add enemy turn pass order:
   - show strength increase animations for non-engaged areas.
   - apply supply/rail bonus increases.
   - move active convoys.
   - deploy expired convoys into new bases.
   - move divisions.
   - produce new divisions from factories/barracks.
   - apply research upgrades to map items.
   - save campaign state.
2. Add strength growth calculations.
   - Base difficulty increases each turn.
   - Nearby airfields/artillery/troops add mission difficulty.
   - Supply depot or rail hub adds extra growth to nearby bases.
3. Add enemy item-specific behavior.
   - Airbase: off-map aircraft support, normal/large variants.
   - Supply depot/rail hub: resource and growth support.
   - Factory: creates armored divisions over time, normal/large variants.
   - Research facility: accelerates unit tiers.
   - Fortified checkpoint: route/difficulty gate.
   - Barracks: creates infantry divisions over time.
4. Add queued UI animation events separate from simulation data.

**Dependencies:** Needs turn state and persistent map item data. Dynamic event movement should reuse the same movement/state patterns.

### Phase 7: Dynamic events, convoy system, and timers

**Already implemented:** No full convoy/event runtime is present yet.

**Implement next:**

1. Add event state with turn duration.
2. Implement Russian convoy event.
   - Spawns on enemy-held route.
   - Moves through enemy regions for `X` turns.
   - Offers starting resource cards or blueprint cards if destroyed.
   - If ignored, deploys into a new enemy base.
3. Add event reward flow into the same result/reward pipeline as missions.
4. Add map UI indicators for event countdown and movement path.

**Dependencies:** Needs turn manager, enemy-held route data, persistent state, and mission/reward pipeline.

### Phase 8: Chapters and landmark gates

**Already implemented:** Enemy wall exists as a placed map enemy item, and mission enum names already include late-game wall/Moscow-style missions.

**Implement next:**

1. Add `EWorldCampaignChapter` and chapter state.
2. Chapter 1:
   - Show the Russian wall at the fog-of-war edge.
   - Show important cities behind the wall as visible landmarks through fog.
   - Keep wall/cities locked or unreachable until chapter conditions are met.
3. Chapter 2:
   - Start attack waves from the wall.
   - Spawn wave events through the enemy turn system.
4. Chapter 3:
   - Unlock wall breakthrough missions.
   - Change wall state after breakthrough.
   - Reveal or activate city missions behind the wall.
5. Add chapter unlock rules to world map selection and UI warnings.

**Dependencies:** Requires persistent state, turn manager, enemy turn events, and fog/visibility state.

### Phase 9: Large world map objects and special mission rules

**Already implemented:** Neutral object enum exists and neutral objects can be placed, but the requested landmark gameplay is not implemented.

**Implement next:**

1. East wall.
   - Treat as a chapter gate and visible fog landmark.
2. Water dam.
   - Place several candidate dam points along rivers.
   - Select one active special mission.
   - On mission victory, destroy two downstream map items.
3. Radixite ground zero.
   - Heavy radiation zones.
   - Restrict some vehicles during missions launched there.
   - Provide high radixite rewards.
4. Black forest.
   - Dense terrain landmark.
   - Restrict missions to infantry and light mechanized troops.
5. Important cities behind the wall.
   - Visible through fog.
   - Used as chapter 3 objectives.

**Dependencies:** Needs mission launch request modifiers, fog/visibility, persistent map item data, and mission result effects.

### Phase 10: Fog of war and visible-through-fog landmarks

**Already implemented:** No clear runtime fog state was found in the world campaign code.

**Implement next:**

1. Add per-anchor visibility state.
   - Unknown.
   - Fogged but landmark visible.
   - Revealed.
   - Active/selectable.
2. Add reveal rules from player HQ, completed missions, roads, scouts, or chapter unlocks.
3. Allow special landmarks to render through fog without exposing full mission/enemy data.
4. Ensure wall and major city visibility works before adding chapter scripting.

**Dependencies:** Needs persistent map item state and landmark types. Chapter 1 depends heavily on this.

### Phase 11: UI polish and feedback loops

**Already implemented:** Main world menu panels and focus switching exist.

**Implement next:**

1. Mission detail panel.
2. Difficulty breakdown panel.
3. Enemy turn animation queue.
4. Convoy/event panel.
5. Chapter objective panel.
6. Profile card selection panel.
7. Save/load campaign slot panel if not already provided elsewhere.
8. Warning dialog for high-risk mission choices.

**Dependencies:** UI should be built on top of stable runtime state and event queues, not directly on generated actors.

## Code quality improvements found during analysis

1. `AWorldPlayerController` should validate `M_WorldGenerator` before using it in `BeginPlay_GenerateOrLoadWorld`.
2. `Beginplay_SetupWorldGenerator` should be renamed to `BeginPlay_SetupWorldGenerator` to match project lifecycle naming rules.
3. `AWorldPlayerController` header formatting should be cleaned up near `M_CampaignSettings`, `M_SelectedDifficulty`, and `M_PlayerFaction`.
4. `M_PlayerFaction` is declared but currently not assigned in the observed startup flow.
5. `UW_TurnCounter::InitResourceWidgets` repeats the same widget initialization five times. Store bound resource widgets in a local array or helper function, then iterate with index validation.
6. `UW_TurnCounter::SetTurnCounterText` has formatting issues and should use a validator for `M_TurnCounter` rather than inline `IsValid` plus local error reporting.
7. `UW_WorldMenu::UpdateMenuForNewFocus` has switch cases that do multiple actions. If these grow, extract per-focus handlers before the world UI becomes harder to reason about.
8. `UWorldProfileAndUIManager` uses ad-hoc error strings for member pointers. Add validator functions with `RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object` because it is a `UActorComponent`, not an actor.
9. World generation is already very large. Future runtime systems should not be added to `AGeneratorWorldCampaign`; keep generation-only code there and put turn/runtime logic into dedicated components or manager UObjects.
10. Avoid storing raw pointers or references to `TArray`/`TMap` elements while implementing map state and enemy turn simulation. Use anchor keys and re-validate indices before access.

## Recommended next coding task

Implement Phase 1 and Phase 2 together in a small vertical slice:

1. Add campaign save/state structs.
2. Convert generated map objects into persistent state after generation.
3. Add a clear startup orchestration function that either creates a new state from generation or loads a saved state.
4. Wire the turn counter to `CurrentTurn` with an initial value of `1`.

That slice creates the foundation needed for every later feature without prematurely implementing enemy AI, chapter scripting, or special landmark missions.
