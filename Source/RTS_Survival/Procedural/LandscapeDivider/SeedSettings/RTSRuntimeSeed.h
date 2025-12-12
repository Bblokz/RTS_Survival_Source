#pragma once

#include "CoreMinimal.h"

#include "RTSRuntimeSeed.generated.h"

// Keeps track of the random stream for placed objects, gets initialised by the
// Landscape Divider.
USTRUCT()
struct FRTSRuntimeSeed
{
	GENERATED_BODY()

	UPROPERTY()
	FRandomStream RandomStreamForPlacedObjects;

	UPROPERTY()
	bool bIsInitialised = false;
};
