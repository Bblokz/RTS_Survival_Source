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
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"

namespace TankPathFollowingDebug
{
	constexpr float ResultTextZOffset = 400.0f;
	constexpr float ResultMyStuckDectionRadiusOffset = 300.0f;
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


bool AAITankMaster::MoveToLocationWithGoalAcceptance(
	const FVector Location,
	const float GoalAcceptanceRadiusOverride)
{
	if (not GetIsValidVehiclePathComp())
	{
		return false;
	}

	const float GoalAcceptanceRadius = GoalAcceptanceRadiusOverride >= 0.f
		? GoalAcceptanceRadiusOverride
		: m_VehiclePathComp->GetGoalAcceptanceRadius();
	return TryMoveToLocationWithOffNavRecovery(Location, GoalAcceptanceRadius);
}

void AAITankMaster::SetQueuedMovementCompletionAbility(const EAbilityID CompletionAbility)
{
	M_QueuedMovementCompletionAbility = CompletionAbility;
	bM_HasQueuedMovementCompletionAbility = CompletionAbility != EAbilityID::IdNoAbility;
}

void AAITankMaster::SetHarvesterMoveBlockDetectionSuppressed(const bool bShouldSuppress)
{
	SetMoveBlockDetection(not bShouldSuppress);

	if (not GetIsValidVehiclePathComp())
	{
		return;
	}

	m_VehiclePathComp->SetEngineBlockDetectionSuppressionLock(bShouldSuppress);
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
		OnPosses_ConfigureBlockingDectionDistanceWithRTSRadius(ControlledTank);
	}
	OnPossess_SetupNavAgent(InPawn);
}

void AAITankMaster::BeginPlay()
{
	Super::BeginPlay();
	m_VehiclePathComp = Cast<UTrackPathFollowingComponent>(GetPathFollowingComponent());
	(void)GetIsValidVehiclePathComp();
}

void AAITankMaster::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (Result.Code == EPathFollowingResult::Success)
	{
		ResetOffNavRetryBudgetForLocation(m_MoveToLocation);
	}

	if constexpr (DeveloperSettings::Debugging::GPathFindingCosts_Compile_DebugSymbols)
	{
		DebugPathFollowingResult(Result);
	}

	if (not bM_HasQueuedMovementCompletionAbility)
	{
		return;
	}

	const EAbilityID QueuedMovementAbility = M_QueuedMovementCompletionAbility;
	bM_HasQueuedMovementCompletionAbility = false;
	M_QueuedMovementCompletionAbility = EAbilityID::IdNoAbility;

	if (Result.Code == EPathFollowingResult::Success)
	{
		OnQueuedMovementCompleted(QueuedMovementAbility);
		return;
	}

	OnQueuedMovementFailed(QueuedMovementAbility, Result.Code);
}

void AAITankMaster::OnQueuedMovementCompleted(const EAbilityID CompletedMovementAbility)
{
	if (not GetIsValidControlledTank())
	{
		return;
	}

	ControlledTank->DoneExecutingCommand(CompletedMovementAbility);
}

void AAITankMaster::OnQueuedMovementFailed(
	const EAbilityID FailedMovementAbility,
	const EPathFollowingResult::Type FailedMovementResultCode)
{
	(void)FailedMovementAbility;
	(void)FailedMovementResultCode;
}

void AAITankMaster::OnQueuedMovementRequestFailed(const EAbilityID FailedMovementAbility)
{
}

void AAITankMaster::FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query,
                                           FNavPathSharedPtr& OutPath) const
{
	OnFindPath_ClearOverlapsForNewMovement();
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

void AAITankMaster::OnPosses_ConfigureBlockingDectionDistanceWithRTSRadius(ATankMaster* MyControlledTank)
{
	UTrackPathFollowingComponent* const vehiclePathComp =
		Cast<UTrackPathFollowingComponent>(GetPathFollowingComponent());
	if (not IsValid(MyControlledTank) || not IsValid(vehiclePathComp))
	{
		RTSFunctionLibrary::ReportError("AAITankMaster::OnPosses_ConfigureBlockingDectionDistanceWithRTSRadius - "
			"ControlledTank or vehiclePathComp is invalid");
		return;
	}
	URTSComponent* RTSComp = MyControlledTank->FindComponentByClass<URTSComponent>();
	if (not RTSComp)
	{
		return;
	}
	const float RTSRadius = RTSComp->GetFormationUnitInnerRadius();
	vehiclePathComp->UpdateBlockDetectionDistanceFromRTSRadius(RTSRadius);
}

void AAITankMaster::OnFindPath_ClearOverlapsForNewMovement() const
{
	if (not GetIsValidVehiclePathComp())
	{
		return;
	}
	m_VehiclePathComp->ClearOverlapsForNewMovementCommand();
}

bool AAITankMaster::TryMoveToLocationWithOffNavRecovery(const FVector& Location, const float GoalAcceptanceRadius)
{
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(Location);
	MoveRequest.SetAcceptanceRadius(GoalAcceptanceRadius);
	FRTSNavigationHelpers::ConfigureMoveRequestForPartialPathFinding(MoveRequest);
	MoveRequest.SetNavigationFilter(DefaultNavigationFilterClass);
	

	const FPathFollowingRequestResult MoveResult = MoveTo(MoveRequest, nullptr);
	if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful ||
		MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		ResetOffNavRetryBudgetForLocation(Location);
		return true;
	}

	if (not TryRecoverFromOffNavStartForMoveLocation(Location))
	{
		return false;
	}

	const FPathFollowingRequestResult RetryResult = MoveTo(MoveRequest, nullptr);
	if (RetryResult.Code == EPathFollowingRequestResult::RequestSuccessful ||
		RetryResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		ResetOffNavRetryBudgetForLocation(Location);
		return true;
	}

	return false;
}

bool AAITankMaster::TryRecoverFromOffNavStartForMoveLocation(const FVector& Location)
{
	APawn* ControlledPawnLocal = GetPawn();
	if (not IsValid(ControlledPawnLocal))
	{
		RTSFunctionLibrary::ReportError("AAITankMaster::TryRecoverFromOffNavStartForMoveLocation invalid pawn");
		return false;
	}

	FVector ProjectedLocation = FVector::ZeroVector;
	if (not TryProjectPawnLocationToNavigation(ProjectedLocation))
	{
		return false;
	}

	if (not TryConsumeOffNavRetryBudgetForLocation(Location))
	{
		return false;
	}

	ControlledPawnLocal->SetActorLocation(ProjectedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	return true;
}

bool AAITankMaster::TryProjectPawnLocationToNavigation(FVector& OutProjectedLocation) const
{
	const APawn* ControlledPawnLocal = GetPawn();
	if (not IsValid(ControlledPawnLocal))
	{
		return false;
	}

	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavigationSystem == nullptr)
	{
		return false;
	}

	FNavLocation ProjectedNavLocation;
	const bool bProjected = NavigationSystem->ProjectPointToNavigation(
		ControlledPawnLocal->GetActorLocation(),
		ProjectedNavLocation,
		M_OffNavProjectionExtent);

	if (not bProjected)
	{
		return false;
	}

	OutProjectedLocation = ProjectedNavLocation.Location;
	return true;
}

bool AAITankMaster::GetIsOffNavProjectionDeltaSignificant(
	const FVector& CurrentLocation,
	const FVector& ProjectedLocation) const
{
	const FVector DeltaLocation = ProjectedLocation - CurrentLocation;
	const float DeltaDistance2D = FVector2D(DeltaLocation.X, DeltaLocation.Y).Size();
	const float DeltaDistanceZ = FMath::Abs(DeltaLocation.Z);
	return DeltaDistance2D >= M_OffNavProjectionDelta2DThresholdUnits ||
		DeltaDistanceZ >= M_OffNavProjectionDeltaZThresholdUnits;
}

FIntVector AAITankMaster::BuildMoveLocationRetryKey(const FVector& Location) const
{
	return FIntVector(
		FMath::RoundToInt(Location.X / M_OffNavRetryGridSizeUnits),
		FMath::RoundToInt(Location.Y / M_OffNavRetryGridSizeUnits),
		FMath::RoundToInt(Location.Z / M_OffNavRetryGridSizeUnits));
}

bool AAITankMaster::TryConsumeOffNavRetryBudgetForLocation(const FVector& Location)
{
	const FIntVector RetryKey = BuildMoveLocationRetryKey(Location);
	int32& RetryCount = M_OffNavRetriesPerLocation.FindOrAdd(RetryKey);
	if (RetryCount >= M_MaxOffNavRecoveryRetriesPerLocation)
	{
		return false;
	}

	RetryCount++;
	return true;
}

void AAITankMaster::ResetOffNavRetryBudgetForLocation(const FVector& Location)
{
	const FIntVector RetryKey = BuildMoveLocationRetryKey(Location);
	M_OffNavRetriesPerLocation.Remove(RetryKey);
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
		// // at 100 units above location print the filter name:
		// DrawDebugString(World, PathPoint.Location + FVector(0, 0, 100.f), QueryFilter, nullptr,
		//                 FColor::White, 5.0f, false, 1.4f);
		// draw a line from this point to the next point (if not last point)
		const int32 PathPointIndex = NavMeshPath->GetPathPoints().IndexOfByKey(PathPoint);
		if (PathPointIndex == 0)
		{
			// Draw line from actor location to first path point
			const FVector ActorLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
			DrawDebugLine(World, ActorLocation, PathPoint.Location, FColor::Yellow, false,
			              5.0f, 0, 5.0f);
			continue;
		}
		if (PathPointIndex < NavMeshPath->GetPathPoints().Num() - 1)
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
	if(IsValid(m_VehiclePathComp))
	{
		// debug draw the my stuck detection radius
		const float MyStuckDetectionRadius = m_VehiclePathComp->GetStuckDetectionDist();
		const FString RadiusText = "My Stuck Detection Radius: " + FString::SanitizeFloat(MyStuckDetectionRadius);
		DrawDebugString(
			world,
			debugLocation + FVector(0.0f, 0.0f, TankPathFollowingDebug::ResultMyStuckDectionRadiusOffset),
			RadiusText,
			/*TestBaseActor=*/nullptr,
			FColor::White,
			TankPathFollowingDebug::ResultTextDurationSeconds,
			/*bDrawShadow=*/false
		);
	}
}
