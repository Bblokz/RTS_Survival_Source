// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "W_ActionUIDescription.generated.h"

class UW_CostDisplay;
/**
 * @brief Used by action-button hover UI to display metadata shared by every action description blueprint.
 */
UCLASS()
class RTS_SURVIVAL_API UW_ActionUIDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetCostsForAbility(const TMap<ERTSResourceType, int32>& ResourceCosts) const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_CostDisplay> M_CostDisplay = nullptr;

	bool EnsureIsValidCostDisplay() const;
};
