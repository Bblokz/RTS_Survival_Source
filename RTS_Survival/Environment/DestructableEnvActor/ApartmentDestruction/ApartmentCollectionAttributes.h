// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "ApartmentCollectionAttributes.generated.h"

/** @brief What a breakable slot of an apartment is; decides how much damage it takes before it gives way. */
UENUM(BlueprintType)
enum class EApartmentPartType : uint8
{
	ExteriorWall,
	InteriorWall,
	Floor,
	Roof,
	Stairs,
	// Trims, cornices, loose doors and other dressing that is not part of a wall or floor slot.
	Detail,
	// Loose rubble on the ground around the building; it lies where it is and never breaks off.
	Rubble
};

/**
 * @brief Per-transform attributes the apartment build command stores in a geometry collection and the apartment
 * actor reads back, so the planner's knowledge of what each piece is survives into the game.
 */
namespace ApartmentCollectionAttributes
{
	// Id of the slot (wall panel with facings, floor cell, ...) a transform belongs to, as in the pieces file.
	inline const FName SlotId(TEXT("ApartmentSlotId"));
	// EApartmentPartType of that slot.
	inline const FName PartType(TEXT("ApartmentPartType"));
	inline constexpr int32 NoSlot = INDEX_NONE;
}
