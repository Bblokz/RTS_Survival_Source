// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GarrisonSeatsTextType.h"
#include "RTS_Survival/GameUI/Healthbar/W_HealthBar.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "W_GarrisonHealthBar.generated.h"

class URichTextBlock;
class USizeBox;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_GarrisonHealthBar : public UW_HealthBar
{
	GENERATED_BODY()

public:
	void SetupGarrison(const int32 Slots, const int32 Seats, const EGarrisonSeatsTextType TypeText);
	void UpdateGarrisonSlot(const int32 SlotIndex, const bool bIsOccupied,
	                        FTrainingOption UnitID, const int32 SeatsTakenOrBecameVacant);

	void OnGarrisonEnabled(const bool bEnabled);

	void ForceHideCargoUI();

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void BP_UpdateGarrisonSlot(const int32 SlotIndex, const bool bIsOccupied,
		FTrainingOption UnitID);
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<USizeBox> GarrisonBox0;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<USizeBox> GarrisonBox1;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<USizeBox> GarrisonBox2;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> SeatText;

private:
	bool EnsureIsValidGarrisonSlot(const int32 SlotIndex) const;

	UPROPERTY()
	int32 M_CurrentSeatsTaken = 0;

	UPROPERTY()
	int32 M_MaxSeats = 0;
	
	UPROPERTY()
	int32 M_Slots = 0;

	UPROPERTY()
	EGarrisonSeatsTextType M_SeatsTextType;

	void UpdateSeatsText() const;

	bool bIsInitialized = false;
};
