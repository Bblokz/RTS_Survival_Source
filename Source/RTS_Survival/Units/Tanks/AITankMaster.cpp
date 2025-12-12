// Copyright (C) Bas Blokzijl - All rights reserved.


#include "AITankMaster.h"

#include "BrainComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "TankMaster.h"


#include "NavMesh/NavMeshPath.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Navigation/RTSNavAgentRegistery/RTSNavAgentRegistery.h"
#include "RTS_Survival/Navigation/RTSNavigationHelpers/FRTSNavigationHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSPathFindingHelpers/FRTSPathFindingHelpers.h"
#include "TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"


AAITankMaster::AAITankMaster(const FObjectInitializer& ObjectInitializer)
	: AVehicleAIController(ObjectInitializer),
	  ControlledTank(NULL)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}


void AAITankMaster::StopBehaviourTreeAI()
{
	UBrainComponent* const Brain = GetBrainComponent();
	if (Brain)
	{
		Brain->StopLogic("Reset");
	}
}

void AAITankMaster::StopMovement()
{
	FNavigationSystem::StopMovement(*this);
}


void AAITankMaster::MoveToLocationWithGoalAcceptance(const FVector Location)
{
	MoveToLocation(Location, m_VehiclePathComp->GetGoalAcceptanceRadius());
}

void AAITankMaster::SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter)
{
	DefaultNavigationFilterClass = NewDefaultFilter;
}

void AAITankMaster::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ControlledTank = Cast<ATankMaster>(InPawn);
	if (IsValid(ControlledTank))
	{
		ControlledTank->SetAIController(this);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this,
		                                                  "ControlledTank",
		                                                  "AAITankMaster::OnPossess");
	}
	OnPossess_SetupNavAgent(InPawn);
}

void AAITankMaster::BeginPlay()
{
	Super::BeginPlay();
	m_VehiclePathComp = Cast<UTrackPathFollowingComponent>(GetPathFollowingComponent());
	if (!IsValid(m_VehiclePathComp))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "M_vehiclePathComp", "AAITankMaster::BeginPlay");
	}
}


void AAITankMaster::FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query,
                                           FNavPathSharedPtr& OutPath) const
{
	/** 
	 * Factor used when deriving Query.CostLimit from the straight-line heuristic
	 * between start and goal. Higher values allow longer / more detoured paths
	 * (including more time in expensive areas) before the search is aborted.
	 *
	 * Typical usage:
	 *  - Decrease for cautious agents that should reject very costly routes
	 *    (e.g. medium tanks avoiding hedgehog killboxes).
	 *  - Increase if valid paths are being rejected too often as "too expensive".
	 */
	constexpr float PathCostLimitFactor = 20.f;

	/**
	 * Absolute lower bound for Query.CostLimit, regardless of start–goal distance.
	 * Prevents very short moves from getting a tiny cost limit that would
	 * incorrectly reject paths which briefly cross expensive nav areas.
	 *
	 * Typical usage:
	 *  - Raise if short moves near obstacles fail pathfinding too often.
	 *  - Lower if you want even short moves to be very cost-sensitive.
	 */
	constexpr float MinimumPathCostLimit = 2000.0f;

	/**
	 * Heuristic scale provided by the active navigation filter and passed into
	 * ComputeCostLimitFromHeuristic. Represents how strongly the nav system
	 * trusts the Euclidean distance estimate during search.
	 *
	 * If the query has no filter or the filter does not override the heuristic,
	 * this remains 1.0f.
	 */
	float heuristicScale = 1.0f;

	const bool bIsHighCostStart =
		FRTSPathFindingHelpers::IsLocationInHighCostArea(GetWorld(), Query.StartLocation, Query);


	if (bIsHighCostStart)
	{
		Query.CostLimit = TNumericLimits<float>::Max();
	}
	else
	{
		if (Query.QueryFilter.IsValid())
		{
			heuristicScale = Query.QueryFilter->GetHeuristicScale();
		}

		Query.CostLimit = FPathFindingQuery::ComputeCostLimitFromHeuristic(
			Query.StartLocation,
			Query.EndLocation,
			heuristicScale,
			PathCostLimitFactor,
			MinimumPathCostLimit);
	}

	if (DeveloperSettings::Debugging::GPathFindingCosts_Compile_DebugSymbols)
	{
		UWorld* const world = GetWorld();
		const APawn* const controlledPawn = GetPawn();

		if (world != nullptr && IsValid(controlledPawn))
		{
			const FVector debugLocation =
				controlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, 500.0f);
			if (bIsHighCostStart)
			{
				const FVector debuglocation2 = debugLocation + FVector(0, 0, 400);
				const FString HighCostsText = "HIGH COST START";
				DrawDebugString(world, debuglocation2, HighCostsText, /*TestBaseActor=*/nullptr,
				                FColor::Red, 5.0f, /*bDrawShadow=*/false);
			}

			const FString debugText = FString::Printf(
				TEXT("Max Path Cost Limit: %.1f"), Query.CostLimit);

			constexpr float lifeTimeSeconds = 5.0f;
			DrawDebugString(
				world,
				debugLocation,
				debugText,
				/*TestBaseActor=*/nullptr,
				FColor::Cyan,
				lifeTimeSeconds,
				/*bDrawShadow=*/false);
		}
	}

	// Now run the real path search with the capped cost
	Super::FindPathForMoveRequest(MoveRequest, Query, OutPath);

	if (DeveloperSettings::Debugging::GPathFindingCosts_Compile_DebugSymbols)
	{
		DebugFoundPathCost(OutPath);
		DebugPathPointsAndFilter(OutPath, MoveRequest);
	}
}

void AAITankMaster::OnPossess_SetupNavAgent(APawn* InPawn) const
{
	if (not InPawn)
	{
		return;
	}
	FRTSNavigationHelpers::SetupNavDataForTypeOnPawn(GetWorld(), InPawn);
}

void AAITankMaster::DebugFoundPathCost(const FNavPathSharedPtr& OutPath) const
{
	UWorld* const world = GetWorld();
	const APawn* const controlledPawn = GetPawn();

	if (world != nullptr && IsValid(controlledPawn) && OutPath.IsValid())
	{
		const FNavMeshPath* const navMeshPath = OutPath->CastPath<FNavMeshPath>();
		if (navMeshPath != nullptr)
		{
			const float pathCost = navMeshPath->GetCost();

			const FVector debugLocation =
				controlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, 700.0f);

			const FString debugText = FString::Printf(
				TEXT("Actual Path Cost: %.1f"), pathCost);

			constexpr float lifeTimeSeconds = 5.0f;
			DrawDebugString(
				world,
				debugLocation,
				debugText,
				/*TestBaseActor=*/nullptr,
				FColor::Green, // different color
				lifeTimeSeconds,
				/*bDrawShadow=*/false);
		}
	}
}

void AAITankMaster::DebugPathPointsAndFilter(const FNavPathSharedPtr& OutPath,
                                             const FAIMoveRequest& MoveRequest) const
{
	UWorld* World = GetWorld();
	/** @note IF using it; the crowd controller overrides the path points. */
	if (not OutPath.IsValid() || not World)
	{
		return;
	}
	OutPath->SetIgnoreInvalidation(true);
	FNavMeshPath* NavMeshPath = OutPath->CastPath<FNavMeshPath>();
	if (not NavMeshPath)
	{
		return;
	}
	for (const FNavPathPoint& PathPoint : NavMeshPath->GetPathPoints())
	{
		DrawDebugSphere(World, PathPoint.Location, 15.0f, 12, FColor::Blue, false, 5.0f);
		const FString QueryFilter = MoveRequest.GetNavigationFilter()
			                            ? MoveRequest.GetNavigationFilter()->GetName()
			                            : TEXT("NONE");
		// at 100 units above location print the filter name:
		DrawDebugString(World, PathPoint.Location + FVector(0, 0, 100.f), QueryFilter, nullptr,
		                FColor::White, 5.0f, false, 1.4f);
	}

	// NavMeshPath->RTSOffsetFromCorners(OffsetDistance);
	// if (DeveloperSettings::Debugging::GRTSNavAgents_Compile_DebugSymbols)
	// {
	// 	for (const FNavPathPoint& PathPoint : NavMeshPath->GetPathPoints())
	// 	{
	// 		DrawDebugSphere(World, PathPoint.Location, 30.0f, 12, FColor::Purple, false, 5.0f);
	// 	}
	// }
}
