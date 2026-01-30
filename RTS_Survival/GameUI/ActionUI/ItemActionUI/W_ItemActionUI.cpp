// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_ItemActionUI.h"

#include "Components/Button.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/GameUI/Hotkey/W_HotKey.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyProviderSubsystem.h"

void UW_ItemActionUI::UpdateItemActionUI(const EAbilityID NewAbility, const int32 CustomType, const int32 CoolDownRemaining, const int32 CooldownTotalDuration)
{
	M_Ability = NewAbility;
	M_CustomType = CustomType;
	OnUpdateActionUI(NewAbility, CustomType, CoolDownRemaining, CooldownTotalDuration);
}

void UW_ItemActionUI::InitActionUIElement(
	ACPPController* PlayerController,
	const int32 IndexActionUIElm, UActionUIManager* ActionUIManager)
{
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError("UW_ItemActionUI received an invalid player controller during init.");
		return;
	}

	M_Index = IndexActionUIElm;
	M_PlayerController = PlayerController;
	M_ActionUIManager = ActionUIManager;

	CacheHotkeyProviderSubsystem();
	UpdateHotkeyFromProvider();
	BindHotkeyUpdateDelegate();
}

void UW_ItemActionUI::SetActionButtonHotkeyHidden(const bool bHideActionButtonHotkey) const
{
	if (not GetIsValidActionItemHotKey())
	{
		return;
	}

	const ESlateVisibility NewVisibility = bHideActionButtonHotkey ? ESlateVisibility::Hidden : ESlateVisibility::Visible;
	M_ActionItemHotKey->SetVisibility(NewVisibility);
}

void UW_ItemActionUI::OnActionUIClicked()
{
	if (GetIsValidPlayerController())
	{
		M_PlayerController->ActivateActionButton(M_Index);
	}
}

void UW_ItemActionUI::OnActionUIHover(const bool bIsHover) const
{
	if (GetIsValidActionUIManager())
	{
		M_ActionUIManager->OnHoverActionUIItem(bIsHover);
	}
}

bool UW_ItemActionUI::GetIsValidPlayerController()
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	if (APlayerController* NewController = UGameplayStatics::GetPlayerController(this, 0);
		IsValid(NewController))
	{
		if (ACPPController* Pc = Cast<ACPPController>(NewController))
		{
			if (IsValid(Pc))
			{
				M_PlayerController = Pc;
				return true;
			}
		}
	}
	RTSFunctionLibrary::ReportError("Not able to find valid controller for UW_ItemActionUI");
	return false;
}

bool UW_ItemActionUI::GetIsValidActionUIManager() const
{
	if (not M_ActionUIManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ActionUIManager"),
			TEXT("UW_ItemActionUI::GetIsValidActionUIManager"),
			this
		);
		return false;
	}
	return true;
}

void UW_ItemActionUI::NativeDestruct()
{
	UnbindHotkeyUpdateDelegate();
	Super::NativeDestruct();
}

void UW_ItemActionUI::UpdateButtonWithGlobalSlateStyle()
{
	if (ButtonStyleAsset)
	{
		const FButtonStyle* ButtonStyle = ButtonStyleAsset->GetStyle<FButtonStyle>();
		if (ButtonStyle && M_ActionItemButton)
		{
			M_ActionItemButton->SetStyle(*ButtonStyle);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("ButtonStyle null."
			"\n at widget: " + GetName() +
			"\n Forgot to set style reference in UW_ItemActionUI::UpdateButtonWithGlobalSlateStyle?");
	}
}

void UW_ItemActionUI::CacheHotkeyProviderSubsystem()
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = M_PlayerController->GetLocalPlayer();
	if (not IsValid(LocalPlayer))
	{
		RTSFunctionLibrary::ReportError("UW_ItemActionUI could not resolve a local player for hotkey lookup.");
		return;
	}

	URTSHotkeyProviderSubsystem* HotkeyProviderSubsystem = LocalPlayer->GetSubsystem<URTSHotkeyProviderSubsystem>();
	if (not IsValid(HotkeyProviderSubsystem))
	{
		RTSFunctionLibrary::ReportError("UW_ItemActionUI could not resolve the hotkey provider subsystem.");
		return;
	}

	M_HotkeyProviderSubsystem = HotkeyProviderSubsystem;
}

void UW_ItemActionUI::BindHotkeyUpdateDelegate()
{
	if (not GetIsValidHotkeyProviderSubsystem())
	{
		return;
	}

	M_ActionSlotHotkeyHandle = M_HotkeyProviderSubsystem->OnActionSlotHotkeyUpdated().AddUObject(
		this,
		&UW_ItemActionUI::HandleActionSlotHotkeyUpdated
	);
}

void UW_ItemActionUI::UnbindHotkeyUpdateDelegate()
{
	if (not GetIsValidHotkeyProviderSubsystem())
	{
		return;
	}

	if (M_ActionSlotHotkeyHandle.IsValid())
	{
		M_HotkeyProviderSubsystem->OnActionSlotHotkeyUpdated().Remove(M_ActionSlotHotkeyHandle);
		M_ActionSlotHotkeyHandle.Reset();
	}
}

void UW_ItemActionUI::UpdateHotkeyFromProvider()
{
	if (not GetIsValidHotkeyProviderSubsystem() || not GetIsValidActionItemHotKey())
	{
		return;
	}

	const FText HotkeyText = M_HotkeyProviderSubsystem->GetDisplayKeyForActionSlot(M_Index);
	M_ActionItemHotKey->SetKeyText(HotkeyText);
}

void UW_ItemActionUI::HandleActionSlotHotkeyUpdated(const int32 ActionSlotIndex, const FText& HotkeyText)
{
	if (ActionSlotIndex != M_Index)
	{
		return;
	}

	if (not GetIsValidActionItemHotKey())
	{
		return;
	}

	M_ActionItemHotKey->SetKeyText(HotkeyText);
}

bool UW_ItemActionUI::GetIsValidActionItemHotKey() const
{
	if (IsValid(M_ActionItemHotKey))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ActionItemHotKey"),
		TEXT("UW_ItemActionUI::GetIsValidActionItemHotKey"),
		this
	);
	return false;
}

bool UW_ItemActionUI::GetIsValidHotkeyProviderSubsystem() const
{
	if (M_HotkeyProviderSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_HotkeyProviderSubsystem"),
		TEXT("UW_ItemActionUI::GetIsValidHotkeyProviderSubsystem"),
		this
	);
	return false;
}
