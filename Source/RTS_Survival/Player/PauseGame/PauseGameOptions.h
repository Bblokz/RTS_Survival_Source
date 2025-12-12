#pragma once

#include "CoreMinimal.h"
#include "NiagaraStatelessBuiltDistribution.h"

#include "PauseGameOptions.generated.h"

UENUM(BlueprintType)
enum class ERTSPauseGameOptions : uint8
{
	// Pauses / unpauses depending on current pause-state.
	FlipFlop,
	ForcePause,
	ForceUnpause
};
