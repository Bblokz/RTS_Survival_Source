# World Divisions Design

World divisions are campaign-map actors that represent mobile combat strength. They move only during turn-end movement passes, can be damaged, and project field-division strength through their influence radius.

This system should live in the world campaign runtime layer, not in `AGeneratorWorldCampaign`. The generator builds the campaign map and anchor actors; divisions use the playable map space after generation/load, with anchors treated as avoidance obstacles rather than navigation nodes.

## Goals

- Let designers place or spawn tank and squad divisions with readable movement, influence, and strength settings.
- Keep runtime identity and ownership controlled by code/save data.
- Move divisions only in XY on the flat global map.
- Move divisions only at the end of their owning side's turn.
- Move orders target arbitrary world points, not anchors.
- Allow long orders to continue over several turns by consuming at most `DistanceTravelledPerTurn` per turn.
- Keep pathfinding inside the world map boundary.
- Route around anchor points so divisions never travel directly through an anchor center/object.
- Use a proper Unreal path made of world-space path points, not an anchor graph path.
- Feed division strength into the existing `FieldDivisions` section of world strength estimation.

## Existing Project Hooks

- `EWorldFieldDivisions` already exists in `WorldStrengthTypes.h`.
- `UWorldStrengthEstimationComponent` already has `ResetFieldDivisionReport()` and `AddFieldDivisionReason()`.
- `AGeneratorWorldCampaign::AdjustDifficultyPercentagesForFieldDivisions()` already exists and is currently a stub.
- `AAnchorPoint` already provides anchor actor locations that can be treated as avoidance obstacles.
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

Data-loaded settings cached on the division:

- `int32 StrengthPercentage`

Runtime/code-owned settings:

- `EWorldFieldDivisions DivisionType`
- `int32 OwningPlayer`

Runtime movement state:

- Current world location.
- Pending target world point.
- Current Unreal path points.
- Current path point index.
- Whether a move order is still pending.

Recommended base functions:

- `SetOwningPlayer(int32 NewOwningPlayer)`
- `SetDivisionType(EWorldFieldDivisions NewDivisionType)`
- `IssueMoveOrderToPoint(const FVector& TargetPoint)`
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

- Damage removes entire tank entries.
- Strength is recalculated from the remaining tank count compared to the starting tank count.
- Later, tank divisions can weight losses by tank class, for example damaging lighter tanks first or picking losses by weighted random selection.

### `AWorldSquadDivision`

Derived division for infantry/squads.

Composition:

- `TArray<ESquadSubtype> SquadSubtypes`

The array represents both amount and type. Duplicate enum values mean several squads of the same subtype.

Damage behavior:

- Damage removes entire squad entries.
- Strength is recalculated from the remaining squad count compared to the starting squad count.

## Strength Percentage

`StrengthPercentage` is the division's contribution when the division is at full current composition. It should be authored in a dedicated field-division section on `UWorldData`, then cached on the division unit when the division is spawned or restored.

Recommended runtime distinction:

- `MaxStrengthPercentage`: loaded from `UWorldData` by `EWorldFieldDivisions` and cached on the division.
- `CurrentStrengthPercentage`: recalculated after damage.
- `StrengthReasonText`: loaded from the same `UWorldData` section so the UI text is data-driven.

Example:

- WorldData defines `SovietMediumArmorDivision` with `StrengthPercentage = 30`.
- A tank division of that type caches `MaxStrengthPercentage = 30`.
- It has 10 tank entries.
- It loses 2 tanks.
- `CurrentStrengthPercentage = 24`.

The current percentage is what gets applied by influence calculations. Save data should persist current strength and current composition, while WorldData remains the source for max/default strength and player-facing reason text.

## Ownership

`OwningPlayer` should not be designer-editable. It should be assigned by the system that spawns or restores the division.

Rules:

- Player divisions move during `MovePlayerDivisions`.
- Enemy divisions move during `MoveEnemyDivisions`.
- Player divisions strengthen player-owned anchors/map objects inside influence range.
- Player divisions also reduce enemy mission strength when nearby.
- Enemy divisions strengthen enemy bases and enemy mission locations inside influence range.
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
- Consume a precomputed list of path points.
- Stop exactly when the turn distance budget is exhausted.
- Report movement finished to the division manager.

The movement component should not decide turn ownership or pathfinding policy. It only plays the movement it is given.

## Pathing Settings

Add shared project settings for world division pathing. These should be constant project-level settings, not per-division knobs.

Recommended location:

- `UWorldCampaignSettings`

Required setting:

- `float WorldDivisionAnchorAvoidanceRadius`

Purpose:

- Defines how far divisions must path around anchor points.
- Used when projecting final target points away from anchors.
- Used when building navigation obstacles or post-processing path segments around anchors.
- Applies to all world divisions so pathing behavior stays consistent.

Optional supporting settings:

- `float WorldDivisionBoundaryProjectionPadding`
- `int32 WorldDivisionPathRepairMaxAttempts`

## Pathfinding

Pathfinding should target an arbitrary world point and return a proper Unreal path made of world-space path points.

Important rule:

- Do not use anchors as navigation nodes.
- Do not use the generated anchor graph as the movement path.
- Anchors are obstacles to route around.
- Divisions can move freely across valid campaign-map space as long as the path stays inside the world boundary and avoids anchor exclusion circles.

Recommended implementation:

- Use Unreal navigation/path queries for the campaign map movement surface.
- If Recast navigation is available, request paths through `UNavigationSystemV1` and consume the returned path points.
- If the world campaign map does not use a standard Recast NavMesh, provide a world-map navigation adapter that still returns Unreal-style path points.
- The path request should be from the division's current world location to the requested target point.
- Store the resulting `TArray<FVector>` path points, not anchor keys.

Anchor avoidance:

- Each `AAnchorPoint` contributes an avoidance circle in XY.
- The avoidance circle radius is `WorldDivisionAnchorAvoidanceRadius`.
- Pathing must not generate a segment that passes through an anchor avoidance circle.
- If the requested target point is inside an anchor avoidance circle, project the final target to the nearest valid standoff point on or outside that circle.
- The division should use a standoff for anchor-adjacent destinations; it should not occupy the anchor center.
- Long-term, anchors should be exposed to Unreal navigation as obstacles or nav modifiers.
- First implementation may also post-process the returned path and insert detour points around any anchor avoidance circle that a path segment crosses.

Boundary rule:

- All generated path points must be inside the sampled `AWorldSplineBoundary` polygon.
- Do not silently move outside the global map.
- If a segment would leave the polygon, find the first point where the segment exits the polygon.
- Project that exit point back inside the polygon, using `WorldDivisionBoundaryProjectionPadding`.
- Try to repair the path by routing around that projected point and continuing toward the original target.
- Revalidate the repaired path.
- If repair fails after `WorldDivisionPathRepairMaxAttempts`, reject the move order and report the pathing failure.

Recommended path-building steps:

1. Clamp or reject the requested target if it is outside the map boundary.
2. Project the target to a standoff point if it is inside an anchor avoidance circle.
3. Request an Unreal navigation path from the current division location to the target point.
4. Post-process path segments against anchor avoidance circles if anchors are not already represented in nav data.
5. Validate each path segment against the `AWorldSplineBoundary` polygon.
6. When a segment exits the polygon, project the first exit point back inside and repair the path around that projected point.
7. Store the final path as `TArray<FVector> PathPoints`.

Path authority:

- The target is a point.
- The authoritative path is a world-space Unreal path.
- Anchor keys are not part of movement state.
- Anchors only influence pathing as avoidance obstacles.

## Long Movement Orders

A pending move order may require more distance than one turn allows.

Per movement pass:

- Set `RemainingDistanceBudget = DistanceTravelledPerTurn`.
- Move from the current location toward the current path point.
- If the path point is reached, subtract the segment length and continue.
- If the budget runs out mid-segment, store the interpolated location and current path point index.
- Keep the move order pending.
- Clear the move order only when the final path point is reached.

This makes long movement deterministic and resumable after save/load.

## Influence Application

After any division moves or takes damage, field-division strength should be recalculated.

Recommended flow:

1. Reset field division reports on all world map objects that can display strength.
2. For each active division:
   - Skip invalid or zero-strength divisions.
   - Check XY distance against `InfluenceAreaRadius`.
   - For player divisions, add positive strength to player-owned anchors/map objects in range.
   - For player divisions, add negative strength to enemy mission objects in range.
   - For enemy divisions, add positive strength to enemy bases in range.
   - For enemy divisions, add positive strength to enemy mission objects in range.
3. Rebuild the existing strength UI through `UWorldStrengthEstimationComponent`.

The reason text and default strength percentage should come from data rather than hardcoded strings. Add a dedicated field-division section beside the existing fortification and strategic support lookups in `UWorldData`.

Suggested data extension:

- `TMap<EWorldFieldDivisions, FWorldDataStrengthReasonDefinition> FieldDivisionDefinitions`
- `FRTSPerDifficultyMlt FieldDivisionMlt`
- `TryGetFieldDivisionDefinition(...)`
- `TryBuildFieldDivisionReason(...)`

The final strength value should use the division's cached `CurrentStrengthPercentage`. WorldData supplies the default/max strength and UI reason text. Damage and attrition modify the cached current value on the division.

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
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FVector> PathPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 CurrentPathPointIndex = 0;

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
- Runtime references use `DivisionKey` and saved world-space path points rather than persistent raw actor pointers.

## Resolved Decisions

- Divisions move to arbitrary world points, not anchors.
- Divisions do not use the anchor graph for navigation.
- Anchors are avoidance obstacles.
- Divisions stop at standoff points near anchors and do not occupy anchor centers.
- Player divisions strengthen player-owned anchors/map objects and reduce enemy mission strength when nearby.
- Enemy divisions strengthen both enemy bases and enemy mission locations.
- `StrengthPercentage` is loaded from a dedicated `UWorldData` field-division section, then cached on the division unit.
- Boundary-escaping path segments are repaired by projecting the first exit point back inside the boundary and trying to route around that projected point.
- Damage removes entire tank or squad entries; entries do not support partial health before removal.

## Recommended Implementation Phases

### Phase 1: Data and actors

- Add `AWorldDivisionBase`, `AWorldTankDivision`, and `AWorldSquadDivision`.
- Add designer settings and runtime ownership/type fields.
- Add `WorldDivisionAnchorAvoidanceRadius` to project/world campaign settings.
- Add field-division strength definitions to `UWorldData`.
- Add composition arrays using `ETankSubtype` and `ESquadSubtype`.
- Add division save data to `FWorldCampaignState`.

### Phase 2: Division manager and turn hooks

- Add `UWorldDivisionManager`.
- Register/spawn/restore live divisions.
- Add `MovePlayerDivisions` and `MoveEnemyDivisions`.
- Call them at the end of the correct turn phases.

### Phase 3: Pathfinding and movement

- Build point-targeted Unreal path requests.
- Represent anchors as pathing obstacles or post-process paths against anchor avoidance circles.
- Add standoff projection for target points near anchors.
- Validate and repair path segments against the world boundary polygon.
- Project boundary exits back inside and route around the projected point.
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

Use point-targeted Unreal pathfinding as the first version of navigation. Anchors should influence movement only as avoidance obstacles, with a shared project setting controlling how far divisions route around them. Keep physical movement as visual interpolation along validated path points, not as the authority for pathfinding. This gives a clean separation:

- Target points decide where a division is going.
- Unreal path points decide how it visually travels there.
- Anchor actors provide obstacle locations and avoidance circles.
- The boundary polygon keeps path points inside the global map.
- The movement component only interpolates.
- The manager decides when movement is allowed.
- Strength estimation decides what the player sees.
