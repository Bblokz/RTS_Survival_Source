// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_RTSCard.h"
#include "RTS_Survival/CardSystem/CardUI/CardMenu/W_CardMenu.h"
#include "RTS_Survival/CardSystem/CardUI/CardScollBox/W_CardScrollBox.h"
#include "RTS_Survival/CardSystem/CardUI/CardSounds/CardSounds.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

FRTSCardData::FRTSCardData()
	: Resources({}),
	  Technology(ETechnology::Tech_NONE),
	  UnitToCreate(FTrainingOption()),
	  TrainingOption(FTrainingOption()),
	  CardType(ECardType::Invalid),
	  CardRarity(ERTSCardRarity::Rarity_Common)
{
}

void UW_RTSCard::InitializeCard(const ERTSCard NewCard)
{
	M_RTSCard = NewCard;
	OnSetupCard();
}


void UW_RTSCard::SetCardMenu(const TObjectPtr<UW_CardMenu>& NewCardMenu)
{
	M_CardMenu = NewCardMenu;
}
# if WITH_EDITOR

void UW_RTSCard::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	InitializeCard(M_RTSCard);
}
#endif

// void UW_RTSCard::OnHovered(bool bIsHovering)
// {
//     if (bIsHovering)
//     {
//         // Increase the scale to make the card "jump out" slightly
//         SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(2,2), FVector2D(1.1f, 1.1f), 0.0f));
//         SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); // Scale around center
//     }
//     else
//     {
//         // Reset the scale
//         SetRenderTransform(FWidgetTransform());
//         SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
//     }
//
// }


void UW_RTSCard::OnHovered(const bool bIsHovering)
{
	if (not GetIsValidCardMenu())
	{
		return;
	}
	M_CardMenu->OnCardHovered(bIsHovering, M_RTSCard, bIsLeftSide);
}

void UW_RTSCard::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (GetIsValidCardMenu() && M_RTSCard != ERTSCard::Card_Empty)
	{
		M_CardMenu->PlayRTSCardSound(ECardSound::Sound_Hover);
	}
	OnHovered(true);
}

void UW_RTSCard::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	OnHovered(false);
}

void UW_RTSCard::BP_InitDragWidget(const ERTSCard RTSCard)
{
	InitializeCard(RTSCard);
}

void UW_RTSCard::OnDropRTSCard(UW_RTSCard* Other, const ERTSCard DroppedCard)
{
	if (not GetIsValidCardMenu())
	{
		return;
	}
	if (not IsValid(Other))
	{
		RTSFunctionLibrary::ReportError("Invalid Other in UW_RTSCard::OnDrop.");
		return;
	}
	if (not GetIsDropAllowed(DroppedCard, Other))
	{
		if (CheckIsDroppingIntoScrollBox(Other))
		{
			// in this case the player attempted to drop a card into the scroll box but cannot switch with the card
			// dropped on as it is not of the correct type. We set dragged card to empty and add the dropped card to the scroll box.
			if (IsValid(Other))
			{
				Other->InitializeCard(ERTSCard::Card_Empty);
				if (UW_CardScrollBox* ScrollBox = M_CardMenu->GetCardScrollBox(); IsValid(ScrollBox))
				{
					ScrollBox->AddCardToScrollBoxPreferEmptySlot(DroppedCard);
				}
			}
			M_CardMenu->PlayRTSCardSound(ECardSound::Sound_Drop);
			return;
		}
		M_CardMenu->PlayRTSCardSound(ECardSound::Sound_DropFailed);
		return;
	}
	M_CardMenu->PlayRTSCardSound(ECardSound::Sound_Drop);
	Other->InitializeCard(M_RTSCard);
	InitializeCard(DroppedCard);
}

void UW_RTSCard::SetDataTableData(const FRTSCardData& NewData)
{
	M_CardData = NewData;
}

UW_CardMenu* UW_RTSCard::GetCardMenu() const
{
	if(not GetIsValidCardMenu())
	{
		return nullptr;
	}
	return M_CardMenu.Get();
}


bool UW_RTSCard::GetIsValidCardMenu() const
{
	static bool bDidErrorReport;
	if (M_CardMenu.IsValid())
	{
		return true;
	}

	if (!bDidErrorReport)
	{
		RTSFunctionLibrary::ReportError("The provided card menu is not valid in UW_RTSCard::GetIsValidCardMenu.");
		bDidErrorReport = true;
	}
	return false;
}

bool UW_RTSCard::GetIsDropAllowed(const ERTSCard DroppedCard, UW_RTSCard* Other) const
{
	TWeakInterfacePtr<ICardHolder> OtherCardHolder = Other->GetCardHolder();
	if (not OtherCardHolder.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid OtherCardHolder in UW_RTSCard::GetIsDropAllowed."
			"\n For card: " + Global_GetCardAsString(Other->GetCard()) +
			"\n As object: " + Other->GetName());
		return false;
	}
	if (not M_CardHolder.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Invalid M_CardHolder (TWeakInterfacePtr<ICardHolder>) in UW_RTSCard::GetIsDropAllowed."
			"\n For card: " + Global_GetCardAsString(M_RTSCard) +
			"\n As object: " + GetName());
		return false;
	}
	if (not M_CardHolder->CheckIsCardTypeAllowed(Other->GetCardType()))
	{
		RTSFunctionLibrary::PrintString(
			"card holder does not accept dropped type: " + Global_GetCardTypeAsString(Other->GetCardType()));
		return false;
	}
	if (not OtherCardHolder->CheckIsCardTypeAllowed(GetCardType()))
	{
		RTSFunctionLibrary::PrintString(
			"other card holder does not accept dropped type: " + Global_GetCardTypeAsString(GetCardType()));
		return false;
	}
	return true;
}

bool UW_RTSCard::CheckIsDroppingIntoScrollBox(UW_RTSCard* Other) const
{
	if (not IsValid(Other))
	{
		RTSFunctionLibrary::ReportError("Invalid Other in UW_RTSCard::CheckIsDroppingIntoScrollBox.");
		return false;
	}
	if (M_CardHolder.IsValid())
	{
		if (M_CardHolder->GetIsScrollBox())
		{
			RTSFunctionLibrary::PrintString("Special Scroll box drop", FColor::Purple);
			return true;
		}
	}
	return false;
}

void UW_RTSCard::OnRightClickCard()
{
	if (GetIsValidCardMenu())
	{
		if (M_CardMenu->GetCardScrollBox())
		{
			M_CardMenu->GetCardScrollBox()->AddCardToScrollBoxPreferEmptySlot(M_RTSCard);
			InitializeCard(ERTSCard::Card_Empty);
		}
		GetCardMenu()->PlayRTSCardSound(ECardSound::Sound_Drop);
	}
}
