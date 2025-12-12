#include "FRTSPathFindingHelpers.h"

#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

bool FRTSPathFindingHelpers::IsLocationInHighCostArea(const UWorld* World, const FVector& Location,
                                                      const FPathFindingQuery& Query)
{
		if (World == nullptr)
		{
			return false;
		}

		const UNavigationSystemV1* navigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (navigationSystem == nullptr)
		{
			return false;
		}

		const ANavigationData* navigationDataFromQuery = Query.NavData.Get();
		const ANavigationData* navigationData =
			navigationDataFromQuery != nullptr
				? navigationDataFromQuery
				: navigationSystem->GetNavDataForProps(Query.NavAgentProperties);

		const ARecastNavMesh* recastNavMesh = Cast<ARecastNavMesh>(navigationData);
		if (recastNavMesh == nullptr)
		{
			return false;
		}

		// Decide which query filter to use: query-specific first, then navmesh default.
		FSharedConstNavQueryFilter sharedQueryFilter = Query.QueryFilter;
		if (!sharedQueryFilter.IsValid())
		{
			sharedQueryFilter = recastNavMesh->GetDefaultQueryFilter();
		}
		if (!sharedQueryFilter.IsValid())
		{
			return false;
		}

		const FNavigationQueryFilter* navigationQueryFilter = sharedQueryFilter.Get();
		if (navigationQueryFilter == nullptr)
		{
			return false;
		}

		// Project to navmesh so we get a valid poly ref.
		FNavLocation navLocation;
		const FVector queryExtent(100.0f, 100.0f, 200.0f);

		const UObject* querierObject = Query.Owner.Get();
		const bool bWasProjected =
			recastNavMesh->ProjectPoint(Location,
			                            navLocation,
			                            queryExtent,
			                            sharedQueryFilter,
			                            querierObject);

		if (!bWasProjected || navLocation.NodeRef == INVALID_NAVNODEREF)
		{
			return false;
		}

		// Read area ID of that poly.
		uint8 polyAreaId = RECAST_DEFAULT_AREA;
		polyAreaId = recastNavMesh->GetPolyAreaID(navLocation.NodeRef); 

		// Pull the full area cost table from the active query filter.
		float areaTravelCosts[RECAST_MAX_AREAS];
		float areaFixedEnteringCosts[RECAST_MAX_AREAS];

		FMemory::Memzero(areaTravelCosts, sizeof(areaTravelCosts));
		FMemory::Memzero(areaFixedEnteringCosts, sizeof(areaFixedEnteringCosts));

		navigationQueryFilter->GetAllAreaCosts(
			areaTravelCosts,
			areaFixedEnteringCosts,
			RECAST_MAX_AREAS);

		// "Default" cost = cost of the default nav area (RECAST_DEFAULT_AREA).
		float defaultAreaCost = areaTravelCosts[RECAST_DEFAULT_AREA];
		if (defaultAreaCost <= 0.0f)
		{
			// Very defensive fallback – Recast defaults are 1.0 for all areas. 
			defaultAreaCost = 1.0f;
		}

		// Cost for the area this poly belongs to.
		float polyAreaTravelCost = defaultAreaCost;
		if (polyAreaId < RECAST_MAX_AREAS)
		{
			const float areaCostFromFilter = areaTravelCosts[polyAreaId];
			if (areaCostFromFilter > 0.0f)
			{
				polyAreaTravelCost = areaCostFromFilter;
			}
		}

		// If the poly has effectively no cost (e.g. filtered out), do not treat it as "high cost".
		if (polyAreaTravelCost <= 0.0f)
		{
			return false;
		}

		// High-cost = at least 10× the default area’s travel cost.
		constexpr float HighCostMultiplier = 10.0f;
		const float highCostThreshold = defaultAreaCost * HighCostMultiplier;

		return polyAreaTravelCost >= highCostThreshold;
	}
