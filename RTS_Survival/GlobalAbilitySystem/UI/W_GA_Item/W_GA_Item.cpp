// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GA_Item.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"

void UW_GA_Item::SetupGa_Item(const TWeakObjectPtr<UGlobalAbility> GlobalAbility, UGlobalAbilitiesManager* GlobalAbilityManager)
{
	M_GlobalAbility = GlobalAbility;
	(void)EnsureIsValidAbility();
	M_GlobalAbilityManager = GlobalAbilityManager;
	(void)EnsureIsValidAbilityManager();
	if (IsValid(AbilityButton))
	{
		M_OriginalButtonStyle = AbilityButton->GetStyle();
		bM_HasOriginalButtonStyle = true;
	}
}

void UW_GA_Item::OnAbilityHovered(UGlobalAbility* HoveredAbility, const bool bIsHover)
{
	if (not EnsureIsValidAbilityManager())
	{
		return;
	}
	M_GlobalAbilityManager.Get()->OnHoveredAbilityButton(HoveredAbility, bIsHover);
}

void UW_GA_Item::SetAbilityAvailable(const bool bIsEnabled, const bool bUseGreyTint)
{
	if (not IsValid(AbilityButton))
	{
		return;
	}
	AbilityButton->SetIsEnabled(bIsEnabled);
	if (not bUseGreyTint && bM_HasOriginalButtonStyle)
	{
		AbilityButton->SetStyle(M_OriginalButtonStyle);
		return;
	}
	FButtonStyle GreyButtonStyle = AbilityButton->GetStyle();
	constexpr float UnavailableTintChannel = 0.25f;
	const FSlateColor GreyTint(FLinearColor(
		UnavailableTintChannel,
		UnavailableTintChannel,
		UnavailableTintChannel,
		1.f));
	GreyButtonStyle.Normal.TintColor = GreyTint;
	GreyButtonStyle.Hovered.TintColor = GreyTint;
	GreyButtonStyle.Pressed.TintColor = GreyTint;
	AbilityButton->SetStyle(GreyButtonStyle);
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

bool UW_GA_Item::EnsureIsValidAbility()
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
