#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AnimatedIconSettings.generated.h"

class UAnimatedIconDataAsset;

/** @brief Assign the icon catalog here; all individual icon and pool settings live in that asset. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Animated Icons"))
class RTS_SURVIVAL_API UAnimatedIconSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAnimatedIconSettings();

	UPROPERTY(Config, EditAnywhere, Category="Animated Icons")
	TSoftObjectPtr<UAnimatedIconDataAsset> IconDataAsset;
};
