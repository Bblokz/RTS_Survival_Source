#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"

#include "EscapeMenuKeyBindingButtonSounds.generated.h"

/**
 * @brief Designer-assigned click sounds shared by the key bindings menu and its spawned popup.
 */
USTRUCT(BlueprintType)
struct FEscapeMenuKeyBindingButtonSounds
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "KeyBindings|Audio")
	TObjectPtr<USoundBase> BackButtonSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "KeyBindings|Audio")
	TObjectPtr<USoundBase> ActionButtonSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "KeyBindings|Audio")
	TObjectPtr<USoundBase> ControlGroupButtonSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "KeyBindings|Audio")
	TObjectPtr<USoundBase> PopupUnderstoodButtonSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "KeyBindings|Audio")
	TObjectPtr<USoundBase> PopupUnbindButtonSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "KeyBindings|Audio")
	TObjectPtr<USoundBase> PopupConfirmExitButtonSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "KeyBindings|Audio")
	TObjectPtr<USoundBase> PopupCancelExitButtonSound = nullptr;
};
