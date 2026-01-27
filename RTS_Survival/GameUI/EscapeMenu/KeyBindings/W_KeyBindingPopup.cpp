#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_KeyBindingPopup.h"

#include "Components/Button.h"
#include "Components/RichTextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "InputAction.h"
#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_EscapeMenuKeyBindingEntry.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace KeyBindingPopupConstants
{
	const TCHAR* ChangeBindingTitle = TEXT("<Text_Title>Change Key Binding</>");
	const TCHAR* ConflictBindingTitle = TEXT("<Text_BadTitle>Conflicting Key Binding</>");
	const TCHAR* TryDifferentBindingTitle = TEXT("<Text_Title>Try Different Key</>");
}

void UW_KeyBindingPopup::NativeConstruct()
{
	Super::NativeConstruct();

	if (not GetIsValidUnderstoodButton())
	{
		return;
	}

	M_UnderstoodButton->OnClicked.RemoveAll(this);
	M_UnderstoodButton->OnClicked.AddDynamic(this, &UW_KeyBindingPopup::HandleUnderstoodClicked);
}

void UW_KeyBindingPopup::SetupPopup(ACPPController* NewPlayerController, UInputAction* ActionToBind,
                                    const FKey& CurrentKey)
{
	if (not SetBindingContext(NewPlayerController, ActionToBind, CurrentKey))
	{
		return;
	}

	SetTitleText(KeyBindingPopupConstants::ChangeBindingTitle);
	bM_ShouldReturnToEntryOnUnderstood = false;
	ShowBindingEntry();
}

void UW_KeyBindingPopup::ShowBindingEntry()
{
	if (not GetIsValidPopupSwitcher())
	{
		return;
	}

	M_PopupSwitcher->SetActiveWidgetIndex(0);
}

void UW_KeyBindingPopup::ShowCollisionMessage(const FString& CollidingActionName)
{
	if (not GetIsValidPopupSwitcher() || not GetIsValidConflictText())
	{
		return;
	}

	const FString MessageText =
		TEXT("<Text_Bad14>cannot set keybinding, key already taken by ")
		+ CollidingActionName
		+ TEXT("</>");

	M_ConflictText->SetText(FText::FromString(MessageText));
	SetTitleText(KeyBindingPopupConstants::ConflictBindingTitle);
	M_PopupSwitcher->SetActiveWidgetIndex(1);
	bM_ShouldReturnToEntryOnUnderstood = bM_HasBindingContext;
}

void UW_KeyBindingPopup::ClosePopup()
{
	RemoveFromParent();
}

UW_EscapeMenuKeyBindingEntry* UW_KeyBindingPopup::GetKeyBindingEntry() const
{
	return M_KeyBindingEntry;
}

bool UW_KeyBindingPopup::GetIsValidKeyBindingEntry() const
{
	if (IsValid(M_KeyBindingEntry))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_KeyBindingEntry"),
		TEXT("UW_KeyBindingPopup::GetIsValidKeyBindingEntry"),
		this
	);
	return false;
}

bool UW_KeyBindingPopup::GetIsValidPopupSwitcher() const
{
	if (IsValid(M_PopupSwitcher))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PopupSwitcher"),
		TEXT("UW_KeyBindingPopup::GetIsValidPopupSwitcher"),
		this
	);
	return false;
}

bool UW_KeyBindingPopup::GetIsValidTitleText() const
{
	if (IsValid(M_Title))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_Title"),
		TEXT("UW_KeyBindingPopup::GetIsValidTitleText"),
		this
	);
	return false;
}

bool UW_KeyBindingPopup::GetIsValidConflictText() const
{
	if (IsValid(M_ConflictText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ConflictText"),
		TEXT("UW_KeyBindingPopup::GetIsValidConflictText"),
		this
	);
	return false;
}

bool UW_KeyBindingPopup::SetBindingContext(ACPPController* NewPlayerController, UInputAction* ActionToBind,
                                           const FKey& CurrentKey)
{
	if (not IsValid(NewPlayerController))
	{
		RTSFunctionLibrary::ReportError("Key binding popup received an invalid player controller reference.");
		bM_HasBindingContext = false;
		return false;
	}

	if (not IsValid(ActionToBind))
	{
		RTSFunctionLibrary::ReportError("Key binding popup received an invalid input action reference.");
		bM_HasBindingContext = false;
		return false;
	}

	if (not GetIsValidKeyBindingEntry())
	{
		bM_HasBindingContext = false;
		return false;
	}

	M_KeyBindingEntry->SetupEntry(NewPlayerController, ActionToBind, CurrentKey);
	bM_HasBindingContext = true;
	return true;
}

void UW_KeyBindingPopup::SetTitleText(const FString& TitleText)
{
	if (not GetIsValidTitleText())
	{
		return;
	}

	M_Title->SetText(FText::FromString(TitleText));
}

bool UW_KeyBindingPopup::GetIsValidUnderstoodButton() const
{
	if (IsValid(M_UnderstoodButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_UnderstoodButton"),
		TEXT("UW_KeyBindingPopup::GetIsValidUnderstoodButton"),
		this
	);
	return false;
}

void UW_KeyBindingPopup::HandleUnderstoodClicked()
{
	if (bM_ShouldReturnToEntryOnUnderstood && bM_HasBindingContext)
	{
		SetTitleText(KeyBindingPopupConstants::TryDifferentBindingTitle);
		ShowBindingEntry();
		bM_ShouldReturnToEntryOnUnderstood = false;
		return;
	}

	ClosePopup();
}
