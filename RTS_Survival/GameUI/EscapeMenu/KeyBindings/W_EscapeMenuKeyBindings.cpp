#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_EscapeMenuKeyBindings.h"

#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "InputMappingContext.h"
#include "RTS_Survival/GameUI/EscapeMenu/KeyBindings/W_EscapeMenuKeyBindingEntry.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

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

		EntryWidget->SetupEntry(
			M_PlayerController.Get(),
			const_cast<UInputAction*>(Mapping.Action.Get()),
			Mapping.Key
		);
		M_KeyBindingsList->AddChild(EntryWidget);
	}
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

void UW_EscapeMenuKeyBindings::HandleBackClicked()
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuCloseKeyBindings();
}
