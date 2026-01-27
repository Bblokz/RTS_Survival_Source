#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_EscapeMenuKeyBindings.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_EscapeMenuKeyBindingEntry.h"
#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_KeyBindingPopup.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EscapeMenuKeyBindingsConstants
{
	constexpr int32 PopupZOrder = 600;
	const TCHAR* ActionButtonPrefix = TEXT("IA_ActionButton");
	const TCHAR* ControlGroupPrefix = TEXT("IA_ControlGroup");
}

void UW_EscapeMenuKeyBindings::SetPlayerController(ACPPController* NewPlayerController)
{
	if (not IsValid(NewPlayerController))
	{
		RTSFunctionLibrary::ReportError("Key bindings menu received an invalid player controller reference.");
		return;
	}

	M_PlayerController = NewPlayerController;
	M_MainGameUI = NewPlayerController->GetMainMenuUI();
}

void UW_EscapeMenuKeyBindings::NativeConstruct()
{
	Super::NativeConstruct();

	BindButtonCallbacks();
	BuildKeyBindingEntries();
}

void UW_EscapeMenuKeyBindings::BindButtonCallbacks()
{
	BindBackButton();
	BindSearchBar();
	BindActionButtons();
	BindControlGroupButtons();
}

void UW_EscapeMenuKeyBindings::BindBackButton()
{
	if (not GetIsValidButtonBack())
	{
		return;
	}

	M_ButtonBack->OnClicked.RemoveAll(this);
	M_ButtonBack->OnClicked.AddDynamic(this, &UW_EscapeMenuKeyBindings::HandleBackClicked);
}

void UW_EscapeMenuKeyBindings::BindSearchBar()
{
	if (not GetIsValidSearchKeyBar())
	{
		return;
	}

	M_SearchKeyBar->OnTextChanged.RemoveAll(this);
	M_SearchKeyBar->OnTextChanged.AddDynamic(this, &UW_EscapeMenuKeyBindings::HandleSearchTextChanged);
}

void UW_EscapeMenuKeyBindings::BindActionButtons()
{
	struct FActionButtonBinding
	{
		UButton* Button;
		const TCHAR* ButtonName;
		void (UW_EscapeMenuKeyBindings::*Handler)();
	};

	const TArray<FActionButtonBinding> ButtonBindings = {
		{M_ActionButton1, TEXT("M_ActionButton1"), &UW_EscapeMenuKeyBindings::HandleActionButton1Clicked},
		{M_ActionButton2, TEXT("M_ActionButton2"), &UW_EscapeMenuKeyBindings::HandleActionButton2Clicked},
		{M_ActionButton3, TEXT("M_ActionButton3"), &UW_EscapeMenuKeyBindings::HandleActionButton3Clicked},
		{M_ActionButton4, TEXT("M_ActionButton4"), &UW_EscapeMenuKeyBindings::HandleActionButton4Clicked},
		{M_ActionButton5, TEXT("M_ActionButton5"), &UW_EscapeMenuKeyBindings::HandleActionButton5Clicked},
		{M_ActionButton6, TEXT("M_ActionButton6"), &UW_EscapeMenuKeyBindings::HandleActionButton6Clicked},
		{M_ActionButton7, TEXT("M_ActionButton7"), &UW_EscapeMenuKeyBindings::HandleActionButton7Clicked},
		{M_ActionButton8, TEXT("M_ActionButton8"), &UW_EscapeMenuKeyBindings::HandleActionButton8Clicked},
		{M_ActionButton9, TEXT("M_ActionButton9"), &UW_EscapeMenuKeyBindings::HandleActionButton9Clicked},
		{M_ActionButton10, TEXT("M_ActionButton10"), &UW_EscapeMenuKeyBindings::HandleActionButton10Clicked},
		{M_ActionButton11, TEXT("M_ActionButton11"), &UW_EscapeMenuKeyBindings::HandleActionButton11Clicked},
		{M_ActionButton12, TEXT("M_ActionButton12"), &UW_EscapeMenuKeyBindings::HandleActionButton12Clicked},
		{M_ActionButton13, TEXT("M_ActionButton13"), &UW_EscapeMenuKeyBindings::HandleActionButton13Clicked},
		{M_ActionButton14, TEXT("M_ActionButton14"), &UW_EscapeMenuKeyBindings::HandleActionButton14Clicked},
		{M_ActionButton15, TEXT("M_ActionButton15"), &UW_EscapeMenuKeyBindings::HandleActionButton15Clicked}
	};

	for (const FActionButtonBinding& Binding : ButtonBindings)
	{
		if (Binding.Button == nullptr)
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
				this,
				Binding.ButtonName,
				TEXT("UW_EscapeMenuKeyBindings::BindActionButtons"),
				this
			);
			continue;
		}

		Binding.Button->OnClicked.RemoveAll(this);
		Binding.Button->OnClicked.AddDynamic(this, Binding.Handler);
	}
}

void UW_EscapeMenuKeyBindings::BindControlGroupButtons()
{
	struct FControlGroupButtonBinding
	{
		UButton* Button;
		const TCHAR* ButtonName;
		void (UW_EscapeMenuKeyBindings::*Handler)();
	};

	const TArray<FControlGroupButtonBinding> ButtonBindings = {
		{M_ControlGroupButton1, TEXT("M_ControlGroupButton1"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton1Clicked},
		{M_ControlGroupButton2, TEXT("M_ControlGroupButton2"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton2Clicked},
		{M_ControlGroupButton3, TEXT("M_ControlGroupButton3"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton3Clicked},
		{M_ControlGroupButton4, TEXT("M_ControlGroupButton4"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton4Clicked},
		{M_ControlGroupButton5, TEXT("M_ControlGroupButton5"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton5Clicked},
		{M_ControlGroupButton6, TEXT("M_ControlGroupButton6"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton6Clicked},
		{M_ControlGroupButton7, TEXT("M_ControlGroupButton7"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton7Clicked},
		{M_ControlGroupButton8, TEXT("M_ControlGroupButton8"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton8Clicked},
		{M_ControlGroupButton9, TEXT("M_ControlGroupButton9"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton9Clicked},
		{M_ControlGroupButton10, TEXT("M_ControlGroupButton10"), &UW_EscapeMenuKeyBindings::HandleControlGroupButton10Clicked}
	};

	for (const FControlGroupButtonBinding& Binding : ButtonBindings)
	{
		if (Binding.Button == nullptr)
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
				this,
				Binding.ButtonName,
				TEXT("UW_EscapeMenuKeyBindings::BindControlGroupButtons"),
				this
			);
			continue;
		}

		Binding.Button->OnClicked.RemoveAll(this);
		Binding.Button->OnClicked.AddDynamic(this, Binding.Handler);
	}
}

void UW_EscapeMenuKeyBindings::BuildKeyBindingEntries()
{
	if (not GetIsValidKeyBindingsList() || not GetIsValidKeyBindingEntryClass())
	{
		return;
	}

	UInputMappingContext* MappingContext = GetDefaultMappingContext();
	if (MappingContext == nullptr)
	{
		return;
	}

	M_KeyBindingsList->ClearChildren();
	M_ActionNameToEntry.Reset();
	M_ActionNameToAction.Reset();
	M_SpecialActionKeyBindings.Reset();
	M_KeyBindingEntries.Reset();

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		if (not Mapping.Action || not Mapping.Key.IsValid())
		{
			continue;
		}

		if (not GetIsValidPlayerController())
		{
			return;
		}

		UW_EscapeMenuKeyBindingEntry* EntryWidget = CreateWidget<UW_EscapeMenuKeyBindingEntry>(
			M_PlayerController.Get(),
			M_KeyBindingEntryClass
		);
		if (EntryWidget == nullptr)
		{
			continue;
		}

		UInputAction* ActionToBind = const_cast<UInputAction*>(Mapping.Action.Get());
		EntryWidget->SetupEntry(M_PlayerController.Get(), ActionToBind, Mapping.Key);
		EntryWidget->OnKeyBindingUpdated().AddUObject(this, &UW_EscapeMenuKeyBindings::HandleKeyBindingUpdated);

		const FName ActionName = ActionToBind->GetFName();
		M_ActionNameToEntry.Add(ActionName, EntryWidget);
		M_ActionNameToAction.Add(ActionName, ActionToBind);

		if (GetIsSpecialBindingAction(ActionName))
		{
			EntryWidget->SetKeyBindingValidationDelegate(
				FKeyBindingValidationDelegate::CreateUObject(this, &UW_EscapeMenuKeyBindings::ValidateSpecialBinding)
			);
			M_SpecialActionKeyBindings.Add(ActionName, Mapping.Key);
		}

		M_KeyBindingsList->AddChild(EntryWidget);
		M_KeyBindingEntries.Add({EntryWidget, EntryWidget->GetActionDisplayName()});
	}

	if (GetIsValidSearchKeyBar())
	{
		ApplySearchFilter(M_SearchKeyBar->GetText().ToString());
	}
}

void UW_EscapeMenuKeyBindings::ApplySearchFilter(const FString& SearchText)
{
	const FString SearchTextLower = SearchText.ToLower();
	const bool bHasSearchText = not SearchTextLower.IsEmpty();

	for (const FEscapeMenuKeyBindingEntryData& EntryData : M_KeyBindingEntries)
	{
		if (not IsValid(EntryData.EntryWidget))
		{
			continue;
		}

		const bool bMatches = not bHasSearchText
			|| EntryData.ActionDisplayName.ToLower().Contains(SearchTextLower);
		EntryData.EntryWidget->SetVisibility(bMatches ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UW_EscapeMenuKeyBindings::HandleActionButtonClicked(const int32 ActionButtonIndex)
{
	const FString ActionName = FString::Printf(
		TEXT("%s%d"),
		EscapeMenuKeyBindingsConstants::ActionButtonPrefix,
		ActionButtonIndex
	);
	OpenKeyBindingPopupForActionName(FName(*ActionName));
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButtonClicked(const int32 ControlGroupIndex)
{
	const FString ActionName = FString::Printf(
		TEXT("%s%d"),
		EscapeMenuKeyBindingsConstants::ControlGroupPrefix,
		ControlGroupIndex
	);
	OpenKeyBindingPopupForActionName(FName(*ActionName));
}

void UW_EscapeMenuKeyBindings::OpenKeyBindingPopupForActionName(const FName& ActionName)
{
	if (not GetIsValidPlayerController() || not GetIsValidKeyBindingPopupClass())
	{
		return;
	}

	UInputAction* ActionToBind = GetInputActionByName(ActionName);
	if (not IsValid(ActionToBind))
	{
		RTSFunctionLibrary::ReportError("Key binding popup failed to find a valid input action.");
		return;
	}

	const FKey CurrentKey = GetCurrentKeyForAction(ActionName);
	if (not CurrentKey.IsValid())
	{
		RTSFunctionLibrary::ReportError("Key binding popup failed to find a valid key for the selected action.");
		return;
	}

	if (M_KeyBindingPopup == nullptr)
	{
		M_KeyBindingPopup = CreateWidget<UW_KeyBindingPopup>(M_PlayerController.Get(), M_KeyBindingPopupClass);
	}

	if (M_KeyBindingPopup == nullptr)
	{
		RTSFunctionLibrary::ReportError("Key binding popup could not be created.");
		return;
	}

	M_KeyBindingPopup->SetupPopup(M_PlayerController.Get(), ActionToBind, CurrentKey);
	M_KeyBindingPopup->AddToViewport(EscapeMenuKeyBindingsConstants::PopupZOrder);

	if (UW_EscapeMenuKeyBindingEntry* PopupEntry = M_KeyBindingPopup->GetKeyBindingEntry())
	{
		PopupEntry->OnKeyBindingUpdated().RemoveAll(this);
		PopupEntry->OnKeyBindingUpdated().AddUObject(this, &UW_EscapeMenuKeyBindings::HandleKeyBindingUpdated);
		PopupEntry->SetKeyBindingValidationDelegate(
			FKeyBindingValidationDelegate::CreateUObject(this, &UW_EscapeMenuKeyBindings::ValidateSpecialBinding)
		);
	}
}

void UW_EscapeMenuKeyBindings::UpdateKeyBindingEntryForAction(const FName& ActionName, const FKey& NewKey)
{
	TObjectPtr<UW_EscapeMenuKeyBindingEntry>* FoundEntry = M_ActionNameToEntry.Find(ActionName);
	if (FoundEntry == nullptr || not IsValid(*FoundEntry))
	{
		return;
	}

	(*FoundEntry)->UpdateKeyBinding(NewKey);
}

void UW_EscapeMenuKeyBindings::EnsureKeyBindingPopupVisible()
{
	if (not GetIsValidPlayerController() || not GetIsValidKeyBindingPopupClass())
	{
		return;
	}

	if (M_KeyBindingPopup == nullptr)
	{
		M_KeyBindingPopup = CreateWidget<UW_KeyBindingPopup>(M_PlayerController.Get(), M_KeyBindingPopupClass);
	}

	if (M_KeyBindingPopup == nullptr)
	{
		RTSFunctionLibrary::ReportError("Key binding popup could not be created.");
		return;
	}

	M_KeyBindingPopup->AddToViewport(EscapeMenuKeyBindingsConstants::PopupZOrder);
}

FName UW_EscapeMenuKeyBindings::GetCollisionActionName(const FName& ActionName, const FKey& ProposedKey) const
{
	for (const TPair<FName, FKey>& Pair : M_SpecialActionKeyBindings)
	{
		if (Pair.Key == ActionName)
		{
			continue;
		}

		if (Pair.Value == ProposedKey)
		{
			return Pair.Key;
		}
	}

	return NAME_None;
}

FString UW_EscapeMenuKeyBindings::GetActionDisplayName(const FName& ActionName) const
{
	FString ActionLabel = ActionName.ToString();
	const FString Prefix = TEXT("IA_");
	if (ActionLabel.StartsWith(Prefix))
	{
		ActionLabel.RightChopInline(Prefix.Len());
	}

	return ActionLabel;
}

bool UW_EscapeMenuKeyBindings::GetIsSpecialBindingAction(const FName& ActionName) const
{
	const FString ActionLabel = ActionName.ToString();
	return ActionLabel.StartsWith(EscapeMenuKeyBindingsConstants::ActionButtonPrefix)
		|| ActionLabel.StartsWith(EscapeMenuKeyBindingsConstants::ControlGroupPrefix);
}

bool UW_EscapeMenuKeyBindings::ValidateSpecialBinding(UInputAction* ActionToBind, const FKey& ProposedKey)
{
	if (not IsValid(ActionToBind))
	{
		RTSFunctionLibrary::ReportError("Key binding validation received an invalid input action reference.");
		return false;
	}

	if (not ProposedKey.IsValid())
	{
		return false;
	}

	const FName ActionName = ActionToBind->GetFName();
	const FName CollisionActionName = GetCollisionActionName(ActionName, ProposedKey);
	if (CollisionActionName.IsNone())
	{
		return true;
	}

	const FKey CurrentKey = GetCurrentKeyForAction(ActionName);
	if (not CurrentKey.IsValid())
	{
		RTSFunctionLibrary::ReportError("Key binding conflict handling failed to find the current binding.");
		return false;
	}

	EnsureKeyBindingPopupVisible();
	if (M_KeyBindingPopup != nullptr)
	{
		M_KeyBindingPopup->SetupPopup(M_PlayerController.Get(), ActionToBind, CurrentKey);
		M_KeyBindingPopup->ShowCollisionMessage(GetActionDisplayName(CollisionActionName));
	}

	return false;
}

UInputAction* UW_EscapeMenuKeyBindings::GetInputActionByName(const FName& ActionName) const
{
	const TWeakObjectPtr<UInputAction>* FoundAction = M_ActionNameToAction.Find(ActionName);
	if (FoundAction != nullptr && FoundAction->IsValid())
	{
		return FoundAction->Get();
	}

	return nullptr;
}

FKey UW_EscapeMenuKeyBindings::GetCurrentKeyForAction(const FName& ActionName) const
{
	const FKey* FoundKey = M_SpecialActionKeyBindings.Find(ActionName);
	if (FoundKey != nullptr)
	{
		return *FoundKey;
	}

	return FKey();
}

UInputMappingContext* UW_EscapeMenuKeyBindings::GetDefaultMappingContext() const
{
	if (not GetIsValidPlayerController())
	{
		return nullptr;
	}

	return M_PlayerController->GetDefaultInputMappingContext();
}

bool UW_EscapeMenuKeyBindings::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("UW_EscapeMenuKeyBindings::GetIsValidPlayerController"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindings::GetIsValidMainGameUI() const
{
	if (M_MainGameUI.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_MainGameUI"),
		TEXT("UW_EscapeMenuKeyBindings::GetIsValidMainGameUI"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindings::GetIsValidKeyBindingsList() const
{
	if (IsValid(M_KeyBindingsList))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_KeyBindingsList"),
		TEXT("UW_EscapeMenuKeyBindings::GetIsValidKeyBindingsList"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindings::GetIsValidKeyBindingEntryClass() const
{
	if (M_KeyBindingEntryClass)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_KeyBindingEntryClass"),
		TEXT("UW_EscapeMenuKeyBindings::GetIsValidKeyBindingEntryClass"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindings::GetIsValidKeyBindingPopupClass() const
{
	if (M_KeyBindingPopupClass)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_KeyBindingPopupClass"),
		TEXT("UW_EscapeMenuKeyBindings::GetIsValidKeyBindingPopupClass"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindings::GetIsValidButtonBack() const
{
	if (IsValid(M_ButtonBack))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ButtonBack"),
		TEXT("UW_EscapeMenuKeyBindings::GetIsValidButtonBack"),
		this
	);
	return false;
}

bool UW_EscapeMenuKeyBindings::GetIsValidSearchKeyBar() const
{
	if (IsValid(M_SearchKeyBar))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_SearchKeyBar"),
		TEXT("UW_EscapeMenuKeyBindings::GetIsValidSearchKeyBar"),
		this
	);
	return false;
}

void UW_EscapeMenuKeyBindings::HandleBackClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuCloseKeyBindings();
}

void UW_EscapeMenuKeyBindings::HandleSearchTextChanged(const FText& NewText)
{
	ApplySearchFilter(NewText.ToString());
}

void UW_EscapeMenuKeyBindings::HandleActionButton1Clicked()
{
	constexpr int32 ActionButtonIndex = 1;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton2Clicked()
{
	constexpr int32 ActionButtonIndex = 2;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton3Clicked()
{
	constexpr int32 ActionButtonIndex = 3;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton4Clicked()
{
	constexpr int32 ActionButtonIndex = 4;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton5Clicked()
{
	constexpr int32 ActionButtonIndex = 5;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton6Clicked()
{
	constexpr int32 ActionButtonIndex = 6;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton7Clicked()
{
	constexpr int32 ActionButtonIndex = 7;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton8Clicked()
{
	constexpr int32 ActionButtonIndex = 8;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton9Clicked()
{
	constexpr int32 ActionButtonIndex = 9;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton10Clicked()
{
	constexpr int32 ActionButtonIndex = 10;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton11Clicked()
{
	constexpr int32 ActionButtonIndex = 11;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton12Clicked()
{
	constexpr int32 ActionButtonIndex = 12;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton13Clicked()
{
	constexpr int32 ActionButtonIndex = 13;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton14Clicked()
{
	constexpr int32 ActionButtonIndex = 14;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleActionButton15Clicked()
{
	constexpr int32 ActionButtonIndex = 15;
	HandleActionButtonClicked(ActionButtonIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton1Clicked()
{
	constexpr int32 ControlGroupIndex = 1;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton2Clicked()
{
	constexpr int32 ControlGroupIndex = 2;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton3Clicked()
{
	constexpr int32 ControlGroupIndex = 3;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton4Clicked()
{
	constexpr int32 ControlGroupIndex = 4;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton5Clicked()
{
	constexpr int32 ControlGroupIndex = 5;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton6Clicked()
{
	constexpr int32 ControlGroupIndex = 6;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton7Clicked()
{
	constexpr int32 ControlGroupIndex = 7;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton8Clicked()
{
	constexpr int32 ControlGroupIndex = 8;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton9Clicked()
{
	constexpr int32 ControlGroupIndex = 9;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleControlGroupButton10Clicked()
{
	constexpr int32 ControlGroupIndex = 10;
	HandleControlGroupButtonClicked(ControlGroupIndex);
}

void UW_EscapeMenuKeyBindings::HandleKeyBindingUpdated(UInputAction* ActionToBind, const FKey& NewKey)
{
	if (not IsValid(ActionToBind))
	{
		RTSFunctionLibrary::ReportError("Key binding update received an invalid input action reference.");
		return;
	}

	if (not NewKey.IsValid())
	{
		return;
	}

	const FName ActionName = ActionToBind->GetFName();
	if (GetIsSpecialBindingAction(ActionName))
	{
		M_SpecialActionKeyBindings.Add(ActionName, NewKey);
	}

	UpdateKeyBindingEntryForAction(ActionName, NewKey);

	if (M_KeyBindingPopup != nullptr && M_KeyBindingPopup->IsInViewport())
	{
		M_KeyBindingPopup->ClosePopup();
	}
}
