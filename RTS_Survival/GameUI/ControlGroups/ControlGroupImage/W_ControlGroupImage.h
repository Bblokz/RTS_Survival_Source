// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "W_ControlGroupImage.generated.h"

struct FTrainingOption;
enum class EAllUnitType : uint8;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_ControlGroupImage : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	void SetupControlGroupIndex(const int32 GroupIndex);

	UFUNCTION(BlueprintImplementableEvent)
	void OnUpdateControlGroup(const FTrainingOption MostFrequentUnitInGroup);
};
