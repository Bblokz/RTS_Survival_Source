// RTSSpatialAudioSettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSSpatialAudioSettings.generated.h"

class USoundAttenuation;

/**
 * @brief Project settings for the pooled spatial voice-line system.
 * Configure the default attenuation and pool size for spatial voice-line audio.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Spatial Audio"))
class RTS_SURVIVAL_API URTSSpatialAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSSpatialAudioSettings();

	/** Default attenuation asset used when a spatial voice-line player does not provide an override. */
	UPROPERTY(Config, EditAnywhere, Category="Pooled Spatial Audio")
	TSoftObjectPtr<USoundAttenuation> DefaultSpatialAttenuation;

	/** Number of pooled UAudioComponents created for spatial voice-lines. */
	UPROPERTY(Config, EditAnywhere, Category="Pooled Spatial Audio", meta=(ClampMin="1", UIMin="1"))
	int32 DefaultPoolSize = 16;
};
