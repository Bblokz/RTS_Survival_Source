# World Divisions Design

World divisions are campaign-map actors that represent mobile combat strength. They move only during turn-end movement passes, can be damaged, and project field-division strength to same-owner anchors inside their influence radius.

This system should live in the world campaign runtime layer, not in `AGeneratorWorldCampaign`. The generator builds the map graph; divisions use that graph after generation/load.

## Goals

- Let designers place or spawn tank and squad divisions with readable movement, influence, and strength settings.
- Keep runtime identity and ownership controlled by code/save data.
- Move divisions only in XY on the flat global map.
- Move divisions only at the end of their owning side's turn.
- Allow long orders to continue over several turns by consuming at most `DistanceTravelledPerTurn` per turn.
- Keep pathfinding inside the world map boundary.
- Route around anchor points so divisions never travel directly through an anchor center/object.
- Feed division strength into the existing `FieldDivisions` section of world strength estimation.

## Existing Project Hooks

- `EWorldFieldDivisions` already exists in `WorldStrengthTypes.h`.
- `UWorldStrengthEstimationComponent` already has `ResetFieldDivisionReport()` and `AddFieldDivisionReason()`.
- `AGeneratorWorldCampaign::AdjustDifficultyPercentagesForFieldDivisions()` already exists and is currently a stub.
- `AAnchorPoint` already stores stable anchor keys and neighbor anchors.
- `AWorldSplineBoundary` can sample the playable map boundary in XY.
- `UWorldStrategicSupportArea` is a useful reference for radius display behavior, but divisions need their own influence/movement ownership.

## Core Types

### `AWorldDivisionBase`

Base actor for all world divisions.

Designer-readable settings:

- `ERTSRadiusType HoverRadiusType`
- `float InfluenceAreaRadius`
- `float Speed`
- `float DistanceTravelledPerTurn`
- `int32 StrengthPercentage`

Runtime/code-owned settings:

- `EWorldFieldDivisions DivisionType`
- `int32 OwningPlayer`

Runtime movement state:

- Current world location.
- Pending target anchor key or target location.
- Current path anchor keys.
- Current movement waypoints.
- Current waypoint index.
- Whether a move order is still pending.

Recommended base functions:

- `SetOwningPlayer(int32 NewOwningPlayer)`
- `SetDivisionType(EWorldFieldDivisions NewDivisionType)`
- `IssueMoveOrderToAnchor(const FGuid& TargetAnchorKey)`
- `GetHasPendingMoveOrder() const`
- `MoveForTurnDistance(float DistanceBudget)`
- `ApplyWorldDivisionDamage(int32 DamagePercentage)`
- `GetCurrentStrengthPercentage() const`
- `GetInfluenceAreaRadius() const`
- `GetHoverRadiusType() const`

`ApplyWorldDivisionDamage` should be virtual. The base version validates the request and delegates the actual composition loss to the derived class.

### `AWorldTankDivision`

Derived division for armor.

Composition:

- `TArray<ETankSubtype> TankSubtypes`

The array represents both amount and type. Duplicate enum values mean several tanks of the same subtype.

Damage behavior:

- Damage removes or disables tank entries.
- Strength is recalculated from the remaining tank count compared to the starting tank count.
- Later, tank divisions can weight losses by tank class, for example damaging lighter tanks first or picking losses by weighted random selection.

### `AWorldSquadDivision`

Derived division for infantry/squads.

Composition:

- `TArray<ESquadSubtype> SquadSubtypes`

The array represents both amount and type. Duplicate enum values mean several squads of the same subtype.

Damage behavior:

- Damage removes or disables squad entries.
- Strength is recalculated from the remaining squad count compared to the starting squad count.
- Later, squad divisions can apply softer attrition, for example reducing squad strength before removing the squad entry completely.

## Strength Percentage

`StrengthPercentage` is the division's contribution when the division is at full current composition.

Recommended runtime distinction:

- `MaxStrengthPercentage`: copied from designer `StrengthPercentage` at initialization.
- `CurrentStrengthPercentage`: recalculated after damage.

Example:

- A tank division starts with `StrengthPercentage = 30`.
- It has 10 tank entries.
- It loses 2 tanks.
- `CurrentStrengthPercentage = 24`.

The current percentage is what gets applied to same-owner anchors.

## Ownership

`OwningPlayer` should not be designer-editable. It should be assigned by the system that spawns or restores the division.

Rules:

- Player divisions move during `MovePlayerDivisions`.
- Enemy divisions move during `MoveEnemyDivisions`.
- A division only supports anchors/map objects whose owner matches `OwningPlayer`.
- Neutral/unowned anchors receive no division strength.

Open implementation detail: campaign map objects need one owner accessor that all division logic can use. If map objects do not yet store ownership directly, infer it initially from object type:

- Player map object: player owner.
- Enemy map object: enemy owner.
- Mission object: probably enemy owner unless a later mission state says otherwise.
- Neutral object: no owner.

## Turn Flow

Divisions should not move continuously while the player is issuing orders. Orders only update pending path state.

At the end of the player turn:

```cpp
void MovePlayerDivisions();
```

At the end of the enemy turn:

```cpp
void MoveEnemyDivisions();
```

Both functions should:

1. Collect divisions owned by that side.
2. Skip divisions without pending move orders.
3. Ask each division to consume up to `DistanceTravelledPerTurn`.
4. Start simple interpolation for the consumed path segment.
5. Wait for all division movement animations to finish before the next turn phase begins.
6. Recalculate field-division influence after movement is complete.
7. Save campaign state.

The functions belong on a runtime system, preferably a `UWorldDivisionManager` owned by the world controller or future world turn manager. `AWorldPlayerController` can expose wrapper functions if the Blueprint turn flow needs exact entry points.

## Movement Component

Use a dedicated movement component, for example `UWorldDivisionMovementComponent`.

Responsibilities:

- Move only in XY.
- Preserve the division's map Z or use a configured map-plane Z.
- Tick only while interpolation is active.
- Interp at `Speed`.
- Consume a precomputed list of waypoints.
- Stop exactly when the turn distance budget is exhausted.
- Report movement finished to the division manager.

The movement component should not decide turn ownership or pathfinding policy. It only plays the movement it is given.

## Pathfinding

Pathfinding should be graph-first, using anchors and connections.

Logical path:

- Start from the nearest/current anchor or current division path position.
- Target a destination anchor.
- Use anchor neighbor links for path search.
- Store the path as stable anchor keys, not raw actor pointers.

Recommended first algorithm:

- Breadth-first search is enough if each connection has equal cost.
- Dijkstra can replace BFS later if connection length or terrain cost matters.

Boundary rule:

- All generated waypoints must be inside the sampled `AWorldSplineBoundary` polygon.
- If a segment would leave the polygon, the order should fail or request a fallback path.
- Do not silently move outside the global map.

Anchor avoidance rule:

- Anchor centers are logical nodes, not physical travel points.
- Movement waypoints should use standoff/offset points around each anchor.
- Intermediate anchors should be passed by an arc or two tangent points around an anchor avoidance radius.
- Final target should stop at an approach/standoff point unless the design explicitly wants the division to occupy the anchor center.

Recommended path-building steps:

1. Build `TArray<FGuid> PathAnchorKeys`.
2. Resolve keys to anchors immediately before building waypoints.
3. Convert each anchor-to-anchor edge into XY waypoints.
4. Insert avoidance waypoints around intermediate anchors.
5. Validate every waypoint and segment against the boundary polygon.
6. Store the resulting waypoints in save/runtime movement state.

## Long Movement Orders

A pending move order may require more distance than one turn allows.

Per movement pass:

- Set `RemainingDistanceBudget = DistanceTravelledPerTurn`.
- Move from the current location toward the current waypoint.
- If the waypoint is reached, subtract the segment length and continue.
- If the budget runs out mid-segment, store the interpolated location and current waypoint index.
- Keep the move order pending.
- Clear the move order only when the final waypoint is reached.

This makes long movement deterministic and resumable after save/load.

## Influence Application

After any division moves or takes damage, field-division strength should be recalculated.

Recommended flow:

1. Reset field division reports on all world map objects that can display strength.
2. For each active division:
   - Skip invalid or zero-strength divisions.
   - Find anchors/map objects with the same owner.
   - Check XY distance against `InfluenceAreaRadius`.
   - Add one `FWorldStrengthReason` to each affected target.
3. Rebuild the existing strength UI through `UWorldStrengthEstimationComponent`.

The reason text should come from data rather than hardcoded strings. Add a field-division lookup beside the existing fortification and strategic support lookups in `UWorldData`.

Suggested data extension:

- `TMap<EWorldFieldDivisions, FWorldDataStrengthReasonDefinition> FieldDivisionDefinitions`
- `FRTSPerDifficultyMlt FieldDivisionMlt`
- `TryGetFieldDivisionDefinition(...)`
- `TryBuildFieldDivisionReason(...)`

The final strength value should use the division's current strength percentage, not only the data asset's base value. If data defines display text and the actor defines strength, the calculation can use the actor percentage and the data reason text.

## Radius Display

Divisions should show their influence radius on hover using the existing radius pool subsystem.

Recommended component:

- `UWorldDivisionInfluenceComponent`

Responsibilities:

- Own hover and selected radius ids.
- Use `HoverRadiusType`.
- Use `InfluenceAreaRadius`.
- Attach radius visuals yaw-only to the division actor.
- Hide radii on end play.

This should mirror the useful behavior of `UWorldStrategicSupportArea`, while keeping division influence logic separate.

## Save Data

Add persistent division data to `FWorldCampaignState`.

Suggested struct:

```cpp
USTRUCT(BlueprintType)
struct FWorldDivisionSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FGuid DivisionKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EWorldFieldDivisions DivisionType = EWorldFieldDivisions::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 OwningPlayer = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 CurrentStrengthPercentage = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FGuid> PathAnchorKeys;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FVector> PathWaypoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 CurrentWaypointIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	bool bHasPendingMoveOrder = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ETankSubtype> TankSubtypes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ESquadSubtype> SquadSubtypes;
};
```

Only one composition array should be populated for a given division subclass.

## Spawning And Restoring

Recommended flow:

- `UWorldDivisionManager` owns live division registration.
- New campaign setup spawns initial divisions from authored setup data.
- Save/load reconstructs divisions from `FWorldDivisionSaveData`.
- Division actor classes are mapped by `EWorldFieldDivisions` in settings or data.
- Runtime references use `DivisionKey` and anchor keys rather than persistent raw actor pointers.

## Open Decisions

1. Should a division stop at a standoff point near an anchor, or can it occupy the anchor center when it reaches its final destination?
2. Should player divisions strengthen player-owned anchors only, or should they also reduce enemy mission strength when nearby?
3. Should enemy divisions strengthen missions, enemy bases, or both?
4. Should `StrengthPercentage` be fully actor-authored, fully data-driven by `EWorldFieldDivisions`, or actor-authored with data-driven UI text?
5. Should damage remove entries immediately, or should entries support partial health before removal?
6. Should pathfinding use only generated connections, or should free-space pathfinding between arbitrary points be allowed later?

## Recommended Implementation Phases

### Phase 1: Data and actors

- Add `AWorldDivisionBase`, `AWorldTankDivision`, and `AWorldSquadDivision`.
- Add designer settings and runtime ownership/type fields.
- Add composition arrays using `ETankSubtype` and `ESquadSubtype`.
- Add division save data to `FWorldCampaignState`.

### Phase 2: Division manager and turn hooks

- Add `UWorldDivisionManager`.
- Register/spawn/restore live divisions.
- Add `MovePlayerDivisions` and `MoveEnemyDivisions`.
- Call them at the end of the correct turn phases.

### Phase 3: Pathfinding and movement

- Build anchor-key pathfinding.
- Convert anchor paths to XY waypoints with anchor avoidance.
- Validate movement inside the world boundary.
- Add simple interpolation movement with per-turn distance budgeting.

### Phase 4: Influence and UI

- Add field-division data lookup to `UWorldData`.
- Apply `FWorldStrengthReason` entries through `UWorldStrengthEstimationComponent`.
- Add hover radius display for divisions.

### Phase 5: Damage behavior

- Implement tank and squad damage rules.
- Recalculate current strength after damage.
- Refresh influence after damage.
- Persist damaged composition and strength.

## Design Recommendation

Use the anchor graph as the first version of navigation. It already exists, is deterministic, and keeps divisions tied to campaign topology. Keep physical movement as visual interpolation along derived waypoints, not as the authority for pathfinding. This gives a clean separation:

- Anchor keys decide where a division is going.
- Waypoints decide how it visually travels there.
- The movement component only interpolates.
- The manager decides when movement is allowed.
- Strength estimation decides what the player sees.
