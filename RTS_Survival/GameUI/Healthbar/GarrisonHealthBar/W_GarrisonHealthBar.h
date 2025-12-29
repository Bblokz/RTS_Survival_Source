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
 * @brief Widget for visualizing cargo occupancy with seat and squad feedback.
 *
 * Displays garrison slot state alongside seat or squad counts to inform players about cargo capacity.
 */
UCLASS()
class RTS_SURVIVAL_API UW_GarrisonHealthBar : public UW_HealthBar
{
	GENERATED_BODY()

public:
	/**
	 * @brief Configure the garrison widget with capacity details.
	 * @param Slots Number of visual slot boxes to show (0 hides all boxes).
	 * @param Seats Maximum seat capacity for seat/unit displays.
	 * @param MaxSquads Maximum squads allowed inside this garrison.
	 * @param TypeText Determines whether to show seats, units, or squads in the text.
	 */
	void SetupGarrison(const int32 Slots, const int32 Seats, const int32 MaxSquads,
	                   const EGarrisonSeatsTextType TypeText);
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
	int32 M_CurrentSquads = 0;

	UPROPERTY()
	int32 M_MaxSquads = 0;
	
	UPROPERTY()
	int32 M_Slots = 0;

	UPROPERTY()
	EGarrisonSeatsTextType M_SeatsTextType;

	UPROPERTY()
	TSet<int32> M_OccupiedSlots;

	void UpdateSeatsText() const;

	bool bM_IsInitialized = false;
};
