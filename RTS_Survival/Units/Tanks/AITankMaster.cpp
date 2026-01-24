// Copyright (C) Bas Blokzijl - All rights reserved.


#include "AITankMaster.h"

#include "BrainComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "TankMaster.h"


#include "Navigation/PathFollowingComponent.h"
#include "NavMesh/NavMeshPath.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Navigation/RTSNavAgentRegistery/RTSNavAgentRegistery.h"
#include "RTS_Survival/Navigation/RTSNavigationHelpers/FRTSNavigationHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSPathFindingHelpers/FRTSPathFindingHelpers.h"
#include "TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"

namespace TankPathFollowingDebug
{
	constexpr float ResultTextZOffset = 300.0f;
	constexpr float ResultTextDurationSeconds = 8.0f;
}

AAITankMaster::AAITankMaster(const FObjectInitializer& ObjectInitializer)
	: AVehicleAIController(ObjectInitializer),
	  ControlledTank(nullptr),
	  m_VehiclePathComp(nullptr)
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
	if (not GetIsValidVehiclePathComp())
	{
		return;
	}

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
	if (GetIsValidControlledTank())
	{
		ControlledTank->SetAIController(this);
	}
	OnPossess_SetupNavAgent(InPawn);
}

void AAITankMaster::BeginPlay()
{
	Super::BeginPlay();
	m_VehiclePathComp = Cast<UTrackPathFollowingComponent>(GetPathFollowingComponent());
	GetIsValidVehiclePathComp();
}

void AAITankMaster::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if constexpr (DeveloperSettings::Debugging::GPathFindingCosts_Compile_DebugSymbols)
	{
		DebugPathFollowingResult(Result);
	}
}

void AAITankMaster::FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query,
                                           FNavPathSharedPtr& OutPath) const
{
	OnFindPath_ClearOverlapsForNewMovement();
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

	if constexpr (DeveloperSettings::Debugging::GPathFindingCosts_Compile_DebugSymbols)
	{
		DebugPathCostLimit(Query, bIsHighCostStart);
	}

	// Now run the real path search with the capped cost
	Super::FindPathForMoveRequest(MoveRequest, Query, OutPath);

	if constexpr (DeveloperSettings::Debugging::GPathFindingCosts_Compile_DebugSymbols)
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

void AAITankMaster::OnFindPath_ClearOverlapsForNewMovement() const
{
	if (not GetIsValidVehiclePathComp())
	{
		return;
	}
	m_VehiclePathComp->ClearOverlapsForNewMovementCommand();
}

void AAITankMaster::DebugPathCostLimit(const FPathFindingQuery& Query, bool bIsHighCostStart) const
{
	constexpr float CostLimitZOffset = 500.0f;
	constexpr float HighCostExtraZOffset = 400.0f;
	constexpr float DebugLifetimeSeconds = 5.0f;

	UWorld* const world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	const APawn* const controlledPawn = GetPawn();
	if (not IsValid(controlledPawn))
	{
		return;
	}

	const FVector debugLocation =
		controlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, CostLimitZOffset);

	if (bIsHighCostStart)
	{
		const FVector highCostDebugLocation =
			debugLocation + FVector(0.0f, 0.0f, HighCostExtraZOffset);
		const FString highCostText = TEXT("HIGH COST START");
		DrawDebugString(
			world,
			highCostDebugLocation,
			highCostText,
			/*TestBaseActor=*/nullptr,
			FColor::Red,
			DebugLifetimeSeconds,
			/*bDrawShadow=*/false
		);
	}

	const FString debugText = FString::Printf(
		TEXT("Max Path Cost Limit: %.1f"), Query.CostLimit);

	DrawDebugString(
		world,
		debugLocation,
		debugText,
		/*TestBaseActor=*/nullptr,
		FColor::Cyan,
		DebugLifetimeSeconds,
		/*bDrawShadow=*/false);
}

void AAITankMaster::DebugFoundPathCost(const FNavPathSharedPtr& OutPath) const
{
	constexpr float PathCostZOffset = 700.0f;
	constexpr float DebugLifetimeSeconds = 5.0f;

	UWorld* const world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	const APawn* const controlledPawn = GetPawn();
	if (not IsValid(controlledPawn))
	{
		return;
	}

	if (not OutPath.IsValid())
	{
		return;
	}

	const FNavMeshPath* const navMeshPath = OutPath->CastPath<FNavMeshPath>();
	if (navMeshPath == nullptr)
	{
		return;
	}

	const float pathCost = navMeshPath->GetCost();

	const FVector debugLocation =
		controlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, PathCostZOffset);

	const FString debugText = FString::Printf(
		TEXT("Actual Path Cost: %.1f"), pathCost);

	DrawDebugString(
		world,
		debugLocation,
		debugText,
		/*TestBaseActor=*/nullptr,
		FColor::Green,
		DebugLifetimeSeconds,
		/*bDrawShadow=*/false);
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
		// draw a line from this point to the next point (if not last point)
		const int32 PathPointIndex = NavMeshPath->GetPathPoints().IndexOfByKey(PathPoint);
		if(PathPointIndex == 0)
		{
			// Draw line from actor location to first path point
			const FVector ActorLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
			DrawDebugLine(World, ActorLocation, PathPoint.Location, FColor::Green, false,
			              5.0f, 0, 5.0f);
			continue;
		}
		if (PathPointIndex < NavMeshPath->GetPathPoints().Num() - 2)
		{
			const FVector NextPointLocation =
				NavMeshPath->GetPathPoints()[PathPointIndex + 1].Location;
			DrawDebugLine(World, PathPoint.Location, NextPointLocation, FColor::Yellow, false,
			              5.0f, 0, 5.0f);
		}
	}

	// NavMeshPath->RTSOffsetFromCorners(OffsetDistance);
	// if constexpr (DeveloperSettings::Debugging::GRTSNavAgents_Compile_DebugSymbols)
	// {
	// 	for (const FNavPathPoint& PathPoint : NavMeshPath->GetPathPoints())
	// 	{
	// 		DrawDebugSphere(World, PathPoint.Location, 30.0f, 12, FColor::Purple, false, 5.0f);
	// 	}
	// }
}

bool AAITankMaster::GetIsValidControlledTank() const
{
	if (IsValid(ControlledTank))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"ControlledTank",
		"AAITankMaster::GetIsValidControlledTank",
		this
	);

	return false;
}

bool AAITankMaster::GetIsValidVehiclePathComp() const
{
	if (IsValid(m_VehiclePathComp))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"m_VehiclePathComp",
		"AAITankMaster::GetIsValidVehiclePathComp",
		this
	);

	return false;
}

void AAITankMaster::DebugPathFollowingResult(const FPathFollowingResult& Result) const
{
	if (not GetIsValidControlledTank())
	{
		return;
	}

	const ATankMaster* const validTankMaster = ControlledTank;
	if (validTankMaster == nullptr)
	{
		return;
	}

	const EPathFollowingResult::Type resultType = Result.Code;

	switch (resultType)
	{
	case EPathFollowingResult::Success:
		DebugPathFollowingResult_Draw(TEXT("Pathfinding Success"), FColor::Green, validTankMaster);
		return;
	case EPathFollowingResult::Blocked:
		DebugPathFollowingResult_Draw(TEXT("Pathfinding Blocked"), FColor::Red, validTankMaster);
		return;
	case EPathFollowingResult::Aborted:
		DebugPathFollowingResult_Draw(TEXT("Pathfinding Aborted"), FColor::Red, validTankMaster);
		return;
	default:
		DebugPathFollowingResult_Draw(TEXT("Pathfinding Result: Other"), FColor::Yellow, validTankMaster);
		return;
	}
}

void AAITankMaster::DebugPathFollowingResult_Draw(const FString& DebugText, const FColor& DebugColor,
                                                  const ATankMaster* ValidTankMaster) const
{
	UWorld* const world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	if (ValidTankMaster == nullptr)
	{
		return;
	}

	const FVector debugLocation = ValidTankMaster->GetActorLocation() +
		FVector(0.0f, 0.0f, TankPathFollowingDebug::ResultTextZOffset);

	DrawDebugString(
		world,
		debugLocation,
		DebugText,
		/*TestBaseActor=*/nullptr,
		DebugColor,
		TankPathFollowingDebug::ResultTextDurationSeconds,
		/*bDrawShadow=*/false
	);
}
