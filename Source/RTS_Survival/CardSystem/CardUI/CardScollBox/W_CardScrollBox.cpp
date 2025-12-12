// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_CardScrollBox.h"

#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/CardSystem/CardUI/CardMenu/W_CardMenu.h"
#include "RTS_Survival/CardSystem/CardUI/CardSounds/CardSounds.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_CardScrollBox::InitCardScrollBox(const TArray<ERTSCard>& Cards, TSubclassOf<UUserWidget> InCardClass,
                                         TObjectPtr<UW_CardMenu> InCardMenu, const float NewScrollBoxHeightMlt)
{
	M_ScrollBoxHeightMlt = NewScrollBoxHeightMlt;
	if (not IsValid(InCardMenu))
	{
		return;
	}
	if (not GetIsValidScrollBox())
	{
		return;
	}
	if (GetIsValidScrollBoxBorder())
	{
		UCanvasPanelSlot* ScrollSlot = Cast<UCanvasPanelSlot>(ScrollBoxBorder->Slot);
		if (IsValid(ScrollSlot))
		{
			M_OriginalScrollBoxHeight = ScrollSlot->GetSize().Y;
		}
	}

	M_CardMenu = InCardMenu;
	if (not IsValid(InCardClass))
	{
		RTSFunctionLibrary::ReportError(
			"Could not initialize card scroll box as the (blueprint derived) card widget class is not valid"
			"\n Make sure the class is properly propagated to init the cardscrollbox!");
		return;
	}
	M_BlueprintCardClass = InCardClass;
	for (auto EachCardType : Cards)
	{
		CreateNewCardWidget(EachCardType);
	}
	OnSortByRarity();
}

TSet<ECardType> UW_CardScrollBox::GetCardTypesAllowed() const
{
	return {
		ECardType::Empty, ECardType::StartingUnit, ECardType::Technology, ECardType::Resource, ECardType::BarracksTrain,
		ECardType::ForgeTrain, ECardType::MechanicalDepotTrain
	};
}

bool UW_CardScrollBox::GetIsScrollBox() const
{
	return true;
}

void UW_CardScrollBox::AddCardToScrollBoxPreferEmptySlot(const ERTSCard CardType)
{
	for (UW_RTSCard* CardWidget : M_CardsInScrollBox)
	{
		if (IsValid(CardWidget) && CardWidget->GetCard() == ERTSCard::Card_Empty)
		{
			CardWidget->InitializeCard(CardType);
			return;
		}
	}

	// No empty card slots found, create a new card
	CreateNewCardWidget(CardType);
}

TArray<TPair<ERTSCard, ECardType>> UW_CardScrollBox::GetNonEmptyCardsHeld() 
{
	TArray<TPair<ERTSCard, ECardType>> NonEmptyCards;
	TPair<ERTSCard , ECardType> Pair;
	EnsureCardArrayIsValid();
	for(const auto EachCard : M_CardsInScrollBox)
	{
		if(IsValid(EachCard) && EachCard->GetCard() != ERTSCard::Card_Empty)
		{
			Pair.Key = EachCard->GetCard();
			Pair.Value = EachCard->GetCardType();
			NonEmptyCards.Add(Pair);
		}
	}
	return NonEmptyCards;
}

TArray<ERTSCard> UW_CardScrollBox::GetCardsOfType(const ECardType CardType)
{
	TArray<ERTSCard> CardsOfType;
	EnsureCardArrayIsValid();
	for(auto EachCard : M_CardsInScrollBox)
	{
		if(EachCard && EachCard->GetCard() == ERTSCard::Card_Empty)
		{
			continue;
		}
		if(EachCard && EachCard->GetCardType() == CardType)
		{
			CardsOfType.Add(EachCard->GetCard());
		}
	}
	return CardsOfType;
}

void UW_CardScrollBox::OnSortByRarity()
{
	if (GetIsValidCardMenu())
	{
		M_CardMenu->PlayRTSCardSound(ECardSound::Sound_SwitchLayout);
	}
	EnsureCardArrayIsValid();
	bM_RaritySortAscending = !bM_RaritySortAscending;

	const int32 NumCards = M_CardsInScrollBox.Num();
	if (NumCards <= 1)
	{
		return;
	}

	// Sort the M_CardsInScrollBox array with a custom predicate
	M_CardsInScrollBox.Sort([this](const UW_RTSCard& A, const UW_RTSCard& B)
	{
		const ERTSCardRarity RarityA = A.GetCardRarity();
		const ERTSCardRarity RarityB = B.GetCardRarity();

		if (bM_RaritySortAscending)
		{
			return RarityA < RarityB;
		}
		else
		{
			return RarityA > RarityB;
		}
	});

	// Update the cards after sorting
	for (int32 i = 0; i < NumCards; ++i)
	{
		UW_RTSCard* Card = M_CardsInScrollBox[i];
		if (!IsValid(Card))
		{
			continue;
		}
		int32 IndexInHZBox = i % 9;
		bool bIsLeft = (IndexInHZBox < 4);
		Card->SetIsLeftSide(bIsLeft);
	}

	// Now, call InitializeCard on each card to update visuals
	for (UW_RTSCard* Card : M_CardsInScrollBox)
	{
		if (IsValid(Card))
		{
			Card->InitializeCard(Card->GetCard());
		}
	}

	// Rearrange the widgets in the UI to match the sorted array
	UpdateCardWidgetsInScrollBox();
}


void UW_CardScrollBox::OnSortByType()
{
    if (GetIsValidCardMenu())
    {
        M_CardMenu->PlayRTSCardSound(ECardSound::Sound_SwitchLayout);
    }
    EnsureCardArrayIsValid();

    // Advance the sort index to cycle through types
    CurrentTypeSortIndex = (CurrentTypeSortIndex + 1) % SortTypeOrder.Num();

    const int32 NumCards = M_CardsInScrollBox.Num();
    if (NumCards <= 1)
    {
        return;
    }

    // Get the number of types
    int32 NumTypes = SortTypeOrder.Num();

    // Sort the M_CardsInScrollBox array with a custom predicate
    M_CardsInScrollBox.Sort([this, NumTypes](const UW_RTSCard& A, const UW_RTSCard& B)
    {
        ECardType TypeA = A.GetCardType();
        ECardType TypeB = B.GetCardType();

        // Get indices of types in SortTypeOrder
        int32 IndexA = SortTypeOrder.Find(TypeA);
        int32 IndexB = SortTypeOrder.Find(TypeB);

        // If type not found (e.g., Invalid or Empty), place it at the end
        if (IndexA == INDEX_NONE) IndexA = NumTypes;
        if (IndexB == INDEX_NONE) IndexB = NumTypes;

        // Compute adjusted indices relative to CurrentTypeSortIndex
        int32 AdjustedIndexA = (IndexA - CurrentTypeSortIndex + NumTypes) % NumTypes;
        int32 AdjustedIndexB = (IndexB - CurrentTypeSortIndex + NumTypes) % NumTypes;

        // Now compare adjusted indices
        if (AdjustedIndexA != AdjustedIndexB)
        {
            return AdjustedIndexA < AdjustedIndexB;
        }
        else
        {
            return A.GetName() < B.GetName();
        }
    });

    // Update the cards after sorting
    for (int32 i = 0; i < NumCards; ++i)
    {
        UW_RTSCard* Card = M_CardsInScrollBox[i];
        if (!IsValid(Card))
        {
            continue;
        }

        bool bIsLeft = (i % 9) < 4;
        Card->SetIsLeftSide(bIsLeft);
    }

    // Now, call InitializeCard on each card to update visuals
    for (UW_RTSCard* Card : M_CardsInScrollBox)
    {
        if (IsValid(Card))
        {
            Card->InitializeCard(Card->GetCard());
        }
    }
    UpdateCardWidgetsInScrollBox();
}


void UW_CardScrollBox::NativeConstruct()
{
    Super::NativeConstruct();

    if (SortTypeOrder.Num() == 0)
    {
        SortTypeOrder = {
            ECardType::StartingUnit,
            ECardType::Technology,
            ECardType::Resource,
            ECardType::BarracksTrain,
            ECardType::ForgeTrain,
            ECardType::MechanicalDepotTrain
            // Exclude Invalid and Empty
        };
    }
}


void UW_CardScrollBox::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UW_CardScrollBox::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (M_OriginalScrollBoxHeight == 0 || M_ScrollBoxHeightMlt == 0)
	{
		return;
	}
	if (GetIsValidCardMenu())
	{
		M_CardMenu->OnMouseInScrollBox(true);
		ResizeBorderScrollBox(false);
	}
}

void UW_CardScrollBox::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (M_OriginalScrollBoxHeight == 0 || M_ScrollBoxHeightMlt == 0)
	{
		return;
	}

	if (GetIsValidCardMenu())
	{
		M_CardMenu->OnMouseInScrollBox(false);
		ResizeBorderScrollBox(true);
	}
}


UHorizontalBox* UW_CardScrollBox::GetVacantHorizontalBox(int32& OutIndexInHzBox)
{
	if (not GetIsValidScrollBox())
	{
		return nullptr;
	}
	UHorizontalBox* LastBox = M_HorizontalBoxes.IsEmpty() ? nullptr : M_HorizontalBoxes.Last();
	if (!IsValid(LastBox) || LastBox->GetChildrenCount() >= 9)
	{
		UHorizontalBox* NewBox = NewObject<UHorizontalBox>(this);
		if (IsValid(NewBox))
		{
			M_HorizontalBoxes.Add(NewBox);
			CardScrollBox->AddChild(NewBox);
			OutIndexInHzBox = 0;
			return NewBox;
		}
		RTSFunctionLibrary::ReportError(
			"Failed to create new horizontal box in UW_CardScrollBox::GetVacantHorizontalBox");
		return nullptr;
	}
	OutIndexInHzBox = LastBox->GetChildrenCount();
	return LastBox;
}

bool UW_CardScrollBox::GetIsValidScrollBox() const
{
	if (IsValid(CardScrollBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No Valid scrollbox set in W_CardScrollBox (The card picker)");
	return false;
}

bool UW_CardScrollBox::GetIsValidCardMenu() const
{
	if (IsValid(M_CardMenu))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid card menu set in W_CardScrollBox");
	return false;
}

bool UW_CardScrollBox::GetIsValidScrollBoxBorder() const
{
	if (IsValid(ScrollBoxBorder))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid scroll box border set in W_CardScrollBox");
	return false;
}

void UW_CardScrollBox::Debug(UW_RTSCard* CreatedCard, ERTSCard CardType)
{
	RTSFunctionLibrary::PrintString(
		"Created card: " + CreatedCard->GetName() + " for card type: " + Global_GetCardAsString(CardType));
}

void UW_CardScrollBox::UpdateCardWidgetsInScrollBox()
{
	if (!GetIsValidScrollBox())
	{
		return;
	}

	// Remove the card horizontal boxes from the scroll box
	for (UHorizontalBox* HorizontalBox : M_HorizontalBoxes)
	{
		if (IsValid(HorizontalBox))
		{
			CardScrollBox->RemoveChild(HorizontalBox);
		}
	}

	// Clear our M_HorizontalBoxes array
	M_HorizontalBoxes.Empty();

	UHorizontalBox* CurrentHorizontalBox = nullptr;
	int32 CardsInCurrentRow = 0;
	const int32 MaxCardsPerRow = 9; // Adjust as needed

	for (UW_RTSCard* Card : M_CardsInScrollBox)
	{
		if (!IsValid(Card))
		{
			continue;
		}

		// Create a new HorizontalBox if needed
		if (CurrentHorizontalBox == nullptr || CardsInCurrentRow >= MaxCardsPerRow)
		{
			CurrentHorizontalBox = NewObject<UHorizontalBox>(this);
			if (!IsValid(CurrentHorizontalBox))
			{
				RTSFunctionLibrary::ReportError("Failed to create HorizontalBox in UpdateCardWidgetsInScrollBox");
				continue;
			}
			CardScrollBox->AddChild(CurrentHorizontalBox);
			M_HorizontalBoxes.Add(CurrentHorizontalBox);
			CardsInCurrentRow = 0;
		}

		// Wrap the card in a ScaleBox
		UScaleBox* ScaleBox = NewObject<UScaleBox>(this);
		if (!IsValid(ScaleBox))
		{
			RTSFunctionLibrary::ReportError("Failed to create ScaleBox in UpdateCardWidgetsInScrollBox");
			continue;
		}
		ScaleBox->SetStretch(EStretch::UserSpecified);
		ScaleBox->SetUserSpecifiedScale(0.5f);
		ScaleBox->SetStretchDirection(EStretchDirection::Both);
		ScaleBox->AddChild(Card);

		// Add the ScaleBox to the HorizontalBox
		CurrentHorizontalBox->AddChild(ScaleBox);
		CardsInCurrentRow++;
	}
}

void UW_CardScrollBox::ResizeBorderScrollBox(const bool bSetToOriginalSize)
{
	if (not GetIsValidScrollBoxBorder())
	{
		return;
	}
	float NewHeight = bSetToOriginalSize ? M_OriginalScrollBoxHeight : M_OriginalScrollBoxHeight * M_ScrollBoxHeightMlt;
	UCanvasPanelSlot* ScrollSlot = Cast<UCanvasPanelSlot>(ScrollBoxBorder->Slot);
	if (IsValid(ScrollSlot))
	{
		ScrollSlot->SetSize(FVector2D(ScrollSlot->GetSize().X, NewHeight));
	}
}


void UW_CardScrollBox::EnsureCardArrayIsValid()
{
	TArray<TObjectPtr<UW_RTSCard>> ValidArray;
	for (auto EachCard : M_CardsInScrollBox)
	{
		if (IsValid(EachCard))
		{
			ValidArray.Add(EachCard);
			continue;
		}
		RTSFunctionLibrary::ReportError("Invalid card in UW_CardScrollBox::EnsureCardArrayIsValid"
			"\n See M_CardsInScrollBox for more information.");
	}
	M_CardsInScrollBox = ValidArray;
}

void UW_CardScrollBox::CreateNewCardWidget(const ERTSCard CardType)
{
	// Create the card widget
	UW_RTSCard* CardWidget = CreateWidget<UW_RTSCard>(GetWorld(), M_BlueprintCardClass);
	if (!IsValid(CardWidget))
	{
		RTSFunctionLibrary::ReportError("Failed to create card widget in UW_CardScrollBox"
			"\n for card type: " + Global_GetCardAsString(CardType));
		return;;
	}

	// Initialize the card widget with the card data
	CardWidget->InitializeCard(CardType);
	CardWidget->SetCardHolder(this);


	// Wrap it in a ScaleBox
	UScaleBox* ScaleBox = NewObject<UScaleBox>(this);
	if (not IsValid(ScaleBox))
	{
		RTSFunctionLibrary::ReportError("Failed to create ScaleBox in UW_CardScrollBox"
			"\n for card type: " + Global_GetCardAsString(CardType));
		return;;
	}
	ScaleBox->SetStretch(EStretch::UserSpecified);
	ScaleBox->SetUserSpecifiedScale(-1.5f);
	ScaleBox->SetStretchDirection(EStretchDirection::Both);
	ScaleBox->AddChild(CardWidget);

	int32 IndexInHzBox = 0;
	// Determine whether we need a new horizontal box to place this card in; we can have 8 cards in one box
	if (UHorizontalBox* HorizontalBox = GetVacantHorizontalBox(IndexInHzBox))
	{
		// Add the card to the horizontal box
		HorizontalBox->AddChild(ScaleBox);
		// If you are in the first 3 cards, then the card is on the left side which means that the menu should use
		// the right side card viewer!
		CardWidget->SetIsLeftSide(IndexInHzBox < 3);
		CardWidget->SetCardMenu(M_CardMenu);
		CardWidget->SetVisibility(ESlateVisibility::Visible);
		M_CardsInScrollBox.Add(CardWidget);
	}
	if (DeveloperSettings::Debugging::GCardSystem_Compile_DebugSymbols)
	{
		Debug(CardWidget, CardType);
	}
}



