// Copyright (C) Bas Blokzijl

#include "ResourceConversionAbilityTypes.h"

namespace
{
	/** @return Whether the deltas hold exactly one cost and one gain; fills both resources when they do. */
	bool GetSingleCostAndGain(const TMap<ERTSResourceType, int32>& ResourceDeltas,
	                          ERTSResourceType& OutCost, ERTSResourceType& OutGain)
	{
		int32 CostCount = 0;
		int32 GainCount = 0;
		for (const TPair<ERTSResourceType, int32>& Delta : ResourceDeltas)
		{
			if (Delta.Value < 0)
			{
				OutCost = Delta.Key;
				++CostCount;
			}
			else if (Delta.Value > 0)
			{
				OutGain = Delta.Key;
				++GainCount;
			}
		}
		return CostCount == 1 && GainCount == 1;
	}
}

EResourceConversionAbilityType FResourceConversionAbilityTypes::GetTypeFromResourceDeltas(
	const TMap<ERTSResourceType, int32>& ResourceDeltas)
{
	ERTSResourceType Cost = ERTSResourceType::Resource_None;
	ERTSResourceType Gain = ERTSResourceType::Resource_None;
	if (not GetSingleCostAndGain(ResourceDeltas, Cost, Gain))
	{
		return EResourceConversionAbilityType::Default;
	}

	if (Cost == ERTSResourceType::Resource_Radixite && Gain == ERTSResourceType::Resource_VehicleParts)
	{
		return EResourceConversionAbilityType::RadixiteToVehicleParts;
	}
	return EResourceConversionAbilityType::Default;
}
