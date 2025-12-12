// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_CostDisplay.generated.h"

class URichTextBlock;
enum class ERTSResourceType : uint8;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_CostDisplay : public UUserWidget
{
	GENERATED_BODY()


public:
	void SetupCost(const TMap<ERTSResourceType, int32>& ResourceCosts);


protected:
	UPROPERTY(meta = (BindWidget),BlueprintReadWrite)
	URichTextBlock* CostTextBlock;

	UPROPERTY(meta = (BindWidget),BlueprintReadWrite)
	URichTextBlock* BlueprintTextBlock;



private:
	FString GetCostText(const TMap<ERTSResourceType, int32>& ResourceCosts, const bool bIsBlueprintCost) const;

	UPROPERTY()
	TMap<ERTSResourceType, int32> M_ResourceCostsMap;
	UPROPERTY()
	float M_TimeCost;
};
