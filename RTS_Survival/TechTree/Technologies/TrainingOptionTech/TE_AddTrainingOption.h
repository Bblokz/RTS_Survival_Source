// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "TE_AddTrainingOption.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_AddTrainingOption : public UTechnologyEffect
{
	GENERATED_BODY()


protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSet<TSubclassOf<AActor>> M_AffectedActors;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSet<FTrainingOption> AddedTrainingOptions;
};
