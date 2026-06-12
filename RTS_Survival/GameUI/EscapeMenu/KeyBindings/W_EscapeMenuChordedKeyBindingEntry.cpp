#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_EscapeMenuChordedKeyBindingEntry.h"

#include "Components/ComboBoxString.h"
#include "Components/InputKeySelector.h"
#include "Components/RichTextBlock.h"
#include "InputAction.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EscapeMenuChordedEntryConstants
{
	const FString ControlOption = TEXT("Control");
	const FString ShiftOption = TEXT("Shift");
	const FString AltOption = TEXT("Alt");
}

void UW_EscapeMenuChordedKeyBindingEntry::NativeConstruct()
{
	Super::NativeConstruct();

	if (not GetIsValidKeySelector() || not GetIsValidModifierSelector())
	{
		return;
	}

	PopulateModifierSelector();
	M_KeySelector->OnKeySelected.RemoveAll(this);
	M_KeySelector->OnKeySelected.AddDynamic(this, &UW_EscapeMenuChordedKeyBindingEntry::HandleKeySelected);
	M_ModifierSelector->OnSelectionChanged.RemoveAll(this);
	M_ModifierSelector->OnSelectionChanged.AddDynamic(
		this,
		&UW_EscapeMenuChordedKeyBindingEntry::HandleModifierSelectionChanged);
}

void UW_EscapeMenuChordedKeyBindingEntry::SetupEntry(
	ACPPController* NewPlayerController,
	UInputAction* ActionToBind,
	const FRTSModifierHotkey& CurrentHotkey)
{
	if (not IsValid(NewPlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("Chorded key binding entry received an invalid player controller reference."));
		return;
	}

	if (not IsValid(ActionToBind))
	{
		RTSFunctionLibrary::ReportError(TEXT("Chorded key binding entry received an invalid input action reference."));
		return;
	}

	M_PlayerController = NewPlayerController;
	M_Action = ActionToBind;
	M_CurrentHotkey = CurrentHotkey;
	M_ActionDisplayName = BuildActionDisplayName(ActionToBind->GetFName());

	if (GetIsValidActionNameText())
	{
		M_ActionNameText->SetText(FText::FromString(M_ActionDisplayName));
	}

	UpdateDisplayedHotkey(CurrentHotkey);
}

void UW_EscapeMenuChordedKeyBindingEntry::SetKeyBindingValidationDelegate(
	FChordedKeyBindingValidationDelegate ValidationDelegate)
{
	M_KeyBindingValidationDelegate = ValidationDelegate;
}

FOnChordedKeyBindingUpdated& UW_EscapeMenuChordedKeyBindingEntry::OnChordedKeyBindingUpdated()
{
	return M_OnChordedKeyBindingUpdated;
}

void UW_EscapeMenuChordedKeyBindingEntry::UpdateKeyBinding(const FRTSModifierHotkey& NewHotkey)
{
	M_CurrentHotkey = NewHotkey;
	UpdateDisplayedHotkey(NewHotkey);
}

bool UW_EscapeMenuChordedKeyBindingEntry::GetIsKeyBound() const
{
	return M_CurrentHotkey.GetIsValid();
}

FRTSModifierHotkey UW_EscapeMenuChordedKeyBindingEntry::GetCurrentHotkey() const
{
	return M_CurrentHotkey;
}

FString UW_EscapeMenuChordedKeyBindingEntry::GetActionDisplayName() const
{
	return M_ActionDisplayName;
}

void UW_EscapeMenuChordedKeyBindingEntry::PopulateModifierSelector()
{
	if (not GetIsValidModifierSelector())
	{
		return;
	}

	M_ModifierSelector->ClearOptions();
	M_ModifierSelector->AddOption(EscapeMenuChordedEntryConstants::ControlOption);
	M_ModifierSelector->AddOption(EscapeMenuChordedEntryConstants::ShiftOption);
	M_ModifierSelector->AddOption(EscapeMenuChordedEntryConstants::AltOption);
}

void UW_EscapeMenuChordedKeyBindingEntry::UpdateDisplayedHotkey(const FRTSModifierHotkey& NewHotkey)
{
	if (GetIsValidModifierSelector())
	{
		switch (NewHotkey.Modifier)
		{
		case ERTSHotkeyModifier::Control:
			M_ModifierSelector->SetSelectedOption(EscapeMenuChordedEntryConstants::ControlOption);
			break;
		case ERTSHotkeyModifier::Shift:
			M_ModifierSelector->SetSelectedOption(EscapeMenuChordedEntryConstants::ShiftOption);
			break;
		case ERTSHotkeyModifier::Alt:
			M_ModifierSelector->SetSelectedOption(EscapeMenuChordedEntryConstants::AltOption);
			break;
		default:
			M_ModifierSelector->ClearSelection();
			break;
		}
	}

	if (not GetIsValidKeySelector())
	{
		return;
	}

	const FInputChord InputChord = NewHotkey.Key.IsValid() ? FInputChord(NewHotkey.Key) : FInputChord();
	M_KeySelector->SetSelectedKey(InputChord);
	M_KeySelector->SetAllowGamepadKeys(false);
	M_KeySelector->SetAllowModifierKeys(false);
}

ERTSHotkeyModifier UW_EscapeMenuChordedKeyBindingEntry::GetSelectedModifier() const
{
	if (not GetIsValidModifierSelector())
	{
		return ERTSHotkeyModifier::Invalid;
	}

	const FString SelectedOption = M_ModifierSelector->GetSelectedOption();
	if (SelectedOption == EscapeMenuChordedEntryConstants::ControlOption)
	{
		return ERTSHotkeyModifier::Control;
	}

	if (SelectedOption == EscapeMenuChordedEntryConstants::ShiftOption)
	{
		return ERTSHotkeyModifier::Shift;
	}

	if (SelectedOption == EscapeMenuChordedEntryConstants::AltOption)
	{
		return ERTSHotkeyModifier::Alt;
	}

	return ERTSHotkeyModifier::Invalid;
}

FString UW_EscapeMenuChordedKeyBindingEntry::BuildActionDisplayName(const FName& ActionName) const
{
	return RTSHotkeyTypes::GetActionDisplayName(ActionName);
}

bool UW_EscapeMenuChordedKeyBindingEntry::GetIsInvalidSecondaryKey(const FKey& Key) const
{
	return not Key.IsValid()
		|| Key.IsGamepadKey()
		|| Key.IsMouseButton()
		|| Key == EKeys::LeftControl
		|| Key == EKeys::RightControl
		|| Key == EKeys::LeftShift
		|| Key == EKeys::RightShift
		|| Key == EKeys::LeftAlt
		|| Key == EKeys::RightAlt;
}

bool UW_EscapeMenuChordedKeyBindingEntry::GetIsDisallowedControlGroupSemanticKey(
	const FRTSModifierHotkey& Hotkey) const
{
	const bool bUsesControlGroupModifier = Hotkey.Modifier == ERTSHotkeyModifier::Control
		|| Hotkey.Modifier == ERTSHotkeyModifier::Shift;
	return bUsesControlGroupModifier && RTSHotkeyTypes::GetIsNumberKey(Hotkey.Key);
}

void UW_EscapeMenuChordedKeyBindingEntry::HandleModifierSelectionChanged(
	FString SelectedItem,
	ESelectInfo::Type SelectionType)
{
	if (SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	if (not GetIsValidPlayerController() || not GetIsValidAction())
	{
		return;
	}

	FRTSModifierHotkey NewHotkey = M_CurrentHotkey;
	NewHotkey.Modifier = GetSelectedModifier();
	if (not NewHotkey.GetIsValid())
	{
		return;
	}

	if (GetIsDisallowedControlGroupSemanticKey(NewHotkey))
	{
		UpdateDisplayedHotkey(M_CurrentHotkey);
		return;
	}

	if (M_KeyBindingValidationDelegate.IsBound() && not M_KeyBindingValidationDelegate.Execute(M_Action, NewHotkey))
	{
		UpdateDisplayedHotkey(M_CurrentHotkey);
		return;
	}

	if (not M_PlayerController->ChangeChordedKeyBinding(M_Action, NewHotkey))
	{
		UpdateDisplayedHotkey(M_CurrentHotkey);
		return;
	}

	M_CurrentHotkey = NewHotkey;
	M_OnChordedKeyBindingUpdated.Broadcast(M_Action, NewHotkey);
}

void UW_EscapeMenuChordedKeyBindingEntry::HandleKeySelected(FInputChord SelectedKey)
{
	if (not GetIsValidPlayerController() || not GetIsValidAction())
	{
		return;
	}

	FRTSModifierHotkey NewHotkey;
	NewHotkey.Modifier = GetSelectedModifier();
	NewHotkey.Key = SelectedKey.Key;
	if (GetIsInvalidSecondaryKey(NewHotkey.Key) || NewHotkey == M_CurrentHotkey)
	{
		UpdateDisplayedHotkey(M_CurrentHotkey);
		return;
	}

	if (GetIsDisallowedControlGroupSemanticKey(NewHotkey))
	{
		UpdateDisplayedHotkey(M_CurrentHotkey);
		return;
	}

	if (M_KeyBindingValidationDelegate.IsBound() && not M_KeyBindingValidationDelegate.Execute(M_Action, NewHotkey))
	{
		UpdateDisplayedHotkey(M_CurrentHotkey);
		return;
	}

	if (not M_PlayerController->ChangeChordedKeyBinding(M_Action, NewHotkey))
	{
		UpdateDisplayedHotkey(M_CurrentHotkey);
		return;
	}

	M_CurrentHotkey = NewHotkey;
	UpdateDisplayedHotkey(NewHotkey);
	M_OnChordedKeyBindingUpdated.Broadcast(M_Action, NewHotkey);
}

bool UW_EscapeMenuChordedKeyBindingEntry::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("UW_EscapeMenuChordedKeyBindingEntry::GetIsValidPlayerController"),
		this
	);
	return false;
}

bool UW_EscapeMenuChordedKeyBindingEntry::GetIsValidAction() const
{
	if (IsValid(M_Action))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_Action"),
		TEXT("UW_EscapeMenuChordedKeyBindingEntry::GetIsValidAction"),
		this
	);
	return false;
}

bool UW_EscapeMenuChordedKeyBindingEntry::GetIsValidActionNameText() const
{
	if (IsValid(M_ActionNameText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ActionNameText"),
		TEXT("UW_EscapeMenuChordedKeyBindingEntry::GetIsValidActionNameText"),
		this
	);
	return false;
}

bool UW_EscapeMenuChordedKeyBindingEntry::GetIsValidModifierSelector() const
{
	if (IsValid(M_ModifierSelector))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ModifierSelector"),
		TEXT("UW_EscapeMenuChordedKeyBindingEntry::GetIsValidModifierSelector"),
		this
	);
	return false;
}

bool UW_EscapeMenuChordedKeyBindingEntry::GetIsValidKeySelector() const
{
	if (IsValid(M_KeySelector))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_KeySelector"),
		TEXT("UW_EscapeMenuChordedKeyBindingEntry::GetIsValidKeySelector"),
		this
	);
	return false;
}
