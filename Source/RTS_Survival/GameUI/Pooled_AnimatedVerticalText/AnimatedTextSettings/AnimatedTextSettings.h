#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AnimatedTextSettings.generated.h"

class UW_RTSVerticalAnimatedText;

/**
 * @brief Project settings for the vertical animated text system.
 * Configure the widget class (Blueprint derived from UW_RTSVerticalAnimatedText) and pool size.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Animated Text"))
class RTS_SURVIVAL_API UAnimatedTextSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAnimatedTextSettings();

	/** Blueprint class derived from UW_RTSVerticalAnimatedText that will be spawned into the pool. */
	UPROPERTY(Config, EditAnywhere, Category="Animated Text")
	TSoftClassPtr<UW_RTSVerticalAnimatedText> WidgetClass;

	/** Blueprint class derived from UW_RTSVerticalAnimatedText that will be spawned into the pool. */
	UPROPERTY(Config, EditAnywhere, Category="Animated Resource Text")
	TSoftClassPtr<UW_RTSVerticalAnimatedText> ResourceWidgetClass;

	/** Default pool size created on world load. */
	UPROPERTY(Config, EditAnywhere, Category="Animated Text", meta=(ClampMin="1", UIMin="1"))
	int32 DefaultPoolSize = 32;

	// New setting to define the pool size for the resource texts.
	UPROPERTY(Config, EditAnywhere, Category="Animated Resource Text", meta=(ClampMin="1", UIMin="1"))
	int32 ResourceTextPoolSize = 16;
};
