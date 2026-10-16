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

	// The index of this item's option in the trainer's training options, counted over all sub sections.
	// The training menu turns it into a widget of the sub section that is on screen.
	int32 WidgetIndex = -1;

	// Whether the spawning process for the unit that is trained by this item has started.
	EAsyncTrainingRequestState AsyncRequestState = EAsyncTrainingRequestState::Async_NoTrainingRequest;

	// The total cost of this training.
	TMap<ERTSResourceType, int32> TotalResourceCosts = {};

	// The remaining cost of this training.
	TMap<ERTSResourceType, int32> RemainingResourceCosts = {};

	// Key into the trainer's live requirement snapshot map (or INDEX_NONE if no requirement).
	int32 RequirementKey = INDEX_NONE;

	// Key into the trainer's planned field defenses; INDEX_NONE for items that train a unit.
	// A field defense item builds its expansion at the planned spot instead of spawning a unit at the trainer.
	int32 FieldDefenseId = INDEX_NONE;

	bool GetIsFieldDefense() const { return FieldDefenseId != INDEX_NONE; }
};
