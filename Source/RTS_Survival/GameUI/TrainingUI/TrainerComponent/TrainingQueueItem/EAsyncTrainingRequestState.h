#pragma once

#include "CoreMinimal.h"

#include "EAsyncTrainingRequestState.generated.h"

// Keeps track of wheter a queue item in training has made a request to spawn its asset.
UENUM()
enum EAsyncTrainingRequestState
{
	Async_NoTrainingRequest,
	Async_LoadingAsset,
	Async_TrainingSpawned,
	Async_SpawningFailed
};
