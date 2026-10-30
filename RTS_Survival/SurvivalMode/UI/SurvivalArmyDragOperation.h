// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"

#include "SurvivalArmyDragOperation.generated.h"

/**
 * @brief Payload of every drag on the army selection screen. Roster entries start it with SourceSlotIndex
 * INDEX_NONE (add a unit); filled army slots start it with their own index (move, or remove when dropped outside
 * a slot). UW_SurvivalArmySelection decides what a drop does.
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalArmyDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// Index into UW_SurvivalArmySelection's roster.
	UPROPERTY()
	int32 RosterIndex = INDEX_NONE;

	// The army slot the unit was dragged out of; INDEX_NONE when it came from the roster.
	UPROPERTY()
	int32 SourceSlotIndex = INDEX_NONE;

	bool GetIsFromSlot() const { return SourceSlotIndex != INDEX_NONE; }
};
