// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FieldTrenchAnchorComponent.h"

#include "FieldTrenchAnchorSubsystem.generated.h"

/**
 * @brief Lists the trench anchors that are available in the world, so the trench placement finds the one near
 * the cursor. A world holds a few dozen at most, so they are searched one by one.
 */
UCLASS()
class RTS_SURVIVAL_API UFieldTrenchAnchorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterAnchors(UFieldTrenchAnchorComponent& AnchorComponent);
	void UnregisterAnchors(UFieldTrenchAnchorComponent& AnchorComponent);

	/**
	 * @param Location Where to look, in the ground plane.
	 * @param Radius How far from the location the anchor may be.
	 * @param OwningPlayer Only anchors of this player's actors are found.
	 * @return The nearest anchor no trench has taken; unset when none is that close.
	 */
	FFieldTrenchAnchorHandle FindFreeAnchorNear(const FVector2D& Location, const float Radius,
	                                            const int32 OwningPlayer) const;

	/**
	 * @brief Lists every anchor of the player that no trench has taken, so the enemy AI can dig out of its pits.
	 * @param OwningPlayer Only anchors of this player's actors are listed.
	 * @param OutAnchors Receives the free anchors; emptied first.
	 */
	void GetFreeAnchorsOfPlayer(const int32 OwningPlayer, TArray<FFieldTrenchAnchorHandle>& OutAnchors) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<UFieldTrenchAnchorComponent>> M_AnchorComponents;
};
