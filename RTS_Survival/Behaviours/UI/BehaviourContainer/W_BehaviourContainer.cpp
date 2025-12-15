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

        CollectBehaviourWidgets();
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

        CollectBehaviourWidgets();
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
        if (not GetIsValidBehaviourDescription())
        {
                return;
        }

        if (bIsHovering)
        {
                M_BehaviourDescription->SetupDescription(BehaviourUIData);
                M_BehaviourDescription->SetVisibility(ESlateVisibility::Visible);
                return;
        }

        M_BehaviourDescription->SetVisibility(ESlateVisibility::Hidden);
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

void UW_BehaviourContainer::CollectBehaviourWidgets()
{
        if (M_BehaviourWidgets.Num() > 0)
        {
                return;
        }

        if (not WidgetTree)
        {
                RTSFunctionLibrary::ReportError(
                        TEXT("UW_BehaviourContainer::CollectBehaviourWidgets: WidgetTree is null!"));
                return;
        }

        TArray<UWidget*> FoundWidgets;
        WidgetTree->GetAllWidgetsOfClass(FoundWidgets, UW_Behaviour::StaticClass(), true);
        for (UWidget* Widget : FoundWidgets)
        {
                if (UW_Behaviour* BehaviourWidget = Cast<UW_Behaviour>(Widget))
                {
                        BehaviourWidget->InitBehaviourWidget(this);
                        M_BehaviourWidgets.Add(BehaviourWidget);
                }
        }
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

void UW_BehaviourContainer::SetupBehavioursOnWidgets(const TArray<TObjectPtr<UBehaviour>>& Behaviours)
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
