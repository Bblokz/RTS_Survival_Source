// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/QueueHelpers/RTSQueueHelpers.h"
#include "UObject/Interface.h"
#include "Trainer.generated.h"


// This class does not need to be modified.
UINTERFACE()
class UTrainer : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API ITrainer
{
	GENERATED_BODY()

	friend class UTrainerComponent;

public:
	virtual UTrainerComponent* GetTrainerComponent() =0;

	FString GetTrainerName();

protected:
	virtual void OnTrainingItemAddedToQueue(const FTrainingOption& AddedOption) = 0;
	virtual void OnTrainingProgressStartedOrResumed(const FTrainingOption& StartedOption) =0;
	virtual void OnTrainingCancelled(const FTrainingOption& CancelledOption, const bool bRemovedActiveItem) =0;
	virtual void OnTrainingComplete(const FTrainingOption& CompletedOption, AActor* TrainedUnit) =0;
};
