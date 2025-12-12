#pragma once

#include "CoreMinimal.h"

#include "TimeProgressBarState.generated.h"

USTRUCT()
struct FPauseStateTimedProgressBar
{
	GENERATED_BODY()

    // Flag to indicate if the progress bar is paused
    bool bIsPaused;

    // Time when the progress bar was paused
    float M_PausedTime;

    // Accumulated paused duration
    float M_TotalPausedDuration;

	bool bM_WasHiddenByPause = false;
	
};
