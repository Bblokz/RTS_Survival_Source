// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyItemSiteTypes.h"
#include "RTS_Survival/Enemy/ConvoyPressure/DormantDefence/DormantDefenceSlot.h"
#include "EnemyItemStrengthSlot.generated.h"

/**
 * @brief A dormant defence on a handmade enemy item map that AEnemyItemSiteDirector switches on when its Condition
 * holds (a world map fortification, a total strength, item types). The rest is ADormantDefenceSlot.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AEnemyItemStrengthSlot : public ADormantDefenceSlot
{
	GENERATED_BODY()

public:
	bool GetShouldActivateForSite(const FEnemyItemSiteContext& SiteContext) const;

	virtual FColor GetPlacementColor() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strength Slot")
	FEnemyItemStrengthSlotCondition Condition;

protected:
	virtual FString GetPlacementKindName() const override { return TEXT("STRENGTH"); }
};
