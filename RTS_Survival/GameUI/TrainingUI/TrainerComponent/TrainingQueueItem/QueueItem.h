#pragma once

#include "CoreMinimal.h"
#include "EAsyncTrainingRequestState.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "QueueItem.generated.h"

enum class ERTSResourceType : uint8;

USTRUCT()
struct FTrainingQueueItem
{
	GENERATED_BODY()

	// Identifies what is trained.
	FTrainingOption ItemID = FTrainingOption();

	// The total amount of time it takes to train this unit.
	int32 TotalTrainingTime = 0;

	// How long the training will take.
	int32 RemainingTrainingTime = 0;

	// The index of the widget in the training menu.
	int32 WidgetIndex = -1;

	// Whether the spawning process for the unit that is trained by this item has started.
	EAsyncTrainingRequestState AsyncRequestState = EAsyncTrainingRequestState::Async_NoTrainingRequest;

	// The total cost of this training.
	TMap<ERTSResourceType, int32> TotalResourceCosts = {};

	// The remaining cost of this training.
	TMap<ERTSResourceType, int32> RemainingResourceCosts = {};

	// Key into the trainer's live requirement snapshot map (or INDEX_NONE if no requirement).
	int32 RequirementKey = INDEX_NONE;
};
