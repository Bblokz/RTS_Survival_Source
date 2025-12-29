// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GarrisonHealthBar.h"

#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

void UW_GarrisonHealthBar::SetupGarrison(const int32 Slots, const int32 Seats, const int32 MaxSquads,
                                         const EGarrisonSeatsTextType TypeText)
{
	// New logic: allow Slots == 0 (special case meaning "no boxes, seats only").
	if (Slots < 0)
	{
		RTSFunctionLibrary::ReportError("Invalid slot count: " + FString::FromInt(Slots));
		return;
	}

	M_Slots = Slots;
	M_MaxSeats = FMath::Max(0, Seats);
	M_MaxSquads = FMath::Max(0, MaxSquads);
	M_CurrentSeatsTaken = 0;
	M_CurrentSquads = 0;
	M_OccupiedSlots.Empty();

	M_SeatsTextType = TypeText;
	UpdateSeatsText();

	// If more than 3 (or 0 as special case), collapse all boxes and rely on seat text.
	const bool bHideBoxes = (Slots == 0 || Slots > 3);

	if (GarrisonBox0) GarrisonBox0->SetVisibility(bHideBoxes ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (GarrisonBox1)
		GarrisonBox1->SetVisibility(bHideBoxes
			                            ? ESlateVisibility::Collapsed
			                            : (Slots >= 2
				                               ? ESlateVisibility::Visible
				                               : ESlateVisibility::Collapsed));
	if (GarrisonBox2)
		GarrisonBox2->SetVisibility(bHideBoxes
			                            ? ESlateVisibility::Collapsed
			                            : (Slots >= 3
				                               ? ESlateVisibility::Visible
				                               : ESlateVisibility::Collapsed));
	bM_IsInitialized = true;
}

void UW_GarrisonHealthBar::UpdateGarrisonSlot(const int32 SlotIndex, const bool bIsOccupied,
                                              const FTrainingOption UnitID, const int32 SeatsTakenOrBecameVacant)
{
	if (not EnsureIsValidGarrisonSlot(SlotIndex))
	{
		return;
	}
	if (bIsOccupied)
	{
		M_CurrentSeatsTaken += SeatsTakenOrBecameVacant;
		if (not M_OccupiedSlots.Contains(SlotIndex))
		{
			M_OccupiedSlots.Add(SlotIndex);
			M_CurrentSquads = FMath::Clamp(M_CurrentSquads + 1, 0, M_MaxSquads);
		}
	}
	else
	{
		M_CurrentSeatsTaken = FMath::Clamp(M_CurrentSeatsTaken - SeatsTakenOrBecameVacant, 0, M_MaxSeats);
		if (M_OccupiedSlots.Contains(SlotIndex))
		{
			M_OccupiedSlots.Remove(SlotIndex);
			M_CurrentSquads = FMath::Clamp(M_CurrentSquads - 1, 0, M_MaxSquads);
		}
	}
	UpdateSeatsText();
	// Zero slots is the special case where we let seats drive the UI only.
	// Cargo slots do still exist on the cargo component and do dictate whether a squad can enter or not.
	if (M_Slots != 0)
	{
		BP_UpdateGarrisonSlot(SlotIndex, bIsOccupied, UnitID);
	}
}

void UW_GarrisonHealthBar::OnGarrisonEnabled(const bool bEnabled)
{
	if (not bM_IsInitialized)
	{
		RTSFunctionLibrary::ReportError("GarrisonHealthBar not initialized before OnGarrisonEnabled called");
		return;
	}
	TArray<TObjectPtr<USizeBox>> BoxesToAffect;
	if (GarrisonBox0) BoxesToAffect.Add(GarrisonBox0);
	if (M_Slots >= 2 && GarrisonBox1) BoxesToAffect.Add(GarrisonBox1);
	if (M_Slots >= 3 && GarrisonBox2) BoxesToAffect.Add(GarrisonBox2);
	ESlateVisibility NewVisibility = bEnabled ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	for (USizeBox* Box : BoxesToAffect)
	{
		Box->SetVisibility(NewVisibility);
	}
	if (SeatText)
	{
		SeatText->SetVisibility(bEnabled ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UW_GarrisonHealthBar::ForceHideCargoUI()
{
	TArray<TObjectPtr<USizeBox>> BoxesToAffect;
	if (GarrisonBox0) BoxesToAffect.Add(GarrisonBox0);
	if (GarrisonBox1) BoxesToAffect.Add(GarrisonBox1);
	if (GarrisonBox2) BoxesToAffect.Add(GarrisonBox2);
	for (USizeBox* Box : BoxesToAffect)
	{
		Box->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (SeatText)
	{
		SeatText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UW_GarrisonHealthBar::EnsureIsValidGarrisonSlot(const int32 SlotIndex) const
{
	if (SlotIndex < 0)
	{
		RTSFunctionLibrary::ReportError("no valid garrison slot"
			"\n provided: " + FString::FromInt(SlotIndex) + "\n must be between 0 and 2");
		return false;
	}

	if (M_Slots != 0 && SlotIndex > 2)
	{
		RTSFunctionLibrary::ReportError("no valid garrison slot"
			"\n provided: " + FString::FromInt(SlotIndex) + "\n must be between 0 and 2");
		return false;
	}
	return true;
}

void UW_GarrisonHealthBar::UpdateSeatsText() const
{
	FString Label;
	int32 Current = 0;
	int32 Max = 0;

	switch (M_SeatsTextType)
	{
	case EGarrisonSeatsTextType::Seats:
		Label = "Seats: ";
		Current = M_CurrentSeatsTaken;
		Max = M_MaxSeats;
		break;
	case EGarrisonSeatsTextType::Units:
		Label = "Units: ";
		Current = M_CurrentSeatsTaken;
		Max = M_MaxSeats;
		break;
	case EGarrisonSeatsTextType::DisplayFullSquads:
		Label = "Squads: ";
		Current = M_CurrentSquads;
		Max = M_MaxSquads;
		break;
	default:
		Label = "Seats: ";
		Current = M_CurrentSeatsTaken;
		Max = M_MaxSeats;
		break;
	}

	if (SeatText)
	{
		const FString SeatTexts = Label + FString::FromInt(Current) + "/" + FString::FromInt(Max);
		const FText MyRichText = FText::FromString(
			FRTSRichTextConverter::MakeRTSRich(SeatTexts, ERTSRichText::Text_Seats));
		SeatText->SetText(MyRichText);
	}
}
