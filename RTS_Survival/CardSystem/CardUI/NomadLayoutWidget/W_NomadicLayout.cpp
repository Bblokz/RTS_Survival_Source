// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_NomadicLayout.h"

#include "Components/HorizontalBox.h"
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

	M_BuildingLayoutsData = BuildingsData;
	M_BuildingLayoutClass = BuildingLayoutClass;
	M_CardClass = InCardClass;
	M_CardMenu = InCardMenu;
	M_CurrentLayoutProfile = ELayoutProfileWidgets::Widgets_None;
	M_CurrentBuildingType = ENomadicLayoutBuildingType::Building_None;

	if (not IsValid(M_BuildingLayoutWidget))
	{
		M_BuildingLayoutWidget = CreateWidget<UW_NomadicLayoutBuilding>(
			GetWorld(), BuildingLayoutClass);
		if (not IsValid(M_BuildingLayoutWidget))
		{
			RTSFunctionLibrary::ReportError("Could not create building layout widget in NomadicLayout");
			return;
		}
	}

	BuildingLayoutBox->ClearChildren();
	BuildingLayoutBox->AddChild(M_BuildingLayoutWidget);

	SetFocusedLayoutProfile(ELayoutProfileWidgets::Widgets_BarracksTrain);
}

bool UW_NomadicLayout::CheckLayoutsFilled(uint32& OutEmptySlots,
                                          TArray<ECardType>& OutUnfilledLayouts, TArray<UW_RTSCard*>& OutSelectedCards,
                                          const TMap<ECardType, TArray<ERTSCard>>& InCardsLeftPerLayout)
{
	UpdateFocusedBuildingData();
	bool bAllFilled = true;
	for (const FNomadicBuildingLayoutData& EachLayout : M_BuildingLayoutsData)
	{
		const ECardType LayoutType = GetCardTypeFromBuildingType(EachLayout.BuildingType);
		if (LayoutType == ECardType::Invalid)
		{
			continue;
		}
		RTSFunctionLibrary::PrintToLog("for layout amount of non empty cards:" + FString::FromInt(EachLayout.Cards.Num()), false);
		const uint32 EmptySlots = EachLayout.Slots - EachLayout.Cards.Num();
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
	AppendSelectedCardsFromLayouts(OutSelectedCards);
	return bAllFilled;
}

TArray<FNomadicBuildingLayoutData> UW_NomadicLayout::GetBuildingLayoutData() const
{
	TArray<FNomadicBuildingLayoutData> BuildingData = M_BuildingLayoutsData;
	if (IsValid(M_BuildingLayoutWidget))
	{
		const int32 DataIndex = GetBuildingLayoutIndex(M_CurrentBuildingType);
		if (DataIndex != INDEX_NONE)
		{
			BuildingData[DataIndex] = M_BuildingLayoutWidget->GetBuildingLayoutData();
		}
	}
	return BuildingData;
}


TArray<ECardType> UW_NomadicLayout::GetAllLayouts() const
{
	TArray<ECardType> Layouts;
	for (const auto& EachLayout : M_BuildingLayoutsData)
	{
		const ECardType LayoutType = GetCardTypeFromBuildingType(EachLayout.BuildingType);
		if (LayoutType != ECardType::Invalid)
		{
			Layouts.Add(LayoutType);
			continue;
		}
		RTSFunctionLibrary::ReportError(
			"No Card types allowed are set on one of the Building layouts in Nomadic Layout"
			"\n layout: " + Global_GetNomadicLayoutBuildingTypeString(EachLayout.BuildingType));
	}
	return Layouts;
}

void UW_NomadicLayout::SetFocusedLayoutProfile(const ELayoutProfileWidgets NewLayoutProfile)
{
	const ENomadicLayoutBuildingType BuildingType = GetBuildingTypeFromLayoutProfile(NewLayoutProfile);
	if (BuildingType == ENomadicLayoutBuildingType::Building_None)
	{
		return;
	}
	if (M_CurrentLayoutProfile == NewLayoutProfile)
	{
		return;
	}
	UpdateFocusedBuildingData();
	const int32 DataIndex = GetBuildingLayoutIndex(BuildingType);
	if (DataIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(
			"Missing building layout data in UW_NomadicLayout::SetFocusedLayoutProfile.");
		return;
	}
	if (not IsValid(M_BuildingLayoutWidget))
	{
		return;
	}
	if (not IsValid(M_CardClass))
	{
		RTSFunctionLibrary::ReportError("Missing card class in UW_NomadicLayout::SetFocusedLayoutProfile.");
		return;
	}
	M_BuildingLayoutWidget->InitNomadicLayoutBuilding(
		M_BuildingLayoutsData[DataIndex],
		M_CardClass,
		M_CardMenu.Get(),
		true);
	M_CurrentLayoutProfile = NewLayoutProfile;
	M_CurrentBuildingType = BuildingType;
}

void UW_NomadicLayout::UpdateFocusedBuildingData()
{
	if (not IsValid(M_BuildingLayoutWidget))
	{
		return;
	}
	if (M_CurrentBuildingType == ENomadicLayoutBuildingType::Building_None)
	{
		return;
	}
	const int32 DataIndex = GetBuildingLayoutIndex(M_CurrentBuildingType);
	if (DataIndex == INDEX_NONE)
	{
		return;
	}
	M_BuildingLayoutsData[DataIndex] = M_BuildingLayoutWidget->GetBuildingLayoutData();
}

int32 UW_NomadicLayout::GetBuildingLayoutIndex(const ENomadicLayoutBuildingType BuildingType) const
{
	for (int32 LayoutIndex = 0; LayoutIndex < M_BuildingLayoutsData.Num(); ++LayoutIndex)
	{
		if (M_BuildingLayoutsData[LayoutIndex].BuildingType == BuildingType)
		{
			return LayoutIndex;
		}
	}
	return INDEX_NONE;
}

ENomadicLayoutBuildingType UW_NomadicLayout::GetBuildingTypeFromLayoutProfile(
	const ELayoutProfileWidgets LayoutProfile) const
{
	switch (LayoutProfile)
	{
	case ELayoutProfileWidgets::Widgets_BarracksTrain:
		return ENomadicLayoutBuildingType::Building_Barracks;
	case ELayoutProfileWidgets::Widgets_MechanisedDepotTrain:
		return ENomadicLayoutBuildingType::Building_MechanicalDepot;
	case ELayoutProfileWidgets::Widgets_ForgeTrain:
		return ENomadicLayoutBuildingType::Building_Forge;
	case ELayoutProfileWidgets::Widgets_T2FactoryTrain:
		return ENomadicLayoutBuildingType::Building_T2Factory;
	case ELayoutProfileWidgets::Widgets_AirbaseTrain:
		return ENomadicLayoutBuildingType::Building_Airbase;
	case ELayoutProfileWidgets::Widgets_ExperimentalFactoryTrain:
		return ENomadicLayoutBuildingType::Building_Experimental;
	default:
		return ENomadicLayoutBuildingType::Building_None;
	}
}

ECardType UW_NomadicLayout::GetCardTypeFromBuildingType(const ENomadicLayoutBuildingType BuildingType) const
{
	switch (BuildingType)
	{
	case ENomadicLayoutBuildingType::Building_Barracks:
		return ECardType::BarracksTrain;
	case ENomadicLayoutBuildingType::Building_Forge:
		return ECardType::ForgeTrain;
	case ENomadicLayoutBuildingType::Building_MechanicalDepot:
		return ECardType::MechanicalDepotTrain;
	case ENomadicLayoutBuildingType::Building_T2Factory:
		return ECardType::T2FactoryTrain;
	case ENomadicLayoutBuildingType::Building_Airbase:
		return ECardType::AirbaseTrain;
	case ENomadicLayoutBuildingType::Building_Experimental:
		return ECardType::ExperimentalFactoryTrain;
	default:
		return ECardType::Invalid;
	}
}

void UW_NomadicLayout::AppendSelectedCardsFromLayouts(TArray<UW_RTSCard*>& OutSelectedCards) const
{
	if (not IsValid(M_CardClass))
	{
		RTSFunctionLibrary::ReportError("Missing card class in UW_NomadicLayout::AppendSelectedCardsFromLayouts.");
		return;
	}
	for (const FNomadicBuildingLayoutData& EachLayout : M_BuildingLayoutsData)
	{
		for (const ERTSCard EachCard : EachLayout.Cards)
		{
			UW_RTSCard* CardWidget = CreateWidget<UW_RTSCard>(GetWorld(), M_CardClass);
			if (not IsValid(CardWidget))
			{
				RTSFunctionLibrary::ReportError(
					"Failed to create card widget in UW_NomadicLayout::AppendSelectedCardsFromLayouts.");
				continue;
			}
			CardWidget->InitializeCard(EachCard);
			if (M_CardMenu.IsValid())
			{
				TObjectPtr<UW_CardMenu> CardMenuPtr = M_CardMenu.Get();
				CardWidget->SetCardMenu(CardMenuPtr);
			}
			OutSelectedCards.Add(CardWidget);
		}
	}
}

bool UW_NomadicLayout::GetIsScrollBoxValid() const
{
	if (IsValid(BuildingLayoutBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Invalid layout box in Nomadic Layout");
	return false;
}
