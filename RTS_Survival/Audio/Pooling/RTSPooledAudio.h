// RTSPooledAudio.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSPooledAudio.generated.h"

/**
 * @brief Invisible owner actor that contains pooled UAudioComponents for spatial voice lines.
 * Spawned and owned by the player's UPlayerAudioController and destroyed in EndPlay.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSPooledAudio : public AActor
{
	GENERATED_BODY()

public:
	ARTSPooledAudio();

protected:
	virtual bool ShouldTickIfViewportsOnly() const override { return false; }
};
