// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_EnergyBarInfo.generated.h"

class URichTextBlock;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_EnergyBarInfo : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSupplyDemand(const int32 EnergySupply, const int32 EnergyDemand);

protected:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* RichText;

private:
	int32 M_EnergySupply = 0;
	int32 M_EnergyDemand = 0;
};
