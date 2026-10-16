// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainingQueueItem/QueueItem.h"

#include "FieldDefensePlacementRequest.generated.h"

class UTrainerComponent;
class URTSRequirement;

/**
 * @brief Kept by the player controller while the player places a field defense picked in a trainer's training
 * menu; the placement click turns it into a queue item of that trainer.
 */
USTRUCT()
struct FFieldDefensePlacementRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UTrainerComponent> Trainer;

	// Costs, training time and widget index of the option; the trainer adds the requirement snapshot.
	UPROPERTY()
	FTrainingQueueItem TrainingItem;

	// The requirement of the option as the training menu manager keeps it; duplicated per queued item.
	UPROPERTY()
	TWeakObjectPtr<URTSRequirement> RequirementTemplate;

	UPROPERTY()
	EBuildingExpansionType BxpType = EBuildingExpansionType::BXT_Invalid;

	void Reset() { *this = FFieldDefensePlacementRequest(); }
};
