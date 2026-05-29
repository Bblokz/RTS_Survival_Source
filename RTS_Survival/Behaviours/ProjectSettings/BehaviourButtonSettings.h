#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BehaviourButtonSettings.generated.h"

class UBehaviourIconStyleDataAsset;

/**
 * @brief Project settings point behaviour widgets to the icon style Data Asset configured for this project.
 * Appears under Project Settings as: UI ► Behaviour Buttons.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Behaviour Buttons"))
class RTS_SURVIVAL_API UBehaviourButtonSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBehaviourButtonSettings();

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Behaviour Button Styles")
	TSoftObjectPtr<UBehaviourIconStyleDataAsset> BehaviourIconStyleDataAsset;

	/** Convenience accessor. */
	static const UBehaviourButtonSettings* Get()
	{
		return GetDefault<UBehaviourButtonSettings>();
	}

	const UBehaviourIconStyleDataAsset* GetBehaviourIconStyleDataAsset() const;
};
