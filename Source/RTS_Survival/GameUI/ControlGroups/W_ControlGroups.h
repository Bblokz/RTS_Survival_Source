// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_ControlGroups.generated.h"

class UW_ControlGroupImage;
struct FTrainingOption;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_ControlGroups : public UUserWidget
{
	GENERATED_BODY()


public:
	void UpdateMostFrequentUnitInGroup(const int32 GroupIndex, const FTrainingOption TrainingOption);


protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Init")
	void InitControlGroups(TArray<UW_ControlGroupImage*> ControlGroupWidgets);


private:
	UPROPERTY()
	TArray<TObjectPtr<UW_ControlGroupImage>> M_ControlGroupWidgets;
};
