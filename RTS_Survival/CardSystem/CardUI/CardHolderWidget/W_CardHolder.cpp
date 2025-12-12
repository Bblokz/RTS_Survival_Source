// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_CardHolder.h"


#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "RTS_Survival/CardSystem/CardUI/CardMenu/W_CardMenu.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"

TSet<ECardType> UW_CardHolder::GetCardTypesAllowed() const
{
	return M_CardTypeToHold;
}

bool UW_CardHolder::GetIsScrollBox() const
{
	return false;
}

void UW_CardHolder::SetupCardHolder(
	const TArray<ERTSCard>& Cards,
	const TSubclassOf<UW_RTSCard> InCardClass, const int32 MaxCards,
	const TSet<ECardType>& AllowedCardTypes, const TObjectPtr<UW_CardMenu> InCardMenu)
{
	if (not GetIsValidCardHolder())
	{
		return;
	}

	if (not IsValid(InCardClass))
	{
		RTSFunctionLibrary::ReportError(
			"Could not initialize card scroll box as the (blueprint derived) card widget class is not valid"
			"\n Make sure the class is properly propagated to init the cardscrollbox!");
		return;
	}
	M_CardTypeToHold = AllowedCardTypes;
	M_BlueprintCardClass = InCardClass;
	M_MaxCardsToHold = MaxCards;
	M_BlueprintCardClass = InCardClass;
	for (const auto EachCardType : Cards)
	{
		if (M_Cards.Num() >= M_MaxCardsToHold)
		{
			break;
		}
		if (UW_RTSCard* CardWidget = CreateCardWidgetInHolder(EachCardType, InCardMenu,
		                                                      AllowedCardTypes.Contains(ECardType::StartingUnit),
		                                                      AllowedCardTypes))
		{
			M_Cards.Add(CardWidget);
		}
	}
	// Create empty cards as long as we have not reached the maximum amount of cards
	while (M_Cards.Num() < M_MaxCardsToHold)
	{
		if (UW_RTSCard* CardWidget = CreateCardWidgetInHolder(ERTSCard::Card_Empty, InCardMenu,
		                                                      AllowedCardTypes.Contains(ECardType::StartingUnit),
		                                                      AllowedCardTypes))
		{
			M_Cards.Add(CardWidget);
		}
	}
}

TArray<TPair<ERTSCard, ECardType>> UW_CardHolder::GetNonEmptyCardsHeld()
{
	TArray<ERTSCard> NonEmptyCards;
	ECardType MyCardType = GetCardTypesAllowed().Contains(ECardType::StartingUnit)
		                       ? ECardType::StartingUnit
		                       : ECardType::Resource;

	for (const UW_RTSCard* CardWidget : M_Cards)
	{
		// RTSlog the card type for any card
		RTSFunctionLibrary::PrintToLog("card type in GetNonEmptyCardsHeld: " + Global_GetCardAsString(CardWidget->GetCard()), false);
		if (CardWidget && CardWidget->GetCard() != ERTSCard::Card_Empty)
		{
			NonEmptyCards.Add(CardWidget->GetCard());
		}
	}
	if (NonEmptyCards.Num() == 0)
	{
		return {};
	}
	TArray<TPair<ERTSCard, ECardType>> CardTypes;
	TPair<ERTSCard, ECardType> CardPair;
	for (auto EachCard : NonEmptyCards)
	{
		CardPair.Key = EachCard;
		CardPair.Value = MyCardType;
		CardTypes.Add(CardPair);
	}
	return CardTypes;
}

int32 UW_CardHolder::GetMaxCardsToHold() const
{
	return M_MaxCardsToHold;
}

TArray<UW_RTSCard*> UW_CardHolder::GetSelectedCards()
{
	TArray<UW_RTSCard*> SelectedCards;
	for (auto EachCard : M_Cards)
	{
		if (IsValid(EachCard) && EachCard->GetCard() != ERTSCard::Card_Empty)
		{
			SelectedCards.Add(EachCard);
		}
	}
	return SelectedCards;
}

UW_RTSCard* UW_CardHolder::CreateCardWidgetInHolder(const ERTSCard& CardType, const TObjectPtr<UW_CardMenu> InCardMenu,
                                                    const bool bIsLeftSide, const TSet<ECardType>& AllowedCardTypes)
{
	if (not GetIsValidCardHolder())
	{
		return nullptr;
	}
	// Create the card widget
	UW_RTSCard* CardWidget = CreateWidget<UW_RTSCard>(GetWorld(), M_BlueprintCardClass);
	if (!IsValid(CardWidget))
	{
		RTSFunctionLibrary::ReportError("Failed to create card widget in UW_CardHolder"
			"\n for card type: " + Global_GetCardAsString(CardType));
		return nullptr;
	}

	// Initialize the card widget with the card data
	CardWidget->InitializeCard(CardType);
	// Starting unit cards are on the left, the right is on the right side.
	CardWidget->SetIsLeftSide(AllowedCardTypes.Contains(ECardType::StartingUnit));
	CardWidget->SetCardMenu(InCardMenu);
	CardWidget->SetCardHolder(this);

	// Wrap it in a ScaleBox
	UScaleBox* ScaleBox = NewObject<UScaleBox>(this);
	if (not IsValid(ScaleBox))
	{
		RTSFunctionLibrary::ReportError("Failed to create ScaleBox in UW_CardHolder"
			"\n for card type: " + Global_GetCardAsString(CardType));
		return nullptr;
	}
	ScaleBox->SetStretch(EStretch::UserSpecified);
	ScaleBox->SetUserSpecifiedScale(0.5f);
	ScaleBox->SetStretchDirection(EStretchDirection::Both);
	ScaleBox->AddChild(CardWidget);
	CardHolder->AddChild(ScaleBox);
	return CardWidget;
}

bool UW_CardHolder::GetIsValidCardHolder() const
{
	if (IsValid(CardHolder))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("The card holder is not valid in UW_CardHolder::GetIsValidCardHolder.");
	return false;
}
