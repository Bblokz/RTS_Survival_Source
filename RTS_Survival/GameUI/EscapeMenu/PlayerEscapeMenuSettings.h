#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"

#include "PlayerEscapeMenuSettings.generated.h"

class UW_EscapeMenu;
class UW_EscapeMenuSettings;

/** @brief Defines the assets and classes needed to create and drive the escape menu flow. */
USTRUCT(BlueprintType)
struct FPlayerEscapeMenuSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu")
	TSubclassOf<UW_EscapeMenu> EscapeMenuClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu")
	TSubclassOf<UW_EscapeMenuSettings> EscapeMenuSettingsClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu|Audio")
	TObjectPtr<USoundBase> OpenMenuSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu|Audio")
	TObjectPtr<USoundBase> CloseMenuSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu|Audio")
	TObjectPtr<USoundBase> OpenSettingsMenuSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu|Audio")
	TObjectPtr<USoundBase> CloseSettingsMenuSound = nullptr;
};
