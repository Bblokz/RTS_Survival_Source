// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavModifierComponent.h"

#include "WallGateNavigation.generated.h"

namespace RTSWallSides
{
	// The human player; every other owning player is the AI side (UGameUnitManager splits units the same way).
	inline constexpr uint8 HumanPlayerIndex = 1;
}

/**
 * @brief Doorway area of a closed wall gate that only player 1's units may plan through (gate AllowedPlayer == 1).
 * Only URTSWallGateNavQueryFilter tells the sides apart; any other filter treats it as plain ground.
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_WallGatePlayerSide : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_WallGatePlayerSide(const FObjectInitializer& ObjectInitializer);
};

/**
 * @brief Doorway area of a closed wall gate that only the AI side's units (any player but 1) may plan through.
 * The default for Soviet wall gates (AllowedPlayer 2).
 */
UCLASS(Config=Engine)
class RTS_SURVIVAL_API UNavArea_WallGateEnemySide : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_WallGateEnemySide(const FObjectInitializer& ObjectInitializer);
};

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
 * @brief Parent of the RTS_NavFilter_* Blueprints so each unit plans through closed wall gates of its own side only;
 * built per querier (AI controller -> pawn -> URTSComponent owning player), the other side's gate area is excluded.
 * Queries without a known unit treat every closed gate as closed.
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
	static bool GetIsQuerierMediumOrHeavyTank(const UObject* Querier);
	static bool GetIsQuerierHeavyTank(const UObject* Querier);
	// Lets the querier through a breachable area: plain ground plus the entering cost (the area's own cost blocks).
	static void OpenBreachableArea(const ANavigationData& NavData, const UClass* AreaClass, FNavigationQueryFilter& Filter,
	                               float EnteringCost);

	static void ExcludeArea(const ANavigationData& NavData, const UClass* AreaClass, FNavigationQueryFilter& Filter);
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
