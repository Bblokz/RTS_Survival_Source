// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BehaviourContainer.h"

#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Behaviours/UI/BehaviourDescription/W_BehaviourDescription.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"

void UW_BehaviourContainer::InitBehaviourContainer(UW_BehaviourDescription* InBehaviourDescription,
                                                   UActionUIManager* InActionUIManger)
{
	M_BehaviourDescription = InBehaviourDescription;
	(void)GetIsValidBehaviourDescription();

	M_ActionUIManager = InActionUIManger;
	(void)GetIsValidActionUIManager();
}

void UW_BehaviourContainer::OnAmmoPickerVisiblityChange(const bool bIsVisible)
{
	if (bIsVisible)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (NeedsVisibility())
	{
		// the ammo picker is hidden, and we have behaviours to show.
		SetVisibility(ESlateVisibility::Visible);
	}
}

void UW_BehaviourContainer::SetupBehaviourContainerForSelectedUnit(UBehaviourComp* PrimarySelectedBehaviourComp)
{
	M_PrimaryBehaviourComponent = PrimarySelectedBehaviourComp;
	if (not NeedsVisibility())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::Visible);
}

bool UW_BehaviourContainer::GetIsValidActionUIManager() const
{
	if (not IsValid(M_ActionUIManager))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UW_BehaviourContainer::GetIsValidActionUIManager: M_ActionUIManager is not valid!"));
		return false;
	}
	return true;
}

bool UW_BehaviourContainer::GetIsValidBehaviourDescription() const
{
	if (not IsValid(M_BehaviourDescription))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UW_BehaviourContainer::GetIsValidBehaviourDescription: M_BehaviourDescription is not valid!"));
		return false;
	}
	return true;
}

bool UW_BehaviourContainer::NeedsVisibility() const
{
	if (not M_PrimaryBehaviourComponent.IsValid())
	{
		return false;
	}
	if (M_PrimaryBehaviourComponent->GetBehaviours().IsEmpty())
	{
		return false;
	}
	return true;
}
