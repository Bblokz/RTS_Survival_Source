#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"

#include "PlayerEscapeMenuSettings.generated.h"

class UW_EscapeMenu;
class UW_EscapeMenuKeyBindings;
class UW_EscapeMenuSettings;

/**
 * @brief Bundles the widget classes and sounds used when the player opens or closes the escape menu flow.
 *
 * Designers assign these assets in data-driven settings so the UI manager can build the menu consistently.
 */
USTRUCT(BlueprintType)
struct FPlayerEscapeMenuSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu")
	TSubclassOf<UW_EscapeMenu> EscapeMenuClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu")
	TSubclassOf<UW_EscapeMenuSettings> EscapeMenuSettingsClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu")
	TSubclassOf<UW_EscapeMenuKeyBindings> EscapeMenuKeyBindingsClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu|Audio")
	TObjectPtr<USoundBase> OpenMenuSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu|Audio")
	TObjectPtr<USoundBase> CloseMenuSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu|Audio")
	TObjectPtr<USoundBase> OpenSettingsMenuSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu|Audio")
	TObjectPtr<USoundBase> CloseSettingsMenuSound = nullptr;
};
