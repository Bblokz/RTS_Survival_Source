# EnemyController AI Setup Overview

This note summarizes what is already in place for EnemyController AI so we can keep the current wave behavior while extending with direct-control building blocks later.

## Current high-level architecture

- `AEnemyController` already composes dedicated AI subcomponents/controllers:
    - `UEnemyWaveController` (wave spawning/orchestration)
    - `UEnemyFormationController` (movement, attack-move support logic, patrol handling)
    - `UEnemyNavigationAIComponent`
    - `UEnemyStrategicAIComponent` (async strategic requests)
    - `UEnemyRetreatController`
- These are initialized from `AEnemyController` constructor and linked back through explicit init calls.

## Wave behaviors currently supported (and actively wired)

The game-side wave API in `AEnemyController` currently supports the three gameplay patterns you mentioned:

1. **Attack wave**
    - `CreateAttackWave`
    - `CreateSingleAttackWave`
2. **Attack-move wave**
    - `CreateAttackMoveWave`
    - `CreateSingleAttackMoveWave`
3. **Patrol wave with attack-move support**
    - `CreateSingleRandomPatrolWithAttackMoveWave`

All of these forward into `UEnemyWaveController`, which then creates wave entries and routes movement orders to formation logic.

## Existing custom structs relevant to this pipeline

### Wave / formation structs

- `FAttackMoveWaveSettings`
    - Help-offset radii, projection scale, max tries for support position projection, and combat linger timing.
- `FRandomPatrolWithAttackMoveSettings`
    - Patrol-specific settings plus embedded attack-move settings.
- `FAttackWave`
    - Wave state including flags for attack-move and random patrol-with-attack-move modes.
- `FFormationData`
    - Formation runtime state with booleans and nested patrol state used by `UEnemyFormationController` tick/update logic.

### EnemyController local async spawn context

`EnemyController.cpp` already has a small local namespace for field-construction async handling:
- `FFieldConstructionSpawnRequest`
- `FFieldConstructionSpawnContext`

This context tracks pending async spawns, collects valid spawned squads, handles failure reporting, and only dispatches a construction order once all pending spawn callbacks complete.

### Strategic AI request/result structs

`StrategicAIRequests.h` is already set up as a block-based request/result model:

- Generic pair helper:
    - `FWeakActorLocations` (weak actor + candidate world locations)
- Request structs:
    - `FFindClosestFlankableEnemyHeavy`
    - `FGetPlayerUnitCountsAndBase`
    - `FFindAlliedTanksToRetreat`
- Result structs:
    - `FResultClosestFlankableEnemyHeavy`
    - `FResultPlayerUnitCounts`
    - `FDamagedTanksRetreatGroup`
    - `FResultAlliedTanksToRetreat`
- Batch containers:
    - `FStrategicAIRequestBatch`
    - `FStrategicAIResultBatch`

This is already very close to a "divide and conquer from blocks" design.

## Async request flow already implemented

- `UEnemyStrategicAIComponent` accumulates requests into `M_PendingRequests` via queue functions:
    - `QueueFindClosestFlankableEnemyHeavyRequest`
    - `QueueGetPlayerUnitCountsAndBaseRequest`
    - `QueueFindAlliedTanksToRetreatRequest`
- A repeating timer (`EnemyStrategicAIThinkingSpeed`) triggers `ProcessStrategicAIRequests`.
- If pending arrays are non-empty, the component:
    1. moves the current batch out,
    2. resets pending batch,
    3. submits the batch to `UGameUnitManager::RequestStrategicAIRequests`,
    4. receives callback via weak self capture,
    5. stores latest snapshot in `M_LatestResults`.

This means strategic decisions are already computed asynchronously and returned in grouped structs tied by `RequestID`.

## Small AI behaviors already in place besides plain wave spawning

- **Attack-move assist behavior** in formation controller:
    - Finds in-combat allies and issues helper movement orders around them.
    - Uses nav projection and retry caps from `FAttackMoveWaveSettings`.
- **Combat-aware waypoint advance**:
    - Attack-move formations can delay progression until combat is resolved or a max linger threshold is reached.
- **Random patrol looping**:
    - Patrol formations cycle patrol points and blend in attack-move assist logic while guarding.
- **Strategic retreat grouping**:
    - Async query groups damaged allied tanks and matches nearby idle hazmat helpers with formation offsets.

## Why this is already compatible with future blueprint-driven direct control

Without changing current wave behavior, the existing setup already supports your intended next step:

- We already have **batch request structs** and **result structs** that can carry weak actor pointers plus generated location sets.
- We already have **request IDs** for correlating asks/results.
- We already have **callback-safe async flow** with weak ownership handling.

So the natural extension is to add blueprint-callable entry points that:

1. accept selected unit pointers (or groups),
2. package them into new request structs (or augment existing ones),
3. submit through the same strategic batch pipeline,
4. read back typed results from `M_LatestResults`,
5. apply controller-issued direct orders for only those selected units.

That keeps the current waves untouched while layering an opt-in "direct AI control block" system on top.