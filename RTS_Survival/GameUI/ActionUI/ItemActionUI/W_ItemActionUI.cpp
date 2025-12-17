// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_ItemActionUI.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/Player/CPPController.h"

void UW_ItemActionUI::UpdateItemActionUI(const EAbilityID NewAbility, const int32 CustomType)
{
	M_Ability = NewAbility;
	M_CustomType = CustomType;
	OnUpdateActionUI(NewAbility, CustomType);
}

void UW_ItemActionUI::InitActionUIElement(
	ACPPController* PlayerController,
	const int32 IndexActionUIElm, UActionUIManager* ActionUIManager)
{
	M_Index = IndexActionUIElm;
	M_PlayerController = PlayerController;
	M_ActionUIManager = ActionUIManager;
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
	if (IsValid(M_PlayerController))
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
	if (not IsValid(M_ActionUIManager))
	{
		RTSFunctionLibrary::ReportError("Action UI Manager reference invalid in UW_ItemActionUI");
		return false;
	}
	return true;
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
