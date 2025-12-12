// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_NomadicLayout.h"

#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "NomadicLayoutBuilding/W_NomadicLayoutBuilding.h"
#include "NomadicLayoutBuilding/NomadicBuildingLayoutData/NomadicBuildingLayoutData.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_NomadicLayout::InitNomadicLayout(const TArray<FNomadicBuildingLayoutData>& BuildingsData,
                                         TSubclassOf<UW_NomadicLayoutBuilding> BuildingLayoutClass,
                                         TSubclassOf<UW_RTSCard> InCardClass, TObjectPtr<UW_CardMenu> InCardMenu)
{
	if (not IsValid(InCardClass))
	{
		RTSFunctionLibrary::PrintString("Cannot initialize Nomadic Layout, InCardClass is not valid");
		return;
	}
	if (not IsValid(BuildingLayoutClass))
	{
		RTSFunctionLibrary::PrintString("Cannot initialize Nomadic Layout, BuildingLayoutClass is not valid");
		return;
	}
	if (not GetIsScrollBoxValid())
	{
		return;
	}
	for (const auto EachData : BuildingsData)
	{
		UW_NomadicLayoutBuilding* BuildingLayoutWidget = CreateWidget<UW_NomadicLayoutBuilding>(
			GetWorld(), BuildingLayoutClass);
		if (not IsValid(BuildingLayoutWidget))
		{
			RTSFunctionLibrary::ReportError("Could not create building layout widget in NomadicLayout");
			continue;
		}
		int32 IndexInHzBox = 0;
		// This also add the hz box to the class private array of hz boxes if we did need to create a new one
		// (max two building layouts per hz box)
		if (UHorizontalBox* HzBox = GetVacantHorizontalBox(IndexInHzBox); IsValid(HzBox))
		{
			BuildingLayoutWidget->InitNomadicLayoutBuilding(EachData, InCardClass, InCardMenu, IndexInHzBox == 0);
			HzBox->AddChild(BuildingLayoutWidget);
			M_BuildingLayoutWidgets.Add(BuildingLayoutWidget);
		}
	}
}

bool UW_NomadicLayout::CheckLayoutsFilled(uint32& OutEmptySlots,
                                          TArray<ECardType>& OutUnfilledLayouts, TArray<UW_RTSCard*>& OutSelectedCards,
                                          const TMap<ECardType, TArray<ERTSCard>>& InCardsLeftPerLayout)
{
	bool bAllFilled = true;
	for (auto EachLayout : M_BuildingLayoutWidgets)
	{
		if (not IsValid(EachLayout))
		{
			continue;
		}
		ECardType LayoutType = EachLayout->GetCardType();
		for (auto SelectedCard : EachLayout->GetSelectedCards())
		{
			OutSelectedCards.Add(SelectedCard);
		}
		if (LayoutType == ECardType::Invalid)
		{
			continue;
		}
		RTSFunctionLibrary::PrintToLog("for layout amount of non empty cards:" + FString::FromInt(EachLayout->GetNonEmptyCardsHeld().Num()), false);
		const uint32 EmptySlots = EachLayout->GetMaxCardsToHold() - EachLayout->GetNonEmptyCardsHeld().Num();
		if (EmptySlots && InCardsLeftPerLayout.Contains(LayoutType))
		{
			if (InCardsLeftPerLayout[LayoutType].Num() > 0)
			{
				OutEmptySlots += EmptySlots;
				OutUnfilledLayouts.Add(LayoutType);
				bAllFilled = false;
			}
		}
	}
	return bAllFilled;
}

TArray<FNomadicBuildingLayoutData> UW_NomadicLayout::GetBuildingLayoutData() const
{
	TArray<FNomadicBuildingLayoutData> TBuildingData;
	for (auto EachLayout : M_BuildingLayoutWidgets)
	{
		if (IsValid(EachLayout))
		{
			TBuildingData.Add(EachLayout->GetBuildingLayoutData());
		}
	}
	return TBuildingData;
}


TArray<ECardType> UW_NomadicLayout::GetAllLayouts() const
{
	TArray<ECardType> Layouts;
	for (auto EachLayout : M_BuildingLayoutWidgets)
	{
		if (IsValid(EachLayout))
		{
			if (EachLayout->GetCardTypesAllowed().Num() > 0)
			{
				Layouts.Add(EachLayout->GetCardTypesAllowed().Array()[0]);
				continue;
			}
			RTSFunctionLibrary::ReportError(
				"No Card types allowed are set on one of the Building layouts in Nomadic Layout"
				"\n layout: " + EachLayout->GetName());
		}
	}
	return Layouts;
}


UHorizontalBox* UW_NomadicLayout::GetVacantHorizontalBox(int32& OutIndexInHzBox)
{
	if (not GetIsScrollBoxValid())
	{
		return nullptr;
	}
	UHorizontalBox* LastBox = M_HorizontalBoxes.IsEmpty() ? nullptr : M_HorizontalBoxes.Last();
	if (!IsValid(LastBox) || LastBox->GetChildrenCount() >= 2)
	{
		UHorizontalBox* NewBox = NewObject<UHorizontalBox>(this);
		if (IsValid(NewBox))
		{
			M_HorizontalBoxes.Add(NewBox);

			OutIndexInHzBox = 0;
			BuildingLayoutScrollBox->AddChild(NewBox);
			return NewBox;
		}
		RTSFunctionLibrary::ReportError(
			"Failed to create new horizontal box in UW_CardScrollBox::GetVacantHorizontalBox");
		return nullptr;
	}
	OutIndexInHzBox = LastBox->GetChildrenCount();
	return LastBox;
}


bool UW_NomadicLayout::GetIsScrollBoxValid() const
{
	if (IsValid(BuildingLayoutScrollBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Invalid scroll box in Nomadic Layout");
	return false;
}
