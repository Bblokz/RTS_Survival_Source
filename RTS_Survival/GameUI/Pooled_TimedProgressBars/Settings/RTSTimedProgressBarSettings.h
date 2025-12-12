#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSTimedProgressBarSettings.generated.h"

class UW_RTSTimedProgressBar;

/**
 * @brief Project settings for the timed progress bar system.
 * Configure the widget class (Blueprint derived from UW_RTSTimedProgressBar) and pool size.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Timed Progress Bar"))
class RTS_SURVIVAL_API URTSTimedProgressBarSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSTimedProgressBarSettings();

	/** Blueprint class derived from UW_RTSTimedProgressBar that will be spawned into the pool. */
	UPROPERTY(Config, EditAnywhere, Category="Timed Progress Bar")
	TSoftClassPtr<UW_RTSTimedProgressBar> WidgetClass;

	/** Default pool size created on world load. */
	UPROPERTY(Config, EditAnywhere, Category="Timed Progress Bar", meta=(ClampMin="1", UIMin="1"))
	int32 DefaultPoolSize = 16;
};
