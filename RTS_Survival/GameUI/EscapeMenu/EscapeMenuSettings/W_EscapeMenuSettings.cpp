#include "RTS_Survival/GameUI/EscapeMenu/EscapeMenuSettings/W_EscapeMenuSettings.h"

#include "Components/Button.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_EscapeMenuSettings::SetPlayerController(ACPPController* NewPlayerController)
{
	if (not IsValid(NewPlayerController))
	{
		RTSFunctionLibrary::ReportError("Escape settings menu received an invalid player controller reference.");
		return;
	}

	M_PlayerController = NewPlayerController;
	M_MainGameUI = NewPlayerController->GetMainMenuUI();
}

void UW_EscapeMenuSettings::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtonCallbacks();
}

void UW_EscapeMenuSettings::BindButtonCallbacks()
{
	BindCloseButton();
}

void UW_EscapeMenuSettings::BindCloseButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenuSettings::BindCloseButton");
	if (not EnsureButtonIsValid(M_ButtonCloseSettings, TEXT("M_ButtonCloseSettings"), FunctionName))
	{
		return;
	}

	M_ButtonCloseSettings->OnClicked.RemoveAll(this);
	M_ButtonCloseSettings->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleCloseClicked);
}

bool UW_EscapeMenuSettings::GetIsValidMainGameUI() const
{
	if (M_MainGameUI.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_MainGameUI"),
		TEXT("UW_EscapeMenuSettings::GetIsValidMainGameUI"),
		this
	);
	return false;
}

void UW_EscapeMenuSettings::HandleCloseClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuCloseSettings();
}

bool UW_EscapeMenuSettings::EnsureButtonIsValid(
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
