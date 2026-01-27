#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_EscapeMenuKeyBindingEntry.h"

#include "Components/InputKeySelector.h"
#include "Components/RichTextBlock.h"
#include "InputAction.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_EscapeMenuKeyBindingEntry::NativeConstruct()
{
	Super::NativeConstruct();

	if (not GetIsValidKeySelector())
	{
		return;
	}

	M_KeySelector->OnKeySelected.RemoveAll(this);
	M_KeySelector->OnKeySelected.AddDynamic(this, &UW_EscapeMenuKeyBindingEntry::HandleKeySelected);
}

void UW_EscapeMenuKeyBindingEntry::SetupEntry(
	ACPPController* NewPlayerController,
	UInputAction* ActionToBind,
	const FKey& CurrentKey)
{
	if (not IsValid(NewPlayerController))
	{
		RTSFunctionLibrary::ReportError("Key binding entry received an invalid player controller reference.");
		return;
	}

	if (not IsValid(ActionToBind))
	{
		RTSFunctionLibrary::ReportError("Key binding entry received an invalid input action reference.");
		return;
	}

	M_PlayerController = NewPlayerController;
	M_Action = ActionToBind;
	M_CurrentKey = CurrentKey;

	if (GetIsValidActionNameText())
	{
		const FText ActionLabel = FText::FromName(ActionToBind->GetFName());
		M_ActionNameText->SetText(ActionLabel);
	}

	UpdateDisplayedKey(CurrentKey);
}

bool UW_EscapeMenuKeyBindingEntry::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("UW_EscapeMenuKeyBindingEntry::GetIsValidPlayerController"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindingEntry::GetIsValidAction() const
{
	if (IsValid(M_Action))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_Action"),
		TEXT("UW_EscapeMenuKeyBindingEntry::GetIsValidAction"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindingEntry::GetIsValidActionNameText() const
{
	if (IsValid(M_ActionNameText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ActionNameText"),
		TEXT("UW_EscapeMenuKeyBindingEntry::GetIsValidActionNameText"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindingEntry::GetIsValidKeySelector() const
{
	if (IsValid(M_KeySelector))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_KeySelector"),
		TEXT("UW_EscapeMenuKeyBindingEntry::GetIsValidKeySelector"),
		this
	);
	return false;
}

void UW_EscapeMenuKeyBindingEntry::UpdateDisplayedKey(const FKey& NewKey)
{
	if (not GetIsValidKeySelector())
	{
		return;
	}

	const FInputChord InputChord(NewKey);
	M_KeySelector->SetSelectedKey(InputChord);
	M_KeySelector->SetAllowGamepadKeys(true);
	M_KeySelector->SetAllowModifierKeys(false);
}

void UW_EscapeMenuKeyBindingEntry::HandleKeySelected(const FInputChord& SelectedKey)
{
	if (not GetIsValidPlayerController() || not GetIsValidAction())
	{
		return;
	}

	const FKey NewKey = SelectedKey.Key;
	if (not NewKey.IsValid() || M_CurrentKey == NewKey)
	{
		UpdateDisplayedKey(M_CurrentKey);
		return;
	}

	M_PlayerController->ChangeKeyBinding(M_Action, M_CurrentKey, NewKey);
	M_CurrentKey = NewKey;
	UpdateDisplayedKey(NewKey);
}
