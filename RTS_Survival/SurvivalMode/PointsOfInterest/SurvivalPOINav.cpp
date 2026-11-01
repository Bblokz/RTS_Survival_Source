// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalPOINav.h"

#include "Engine/World.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationSystem.h"

namespace SurvivalPOINavConstants
{
	constexpr int32 MaxPathHops = 5;
	constexpr double PathArriveTolerance = 400.0;
	constexpr double NavProjectExtent = 900.0;
	constexpr double NavProjectExtentZ = 1500.0;
}

namespace SurvivalPOINav
{
	FChainedPathResult FindChainedPath(UWorld* World, const FVector& From, const FVector& To, const float AgentRadius,
	                                   const float AgentHeight)
	{
		using namespace SurvivalPOINavConstants;
		FChainedPathResult Result;
		UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavigationSystem == nullptr)
		{
			return Result;
		}
		FNavAgentProperties Agent;
		Agent.AgentRadius = AgentRadius;
		Agent.AgentHeight = AgentHeight;
		const ANavigationData* NavData = NavigationSystem->GetNavDataForProps(Agent);
		if (NavData == nullptr)
		{
			return Result;
		}
		const FVector Extent(NavProjectExtent, NavProjectExtent, NavProjectExtentZ);
		FNavLocation Start;
		FNavLocation Goal;
		if (not NavigationSystem->ProjectPointToNavigation(From, Start, Extent, NavData)
			or not NavigationSystem->ProjectPointToNavigation(To, Goal, Extent, NavData))
		{
			return Result;
		}
		FVector Current = Start.Location;
		for (int32 Hop = 0; Hop < MaxPathHops; ++Hop)
		{
			const FPathFindingQuery Query(nullptr, *NavData, Current, Goal.Location,
			                              UNavigationQueryFilter::GetQueryFilter(*NavData, nullptr, nullptr));
			const FPathFindingResult Path = NavigationSystem->FindPathSync(Agent, Query);
			Result.Hops = Hop + 1;
			if (not Path.IsSuccessful() or not Path.Path.IsValid() or Path.Path->GetPathPoints().IsEmpty())
			{
				return Result;
			}
			Result.Length += Path.Path->GetLength();
			const FVector End = Path.Path->GetPathPoints().Last().Location;
			if (FVector::Dist2D(End, Goal.Location) <= PathArriveTolerance)
			{
				Result.bReached = true;
				return Result;
			}
			if (FVector::Dist2D(End, Current) < 1.0)
			{
				return Result;
			}
			Current = End;
		}
		return Result;
	}
}
