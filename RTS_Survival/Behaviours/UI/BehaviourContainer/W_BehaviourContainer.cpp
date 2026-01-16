// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BehaviourContainer.h"

#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Behaviours/UI/BehaviourUIData.h"
#include "RTS_Survival/Behaviours/UI/BehaviourDescription/W_BehaviourDescription.h"
#include "RTS_Survival/Behaviours/UI/BehaviourWidget/W_Behaviour.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Blueprint/WidgetTree.h"

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
                if (GetIsValidBehaviourDescription())
                {
                        M_BehaviourDescription->SetVisibility(ESlateVisibility::Hidden);
                }
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
        if (not GetIsValidBehaviourDescription())
        {
                return;
        }

        if (not GetIsValidActionUIManager())
        {
                return;
        }

        if (not NeedsVisibility())
        {
                SetVisibility(ESlateVisibility::Collapsed);
                HideUnusedBehaviourWidgets(0);
                M_BehaviourDescription->SetVisibility(ESlateVisibility::Hidden);
                return;
        }

        SetupBehavioursOnWidgets(M_PrimaryBehaviourComponent->GetBehaviours());
        SetVisibility(ESlateVisibility::Visible);
}

void UW_BehaviourContainer::OnBehaviourHovered(const bool bIsHovering, const FBehaviourUIData& BehaviourUIData)
{
        if (GetIsValidBehaviourDescription())
        {
                if (bIsHovering)
                {
                        M_BehaviourDescription->SetupDescription(BehaviourUIData);
                        M_BehaviourDescription->SetVisibility(ESlateVisibility::Visible);
                }
                else
                {
                        M_BehaviourDescription->SetVisibility(ESlateVisibility::Hidden);
                }
        }

        if (GetIsValidPrimaryBehaviourComponent())
        {
                M_PrimaryBehaviourComponent->OnBehaviourHovered(bIsHovering, BehaviourUIData);
        }
}

void UW_BehaviourContainer::SetBehaviourWidgets(TArray<UW_Behaviour*> Widgets)
{
        M_BehaviourWidgets = Widgets;
        for(auto EachWidget: Widgets)
        {
                if (not IsValid(EachWidget))
                {
                        continue;
                }
                EachWidget->InitBehaviourWidget(this);
        }
}

bool UW_BehaviourContainer::GetIsValidActionUIManager() const
{
        if (not M_ActionUIManager.IsValid())
        {
		RTSFunctionLibrary::ReportError(
			TEXT("UW_BehaviourContainer::GetIsValidActionUIManager: M_ActionUIManager is not valid!"));
		return false;
	}
	return true;
}

bool UW_BehaviourContainer::GetIsValidBehaviourDescription() const
{
	if (not M_BehaviourDescription.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UW_BehaviourContainer::GetIsValidBehaviourDescription: M_BehaviourDescription is not valid!"));
		return false;
	}
	return true;
}

bool UW_BehaviourContainer::GetIsValidPrimaryBehaviourComponent() const
{
        if (M_PrimaryBehaviourComponent.IsValid())
        {
                return true;
        }

        RTSFunctionLibrary::ReportError(
                TEXT("UW_BehaviourContainer::GetIsValidPrimaryBehaviourComponent: M_PrimaryBehaviourComponent is not valid!"));
        return false;
}

bool UW_BehaviourContainer::NeedsVisibility() const
{
        if (not GetIsValidPrimaryBehaviourComponent())
        {
                return false;
        }
        if (M_PrimaryBehaviourComponent->GetBehaviours().IsEmpty())
        {
                return false;
        }
        return true;
}



void UW_BehaviourContainer::HideUnusedBehaviourWidgets(const int32 StartIndex)
{
        for (int32 WidgetIndex = StartIndex; WidgetIndex < M_BehaviourWidgets.Num(); WidgetIndex++)
        {
                if (IsValid(M_BehaviourWidgets[WidgetIndex]))
                {
                        M_BehaviourWidgets[WidgetIndex]->SetVisibility(ESlateVisibility::Collapsed);
                }
        }
}

void UW_BehaviourContainer::SetupBehavioursOnWidgets(const TArray<UBehaviour*>& Behaviours)
{
        int32 BehaviourWidgetIndex = 0;

        for (; BehaviourWidgetIndex < M_BehaviourWidgets.Num() && BehaviourWidgetIndex < Behaviours.Num(); BehaviourWidgetIndex++)
        {
                UW_Behaviour* BehaviourWidget = M_BehaviourWidgets[BehaviourWidgetIndex];
                if (not IsValid(BehaviourWidget))
                {
                        continue;
                }

                if (not IsValid(Behaviours[BehaviourWidgetIndex]))
                {
                        BehaviourWidget->SetVisibility(ESlateVisibility::Collapsed);
                        continue;
                }

                FBehaviourUIData BehaviourUIData;
                Behaviours[BehaviourWidgetIndex]->GetUIData(BehaviourUIData);
                BehaviourWidget->SetupBehaviourWidget(BehaviourUIData);
                BehaviourWidget->SetVisibility(ESlateVisibility::Visible);
        }

        HideUnusedBehaviourWidgets(BehaviourWidgetIndex);
}
