// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace SurvivalPOINav
{
	constexpr float HeavyTankRadius = 200.f;
	constexpr float HeavyTankHeight = 300.f;
	constexpr float CharacterRadius = 35.f;
	constexpr float CharacterHeight = 144.f;

	struct FChainedPathResult
	{
		bool bReached = false;
		double Length = 0.0;
		int32 Hops = 0;
	};

	/**
	 * @brief A path for one nav agent, chained from each partial end, because single queries stop at the search node
	 * limit on long drives (the map's paths over ~40 000 come back partial).
	 * @return Whether the goal was reached, the summed length and the number of queries used.
	 */
	FChainedPathResult FindChainedPath(UWorld* World, const FVector& From, const FVector& To, float AgentRadius, float AgentHeight);
}
