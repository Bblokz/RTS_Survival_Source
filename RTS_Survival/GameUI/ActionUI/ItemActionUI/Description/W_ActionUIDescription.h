// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_ActionUIDescription.generated.h"

enum class ERTSResourceType : uint8;
class UW_CostDisplay;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_ActionUIDescription : public UUserWidget
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable)
	void SetCostsForAbility(const TMap<ERTSResourceType, int32>& ResourceCosts) const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_CostDisplay* M_CostDisplay = nullptr;

	bool EnsureIsValidCostDisplay() const;
};
