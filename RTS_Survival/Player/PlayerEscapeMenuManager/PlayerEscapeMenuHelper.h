#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/EscapeMenu/PlayerEscapeMenuSettings.h"

#include "PlayerEscapeMenuHelper.generated.h"

class ACPPController;
class USoundBase;
class UUserWidget;
class UW_EscapeMenu;
class UW_EscapeMenuKeyBindings;
class UW_EscapeMenuSettings;

/** @brief Handles escape menu widget lifetime, visibility, and pause/input state for the player controller. */
USTRUCT()
struct FPlayerEscapeMenuHelper
{
	GENERATED_BODY()

public:
	void InitEscapeMenuHelper(ACPPController* PlayerController);

	void OpenEscapeMenu(const FPlayerEscapeMenuSettings& EscapeMenuSettings);
	void CloseEscapeMenu(const FPlayerEscapeMenuSettings& EscapeMenuSettings);

	void OpenEscapeMenuSettings(const FPlayerEscapeMenuSettings& EscapeMenuSettings);
	void CloseEscapeMenuSettings(const FPlayerEscapeMenuSettings& EscapeMenuSettings);

	void OpenEscapeMenuKeyBindings(const FPlayerEscapeMenuSettings& EscapeMenuSettings);
	void CloseEscapeMenuKeyBindings(const FPlayerEscapeMenuSettings& EscapeMenuSettings);

private:
	bool GetIsValidPlayerController() const;
	bool GetIsValidEscapeMenuWidget() const;
	bool GetIsValidEscapeMenuSettingsWidget() const;
	bool GetIsValidEscapeMenuKeyBindingsWidget() const;

	bool EnsureEscapeMenuWidgetCreated(const FPlayerEscapeMenuSettings& EscapeMenuSettings);
	bool EnsureEscapeMenuSettingsWidgetCreated(const FPlayerEscapeMenuSettings& EscapeMenuSettings);
	bool EnsureEscapeMenuKeyBindingsWidgetCreated(const FPlayerEscapeMenuSettings& EscapeMenuSettings);

	bool EnsureEscapeMenuClassIsValid(const FPlayerEscapeMenuSettings& EscapeMenuSettings) const;
	bool EnsureEscapeMenuSettingsClassIsValid(const FPlayerEscapeMenuSettings& EscapeMenuSettings) const;
	bool EnsureEscapeMenuKeyBindingsClassIsValid(const FPlayerEscapeMenuSettings& EscapeMenuSettings) const;

	void ShowEscapeMenu();
	void ShowEscapeMenuSettings();
	void ShowEscapeMenuKeyBindings();

	void MakeWidgetDormant(UUserWidget* WidgetToDormant) const;
	void WakeWidget(UUserWidget* WidgetToWake) const;
	void AddWidgetToViewport(UUserWidget* WidgetToAdd, const int32 ZOrder) const;
	void ApplyInputModeForWidget(UUserWidget* WidgetToFocus) const;

	void PauseGameForEscapeMenu() const;
	void UnpauseGameForEscapeMenu() const;

	void PlaySoundIfSet(USoundBase* SoundToPlay) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UW_EscapeMenu> M_EscapeMenuWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UW_EscapeMenuSettings> M_EscapeMenuSettingsWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UW_EscapeMenuKeyBindings> M_EscapeMenuKeyBindingsWidget = nullptr;
};
