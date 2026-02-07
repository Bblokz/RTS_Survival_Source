// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_NomadicLayoutBuilding.h"

#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "NomadicBuildingLayoutData/NomadicBuildingLayoutData.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_NomadicLayoutBuilding::InitNomadicLayoutBuilding(const FNomadicBuildingLayoutData& BuildingData,
                                                         const TSubclassOf<UW_RTSCard> InCardClass, TObjectPtr<UW_CardMenu> CardMenu, const bool bIsLeftSide)
{
	if (not IsValid(InCardClass))
	{
		RTSFunctionLibrary::PrintString("Cannot initialize Nomadic Layout Building, InCardClass is not valid");
		return;
	}
	if (not GetIsValidCardScrollBox())
	{
		return;
	}
	if(BuildingData.Slots == 0 || BuildingData.Slots > 100)
	{
		RTSFunctionLibrary::ReportError("Cannot initialize Nomadic Layout Building, BuildingData.Slots is invalid"
								  "\n Amount of slots attempted to create: " + FString::FromInt(BuildingData.Slots));
		return;
	}
	if(BuildingData.BuildingType == ENomadicLayoutBuildingType::Building_None)
	{
		RTSFunctionLibrary::ReportError("Cannot initialize Nomadic Layout Building, BuildingData.BuildingType is invalid"
								  "\n BuildingType attempted to create: Building_NONE");
		return;
	}

	CardScrollBox->ClearChildren();
	M_CardsInScrollBox.Empty();
	M_AllowedCardTypes.Empty();

	M_Slots = BuildingData.Slots;
	M_BuildingType = BuildingData.BuildingType;
	for (const auto EachCardType : BuildingData.Cards)
	{
		CreateCardOfType(EachCardType, InCardClass, CardMenu, bIsLeftSide);
	}
	// Create empty cards as long as we have not reached the maximum amount of cards
	while (M_CardsInScrollBox.Num() < M_Slots)
	{
		CreateCardOfType(ERTSCard::Card_Empty, InCardClass, CardMenu, bIsLeftSide);
	}
	SetupAllowedCardsFromBuildingType(M_BuildingType);
	// Update visuals in blueprint event.
	OnSetupNOmadicBuildingLayout(M_BuildingType);
}

TSet<ECardType> UW_NomadicLayoutBuilding::GetCardTypesAllowed() const
{
	return M_AllowedCardTypes; 
}

bool UW_NomadicLayoutBuilding::GetIsScrollBox() const
{
	return false;
}

TArray<TPair<ERTSCard, ECardType>> UW_NomadicLayoutBuilding::GetNonEmptyCardsHeld()
{
	TArray<TPair<ERTSCard, ECardType>> NonEmptyCards;
	TPair<ERTSCard, ECardType> Pair;
	for(auto EachCard: M_CardsInScrollBox)
	{
		if(IsValid(EachCard) && EachCard->GetCard() != ERTSCard::Card_Empty)
		{
			Pair.Value = EachCard->GetCardType();
			Pair.Key = EachCard->GetCard();
			NonEmptyCards.Add(Pair);
		}
	}
	return NonEmptyCards;
}

TArray<UW_RTSCard*> UW_NomadicLayoutBuilding::GetSelectedCards()
{
	TArray<UW_RTSCard*> SelectedCards;
	for(auto EachCard: M_CardsInScrollBox)
	{
		if(IsValid(EachCard) && EachCard->GetCard() != ERTSCard::Card_Empty)
		{
			SelectedCards.Add(EachCard);
		}
	}
	return SelectedCards;
}

ECardType UW_NomadicLayoutBuilding::GetCardType() const
{
	if(M_AllowedCardTypes.Num() !=0)
	{
		return M_AllowedCardTypes.Array()[0];
	}
	RTSFunctionLibrary::ReportError("no allowed card types set in UW_NomadicLayoutBuilding, "
								 "\n name: " + GetName());
	return ECardType::Invalid;
}

FNomadicBuildingLayoutData UW_NomadicLayoutBuilding::GetBuildingLayoutData() const
{
	FNomadicBuildingLayoutData BuildingData;
	BuildingData.BuildingType = M_BuildingType;
	BuildingData.Slots = M_Slots;
	for(auto EachCard: M_CardsInScrollBox)
	{
		if(IsValid(EachCard))
		{
			if(EachCard->GetCard() == ERTSCard::Card_Empty)
			{
				continue;
			}
			BuildingData.Cards.Add(EachCard->GetCard());
		}
	}
	return BuildingData;
	
}

void UW_NomadicLayoutBuilding::CreateCardOfType(const ERTSCard Type, const TSubclassOf<UW_RTSCard> InCardClass, TObjectPtr<UW_CardMenu> InCardMenu, const bool
                                                bIsLeftSide)
{
	// Create the card widget
	UW_RTSCard* CardWidget = CreateWidget<UW_RTSCard>(GetWorld(), InCardClass);
	if (not IsValid(CardWidget))
	{
		RTSFunctionLibrary::ReportError("Failed to create card widget in UW_NomadicLayoutBuilding"
			"\n for card type: " + Global_GetCardAsString(Type));
		return;
	}

	// Initialize the card widget with the card data
	CardWidget->InitializeCard(Type);
	CardWidget->SetCardHolder(this);
	CardWidget->SetCardMenu(InCardMenu);
	CardWidget->SetIsLeftSide(bIsLeftSide);
	

	// Wrap it in a ScaleBox
	UScaleBox* ScaleBox = NewObject<UScaleBox>(this);
	if (not IsValid(ScaleBox))
	{
		RTSFunctionLibrary::ReportError("Failed to create ScaleBox in UW_NomadicLayoutBuilding"
			"\n for card type: " + Global_GetCardAsString(Type));
		return;
	}
	ScaleBox->SetStretch(EStretch::UserSpecified);
	ScaleBox->SetUserSpecifiedScale(CardsScale);
	ScaleBox->SetStretchDirection(EStretchDirection::Both);
	ScaleBox->AddChild(CardWidget);
	CardScrollBox->AddChild(ScaleBox);
	M_CardsInScrollBox.Add(CardWidget);
}

void UW_NomadicLayoutBuilding::SetupAllowedCardsFromBuildingType(const ENomadicLayoutBuildingType BuildingType)
{
	switch (BuildingType) {
	case ENomadicLayoutBuildingType::Building_None:
		M_AllowedCardTypes = {ECardType::Empty};
		break;
	case ENomadicLayoutBuildingType::Building_Barracks:
		M_AllowedCardTypes = {ECardType::BarracksTrain, ECardType::Empty};
		break;
	case ENomadicLayoutBuildingType::Building_Forge:
		M_AllowedCardTypes = {ECardType::ForgeTrain, ECardType::Empty};
		break;
	case ENomadicLayoutBuildingType::Building_MechanicalDepot:
		M_AllowedCardTypes = {ECardType::MechanicalDepotTrain, ECardType::Empty};
		break;
	case ENomadicLayoutBuildingType::Building_T2Factory:
		M_AllowedCardTypes = {ECardType::T2FactoryTrain, ECardType::Empty};
		break;
	case ENomadicLayoutBuildingType::Building_Airbase:
		M_AllowedCardTypes = {ECardType::AirbaseTrain, ECardType::Empty};
		break;
	case ENomadicLayoutBuildingType::Building_Experimental:
		M_AllowedCardTypes = {ECardType::ExperimentalFactoryTrain, ECardType::Empty};
		break;
	}
}

bool UW_NomadicLayoutBuilding::GetIsValidCardScrollBox() const
{
	if (IsValid(CardScrollBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("CardScrollBox is not valid in UW_NomadicLayoutBuilding");
	return false;
}
