// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavModifierComponent.h"

#include "WallGateNavigation.generated.h"

namespace RTSWallSides
{
	// Queries without the RTS filter see a breachable wall at an obstacle's cost (1e6 per cm): a path that costs this
	// much went through an intact wall that nobody using that query may break (an enclosed Soviet base). Plain ground
	// costs 1 per cm, so no real path comes near it. Callers treat such a path as no path.
	inline constexpr double WallCrossingPathCost = 1.0e6;
	inline bool GetDoesPathCrossIntactWall(const double PathCost)
	{
		return PathCost >= WallCrossingPathCost;
	}

	// The human player; every other owning player is the AI side (UGameUnitManager splits units the same way).
	inline constexpr uint8 HumanPlayerIndex = 1;
}

/**
 * @brief Doorway area of a closed wall gate whose allowed player is player 1: only units of player 1 and its allies
 * plan through it. Only URTSWallGateNavQueryFilter tells the players apart; any other filter treats it as plain ground.
 * There is one doorway area class per player (RTSWallGateNavAreas::GetClosedGateAreaOfPlayer).
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_WallGatePlayerSide : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_WallGatePlayerSide(const FObjectInitializer& ObjectInitializer);
};

/**
 * @brief Doorway area of a closed wall gate whose allowed player is player 2 (the default of the Soviet wall gates):
 * only units of player 2 and its allies plan through it. The name predates four players and is kept, so saved
 * navigation data and two-player games stay as they were.
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_WallGateEnemySide : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_WallGateEnemySide(const FObjectInitializer& ObjectInitializer);
};

/** @brief Doorway area of a closed wall gate whose allowed player is player 3: player 3 and its allies plan through. */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_WallGatePlayer3 : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_WallGatePlayer3(const FObjectInitializer& ObjectInitializer);
};

/** @brief Doorway area of a closed wall gate whose allowed player is player 4: player 4 and its allies plan through. */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_WallGatePlayer4 : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_WallGatePlayer4(const FObjectInitializer& ObjectInitializer);
};

namespace RTSWallGateNavAreas
{
	/**
	 * @brief A closed gate marks its doorway with the area of its allowed player; a query filter lets a unit through
	 * the areas of its own player and its allies only, so one fixed area per player covers every team setup.
	 * @param Player Allowed player of the gate; unset owners (222) count as player 2, like everywhere else.
	 * @return The doorway area of that player, or null for the neutral owner (no player may pass).
	 */
	RTS_SURVIVAL_API TSubclassOf<UNavArea> GetClosedGateAreaOfPlayer(int32 Player);
}

/**
 * @brief A breachable wall of player 1 (UWallTankBreachComponent). Only the human player's tanks break walls, so no
 * unit plans through it; it exists so player-owned breachable walls block like the AI's do for the AI.
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_BreachableWallPlayerOwned : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_BreachableWallPlayerOwned(const FObjectInitializer& ObjectInitializer);
};

/**
 * @brief A wall of the AI side (the Soviet walls): only player 1's medium and heavy tanks may plan through it; the
 * wall's own side never drives through (or breaks) its own wall. Its default cost is an obstacle's, so pathfinding
 * without the RTS filters (convoy routes, planners) goes round; the filter lowers it for the tanks that may breach.
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_BreachableWallEnemyOwned : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_BreachableWallEnemyOwned(const FObjectInitializer& ObjectInitializer);
};

/**
 * @brief A wall only heavy tanks break (the Berlin ruin walls, unowned): only the human player's heavy tanks plan
 * through it; mediums, light vehicles and infantry go round. An obstacle's cost for unfiltered queries.
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_BreachableWallHeavyOnly : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_BreachableWallHeavyOnly(const FObjectInitializer& ObjectInitializer);
};

/**
 * @brief An obstacle every vehicle drives through, whoever owns it (wire fences): light, medium and heavy vehicles of
 * every player plan through it; infantry go round. Plain ground for unfiltered queries and for
 * URTSClosedBreachableWallsQueryFilter (a fence never closes a route for a planner).
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_BreachableAnyVehicle : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_BreachableAnyVehicle(const FObjectInitializer& ObjectInitializer);
};

/**
 * @brief A breachable obstacle without sides that only medium and heavy vehicles break (concrete fences): medium and
 * heavy vehicles of every player plan through it; light vehicles and infantry go round. Plain ground for unfiltered
 * queries and planners, as the fences' old BlockCharLight area was.
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_BreachableAnySideMediumHeavy : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_BreachableAnySideMediumHeavy(const FObjectInitializer& ObjectInitializer);
};

/**
 * @brief Parent of the RTS_NavFilter_* Blueprints so each unit plans only through closed wall gates of its own player
 * and its allies; built per querier (AI controller -> pawn -> URTSComponent owning player), the doorway areas of every
 * other player are excluded. Relations are read when the filter is built (each new path request); a path planned
 * before a team change keeps its filter, also on repaths, until the unit gets its next move.
 * Queries without a known unit, and units of the neutral owner, treat every closed gate as closed.
 * @note Reparent RTS_NavFilter_Characters / _LightTanks / _MediumTanks / _HeavyTanks onto this class; until then wall
 * gates fall back to blocking everyone while closed.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API URTSWallGateNavQueryFilter : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	URTSWallGateNavQueryFilter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier,
	                              FNavigationQueryFilter& Filter) const override;

private:
	/**
	 * @brief The querier is an AI controller for unit moves, the actor itself for direct queries.
	 * @param Querier Object that asked for the path; may be null or the world.
	 * @param OutOwningPlayer Owning player of the unit behind the querier.
	 * @return False when no unit with an RTS component stands behind the querier.
	 */
	static bool TryGetQuerierOwningPlayer(const UObject* Querier, uint8& OutOwningPlayer);

	/**
	 * @brief Breachable walls: only the other side's medium and heavy tanks plan through them.
	 * @param NavData Navigation data the filter is built for.
	 * @param Querier Object that asked for the path.
	 * @param bHasKnownSide False when no unit stands behind the querier (every breachable wall is closed then).
	 * @param bIsHumanPlayer The querier's unit belongs to player 1.
	 * @param Filter Filter to exclude the closed areas in.
	 */
	static void ExcludeBreachableWalls(const ANavigationData& NavData, const UObject* Querier, bool bHasKnownSide,
	                                   bool bIsHumanPlayer, FNavigationQueryFilter& Filter);

	/**
	 * @brief Closed gates: the querier plans through the doorway areas of its own player and its allies only.
	 * @param NavData Navigation data the filter is built for.
	 * @param bHasKnownSide False when no unit stands behind the querier (every closed gate is closed then).
	 * @param QuerierPlayer Owning player of the querier's unit.
	 * @param Filter Filter to exclude the closed doorway areas in.
	 */
	static void ExcludeClosedGatesOfNonAllies(const ANavigationData& NavData, bool bHasKnownSide, uint8 QuerierPlayer,
	                                          FNavigationQueryFilter& Filter);
	static bool GetIsQuerierMediumOrHeavyTank(const UObject* Querier);
	static bool GetIsQuerierHeavyTank(const UObject* Querier);
	static bool GetIsQuerierVehicle(const UObject* Querier);
	// Lets the querier through a breachable area: plain ground plus the entering cost (the area's own cost blocks).
	static void OpenBreachableArea(const ANavigationData& NavData, const UClass* AreaClass, FNavigationQueryFilter& Filter,
	                               float TankCost, float EnteringCost);

	static void ExcludeArea(const ANavigationData& NavData, const UClass* AreaClass, FNavigationQueryFilter& Filter);
};

/**
 * @brief For planners that ask for paths without a unit (strategic AI waypoints, convoy routes, survival reach checks,
 * wanderers): every breachable wall is closed, everything else keeps the navmesh's defaults. Without it such a query
 * sees an intact wall at an obstacle's cost and routes through an enclosed base. Pass the class as the query filter.
 */
UCLASS()
class RTS_SURVIVAL_API URTSClosedBreachableWallsQueryFilter : public UNavigationQueryFilter
{
	GENERATED_BODY()

protected:
	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier,
	                              FNavigationQueryFilter& Filter) const override;
};

/**
 * @brief Nav modifier over explicit oriented boxes instead of the owner's collision, which a wall gate switches off
 * while open. AWallGate owns two: the doorway (UWallGateNavModifier) and the frame (UWallGateFrameNavModifier).
 */
UCLASS(Abstract)
class RTS_SURVIVAL_API UWallGateNavModifierBase : public UNavModifierComponent
{
	GENERATED_BODY()

public:
	UWallGateNavModifierBase(const FObjectInitializer& ObjectInitializer);

protected:
	/**
	 * @brief Adds one box to the modifier and its world bounds.
	 * @param LocalBox Box in the space of LocalToWorld.
	 * @param LocalToWorld The mesh component's world transform (its scale is applied to the box).
	 * @param WorldPadding Grows the box by this much on every side, after scaling (cm).
	 */
	void AddOrientedBox(const FBox& LocalBox, const FTransform& LocalToWorld, float WorldPadding) const;

	// A valid, tiny box keeps the navigation octree happy while there is nothing to mark.
	void SetEmptyBounds() const;
};

/**
 * @brief Marks a wall gate's doorway (the gate leaf's rest-pose bounds, thickened across the wall) as one navigation
 * area; AWallGate sets the area with SetAreaClass as the gate opens and closes.
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API UWallGateNavModifier : public UWallGateNavModifierBase
{
	GENERATED_BODY()

public:
	UWallGateNavModifier(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void CalculateBounds() const override;
};

/**
 * @brief Blocks navigation (NavArea_Null) at the wall gate's frame: one box per simple collision element of the frame
 * mesh, padded, skipping elements above the ground such as a lintel, so the doorway stays free for GateNavModifier.
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API UWallGateFrameNavModifier : public UWallGateNavModifierBase
{
	GENERATED_BODY()

public:
	UWallGateFrameNavModifier(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void CalculateBounds() const override;
};
