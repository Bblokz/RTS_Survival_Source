#include "RTS_Survival/Player/PlayerEscapeMenuManager/PlayerEscapeMenuHelper.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/EscapeMenu/EscapeMenuSettings/W_EscapeMenuSettings.h"
#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_EscapeMenuKeyBindings.h"
#include "RTS_Survival/GameUI/EscapeMenu/W_EscapeMenu.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSInputModeDefaults.h"

namespace EscapeMenuZOrder
{
	constexpr int32 EscapeMenu = 1900;
	constexpr int32 EscapeMenuSettings = 1910;
	constexpr int32 EscapeMenuKeyBindings = 1920;
}

void FPlayerEscapeMenuHelper::InitEscapeMenuHelper(ACPPController* PlayerController)
{
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError("Escape menu helper received an invalid player controller.");
		return;
	}

	M_PlayerController = PlayerController;
}

void FPlayerEscapeMenuHelper::OpenEscapeMenu(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (not EnsureEscapeMenuWidgetCreated(EscapeMenuSettings))
	{
		return;
	}

	if (M_EscapeMenuSettingsWidget != nullptr)
	{
		MakeWidgetDormant(M_EscapeMenuSettingsWidget);
	}

	if (M_EscapeMenuKeyBindingsWidget != nullptr)
	{
		MakeWidgetDormant(M_EscapeMenuKeyBindingsWidget);
	}

	ShowEscapeMenu();
	PlaySoundIfSet(EscapeMenuSettings.OpenMenuSound);
	PauseGameForEscapeMenu();
}

void FPlayerEscapeMenuHelper::CloseEscapeMenu(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (M_EscapeMenuWidget == nullptr)
	{
		return;
	}

	MakeWidgetDormant(M_EscapeMenuWidget);
	if (M_EscapeMenuSettingsWidget != nullptr)
	{
		MakeWidgetDormant(M_EscapeMenuSettingsWidget);
	}

	if (M_EscapeMenuKeyBindingsWidget != nullptr)
	{
		MakeWidgetDormant(M_EscapeMenuKeyBindingsWidget);
	}

	PlaySoundIfSet(EscapeMenuSettings.CloseMenuSound);
	UnpauseGameForEscapeMenu();
}

void FPlayerEscapeMenuHelper::OpenEscapeMenuSettings(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (not EnsureEscapeMenuSettingsWidgetCreated(EscapeMenuSettings))
	{
		return;
	}

	if (M_EscapeMenuWidget != nullptr)
	{
		MakeWidgetDormant(M_EscapeMenuWidget);
	}

	if (M_EscapeMenuKeyBindingsWidget != nullptr)
	{
		MakeWidgetDormant(M_EscapeMenuKeyBindingsWidget);
	}

	ShowEscapeMenuSettings();
	PlaySoundIfSet(EscapeMenuSettings.OpenSettingsMenuSound);
	PauseGameForEscapeMenu();
}

void FPlayerEscapeMenuHelper::CloseEscapeMenuSettings(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (M_EscapeMenuSettingsWidget == nullptr)
	{
		return;
	}

	MakeWidgetDormant(M_EscapeMenuSettingsWidget);
	PlaySoundIfSet(EscapeMenuSettings.CloseSettingsMenuSound);

	if (EnsureEscapeMenuWidgetCreated(EscapeMenuSettings))
	{
		ShowEscapeMenu();
	}

	PauseGameForEscapeMenu();
}

void FPlayerEscapeMenuHelper::OpenEscapeMenuKeyBindings(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (not EnsureEscapeMenuKeyBindingsWidgetCreated(EscapeMenuSettings))
	{
		return;
	}

	if (M_EscapeMenuWidget != nullptr)
	{
		MakeWidgetDormant(M_EscapeMenuWidget);
	}

	if (M_EscapeMenuSettingsWidget != nullptr)
	{
		MakeWidgetDormant(M_EscapeMenuSettingsWidget);
	}

	ShowEscapeMenuKeyBindings();
	PlaySoundIfSet(EscapeMenuSettings.OpenSettingsMenuSound);
	PauseGameForEscapeMenu();
}

void FPlayerEscapeMenuHelper::CloseEscapeMenuKeyBindings(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (M_EscapeMenuKeyBindingsWidget == nullptr)
	{
		return;
	}

	MakeWidgetDormant(M_EscapeMenuKeyBindingsWidget);
	PlaySoundIfSet(EscapeMenuSettings.CloseSettingsMenuSound);

	if (EnsureEscapeMenuWidgetCreated(EscapeMenuSettings))
	{
		ShowEscapeMenu();
	}

	PauseGameForEscapeMenu();
}

bool FPlayerEscapeMenuHelper::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		M_PlayerController.Get(),
		TEXT("M_PlayerController"),
		TEXT("FPlayerEscapeMenuHelper::GetIsValidPlayerController"),
		M_PlayerController.Get()
	);
	return false;
}

bool FPlayerEscapeMenuHelper::GetIsValidEscapeMenuWidget() const
{
	if (IsValid(M_EscapeMenuWidget))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		M_PlayerController.Get(),
		TEXT("M_EscapeMenuWidget"),
		TEXT("FPlayerEscapeMenuHelper::GetIsValidEscapeMenuWidget"),
		M_PlayerController.Get()
	);
	return false;
}

bool FPlayerEscapeMenuHelper::GetIsValidEscapeMenuSettingsWidget() const
{
	if (IsValid(M_EscapeMenuSettingsWidget))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		M_PlayerController.Get(),
		TEXT("M_EscapeMenuSettingsWidget"),
		TEXT("FPlayerEscapeMenuHelper::GetIsValidEscapeMenuSettingsWidget"),
		M_PlayerController.Get()
	);
	return false;
}

bool FPlayerEscapeMenuHelper::GetIsValidEscapeMenuKeyBindingsWidget() const
{
	if (IsValid(M_EscapeMenuKeyBindingsWidget))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		M_PlayerController.Get(),
		TEXT("M_EscapeMenuKeyBindingsWidget"),
		TEXT("FPlayerEscapeMenuHelper::GetIsValidEscapeMenuKeyBindingsWidget"),
		M_PlayerController.Get()
	);
	return false;
}

bool FPlayerEscapeMenuHelper::EnsureEscapeMenuWidgetCreated(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (M_EscapeMenuWidget != nullptr)
	{
		return true;
	}

	if (not GetIsValidPlayerController() || not EnsureEscapeMenuClassIsValid(EscapeMenuSettings))
	{
		return false;
	}

	UW_EscapeMenu* EscapeMenuWidget = CreateWidget<UW_EscapeMenu>(
		M_PlayerController.Get(),
		EscapeMenuSettings.EscapeMenuClass
	);
	if (EscapeMenuWidget == nullptr)
	{
		RTSFunctionLibrary::ReportError("Failed to create escape menu widget.");
		return false;
	}

	EscapeMenuWidget->SetPlayerController(M_PlayerController.Get());
	M_EscapeMenuWidget = EscapeMenuWidget;
	MakeWidgetDormant(EscapeMenuWidget);
	return true;
}

bool FPlayerEscapeMenuHelper::EnsureEscapeMenuSettingsWidgetCreated(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (M_EscapeMenuSettingsWidget != nullptr)
	{
		return true;
	}

	if (not GetIsValidPlayerController() || not EnsureEscapeMenuSettingsClassIsValid(EscapeMenuSettings))
	{
		return false;
	}

	UW_EscapeMenuSettings* EscapeMenuSettingsWidget = CreateWidget<UW_EscapeMenuSettings>(
		M_PlayerController.Get(),
		EscapeMenuSettings.EscapeMenuSettingsClass
	);
	if (EscapeMenuSettingsWidget == nullptr)
	{
		RTSFunctionLibrary::ReportError("Failed to create escape menu settings widget.");
		return false;
	}

	EscapeMenuSettingsWidget->SetPlayerController(M_PlayerController.Get());
	M_EscapeMenuSettingsWidget = EscapeMenuSettingsWidget;
	MakeWidgetDormant(EscapeMenuSettingsWidget);
	return true;
}

bool FPlayerEscapeMenuHelper::EnsureEscapeMenuKeyBindingsWidgetCreated(const FPlayerEscapeMenuSettings& EscapeMenuSettings)
{
	if (M_EscapeMenuKeyBindingsWidget != nullptr)
	{
		return true;
	}

	if (not GetIsValidPlayerController() || not EnsureEscapeMenuKeyBindingsClassIsValid(EscapeMenuSettings))
	{
		return false;
	}

	UW_EscapeMenuKeyBindings* EscapeMenuKeyBindingsWidget = CreateWidget<UW_EscapeMenuKeyBindings>(
		M_PlayerController.Get(),
		EscapeMenuSettings.EscapeMenuKeyBindingsClass
	);
	if (EscapeMenuKeyBindingsWidget == nullptr)
	{
		RTSFunctionLibrary::ReportError("Failed to create escape menu key bindings widget.");
		return false;
	}

	EscapeMenuKeyBindingsWidget->SetPlayerController(M_PlayerController.Get());
	M_EscapeMenuKeyBindingsWidget = EscapeMenuKeyBindingsWidget;
	MakeWidgetDormant(EscapeMenuKeyBindingsWidget);
	return true;
}

bool FPlayerEscapeMenuHelper::EnsureEscapeMenuClassIsValid(const FPlayerEscapeMenuSettings& EscapeMenuSettings) const
{
	if (EscapeMenuSettings.EscapeMenuClass)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		M_PlayerController.Get(),
		TEXT("EscapeMenuClass"),
		TEXT("FPlayerEscapeMenuHelper::EnsureEscapeMenuClassIsValid"),
		M_PlayerController.Get()
	);
	return false;
}

bool FPlayerEscapeMenuHelper::EnsureEscapeMenuSettingsClassIsValid(const FPlayerEscapeMenuSettings& EscapeMenuSettings) const
{
	if (EscapeMenuSettings.EscapeMenuSettingsClass)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		M_PlayerController.Get(),
		TEXT("EscapeMenuSettingsClass"),
		TEXT("FPlayerEscapeMenuHelper::EnsureEscapeMenuSettingsClassIsValid"),
		M_PlayerController.Get()
	);
	return false;
}

bool FPlayerEscapeMenuHelper::EnsureEscapeMenuKeyBindingsClassIsValid(
	const FPlayerEscapeMenuSettings& EscapeMenuSettings) const
{
	if (EscapeMenuSettings.EscapeMenuKeyBindingsClass)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		M_PlayerController.Get(),
		TEXT("EscapeMenuKeyBindingsClass"),
		TEXT("FPlayerEscapeMenuHelper::EnsureEscapeMenuKeyBindingsClassIsValid"),
		M_PlayerController.Get()
	);
	return false;
}

void FPlayerEscapeMenuHelper::ShowEscapeMenu()
{
	if (not GetIsValidEscapeMenuWidget())
	{
		return;
	}

	WakeWidget(M_EscapeMenuWidget);
	AddWidgetToViewport(M_EscapeMenuWidget, EscapeMenuZOrder::EscapeMenu);
	ApplyInputModeForWidget(M_EscapeMenuWidget);
}

void FPlayerEscapeMenuHelper::ShowEscapeMenuSettings()
{
	if (not GetIsValidEscapeMenuSettingsWidget())
	{
		return;
	}

	WakeWidget(M_EscapeMenuSettingsWidget);
	AddWidgetToViewport(M_EscapeMenuSettingsWidget, EscapeMenuZOrder::EscapeMenuSettings);
	ApplyInputModeForWidget(M_EscapeMenuSettingsWidget);
}

void FPlayerEscapeMenuHelper::ShowEscapeMenuKeyBindings()
{
	if (not GetIsValidEscapeMenuKeyBindingsWidget())
	{
		return;
	}

	WakeWidget(M_EscapeMenuKeyBindingsWidget);
	AddWidgetToViewport(M_EscapeMenuKeyBindingsWidget, EscapeMenuZOrder::EscapeMenuKeyBindings);
	ApplyInputModeForWidget(M_EscapeMenuKeyBindingsWidget);
}

void FPlayerEscapeMenuHelper::MakeWidgetDormant(UUserWidget* WidgetToDormant) const
{
	if (not WidgetToDormant)
	{
		return;
	}

	WidgetToDormant->SetIsEnabled(false);
	WidgetToDormant->SetVisibility(ESlateVisibility::Collapsed);
	WidgetToDormant->RemoveFromParent();
}

void FPlayerEscapeMenuHelper::WakeWidget(UUserWidget* WidgetToWake) const
{
	if (not WidgetToWake)
	{
		return;
	}

	WidgetToWake->SetIsEnabled(true);
	WidgetToWake->SetVisibility(ESlateVisibility::Visible);
}

void FPlayerEscapeMenuHelper::AddWidgetToViewport(UUserWidget* WidgetToAdd, const int32 ZOrder) const
{
	if (not WidgetToAdd)
	{
		return;
	}

	WidgetToAdd->RemoveFromParent();
	WidgetToAdd->AddToViewport(ZOrder);
}

void FPlayerEscapeMenuHelper::ApplyInputModeForWidget(UUserWidget* WidgetToFocus) const
{
	if (not GetIsValidPlayerController() || not WidgetToFocus)
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetWidgetToFocus(WidgetToFocus->TakeWidget());
	M_PlayerController->SetInputMode(InputMode);
	M_PlayerController->bShowMouseCursor = true;
	M_PlayerController->bEnableClickEvents = true;
	M_PlayerController->bEnableMouseOverEvents = true;
}

void FPlayerEscapeMenuHelper::PauseGameForEscapeMenu() const
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_PlayerController->PauseAndLockGame(true);
}

void FPlayerEscapeMenuHelper::UnpauseGameForEscapeMenu() const
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_PlayerController->PauseAndLockGame(false);
	RTSInputModeDefaults::ApplyRegularGameInputMode(M_PlayerController.Get());
}

void FPlayerEscapeMenuHelper::PlaySoundIfSet(USoundBase* SoundToPlay) const
{
	if (not SoundToPlay || not GetIsValidPlayerController())
	{
		return;
	}

	UGameplayStatics::PlaySound2D(M_PlayerController.Get(), SoundToPlay);
}
