// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GA_Item.h"

#include "RTS_Survival/GameUI/CooldownItem/W_CoolDownItem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GameUI/Hotkey/W_HotKey.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyTypes.h"

namespace GA_ItemHotkeyLabel
{
	// Rows in DT_RichText: a small dimmed modifier next to the bright key, like a key cap legend.
	const TCHAR* ModifierStyle = TEXT("Text_GAHotkeyMod");
	const TCHAR* KeyStyle = TEXT("Text_GAHotkeyKey");
}

void UW_GA_Item::SetupGa_Item(const TWeakObjectPtr<UGlobalAbility> GlobalAbility, UGlobalAbilitiesManager* GlobalAbilityManager)
{
	M_GlobalAbility = GlobalAbility;
	M_GlobalAbilityManager = GlobalAbilityManager;

	if (not EnsureIsValidAbility() || not EnsureIsValidAbilityManager())
	{
		return;
	}

	SetupAbilityButton();
}

void UW_GA_Item::OnAbilityHovered(UGlobalAbility* HoveredAbility, const bool bIsHover)
{
	if (not EnsureIsValidAbilityManager())
	{
		return;
	}
	M_GlobalAbilityManager.Get()->OnHoveredAbilityButton(HoveredAbility, bIsHover);
}

void UW_GA_Item::SetAbilityAvailable(const bool bIsGaEnabled, const bool bUseGreyTint) const
{
	UButton* const CooldownButton = GetCooldownButton();

	if (not IsValid(CooldownButton))
	{
		return;
	}

	CooldownButton->SetIsEnabled(bIsGaEnabled);

	if (not bUseGreyTint && bM_HasOriginalButtonStyle)
	{
		CooldownButton->SetStyle(M_OriginalButtonStyle);
		return;
	}

	FButtonStyle GreyButtonStyle = CooldownButton->GetStyle();
	constexpr float UnavailableTintChannel = 0.25f;
	const FSlateColor GreyTint(FLinearColor(
		UnavailableTintChannel,
		UnavailableTintChannel,
		UnavailableTintChannel,
		1.f));
	GreyButtonStyle.Normal.TintColor = GreyTint;
	GreyButtonStyle.Hovered.TintColor = GreyTint;
	GreyButtonStyle.Pressed.TintColor = GreyTint;
	CooldownButton->SetStyle(GreyButtonStyle);
}

void UW_GA_Item::RefreshCooldownFromLoadedAbility() const
{
	if (not EnsureIsValidAbility() || not EnsureIsValidAbilityButton())
	{
		return;
	}

	if (not AbilityButton->GetWasInitialized(false))
	{
		return;
	}

	const FGLobalAbilityCostState& AbilityCosts = M_GlobalAbility.Get()->GetAbilityCosts();

	if (AbilityCosts.CoolDownRemaining <= 0)
	{
		AbilityButton->InstantlyResetCooldown();
		return;
	}

	if (AbilityButton->GetIsOnCoolDown())
	{
		return;
	}

	AbilityButton->SetCooldownState(
		static_cast<float>(AbilityCosts.CoolDownTime),
		static_cast<float>(AbilityCosts.CoolDownRemaining));
}

void UW_GA_Item::SetHotkeySlotIndex(const int32 SlotIndex)
{
	if (not GetIsValidHotkeyWidget())
	{
		return;
	}

	HotkeyWidget->ShowChordedAction(
		RTSHotkeyTypes::GetGlobalAbilityActionName(SlotIndex),
		GA_ItemHotkeyLabel::ModifierStyle,
		GA_ItemHotkeyLabel::KeyStyle);
}

bool UW_GA_Item::TryActivateFromHotkey()
{
	// Unused slots are collapsed and hold no ability; neither is an error for a hotkey press.
	if (not IsVisible() || not M_GlobalAbility.IsValid())
	{
		return false;
	}

	// The button is disabled while the ability is unaffordable, on cooldown or locked: same gate as a mouse click.
	const UButton* const CooldownButton = GetCooldownButton();
	if (not IsValid(CooldownButton) || not CooldownButton->GetIsEnabled())
	{
		return false;
	}

	OnClickedAbilityButton();
	return true;
}

void UW_GA_Item::NativeDestruct()
{
	UnbindAbilityButtonCallbacks();
	Super::NativeDestruct();
}

bool UW_GA_Item::GetIsValidHotkeyWidget() const
{
	if (IsValid(HotkeyWidget.Get()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("HotkeyWidget"),
		TEXT("UW_GA_Item::GetIsValidHotkeyWidget"),
		this
	);
	return false;
}

void UW_GA_Item::OnClickedAbilityButton()
{
	if (not EnsureIsValidAbility() || not EnsureIsValidAbilityManager())
	{
		return;
	}
	M_GlobalAbilityManager.Get()->OnClickedAbilityButton(M_GlobalAbility.Get());
}

void UW_GA_Item::OnHoveredAbilityButton(const bool bIsHover)
{
	if (not EnsureIsValidAbility())
	{
		return;
	}
	OnAbilityHovered(M_GlobalAbility.Get(), bIsHover);
}

void UW_GA_Item::HandleAbilityButtonHovered()
{
	OnHoveredAbilityButton(true);
}

void UW_GA_Item::HandleAbilityButtonUnhovered()
{
	OnHoveredAbilityButton(false);
}

bool UW_GA_Item::EnsureIsValidAbility() const
{
	if (not M_GlobalAbility.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GlobalAbility"),
			TEXT("UW_GA_Item::EnsureIsValidAbility"),
			this
		);
		return false;
	}
	return true;
}

void UW_GA_Item::SetupAbilityButton()
{
	if (not EnsureIsValidAbilityButton() || not EnsureIsValidAbility())
	{
		return;
	}

	const FGlobalAbilityUISettings& AbilityUISettings = M_GlobalAbility.Get()->GetUISettings();
	const FGLobalAbilityCostState& AbilityCosts = M_GlobalAbility.Get()->GetAbilityCosts();

	AbilityButton->Init(
		TWeakObjectPtr<UObject>(this),
		AbilityUISettings.AbilityIcon.Get(),
		static_cast<float>(AbilityCosts.CoolDownTime),
		false);

	UButton* const CooldownButton = GetCooldownButton();

	if (not IsValid(CooldownButton))
	{
		return;
	}

	M_OriginalButtonStyle = CooldownButton->GetStyle();
	bM_HasOriginalButtonStyle = true;

	BindAbilityButtonCallbacks();
	RefreshCooldownFromLoadedAbility();
}

void UW_GA_Item::BindAbilityButtonCallbacks()
{
	UButton* const CooldownButton = GetCooldownButton();

	if (not IsValid(CooldownButton))
	{
		return;
	}

	CooldownButton->OnClicked.RemoveDynamic(this, &UW_GA_Item::OnClickedAbilityButton);
	CooldownButton->OnHovered.RemoveDynamic(this, &UW_GA_Item::HandleAbilityButtonHovered);
	CooldownButton->OnUnhovered.RemoveDynamic(this, &UW_GA_Item::HandleAbilityButtonUnhovered);

	CooldownButton->OnClicked.AddDynamic(this, &UW_GA_Item::OnClickedAbilityButton);
	CooldownButton->OnHovered.AddDynamic(this, &UW_GA_Item::HandleAbilityButtonHovered);
	CooldownButton->OnUnhovered.AddDynamic(this, &UW_GA_Item::HandleAbilityButtonUnhovered);
}

void UW_GA_Item::UnbindAbilityButtonCallbacks()
{
	if (not IsValid(AbilityButton.Get()))
	{
		return;
	}

	UButton* const CooldownButton = AbilityButton->GetButton();

	if (not IsValid(CooldownButton))
	{
		return;
	}

	CooldownButton->OnClicked.RemoveDynamic(this, &UW_GA_Item::OnClickedAbilityButton);
	CooldownButton->OnHovered.RemoveDynamic(this, &UW_GA_Item::HandleAbilityButtonHovered);
	CooldownButton->OnUnhovered.RemoveDynamic(this, &UW_GA_Item::HandleAbilityButtonUnhovered);
}

UButton* UW_GA_Item::GetCooldownButton() const
{
	if (not EnsureIsValidAbilityButton())
	{
		return nullptr;
	}

	return AbilityButton->GetButton();
}

bool UW_GA_Item::EnsureIsValidAbilityButton() const
{
	if (IsValid(AbilityButton.Get()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("AbilityButton"),
		TEXT("UW_GA_Item::EnsureIsValidAbilityButton"),
		this
	);
	return false;
}

bool UW_GA_Item::EnsureIsValidAbilityManager()
{
	if (not M_GlobalAbilityManager.IsValid())
	{
		
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GlobalAbilityManager"),
			TEXT("UW_GA_Item::EnsureIsValidAbilityManager"),
			this
		);
		return false;
	}
	return true;
}
