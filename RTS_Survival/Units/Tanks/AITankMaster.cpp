// Copyright (C) Bas Blokzijl - All rights reserved.


#include "AITankMaster.h"

#include "BrainComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "TankMaster.h"
#include "TimerManager.h"


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
	const FPathFollowingRequestResult MoveResult = TryMoveToLocationWithOffNavRecovery(
		Location,
		GoalAcceptanceRadius);
	return MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful ||
		MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal;
}

bool AAITankMaster::IssueQueuedMovementRequest(
	const FVector& Location,
	const EAbilityID CompletionAbility,
	const uint64 CommandExecutionSerial,
	const float GoalAcceptanceRadiusOverride)
{
	if (CompletionAbility == EAbilityID::IdNoAbility || CommandExecutionSerial == 0)
	{
		RTSFunctionLibrary::ReportError(
			"AAITankMaster::IssueQueuedMovementRequest received invalid command ownership.");
		return false;
	}
	if (not GetIsValidVehiclePathComp())
	{
		DeferQueuedMovementRequestFailure(CompletionAbility, CommandExecutionSerial);
		return false;
	}

	ResetQueuedMovementRequestOwnership();
	M_QueuedMovementRequest.State = EQueuedTankMovementRequestState::Issuing;
	M_QueuedMovementRequest.Owner = ETankMovementRequestOwner::QueuedCommand;
	M_QueuedMovementRequest.RequestID = FAIRequestID::InvalidRequest;
	M_QueuedMovementRequest.CompletionAbility = CompletionAbility;
	M_QueuedMovementRequest.CommandExecutionSerial = CommandExecutionSerial;

	const float GoalAcceptanceRadius = GoalAcceptanceRadiusOverride >= 0.f
		                                   ? GoalAcceptanceRadiusOverride
		                                   : m_VehiclePathComp->GetGoalAcceptanceRadius();
	const FPathFollowingRequestResult MoveResult = TryMoveToLocationWithOffNavRecovery(
		Location,
		GoalAcceptanceRadius);

	if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		M_QueuedMovementRequest.State = EQueuedTankMovementRequestState::Active;
		M_QueuedMovementRequest.RequestID = MoveResult.MoveId;
		return true;
	}

	ResetQueuedMovementRequestOwnership();
	if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// Unreal reports this synchronously from MoveTo. Resolve next tick so command start logic can finish first.
		DeferQueuedMovementCompletion(CompletionAbility, CommandExecutionSerial);
		return true;
	}

	// Request creation failure can also emit a synchronous Invalid callback. The issuing state deliberately ignored it.
	DeferQueuedMovementRequestFailure(CompletionAbility, CommandExecutionSerial);
	return false;
}

bool AAITankMaster::IssueTurretRangeMovementRequest(
	const FVector& Location,
	const uint64 TurretRangePursuitSerial)
{
	if (TurretRangePursuitSerial == 0)
	{
		RTSFunctionLibrary::ReportError(
			"AAITankMaster::IssueTurretRangeMovementRequest received an invalid pursuit serial.");
		return false;
	}
	if (not GetIsValidVehiclePathComp())
	{
		DeferTurretRangeMovementResult(TurretRangePursuitSerial, EPathFollowingResult::Invalid);
		return false;
	}

	ResetQueuedMovementRequestOwnership();
	M_QueuedMovementRequest.State = EQueuedTankMovementRequestState::Issuing;
	M_QueuedMovementRequest.Owner = ETankMovementRequestOwner::TurretRange;
	M_QueuedMovementRequest.RequestID = FAIRequestID::InvalidRequest;
	M_QueuedMovementRequest.CompletionAbility = EAbilityID::IdNoAbility;
	M_QueuedMovementRequest.CommandExecutionSerial = 0;
	M_QueuedMovementRequest.TurretRangePursuitSerial = TurretRangePursuitSerial;

	const FPathFollowingRequestResult MoveResult = TryMoveToLocationWithOffNavRecovery(
		Location,
		m_VehiclePathComp->GetGoalAcceptanceRadius());
	if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		M_QueuedMovementRequest.State = EQueuedTankMovementRequestState::Active;
		M_QueuedMovementRequest.RequestID = MoveResult.MoveId;
		return true;
	}

	ResetQueuedMovementRequestOwnership();
	const EPathFollowingResult::Type ImmediateResult =
		MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal
			? EPathFollowingResult::Success
			: EPathFollowingResult::Invalid;
	DeferTurretRangeMovementResult(TurretRangePursuitSerial, ImmediateResult);
	return MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal;
}

void AAITankMaster::ClearMovementRequestOwnership()
{
	ResetQueuedMovementRequestOwnership();
}

void AAITankMaster::ClearTurretRangeMovementRequestOwnership(const uint64 TurretRangePursuitSerial)
{
	const bool bOwnsMatchingTurretPursuit =
		M_QueuedMovementRequest.Owner == ETankMovementRequestOwner::TurretRange &&
		M_QueuedMovementRequest.TurretRangePursuitSerial == TurretRangePursuitSerial;
	if (bOwnsMatchingTurretPursuit)
	{
		ResetQueuedMovementRequestOwnership();
	}
}

bool AAITankMaster::GetHasQueuedMovementRequestOwnership(const uint64 CommandExecutionSerial) const
{
	return M_QueuedMovementRequest.Owner == ETankMovementRequestOwner::QueuedCommand &&
		M_QueuedMovementRequest.State != EQueuedTankMovementRequestState::None &&
		M_QueuedMovementRequest.CommandExecutionSerial == CommandExecutionSerial;
}

bool AAITankMaster::GetHasTurretRangeMovementRequestOwnership(const uint64 TurretRangePursuitSerial) const
{
	return M_QueuedMovementRequest.Owner == ETankMovementRequestOwner::TurretRange &&
		M_QueuedMovementRequest.State != EQueuedTankMovementRequestState::None &&
		M_QueuedMovementRequest.TurretRangePursuitSerial == TurretRangePursuitSerial;
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

	if constexpr (DeveloperSettings::Debugging::GPathFindingCosts_Compile_DebugSymbols)
	{
		DebugPathFollowingResult(Result);
	}

	// MoveTo can synchronously finish the old request or an immediate result while the new request is being issued.
	// IssueQueuedMovementRequest owns resolving the returned result after that call unwinds.
	if (M_QueuedMovementRequest.State == EQueuedTankMovementRequestState::Issuing)
	{
		return;
	}

	const bool bOwnsCompletedRequest =
		M_QueuedMovementRequest.State == EQueuedTankMovementRequestState::Active &&
		RequestID.IsValid() &&
		M_QueuedMovementRequest.RequestID.IsValid() &&
		RequestID.GetID() == M_QueuedMovementRequest.RequestID.GetID();
	if (not bOwnsCompletedRequest)
	{
		return;
	}

	const ETankMovementRequestOwner RequestOwner = M_QueuedMovementRequest.Owner;
	const EAbilityID QueuedMovementAbility = M_QueuedMovementRequest.CompletionAbility;
	const uint64 CommandExecutionSerial = M_QueuedMovementRequest.CommandExecutionSerial;
	const uint64 TurretRangePursuitSerial = M_QueuedMovementRequest.TurretRangePursuitSerial;
	ResetQueuedMovementRequestOwnership();

	if (Result.Code == EPathFollowingResult::Aborted &&
		Result.HasFlag(FPathFollowingResultFlags::NewRequest))
	{
		// Replacement requests own their own completion. Never let this stale callback resolve either command.
		return;
	}
	if (RequestOwner == ETankMovementRequestOwner::TurretRange)
	{
		DispatchTurretRangeMovementResult(TurretRangePursuitSerial, Result.Code);
		return;
	}
	if (RequestOwner != ETankMovementRequestOwner::QueuedCommand)
	{
		return;
	}

	if (not GetDoesCommandExecutionStillOwnRequest(QueuedMovementAbility, CommandExecutionSerial))
	{
		return;
	}

	if (Result.Code == EPathFollowingResult::Success)
	{
		OnQueuedMovementCompleted(QueuedMovementAbility, CommandExecutionSerial);
		return;
	}

	OnQueuedMovementFailed(QueuedMovementAbility, CommandExecutionSerial, Result.Code);
}

void AAITankMaster::OnQueuedMovementCompleted(
	const EAbilityID CompletedMovementAbility,
	const uint64 CommandExecutionSerial)
{
	if (not GetIsValidControlledTank())
	{
		return;
	}

	ControlledTank->TryDoneExecutingCommand(CompletedMovementAbility, CommandExecutionSerial);
}

void AAITankMaster::OnQueuedMovementFailed(
	const EAbilityID FailedMovementAbility,
	const uint64 CommandExecutionSerial,
	const EPathFollowingResult::Type FailedMovementResultCode)
{
	(void)FailedMovementResultCode;
	if (not GetIsValidControlledTank())
	{
		return;
	}

	ControlledTank->TryDoneExecutingCommand(FailedMovementAbility, CommandExecutionSerial);
}

void AAITankMaster::OnQueuedMovementRequestFailed(
	const EAbilityID FailedMovementAbility,
	const uint64 CommandExecutionSerial)
{
	if (not GetIsValidControlledTank())
	{
		return;
	}

	ControlledTank->TryDoneExecutingCommand(FailedMovementAbility, CommandExecutionSerial);
}

void AAITankMaster::ResetQueuedMovementRequestOwnership()
{
	M_QueuedMovementRequest = FQueuedTankMovementRequest();
}

bool AAITankMaster::GetDoesCommandExecutionStillOwnRequest(
	const EAbilityID ExpectedAbility,
	const uint64 CommandExecutionSerial) const
{
	if (not GetIsValidControlledTank())
	{
		return false;
	}

	return ControlledTank->GetDoesCurrentCommandExecutionMatch(
		ExpectedAbility,
		CommandExecutionSerial);
}

void AAITankMaster::DeferQueuedMovementCompletion(
	const EAbilityID CompletionAbility,
	const uint64 CommandExecutionSerial)
{
	TWeakObjectPtr<AAITankMaster> WeakController(this);
	FTimerDelegate CompletionDelegate;
	CompletionDelegate.BindLambda([WeakController, CompletionAbility, CommandExecutionSerial]()
	{
		if (not WeakController.IsValid())
		{
			return;
		}

		AAITankMaster* const Controller = WeakController.Get();
		if (not Controller->GetDoesCommandExecutionStillOwnRequest(CompletionAbility, CommandExecutionSerial))
		{
			return;
		}

		Controller->OnQueuedMovementCompleted(CompletionAbility, CommandExecutionSerial);
	});

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(CompletionDelegate);
		return;
	}

	if (GetDoesCommandExecutionStillOwnRequest(CompletionAbility, CommandExecutionSerial))
	{
		OnQueuedMovementCompleted(CompletionAbility, CommandExecutionSerial);
	}
}

void AAITankMaster::DeferQueuedMovementRequestFailure(
	const EAbilityID FailedAbility,
	const uint64 CommandExecutionSerial)
{
	TWeakObjectPtr<AAITankMaster> WeakController(this);
	FTimerDelegate FailureDelegate;
	FailureDelegate.BindLambda([WeakController, FailedAbility, CommandExecutionSerial]()
	{
		if (not WeakController.IsValid())
		{
			return;
		}

		AAITankMaster* const Controller = WeakController.Get();
		if (not Controller->GetDoesCommandExecutionStillOwnRequest(FailedAbility, CommandExecutionSerial))
		{
			return;
		}

		Controller->OnQueuedMovementRequestFailed(FailedAbility, CommandExecutionSerial);
	});

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FailureDelegate);
		return;
	}

	if (GetDoesCommandExecutionStillOwnRequest(FailedAbility, CommandExecutionSerial))
	{
		OnQueuedMovementRequestFailed(FailedAbility, CommandExecutionSerial);
	}
}

void AAITankMaster::DeferTurretRangeMovementResult(
	const uint64 TurretRangePursuitSerial,
	const EPathFollowingResult::Type MovementResultCode)
{
	TWeakObjectPtr<AAITankMaster> WeakController(this);
	FTimerDelegate ResultDelegate;
	ResultDelegate.BindLambda([WeakController, TurretRangePursuitSerial, MovementResultCode]()
	{
		if (not WeakController.IsValid())
		{
			return;
		}

		WeakController->DispatchTurretRangeMovementResult(
			TurretRangePursuitSerial,
			MovementResultCode);
	});

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(ResultDelegate);
		return;
	}

	DispatchTurretRangeMovementResult(TurretRangePursuitSerial, MovementResultCode);
}

void AAITankMaster::DispatchTurretRangeMovementResult(
	const uint64 TurretRangePursuitSerial,
	const EPathFollowingResult::Type MovementResultCode) const
{
	if (not GetIsValidControlledTank())
	{
		return;
	}

	ControlledTank->OnTurretRangeMovementRequestFinished(
		TurretRangePursuitSerial,
		MovementResultCode);
}

void AAITankMaster::FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query,
                                           FNavPathSharedPtr& OutPath) const
{
	OnFindPath_ClearOverlapsForNewMovement();
	Super::FindPathForMoveRequest(MoveRequest, Query, OutPath);
	if (OutPath.IsValid())
	{
		// Keep the validated corridor stable while dynamic tank modifiers rebuild around physics-driven vehicles.
		OutPath->SetIgnoreInvalidation(true);
	}

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
	const float FormationUnitInnerRadius = RTSComp->GetFormationUnitInnerRadius();
	M_FormationUnitInnerRadius = FormationUnitInnerRadius;
	vehiclePathComp->UpdateBlockDetectionDistanceFromRTSRadius(FormationUnitInnerRadius);
}

void AAITankMaster::OnFindPath_ClearOverlapsForNewMovement() const
{
	if (not GetIsValidVehiclePathComp())
	{
		return;
	}
	m_VehiclePathComp->ClearOverlapsForNewMovementCommand();
}

FPathFollowingRequestResult AAITankMaster::TryMoveToLocationWithOffNavRecovery(
	const FVector& Location,
	const float GoalAcceptanceRadius)
{
	FPathFollowingRequestResult FailedMoveResult;
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(Location);
	MoveRequest.SetAcceptanceRadius(GoalAcceptanceRadius);
	FRTSNavigationHelpers::ConfigureMoveRequestForPartialPathFinding(MoveRequest);
	MoveRequest.SetNavigationFilter(DefaultNavigationFilterClass);

	if (not EnsureMoveStartIsNavigable(MoveRequest))
	{
		return FailedMoveResult;
	}

	return MoveTo(MoveRequest, nullptr);
}

bool AAITankMaster::EnsureMoveStartIsNavigable(const FAIMoveRequest& MoveRequest)
{
	FPathFindingQuery PathFindingQuery;
	if (not BuildPathfindingQuery(MoveRequest, PathFindingQuery))
	{
		return false;
	}

	if (GetIsPathFindingStartNavigable(PathFindingQuery))
	{
		return true;
	}

	return TryRecoverMoveStartToFilterValidLocation(PathFindingQuery);
}

bool AAITankMaster::GetIsPathFindingStartNavigable(const FPathFindingQuery& PathFindingQuery) const
{
	const ANavigationData* NavigationData = PathFindingQuery.NavData.Get();
	if (NavigationData == nullptr || not PathFindingQuery.QueryFilter.IsValid())
	{
		return false;
	}

	FNavLocation ProjectedStartLocation;
	return NavigationData->ProjectPoint(
		PathFindingQuery.StartLocation,
		ProjectedStartLocation,
		NavigationData->GetDefaultQueryExtent(),
		PathFindingQuery.QueryFilter,
		this);
}

bool AAITankMaster::TryRecoverMoveStartToFilterValidLocation(const FPathFindingQuery& PathFindingQuery)
{
	FNavLocation RecoveryNavigationLocation;
	if (not TryProjectFilterValidRecoveryLocation(PathFindingQuery, RecoveryNavigationLocation))
	{
		return false;
	}

	if (not GetIsRecoveryProjectionWithinBounds(PathFindingQuery.StartLocation, RecoveryNavigationLocation.Location))
	{
		return false;
	}

	if (not GetDoesRecoveryPointProducePath(PathFindingQuery, RecoveryNavigationLocation))
	{
		return false;
	}

	return TryTeleportPawnToRecoveryLocation(PathFindingQuery, RecoveryNavigationLocation.Location);
}

bool AAITankMaster::TryProjectFilterValidRecoveryLocation(
	const FPathFindingQuery& PathFindingQuery,
	FNavLocation& OutRecoveryNavigationLocation) const
{
	const ANavigationData* NavigationData = PathFindingQuery.NavData.Get();
	if (NavigationData == nullptr || not PathFindingQuery.QueryFilter.IsValid())
	{
		return false;
	}

	if (M_FormationUnitInnerRadius <= 0.0f)
	{
		RTSFunctionLibrary::ReportError(
			"AAITankMaster::TryProjectFilterValidRecoveryLocation invalid formation radius");
		return false;
	}

	const FVector DefaultQueryExtent = NavigationData->GetDefaultQueryExtent();
	const FVector RecoveryProjectionExtent(
		FMath::Max(DefaultQueryExtent.X,
		           M_FormationUnitInnerRadius *
		           DeveloperSettings::GamePlay::Navigation::VehicleOffNavAreaProjectionMlt),
		FMath::Max(DefaultQueryExtent.Y,
		           M_FormationUnitInnerRadius *
		           DeveloperSettings::GamePlay::Navigation::VehicleOffNavAreaProjectionMlt),
		DefaultQueryExtent.Z);

	const bool bProjected = NavigationData->ProjectPoint(
		PathFindingQuery.StartLocation,
		OutRecoveryNavigationLocation,
		RecoveryProjectionExtent,
		PathFindingQuery.QueryFilter,
		this);
	return bProjected && OutRecoveryNavigationLocation.NodeRef != INVALID_NAVNODEREF;
}

bool AAITankMaster::GetIsRecoveryProjectionWithinBounds(
	const FVector& PathFindingStartLocation,
	const FVector& RecoveryNavigationLocation) const
{
	const FVector RecoveryDelta = RecoveryNavigationLocation - PathFindingStartLocation;
	const float RecoveryDistance2D = RecoveryDelta.Size2D();
	const float RecoveryDistanceZ = FMath::Abs(RecoveryDelta.Z);
	return RecoveryDistance2D > UE_KINDA_SMALL_NUMBER &&
		RecoveryDistance2D <= M_FormationUnitInnerRadius &&
		RecoveryDistanceZ <= M_FormationUnitInnerRadius;
}

bool AAITankMaster::GetDoesRecoveryPointProducePath(
	const FPathFindingQuery& PathFindingQuery,
	const FNavLocation& RecoveryNavigationLocation) const
{
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavigationSystem == nullptr)
	{
		return false;
	}

	FPathFindingQuery RecoveryPathFindingQuery = PathFindingQuery;
	RecoveryPathFindingQuery.StartLocation = RecoveryNavigationLocation.Location;
	const FPathFindingResult RecoveryPathResult = NavigationSystem->FindPathSync(RecoveryPathFindingQuery);
	return RecoveryPathResult.IsSuccessful() && RecoveryPathResult.Path.IsValid();
}

bool AAITankMaster::TryTeleportPawnToRecoveryLocation(
	const FPathFindingQuery& PathFindingQuery,
	const FVector& RecoveryNavigationLocation)
{
	APawn* ControlledPawnLocal = GetPawn();
	if (not IsValid(ControlledPawnLocal))
	{
		RTSFunctionLibrary::ReportError("AAITankMaster::TryTeleportPawnToRecoveryLocation invalid pawn");
		return false;
	}

	const FVector RecoveryDelta = RecoveryNavigationLocation - PathFindingQuery.StartLocation;
	const FVector RecoveryActorLocation = ControlledPawnLocal->GetActorLocation() + RecoveryDelta;
	const bool bMovedToRecoveryLocation = ControlledPawnLocal->SetActorLocation(
		RecoveryActorLocation,
		false,
		nullptr,
		ETeleportType::ResetPhysics);
	if (not bMovedToRecoveryLocation)
	{
		return false;
	}

	FPathFindingQuery VerificationQuery = PathFindingQuery;
	VerificationQuery.StartLocation = GetNavAgentLocation();
	return GetIsPathFindingStartNavigable(VerificationQuery);
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
	if (IsValid(m_VehiclePathComp))
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
