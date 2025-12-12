// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_TrainingRequirement.generated.h"

struct FTrainingWidgetState;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_TrainingRequirement : public UUserWidget
{
	GENERATED_BODY()

public:
	void UpdateRequirement(const FTrainingWidgetState& TrainingWidgetState);

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnUpdateRequirement(const FTrainingWidgetState& TrainingWidgetState);
};
