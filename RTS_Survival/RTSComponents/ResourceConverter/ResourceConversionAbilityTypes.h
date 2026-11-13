// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

/**
 * @brief Card subtype a resource converter stores as CustomType on its Enable/Disable Resource Conversion entries,
 * so the action UI can show what the converter turns into what (Buttons_DataTableb CustomTypeMapping).
 * The values are data table keys: never renumber them.
 */
enum class EResourceConversionAbilityType : uint8
{
	// Uses the data table row's own icon and text.
	Default = 0,
	RadixiteToVehicleParts = 1,
};

namespace FResourceConversionAbilityTypes
{
	/**
	 * @brief Derives the card subtype from the converter's per-tick deltas, so a converter's icon always matches
	 * what it actually converts without separate Blueprint data.
	 * @param ResourceDeltas Resource -> delta per tick (negative = cost, positive = gain).
	 * @return The subtype of the one cost / one gain pair; Default for any other set of deltas.
	 */
	EResourceConversionAbilityType GetTypeFromResourceDeltas(const TMap<ERTSResourceType, int32>& ResourceDeltas);
}
