#include "RTS_Survival/GameUI/EscapeMenu/W_EscapeMenu.h"

#include "Components/Button.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_EscapeMenu::SetPlayerController(ACPPController* NewPlayerController)
{
	if (not IsValid(NewPlayerController))
	{
		RTSFunctionLibrary::ReportError("Escape menu received an invalid player controller reference.");
		return;
	}

	M_PlayerController = NewPlayerController;
	M_MainGameUI = NewPlayerController->GetMainMenuUI();
}

void UW_EscapeMenu::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtonCallbacks();
}

void UW_EscapeMenu::BindButtonCallbacks()
{
	BindResumeGameButton();
	BindSettingsButton();
	BindKeyBindingsButton();
	BindArchiveButton();
	BindRestartLevelButton();
	BindExitToMainMenuButton();
}

void UW_EscapeMenu::BindResumeGameButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenu::BindResumeGameButton");
	if (not EnsureButtonIsValid(M_ButtonResumeGame, TEXT("M_ButtonResumeGame"), FunctionName))
	{
		return;
	}

	M_ButtonResumeGame->OnClicked.RemoveAll(this);
	M_ButtonResumeGame->OnClicked.AddDynamic(this, &UW_EscapeMenu::HandleResumeGameClicked);
}

void UW_EscapeMenu::BindSettingsButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenu::BindSettingsButton");
	if (not EnsureButtonIsValid(M_ButtonSettings, TEXT("M_ButtonSettings"), FunctionName))
	{
		return;
	}

	M_ButtonSettings->OnClicked.RemoveAll(this);
	M_ButtonSettings->OnClicked.AddDynamic(this, &UW_EscapeMenu::HandleSettingsClicked);
}

void UW_EscapeMenu::BindKeyBindingsButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenu::BindKeyBindingsButton");
	if (not EnsureButtonIsValid(M_ButtonKeyBindings, TEXT("M_ButtonKeyBindings"), FunctionName))
	{
		return;
	}

	M_ButtonKeyBindings->OnClicked.RemoveAll(this);
	M_ButtonKeyBindings->OnClicked.AddDynamic(this, &UW_EscapeMenu::HandleKeyBindingsClicked);
}

void UW_EscapeMenu::BindArchiveButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenu::BindArchiveButton");
	if (not EnsureButtonIsValid(M_ButtonArchive, TEXT("M_ButtonArchive"), FunctionName))
	{
		return;
	}

	M_ButtonArchive->OnClicked.RemoveAll(this);
	M_ButtonArchive->OnClicked.AddDynamic(this, &UW_EscapeMenu::HandleArchiveClicked);
}

void UW_EscapeMenu::BindRestartLevelButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenu::BindRestartLevelButton");
	if (not EnsureButtonIsValid(M_ButtonRestartLevel, TEXT("M_ButtonRestartLevel"), FunctionName))
	{
		return;
	}

	M_ButtonRestartLevel->OnClicked.RemoveAll(this);
	M_ButtonRestartLevel->OnClicked.AddDynamic(this, &UW_EscapeMenu::HandleRestartLevelClicked);
}

void UW_EscapeMenu::BindExitToMainMenuButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenu::BindExitToMainMenuButton");
	if (not EnsureButtonIsValid(M_ButtonExitToMainMenu, TEXT("M_ButtonExitToMainMenu"), FunctionName))
	{
		return;
	}

	M_ButtonExitToMainMenu->OnClicked.RemoveAll(this);
	M_ButtonExitToMainMenu->OnClicked.AddDynamic(this, &UW_EscapeMenu::HandleExitToMainMenuClicked);
}

bool UW_EscapeMenu::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("UW_EscapeMenu::GetIsValidPlayerController"),
		this
	);
	return false;
}

bool UW_EscapeMenu::GetIsValidMainGameUI() const
{
	if (M_MainGameUI.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_MainGameUI"),
		TEXT("UW_EscapeMenu::GetIsValidMainGameUI"),
		this
	);
	return false;
}

void UW_EscapeMenu::HandleResumeGameClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuResumeGame();
}

void UW_EscapeMenu::HandleSettingsClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuOpenSettings();
}

void UW_EscapeMenu::HandleKeyBindingsClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuOpenKeyBindings();
}

void UW_EscapeMenu::HandleArchiveClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuOpenArchive();
}

void UW_EscapeMenu::HandleRestartLevelClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuRestartLevel();
}

void UW_EscapeMenu::HandleExitToMainMenuClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuExitToMainMenu();
}

bool UW_EscapeMenu::EnsureButtonIsValid(
	const UButton* ButtonToCheck,
	const FString& ButtonName,
	const FString& FunctionName) const
{
	if (ButtonToCheck)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, ButtonName, FunctionName, this);
	return false;
}
