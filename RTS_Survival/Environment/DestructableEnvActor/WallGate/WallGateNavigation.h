// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavModifierComponent.h"

#include "WallGateNavigation.generated.h"

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
