// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrackedTankMaster.h"

#include "TrackedAnimationInstance.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodySetup.h"
#include "RTS_Survival/Units/Tanks/VehicleAvoidance/RTSVehicleAvoidanceSubsystem.h"
#include "RTS_Survival/Units/Tanks/VehicleAvoidance/RTSVehicleFootprint.h"
#include "Sound/SoundBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "PathFollowingComponent/TrackPathFollowingComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Audio/SpacialVoiceLinePlayer/SpatialVoiceLinePlayer.h"
#include "RTS_Survival/Buildings/EnergyComponent/TankEnergyComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRockets.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInComponent.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceComponent.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/TankTargetAcquisition/TankTargetAcquisition.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"


FName ATrackedTankMaster::TrackPhysicsMovementName(TEXT("TrackPhysicsCalcMovement"));

namespace TrackedMovementInvariant
{
	constexpr float CheckIntervalSeconds = 0.25f;
	constexpr double OrphanGracePeriodSeconds = 0.5;
	constexpr double NoOrphanStartTime = -1.0;
}

namespace TrackedHullBlocker
{
	// How far the hull box reaches above and below the track collision.
	constexpr float ExtraHalfHeight = 120.0f;
	constexpr float Friction = 0.05f;
}

namespace TrackedParkedFacing
{
	constexpr float CheckIntervalSeconds = 1.5f;
	// Turned further away than this from the facing it was given, a parked tank turns back.
	constexpr float ToleranceDegrees = 3.0f;
	// A tank this close to where its last move went still counts as standing on that spot.
	constexpr float GoalStillHeldDistance = 450.0f;
	constexpr float RestingYawRateDegrees = 2.0f;
	constexpr float RestingSpeed = 5.0f;
	// Further than its goal acceptance plus this from the spot it was ordered to, a parked tank drives back.
	constexpr float ShovedOffSlotMargin = 40.0f;
	constexpr int32 MaximumReturnsToOneSpot = 2;
}

namespace TrackedRotateTowards
{
	constexpr float FullSteeringYawErrorDegrees = 20.0f;
	constexpr float CompletionYawToleranceDegrees = 1.0f;
	constexpr float CompletionYawRateToleranceDegrees = 1.0f;

	// A turn that reaches less than this part of the asked yaw rate is held up by something.
	constexpr float HeldUpYawRateFraction = 0.25f;
	// Close to the wanted facing the turn is slow by itself; that is not being held up.
	constexpr float HeldUpMinimumRemainingYawDegrees = 3.0f;
	constexpr float HeldUpSecondsBeforeFreeing = 0.6f;
	// Hulls further apart than this are not what holds the turn up.
	constexpr float HeldUpNeighbourHullDistance = 60.0f;
	// Just enough throttle to slide away from the other hull while the turn goes on.
	constexpr float FreeingThrottle = 0.12f;
	constexpr float SecondsBetweenDisplacements = 3.0f;
}


ATrackedTankMaster::ATrackedTankMaster(const FObjectInitializer& ObjectInitializer)
	: ATankMaster(ObjectInitializer), TankAnimationBP(nullptr), ChassisMesh(NULL)
{
	TrackPhysicsMovement = CreateDefaultSubobject<UTrackPhysicsMovement>(TrackPhysicsMovementName);
	ExperienceComponent = CreateDefaultSubobject<URTSExperienceComp>(TEXT("ExperienceComponent"));
	RTSOverlapEvasionComponent = CreateDefaultSubobject<
		URTSOverlapEvasionComponent>(TEXT("RTSOverlapEvasionComponent"));
}

ERTSAggroBehaviour ATrackedTankMaster::GetEngagementStance() const
{
	if (not GetIsValidTargetAcquisition())
	{
		return ERTSAggroBehaviour::Stance_None;
	}
	return M_TargetAcquisition->GetEngagementStance();
}

void ATrackedTankMaster::SetAggroStance(const ERTSAggroBehaviour NewStance)
{
	if (not GetIsValidTargetAcquisition())
	{
		return;
	}
	M_TargetAcquisition->SetEngagementStance(NewStance);
}

void ATrackedTankMaster::UpdateVehicle_Implementation(
	float TargetAngle,
	float DestinationDistance,
	float DesiredSpeed,
	float CalculatedThrottle,
	float CurrentSpeed,
	float DeltaTime,
	float CalculatedBreak,
	float CalculatedSteering)
{
	if (not GetIsValidTrackPhysicsMovement())
	{
		return;
	}

	// Crew repair holds a movement lock: late path-following updates brake instead of driving.
	if (GetIsCrewRepairActive())
	{
		TrackPhysicsMovement->OnPathFollowingFinished();
		return;
	}

	TrackPhysicsMovement->UpdateTankMovement(
		DeltaTime,
		CurrentSpeed,
		CalculatedThrottle,
		CalculatedSteering);
}

void ATrackedTankMaster::SetRTSOverlapEvasionEnabled(const bool bEnabled)
{
	if (not EnsureRTSOverlapEvasionComponent())
	{
		return;
	}
	RTSOverlapEvasionComponent->SetOverlapEvasionEnabled(bEnabled);
}

bool ATrackedTankMaster::DoesVehicleHaveAnyWeapons() const
{
	return not (Turrets.IsEmpty() && HullWeapons.IsEmpty());
}

void ATrackedTankMaster::OnFinishedPathFollowing()
{
	if (not GetIsValidTrackPhysicsMovement())
	{
		return;
	}

	TrackPhysicsMovement->OnPathFollowingFinished();

	if (TrackPhysicsMovement->GetLastNoneZeroThrottle() < 0)
	{
		// We arrived in reverse.
		bWasLastMovementReverse = true;

		// If not forced; reverse clears the rotation.
		if (not GetForceFinalRotationRegardlessOfReverse())
		{
			ResetRotateTowardsFinalMovementRotation();
		}
		return;
	}

	bWasLastMovementReverse = false;
}

USkeletalMeshComponent* ATrackedTankMaster::GetTankMesh() const
{
	return ChassisMesh;
}


void ATrackedTankMaster::UpgradeTurnRate(const float NewTurnRate)
{
	if (TurnRate <= 0)
	{
		RTSFunctionLibrary::ReportError("Attempted to update tank turnrate with <= 0 value"
			"\n ATrackedTankMaster::UpdateTurnRate"
			"\n for tank: " + GetName());
		return;
	}
	if (not GetIsValidTrackPhysicsMovement())
	{
		return;
	}

	TrackPhysicsMovement->UpdateTurnRate(NewTurnRate);
	TurnRate = NewTurnRate;
}

void ATrackedTankMaster::OnRTSUnitSpawned(const bool bSetDisabled, const float TimeNotSelectable, const FVector MoveTo)
{
	if (not IsValid(ChassisMesh))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "Tank mesh", "ATrackedTankMaster");
		return;
	}
	if (bSetDisabled)
	{
		OnRTSUnitSpawned_SetDisabled();
	}
	else
	{
		OnRTSUnitSpawned_SetEnabled(TimeNotSelectable, MoveTo);
	}
}

void ATrackedTankMaster::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	PostInitializeComponents_SetupEngineSounds();
	M_DigInComponent = FindComponentByClass<UDigInComponent>();
	M_AttachedRockets = FindComponentByClass<UAttachedRockets>();
	M_TargetAcquisition = FindComponentByClass<UTankTargetAcquisition>();
	(void)GetIsValidTargetAcquisition();
	if (not GetIsValidDigInComponent())
	{
		return;
	}
	M_DigInComponent->SetupOwner(this);
}

void ATrackedTankMaster::BeginPlay()
{
	Super::BeginPlay();
	// Needs the proper Owning player to set which is why we cannot do this in post initialize components!
	BeginPlay_SetupExperienceComponent();
	if (ensure(GetIsValidTargetAcquisition()))
	{
		M_TargetAcquisition->ActivateAcquisition();
	}
}

void ATrackedTankMaster::BeginDestroy()
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		if (M_EngineSoundHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(M_EngineSoundHandle);
		}
		World->GetTimerManager().ClearTimer(M_MovementCommandInvariantTimerHandle);
	}
	Super::BeginDestroy();
}

void ATrackedTankMaster::UnitDies(const ERTSDeathType DeathType)
{
	if (GetIsValidDigInComponent())
	{
		M_DigInComponent->OnOwningUnitDeath();
	}
	Super::UnitDies(DeathType);
}

void ATrackedTankMaster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ATrackedTankMaster::OnUnitIdleAndNoNewCommands()
{
	StopMovementCommandInvariantMonitoring();
	// Important; calls the delegate in base ICommands.
	Super::OnUnitIdleAndNoNewCommands();
	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(true);
	}
}

void ATrackedTankMaster::SetupChassisMeshCollision()
{
	if (not IsValid(ChassisMesh))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "ChassisMesh",
		                                             "ATrackedTankMaster::SetupChassisMeshCollision");
		return;
	}
	if (IsValid(RTSComponent))
	{
		if (RTSComponent->GetOwningPlayer() == 1)
		{
			FRTS_CollisionSetup::SetupPlayerVehicleMovementMeshCollision(ChassisMesh);
		}
		else
		{
			FRTS_CollisionSetup::SetupEnemyVehicleMovementMeshCollision(ChassisMesh);
		}
	}
	SetupHullBlocker();
}

void ATrackedTankMaster::SetupHullBlocker()
{
	if (IsValid(M_HullBlocker) || not IsValid(ChassisMesh))
	{
		return;
	}
	const FBox ChassisCollisionBox = RTSVehicleFootprint::GetChassisCollisionBox(ChassisMesh, FTransform::Identity);
	if (not ChassisCollisionBox.IsValid)
	{
		return;
	}

	M_HullBlockerMaterial = NewObject<UPhysicalMaterial>(this, TEXT("HullBlockerMaterial"));
	M_HullBlockerMaterial->Friction = TrackedHullBlocker::Friction;
	M_HullBlockerMaterial->StaticFriction = TrackedHullBlocker::Friction;
	M_HullBlockerMaterial->Restitution = 0.0f;
	M_HullBlockerMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
	M_HullBlockerMaterial->bOverrideFrictionCombineMode = true;

	M_HullBlocker = NewObject<UBoxComponent>(this, TEXT("HullBlocker"));
	M_HullBlocker->SetupAttachment(ChassisMesh);
	// Taller than the tracks, so one hull cannot climb onto another, and centred on the height of the track
	// collision: a push that lands above the weight of the tank would lever its nose up. The part that reaches
	// below the ground touches nothing, the box only meets other hull boxes.
	const FVector BlockerExtent = ChassisCollisionBox.GetExtent() +
		FVector(0.0f, 0.0f, TrackedHullBlocker::ExtraHalfHeight);
	M_HullBlocker->SetRelativeLocation(ChassisCollisionBox.GetCenter());
	M_HullBlocker->InitBoxExtent(BlockerExtent);
	FRTS_CollisionSetup::SetupVehicleHullBlockerCollision(M_HullBlocker);
	M_HullBlocker->SetPhysMaterialOverride(M_HullBlockerMaterial);
	// The tuned mass, balance and turning of the chassis must not change because of the box.
	M_HullBlocker->UpdateBodySetup();
	if (UBodySetup* BlockerBodySetup = M_HullBlocker->GetBodySetup();
		IsValid(BlockerBodySetup) && BlockerBodySetup->AggGeom.BoxElems.Num() > 0)
	{
		BlockerBodySetup->AggGeom.BoxElems[0].SetContributeToMass(false);
	}
	M_HullBlocker->RegisterComponent();
}

void ATrackedTankMaster::ExecuteHarvestResourceCommand(ACPPResourceMaster* TargetResource)
{
	// For harvesters; make sure we turn off overlap evasion.
	if (EnsureRTSOverlapEvasionComponent())
	{
		RTSOverlapEvasionComponent->SetOverlapEvasionEnabled(false);
	}
	Super::ExecuteHarvestResourceCommand(TargetResource);
}

void ATrackedTankMaster::TerminateHarvestResourceCommand()
{
	if (EnsureRTSOverlapEvasionComponent())
	{
		RTSOverlapEvasionComponent->SetOverlapEvasionEnabled(true);
	}
	Super::TerminateHarvestResourceCommand();
}

bool ATrackedTankMaster::EnsureRTSOverlapEvasionComponent() const
{
	if (not IsValid(RTSOverlapEvasionComponent))
	{
		return false;
	}
	return true;
}

bool ATrackedTankMaster::GetIsValidTargetAcquisition() const
{
	if (IsValid(M_TargetAcquisition))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_TargetAcquisition",
		"ATrackedTankMaster::GetIsValidTargetAcquisition",
		this
	);
	return false;
}

URTSExperienceComp* ATrackedTankMaster::GetExperienceComponent() const
{
	return ExperienceComponent;
}

void ATrackedTankMaster::OnUnitLevelUp(const int32 Level, const EVeterancyIconSet IconSetUsed)
{
	OnLevelUp_UpdateHealthbarRankIcon(Level, IconSetUsed);
	HandleVoiceLineOnLevelUp();
	BP_OnUnitLevelUp();
}

void ATrackedTankMaster::OnTankKilledAnyActor(AActor* KilledActor)
{
	// Call the base function for voice lines.
	Super::OnTankKilledAnyActor(KilledActor);
	// Exp interface function to collect (possible) experience from kill.
	IExpOnKilledActor(KilledActor);
}

void ATrackedTankMaster::InitTrackedTank(
	USkeletalMeshComponent* Mesh,
	const float NewTrackForceMultiplier,
	const float NewTurnRate,
	UChassisAnimInstance* TrackedAnimBP,
	const float TankCornerOffset,
	const float TankMeshZOffset, const float AngularDamping, const float LinearDamping,
	const int32 Energy)
{
	if (Energy != 0)
	{
		OnInit_FindEnergyComponent(Energy);
	}
	if (IsValid(Mesh))
	{
		ChassisMesh = Mesh;
		SetupChassisMeshCollision();
		Mesh->SetAngularDamping(AngularDamping);
		Mesh->SetLinearDamping(LinearDamping);
		if (EnsureRTSOverlapEvasionComponent())
		{
			RTSOverlapEvasionComponent->TrackOverlapMeshOfOwner(this, ChassisMesh);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "TankMesh",
		                                             "ATrackedTankMaster::InitTrackedTank");
	}
	if (IsValid(TrackedAnimBP))
	{
		TankAnimationBP = TrackedAnimBP;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this, "TrackedAnimBp", "ATrackedTankMaster::INitTrackedTank");
	}

	// The turn rate is also used by TankMaster to rotate towards a specific location.
	TurnRate = NewTurnRate;
	SetTankCornerOffset(TankCornerOffset);
	if (GetIsValidTrackPhysicsMovement())
	{
		TrackPhysicsMovement->InitTrackPhysicsMovement(
			Mesh,
			NewTrackForceMultiplier,
			NewTurnRate,
			TrackedAnimBP,
			TankMeshZOffset);
	}
}

void ATrackedTankMaster::ExecuteMoveCommand(const FVector MoveToLocation)
{
	StartMovementCommandInvariantMonitoring();
	ExecuteTrackedMoveWithNavSettleDelay(MoveToLocation, false);
	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(false);
	}
	// To auto engage turrets.
	Super::ExecuteMoveCommand(MoveToLocation);
}

void ATrackedTankMaster::TerminateMoveCommand()
{
	Super::TerminateMoveCommand();
	UCommandData* CommandData = GetIsValidCommandData();
	if (not IsValid(CommandData))
	{
		return;
	}

	if (not CommandData->GetHasQueuedMovementCommandAfterActive())
	{
		FullyStopTrackedMovementCommand();
		if (GetIsValidRTSNavCollision())
		{
			RTSNavCollision->EnableAffectNavmesh(true);
		}
	}

	CheckFootPrintForOverlaps();
}

void ATrackedTankMaster::TerminateMoveCommandForMovementReplacement()
{
	Super::TerminateMoveCommandForMovementReplacement();
	CancelPendingTrackedMove();
	if (GetIsValidAITankController())
	{
		AITankController->ClearMovementRequestOwnership();
	}
	CheckFootPrintForOverlaps();
}

void ATrackedTankMaster::ExecuteReverseCommand(const FVector ReverseToLocation)
{
	StartMovementCommandInvariantMonitoring();
	ExecuteTrackedMoveWithNavSettleDelay(ReverseToLocation, true);

	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(false);
	}

	Super::ExecuteReverseCommand(ReverseToLocation);
}

void ATrackedTankMaster::TerminateReverseCommand()
{
	UCommandData* CommandData = GetIsValidCommandData();
	if (not IsValid(CommandData))
	{
		return;
	}

	if (not CommandData->GetHasQueuedMovementCommandAfterActive())
	{
		ResetTrackedReversePathFollowing();
		FullyStopTrackedMovementCommand();
		if (GetIsValidRTSNavCollision())
		{
			RTSNavCollision->EnableAffectNavmesh(true);
		}
	}
	CheckFootPrintForOverlaps();
	Super::TerminateReverseCommand();
}

void ATrackedTankMaster::TerminateReverseCommandForMovementReplacement()
{
	Super::TerminateReverseCommandForMovementReplacement();
	CancelPendingTrackedMove();
	if (GetIsValidAITankController())
	{
		AITankController->ClearMovementRequestOwnership();
	}
}

void ATrackedTankMaster::ExecuteTrackedMoveWithNavSettleDelay(const FVector& TargetLocation, const bool bIsReverse)
{
	ExecuteTrackedMoveWithNavSettleDelayForAbility(
		TargetLocation,
		bIsReverse,
		bIsReverse ? EAbilityID::IdReverseMove : EAbilityID::IdMove);
}

void ATrackedTankMaster::ExecuteTrackedMoveWithNavSettleDelayForAbility(
	const FVector& TargetLocation,
	const bool bIsReverse,
	const EAbilityID CompletionAbility,
	const float GoalAcceptanceRadiusOverride)
{
	UCommandData* CommandData = GetIsValidCommandData();
	if (not IsValid(CommandData))
	{
		return;
	}
	const uint64 CommandExecutionSerial = CommandData->GetCurrentCommandExecutionSerial();

	if (not GetIsValidAITankController())
	{
		DeferTrackedMovementRequestFailure(CompletionAbility, CommandExecutionSerial);
		return;
	}

	constexpr float MovingSpeedThresholdUnitsPerSecond = 3.f;
	const bool bIsMovingNow = GetVelocity().SizeSquared2D() > FMath::Square(MovingSpeedThresholdUnitsPerSecond);
	const bool bIsMovementChainTransition = CommandData->GetHasPreviousMovementCommandBeforeActive();
	const bool bShouldSkipNavSettleDelay = bIsMovingNow || bIsMovementChainTransition;
	const bool bHadPendingStationaryQueuedMove =
		M_QueuedMoveState.bM_HasPendingQueuedMove && M_QueuedMoveState.bM_IsStationaryWhenQueued;

	M_QueuedMoveState.M_TargetLocation = TargetLocation;
	M_QueuedMoveState.bM_HasPendingQueuedMove = true;
	M_QueuedMoveState.bM_IsReverse = bIsReverse;
	M_QueuedMoveState.M_CompletionAbility = CompletionAbility;
	M_QueuedMoveState.M_CommandExecutionSerial = CommandExecutionSerial;
	M_QueuedMoveState.M_GoalAcceptanceRadiusOverride = GoalAcceptanceRadiusOverride;
	M_QueuedMoveState.bM_IsStationaryWhenQueued = not bShouldSkipNavSettleDelay;
	// NOTE: This is the authoritative coalescing point for queued tracked moves.
	// Any later command issued before the timer fires overwrites these fields by design.

	if (GetWorld() == nullptr)
	{
		RTSFunctionLibrary::ReportError("ATrackedTankMaster::ExecuteTrackedMoveWithNavSettleDelay - World is null.");
		M_QueuedMoveState.bM_HasPendingQueuedMove = false;
		DeferTrackedMovementRequestFailure(CompletionAbility, CommandExecutionSerial);
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(M_DeferredTrackedMoveHandle);

	if (M_QueuedMoveState.bM_IsStationaryWhenQueued)
	{
		if (not bHadPendingStationaryQueuedMove)
		{
			PlayEngineGasSoundEffect();
		}

		constexpr float NavSettleDelaySeconds = 0.4f;
		GetWorld()->GetTimerManager().SetTimer(
			M_DeferredTrackedMoveHandle,
			this,
			&ATrackedTankMaster::ExecuteTrackedMoveWithNavSettleDelay_Deferred,
			NavSettleDelaySeconds,
			false);
		return;
	}

	ExecuteTrackedMoveWithNavSettleDelay_Deferred();
}

void ATrackedTankMaster::ExecuteTrackedMoveWithNavSettleDelay_Deferred()
{
	if (not M_QueuedMoveState.bM_HasPendingQueuedMove)
	{
		return;
	}

	const FVector TargetLocation = M_QueuedMoveState.M_TargetLocation;
	const bool bIsReverse = M_QueuedMoveState.bM_IsReverse;
	const EAbilityID CompletionAbility = M_QueuedMoveState.M_CompletionAbility;
	const uint64 CommandExecutionSerial = M_QueuedMoveState.M_CommandExecutionSerial;
	const float GoalAcceptanceRadiusOverride = M_QueuedMoveState.M_GoalAcceptanceRadiusOverride;
	M_QueuedMoveState.bM_HasPendingQueuedMove = false;
	// NOTE: Pending flag is cleared before issuing so failure callbacks can safely enqueue a retry.
	ExecuteTrackedMoveNow(
		TargetLocation,
		bIsReverse,
		CompletionAbility,
		CommandExecutionSerial,
		GoalAcceptanceRadiusOverride);
}

void ATrackedTankMaster::ResetTrackedReversePathFollowing()
{
	if (not GetIsValidAITankController())
	{
		return;
	}

	UTrackPathFollowingComponent* TrackPathFollowingComponent =
		Cast<UTrackPathFollowingComponent>(AITankController->GetPathFollowingComponent());
	if (not IsValid(TrackPathFollowingComponent))
	{
		return;
	}

	TrackPathFollowingComponent->SetReverse(false);
}

void ATrackedTankMaster::FullyStopTrackedMovementCommand()
{
	CancelPendingTrackedMove();
	StopMovementCommandInvariantMonitoring();
	StopBehaviourTree();
	if (GetIsValidAITankController())
	{
		AITankController->ClearMovementRequestOwnership();
		AITankController->StopMovement();
	}

	if (not GetIsValidTankAnimationBP())
	{
		return;
	}

	// Stop all animations.
	TankAnimationBP->SetChassisAnimToIdle();
}

void ATrackedTankMaster::OnCancelMovementToGetInRangeOfTurret()
{
	// Re-enables the nav collision.
	Super::OnCancelMovementToGetInRangeOfTurret();
	FullyStopTrackedMovementCommand();
}

void ATrackedTankMaster::CancelPendingTrackedMove()
{
	M_QueuedMoveState.bM_HasPendingQueuedMove = false;

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_DeferredTrackedMoveHandle);
}

void ATrackedTankMaster::DeferTrackedMovementRequestFailure(
	const EAbilityID FailedAbility,
	const uint64 CommandExecutionSerial)
{
	TWeakObjectPtr<ATrackedTankMaster> WeakTank(this);
	FTimerDelegate FailureDelegate;
	FailureDelegate.BindLambda([WeakTank, FailedAbility, CommandExecutionSerial]()
	{
		if (not WeakTank.IsValid())
		{
			return;
		}

		WeakTank->TryDoneExecutingCommand(FailedAbility, CommandExecutionSerial);
	});

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FailureDelegate);
		return;
	}

	TryDoneExecutingCommand(FailedAbility, CommandExecutionSerial);
}

void ATrackedTankMaster::StartMovementCommandInvariantMonitoring()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || World->GetTimerManager().IsTimerActive(M_MovementCommandInvariantTimerHandle))
	{
		return;
	}

	ResetOrphanedMovementInvariantGracePeriod();
	World->GetTimerManager().SetTimer(
		M_MovementCommandInvariantTimerHandle,
		this,
		&ATrackedTankMaster::CheckMovementCommandInvariant,
		TrackedMovementInvariant::CheckIntervalSeconds,
		true);
}

void ATrackedTankMaster::StopMovementCommandInvariantMonitoring()
{
	ResetOrphanedMovementInvariantGracePeriod();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_MovementCommandInvariantTimerHandle);
	}
}

void ATrackedTankMaster::CheckMovementCommandInvariant()
{
	UCommandData* const CommandData = GetIsValidCommandData();
	if (not IsValid(CommandData) || not CommandData->GetIsCurrentCommandAMovementCommand())
	{
		StopMovementCommandInvariantMonitoring();
		return;
	}

	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const uint64 CommandExecutionSerial = CommandData->GetCurrentCommandExecutionSerial();
	AAITankMaster* const TankController = GetAIController();
	const bool bHasPendingMoveTimer = M_QueuedMoveState.bM_HasPendingQueuedMove &&
		World->GetTimerManager().IsTimerActive(M_DeferredTrackedMoveHandle);
	const bool bHasOwnedRequest = IsValid(TankController) &&
		TankController->GetHasQueuedMovementRequestOwnership(CommandExecutionSerial);
	const bool bIsPathFollowingIdle = not IsValid(TankController) ||
		TankController->GetMoveStatus() == EPathFollowingStatus::Idle;

	if (bHasPendingMoveTimer || bHasOwnedRequest || not bIsPathFollowingIdle)
	{
		ResetOrphanedMovementInvariantGracePeriod();
		return;
	}

	const double CurrentTimeSeconds = World->GetTimeSeconds();
	if (M_OrphanedMovementInvariantStartTimeSeconds < 0.0)
	{
		M_OrphanedMovementInvariantStartTimeSeconds = CurrentTimeSeconds;
		return;
	}

	if (CurrentTimeSeconds - M_OrphanedMovementInvariantStartTimeSeconds <
		TrackedMovementInvariant::OrphanGracePeriodSeconds)
	{
		return;
	}

	RecoverOrphanedMovementCommand(CommandData, TankController);
}

void ATrackedTankMaster::RecoverOrphanedMovementCommand(
	UCommandData* CommandData,
	AAITankMaster* TankController)
{
	if (not IsValid(CommandData))
	{
		return;
	}

	const EAbilityID OrphanedAbility = CommandData->GetCurrentlyActiveCommandType();
	const uint64 CommandExecutionSerial = CommandData->GetCurrentCommandExecutionSerial();
	RTSFunctionLibrary::ReportError(
		"Recovered orphaned tracked movement command."
		"\n Tank: " + GetName() +
		"\n Ability: " + FString::FromInt(static_cast<int32>(OrphanedAbility)) +
		"\n Command execution serial: " + FString::Printf(TEXT("%llu"), CommandExecutionSerial));

	StopMovementCommandInvariantMonitoring();
	CancelPendingTrackedMove();
	if (IsValid(TankController))
	{
		TankController->ClearMovementRequestOwnership();
	}
	TryDoneExecutingCommand(OrphanedAbility, CommandExecutionSerial);
}

void ATrackedTankMaster::ResetOrphanedMovementInvariantGracePeriod()
{
	M_OrphanedMovementInvariantStartTimeSeconds = TrackedMovementInvariant::NoOrphanStartTime;
}

void ATrackedTankMaster::ExecuteTrackedMoveNow(
	const FVector& TargetLocation,
	const bool bIsReverse,
	const EAbilityID CompletionAbility,
	const uint64 CommandExecutionSerial,
	const float GoalAcceptanceRadiusOverride)
{
	if (not GetIsValidAITankController())
	{
		DeferTrackedMovementRequestFailure(CompletionAbility, CommandExecutionSerial);
		return;
	}

	if (UTrackPathFollowingComponent* TrackPFC = Cast<UTrackPathFollowingComponent>(
		AITankController->GetPathFollowingComponent()))
	{
		TrackPFC->SetReverse(bIsReverse);
	}

	AITankController->SetMoveToLocation(TargetLocation);
	if (not AITankController->IssueQueuedMovementRequest(
		TargetLocation,
		CompletionAbility,
		CommandExecutionSerial,
		GoalAcceptanceRadiusOverride))
	{
		return;
	}

	bWasLastMovementReverse = bIsReverse;
}


void ATrackedTankMaster::StopMovement()
{
	// todo kill forces.
	Super::StopMovement();
}

void ATrackedTankMaster::ExecuteAttackCommand(AActor* Target)
{
	// Set turrets to engage target.
	Super::ExecuteAttackCommand(Target);
}

void ATrackedTankMaster::ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand)
{
	// Rotation logic in TankMaster.
	Super::ExecuteRotateTowardsCommand(RotateToRotator, IsQueueCommand);
	if (IsValid(TankAnimationBP))
	{
		const bool bIsRotateToRight = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, RotateToRotator.Yaw) > 0;
		TankAnimationBP->SetChassisAnimToStationaryRotation(bIsRotateToRight);
	}
}

void ATrackedTankMaster::TerminateRotateTowardsCommand()
{
	if (IsValid(TankAnimationBP))
	{
		TankAnimationBP->SetChassisAnimToIdle();
	}
	Super::TerminateRotateTowardsCommand();
}

bool ATrackedTankMaster::ApplyRotateTowardsStep(const float RemainingYawDegrees, const float DeltaSeconds)
{
	if (not GetIsValidTrackPhysicsMovement() or not IsValid(ChassisMesh))
	{
		return Super::ApplyRotateTowardsStep(RemainingYawDegrees, DeltaSeconds);
	}

	const FVector AngularVelocityDegrees = ChassisMesh->GetPhysicsAngularVelocityInDegrees();
	const float CurrentYawRateDegrees = FVector::DotProduct(
		AngularVelocityDegrees,
		ChassisMesh->GetUpVector());
	const bool bYawWithinTolerance = FMath::Abs(RemainingYawDegrees)
		<= TrackedRotateTowards::CompletionYawToleranceDegrees;
	const bool bYawRateWithinTolerance = FMath::Abs(CurrentYawRateDegrees)
		<= TrackedRotateTowards::CompletionYawRateToleranceDegrees;
	const float SteeringCommand = bYawWithinTolerance
		? 0.0f
		: FMath::Clamp(
			RemainingYawDegrees / TrackedRotateTowards::FullSteeringYawErrorDegrees,
			-1.0f,
			1.0f);
	const float CurrentPlanarSpeed = ChassisMesh->GetComponentVelocity().Size2D();
	const float AskedYawRateDegrees = FMath::Abs(SteeringCommand) * TurnRate;
	const bool bIsTurnHeldUp =
		FMath::Abs(RemainingYawDegrees) > TrackedRotateTowards::HeldUpMinimumRemainingYawDegrees &&
		FMath::Abs(CurrentYawRateDegrees) < AskedYawRateDegrees * TrackedRotateTowards::HeldUpYawRateFraction;
	const float RotateTowardsThrottle = GetThrottleToFreeHeldUpTurn(bIsTurnHeldUp, DeltaSeconds);
	TrackPhysicsMovement->UpdateTankMovement(
		DeltaSeconds,
		CurrentPlanarSpeed,
		RotateTowardsThrottle,
		SteeringCommand);
	const bool bIsTurnDone = bYawWithinTolerance && bYawRateWithinTolerance;
	if (bIsTurnDone)
	{
		RememberParkedFacing(GetActorRotation().Yaw + RemainingYawDegrees);
	}
	return bIsTurnDone;
}

void ATrackedTankMaster::OnCommandExecutionStarting(const EAbilityID AbilityStarting)
{
	Super::OnCommandExecutionStarting(AbilityStarting);
	// Whatever comes next decides the facing anew.
	ForgetParkedFacing();
}

void ATrackedTankMaster::GetParkedPose(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = GetActorLocation();
	OutRotation = bM_HasParkedFacing ? FRotator(0.0f, M_ParkedFacingYaw, 0.0f) : GetActorRotation();

	const AAITankMaster* TankController = GetAIController();
	const UTrackPathFollowingComponent* PathFollowing = IsValid(TankController)
		                                                    ? Cast<UTrackPathFollowingComponent>(
			                                                    TankController->GetPathFollowingComponent())
		                                                    : nullptr;
	if (not IsValid(PathFollowing))
	{
		return;
	}
	const FVehicleAvoidanceState& AvoidanceState = PathFollowing->GetVehicleAvoidanceState();
	const bool bStandsOnItsGoal = AvoidanceState.bHasLastGoalLocation &&
		AvoidanceState.LastMoveEnd == EVehicleMoveEnd::Arrived &&
		FVector::DistSquared2D(AvoidanceState.LastGoalLocation, OutLocation) <
		FMath::Square(TrackedParkedFacing::GoalStillHeldDistance);
	if (bStandsOnItsGoal)
	{
		OutLocation = FVector(AvoidanceState.LastGoalLocation.X, AvoidanceState.LastGoalLocation.Y, OutLocation.Z);
	}
}

void ATrackedTankMaster::RememberParkedFacing(const float FacingYaw)
{
	M_ParkedFacingYaw = FacingYaw;
	bM_HasParkedFacing = true;
	UWorld* World = GetWorld();
	if (not IsValid(World) || World->GetTimerManager().IsTimerActive(M_ParkedFacingTimerHandle))
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		M_ParkedFacingTimerHandle,
		this,
		&ATrackedTankMaster::RestoreParkedFacingWhenTurnedAway,
		TrackedParkedFacing::CheckIntervalSeconds,
		true);
}

void ATrackedTankMaster::ForgetParkedFacing()
{
	bM_HasParkedFacing = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ParkedFacingTimerHandle);
	}
}

void ATrackedTankMaster::RestoreParkedFacingWhenTurnedAway()
{
	if (not bM_HasParkedFacing || not IsValid(ChassisMesh) || not GetIsUnitIdle())
	{
		return;
	}
	// Whoever shoved it may still be leaning on it; going back waits until the hull is left alone.
	const float YawRateDegrees = FVector::DotProduct(
		ChassisMesh->GetPhysicsAngularVelocityInDegrees(),
		ChassisMesh->GetUpVector());
	const bool bIsAtRest = FMath::Abs(YawRateDegrees) <= TrackedParkedFacing::RestingYawRateDegrees &&
		GetVelocity().SizeSquared2D() <= FMath::Square(TrackedParkedFacing::RestingSpeed);
	if (not bIsAtRest || TryReturnToParkedGoal())
	{
		return;
	}
	const float TurnedAwayDegrees = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, M_ParkedFacingYaw);
	if (FMath::Abs(TurnedAwayDegrees) <= TrackedParkedFacing::ToleranceDegrees)
	{
		return;
	}
	RotateTowards(FRotator(0.0f, M_ParkedFacingYaw, 0.0f), true);
}

bool ATrackedTankMaster::TryReturnToParkedGoal()
{
	if (not GetIsValidAITankController())
	{
		return false;
	}
	const UTrackPathFollowingComponent* PathFollowing =
		Cast<UTrackPathFollowingComponent>(AITankController->GetPathFollowingComponent());
	if (not IsValid(PathFollowing))
	{
		return false;
	}

	FVector ParkedLocation;
	FRotator ParkedRotation;
	GetParkedPose(ParkedLocation, ParkedRotation);
	const float AllowedDistance = PathFollowing->GetGoalAcceptanceRadius() + TrackedParkedFacing::ShovedOffSlotMargin;
	if (FVector::DistSquared2D(ParkedLocation, GetActorLocation()) <= FMath::Square(AllowedDistance))
	{
		return false;
	}
	if (FVector::DistSquared2D(ParkedLocation, M_ParkedReturnGoal) > 1.0f)
	{
		M_ParkedReturnGoal = ParkedLocation;
		M_ParkedReturnAttempts = 0;
	}
	if (M_ParkedReturnAttempts >= TrackedParkedFacing::MaximumReturnsToOneSpot)
	{
		return false;
	}
	++M_ParkedReturnAttempts;
	return MoveToLocation(ParkedLocation, true, ParkedRotation, true) == ECommandQueueError::NoError;
}

float ATrackedTankMaster::GetThrottleToFreeHeldUpTurn(const bool bIsTurnHeldUp, const float DeltaSeconds)
{
	using namespace TrackedRotateTowards;
	M_SecondsUntilRotateTowardsDisplacement -= DeltaSeconds;
	if (not bIsTurnHeldUp)
	{
		M_RotateTowardsStallSeconds = 0.0f;
		return 0.0f;
	}
	M_RotateTowardsStallSeconds += DeltaSeconds;
	if (M_RotateTowardsStallSeconds < HeldUpSecondsBeforeFreeing)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	URTSVehicleAvoidanceSubsystem* AvoidanceSubsystem = IsValid(World)
		                                                   ? World->GetSubsystem<URTSVehicleAvoidanceSubsystem>()
		                                                   : nullptr;
	FRTSVehicleAvoidanceAgent NearestVehicle;
	if (not IsValid(AvoidanceSubsystem) ||
		not AvoidanceSubsystem->FindNearestVehicle(this, HeldUpNeighbourHullDistance, NearestVehicle))
	{
		return 0.0f;
	}

	APawn* OtherVehicle = NearestVehicle.Pawn.Get();
	if (M_SecondsUntilRotateTowardsDisplacement <= 0.0f && EnsureRTSOverlapEvasionComponent())
	{
		// An idle ally makes room and comes back afterwards; anything else is slid away from below.
		M_SecondsUntilRotateTowardsDisplacement = SecondsBetweenDisplacements;
		RTSOverlapEvasionComponent->TryDisplaceIdleOverlappingVehicle(OtherVehicle);
	}
	const FVector AwayFromOther = GetActorLocation() - OtherVehicle->GetActorLocation();
	const bool bIsOtherBehind = FVector::DotProduct(AwayFromOther, GetActorForwardVector()) >= 0.0f;
	return bIsOtherBehind ? FreeingThrottle : -FreeingThrottle;
}

void ATrackedTankMaster::OnRotateTowardsFinished()
{
	if (not GetIsValidTrackPhysicsMovement())
	{
		return;
	}
	TrackPhysicsMovement->OnPathFollowingFinished();
}

void ATrackedTankMaster::ExecuteDigIn()
{
	Super::ExecuteDigIn();
	if (not GetIsValidDigInComponent())
	{
		return;
	}
	M_DigInComponent->ExecuteDigInCommand();
}

void ATrackedTankMaster::TerminateDigIn()
{
	Super::TerminateDigIn();
	// Nothing to do here, the dig in command is completed so the tank is dug in now and the command queue
	// is free for attack commands.
}

void ATrackedTankMaster::ExecuteBreakCover()
{
	Super::ExecuteBreakCover();
	if (not GetIsValidDigInComponent())
	{
		return;
	}
	// Will call OnBreakCoverCompleted when the unit is movable again.
	M_DigInComponent->TerminateDigInCommand();
}

void ATrackedTankMaster::TerminateBreakCover()
{
	// cover is broken and unit is movable; no state to clean up.
}

void ATrackedTankMaster::ExecuteFireRockets()
{
	if (IsValid(M_AttachedRockets))
	{
		M_AttachedRockets->ExecuteFireAllRockets();
	}
}

void ATrackedTankMaster::TerminateFireRockets()
{
	if (IsValid(M_AttachedRockets))
	{
		M_AttachedRockets->TerminateFireAllRockets();
	}
}

void ATrackedTankMaster::ExecuteCancelFireRockets()
{
	if (IsValid(M_AttachedRockets))
	{
		M_AttachedRockets->CancelFireRockets();
	}
}

void ATrackedTankMaster::TerminateCancelFireRockets()
{
	Super::TerminateCancelFireRockets();
}

void ATrackedTankMaster::OnStartDigIn()
{
	if (not SwapAbility(EAbilityID::IdDigIn, EAbilityID::IdBreakCover))
	{
		RTSFunctionLibrary::ReportError("Failed to swap dig in ability"
			"\n At function: ATrackedTankMaster::OnStartDigIn"
			"\n For tank: " + GetName());
		return;
	}
	const bool bMoveAbilityRemoved = RemoveAbility(EAbilityID::IdMove);
	const bool bReverseMoveAbilityRemoved = RemoveAbility(EAbilityID::IdReverseMove);
	const bool bRotateTowardsAbilityRemoved = RemoveAbility(EAbilityID::IdRotateTowards);
	if (not(bMoveAbilityRemoved && bReverseMoveAbilityRemoved && bRotateTowardsAbilityRemoved))
	{
		RTSFunctionLibrary::ReportError("Failed to remove move, reverse move or rotate towards ability"
			"\n At function: ATrackedTankMaster::OnStartDigIn"
			"\n For tank: " + GetName());
		return;
	}
	SetTurretsDisabled();
}


void ATrackedTankMaster::OnDigInCompleted()
{
	// Re-enable the mounted weapons.
	SetTurretsToAutoEngage(true);

	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	if (GetIsSelected())
	{
		// If selected; Queue the voice line to play over radio.
		GetSpatialVoiceLinePlayer()->PlayVoiceLineOverRadio(ERTSVoiceLine::CommandCompleted, false, true);
	}
	else
	{
		// If not selected; play the voice line spatial. Stop any other spatial audio.
		GetSpatialVoiceLinePlayer()->ForcePlaySpatialVoiceLine(ERTSVoiceLine::CommandCompleted, GetActorLocation());
	}
}

void ATrackedTankMaster::OnBreakCoverCompleted()
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	// We want to put back the move ability at the index it used to be.
	const int32 MoveIndex = FRTS_Statics::GetIndexOfAbilityForBaseTank(RTSComponent->GetOwningPlayer(),
	                                                                   RTSComponent->GetSubtypeAsTankSubtype(),
	                                                                   EAbilityID::IdMove, this);
	AddAbility(EAbilityID::IdMove, MoveIndex);
	const int32 ReverseMoveIndex = FRTS_Statics::GetIndexOfAbilityForBaseTank(
		RTSComponent->GetOwningPlayer(), RTSComponent->GetSubtypeAsTankSubtype(), EAbilityID::IdReverseMove, this);
	AddAbility(EAbilityID::IdReverseMove, ReverseMoveIndex);
	const int32 RotateTowardsIndex = FRTS_Statics::GetIndexOfAbilityForBaseTank(
		RTSComponent->GetOwningPlayer(), RTSComponent->GetSubtypeAsTankSubtype(), EAbilityID::IdRotateTowards, this);
	AddAbility(EAbilityID::IdRotateTowards, RotateTowardsIndex);
	SwapAbility(EAbilityID::IdBreakCover, EAbilityID::IdDigIn);
}


void ATrackedTankMaster::WallGotDestroyedForceBreakCover()
{
	const ECommandQueueError Error = BreakCover(true);
	if (Error != ECommandQueueError::NoError)
	{
		const FString ErrorString = UEnum::GetValueAsString(Error);
		RTSFunctionLibrary::ReportError("Failed to break cover when wall got destroyed"
			"\n At function: ATrackedTankMaster::WallGotDestroyedForceBreakCover"
			"\n For tank: " + GetName());
	}
}


void ATrackedTankMaster::UpdateEngineSound() const
{
	if (not GetIsValidEngineSoundComponent() or not IsValid(ChassisMesh))
	{
		return;
	}

	const float Speed = ChassisMesh->GetBoneLinearVelocity(NAME_None).Length();
	M_EngineSoundComponent->SetFloatParameter(AudioSpeedParam, Speed);
}

void ATrackedTankMaster::PostInitializeComponents_SetupEngineSounds()
{
	UAudioComponent* const EngineSoundComponent = FindComponentByClass<UAudioComponent>();
	if (not IsValid(EngineSoundComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			TEXT("M_EngineSoundComponent"),
			TEXT("PostInitializeComponents_SetupEngineSounds"),
			this);
		return;
	}

	M_EngineSoundComponent = EngineSoundComponent;
	SetupEngineGasSoundComponent();

	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	M_EngineSoundDel.BindUObject(this, &ATrackedTankMaster::UpdateEngineSound);
	World->GetTimerManager().SetTimer(
		M_EngineSoundHandle,
		M_EngineSoundDel,
		DeveloperSettings::Optimization::UpdateEngineSoundsInterval,
		true);
}

void ATrackedTankMaster::SetupEngineGasSoundComponent()
{
	if (not GetIsValidEngineSoundComponent())
	{
		return;
	}

	if (M_EngineGasSoundComponent != nullptr)
	{
		return;
	}

	// Fixed: Removed RF_Transactional and the Template argument
	M_EngineGasSoundComponent = NewObject<UAudioComponent>(
		this,
		UAudioComponent::StaticClass(), // Explicit class type
		EngineGasSoundComponentName,
		RF_NoFlags);

	if (not GetIsValidEngineGasSoundComponent())
	{
		return;
	}

	M_EngineGasSoundComponent->SetSound(nullptr);
	M_EngineGasSoundComponent->SetBoolParameter(EngineGasSoundLoopingParam, false);
	M_EngineGasSoundComponent->bAutoActivate = false;
	M_EngineGasSoundComponent->bAutoDestroy = false;
	M_EngineGasSoundComponent->bCanPlayMultipleInstances = false;
	M_EngineGasSoundComponent->bAllowSpatialization = true;
	M_EngineGasSoundComponent->SetUISound(false);
	M_EngineGasSoundComponent->AttenuationSettings = M_EngineSoundComponent->AttenuationSettings;
	M_EngineGasSoundComponent->bOverrideAttenuation = M_EngineSoundComponent->bOverrideAttenuation;
	M_EngineGasSoundComponent->AttenuationOverrides = M_EngineSoundComponent->AttenuationOverrides;
	M_EngineGasSoundComponent->SetRelativeTransform(M_EngineSoundComponent->GetRelativeTransform());

	if (USceneComponent* const EngineSoundParent = M_EngineSoundComponent->GetAttachParent())
	{
		M_EngineGasSoundComponent->AttachToComponent(
			EngineSoundParent,
			FAttachmentTransformRules::KeepRelativeTransform,
			M_EngineSoundComponent->GetAttachSocketName());
	}
	else if (USceneComponent* const RootSceneComponent = GetRootComponent())
	{
		M_EngineGasSoundComponent->AttachToComponent(
			RootSceneComponent,
			FAttachmentTransformRules::KeepRelativeTransform);
	}

	// Fixed: Removed AddInstanceComponent
	M_EngineGasSoundComponent->RegisterComponent();
}

void ATrackedTankMaster::PlayEngineGasSoundEffect() const
{
	if (not GetIsValidEngineGasSound() or not GetIsValidEngineGasSoundComponent())
	{
		return;
	}

	M_EngineGasSoundComponent->Stop();
	M_EngineGasSoundComponent->SetSound(M_EngineGasSound);
	M_EngineGasSoundComponent->SetBoolParameter(EngineGasSoundLoopingParam, false);
	M_EngineGasSoundComponent->Play();
}

void ATrackedTankMaster::OnLevelUp_UpdateHealthbarRankIcon(const int32 Level, const EVeterancyIconSet IconSetUsed) const
{
	if (not GetIsValidHealthComponent())
	{
		return;
	}
	GetHealthComponent()->UpdateRankIcon(Level, IconSetUsed);
}

bool ATrackedTankMaster::EnsureValidExperienceComponent()
{
	if (IsValid(ExperienceComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Experience component is not valid"
		"\n At function: ATrackedTankMaster::EnsureValidExperienceComponent"
		"\n For tank: " + GetName());
	ExperienceComponent = NewObject<URTSExperienceComp>(this, "ExperienceComponent");
	return IsValid(ExperienceComponent);
}

bool ATrackedTankMaster::GetIsValidDigInComponent() const
{
	if (IsValid(M_DigInComponent))
	{
		return true;
	}

	// IMPORTANT: no error report here as some tanks do not use this component!!
	return false;
}

void ATrackedTankMaster::OnInit_FindEnergyComponent(const int32 MyEnergy)
{
	UTankEnergyComponent* FoundEnergyComp = FindComponentByClass<UTankEnergyComponent>();
	if (not IsValid(FoundEnergyComp))
	{
		RTSFunctionLibrary::ReportError(
			"The tank could not find a UTankEnergyComponent on it eventhough it has non-zero energy!"
			"tank: " + GetName()
			+ "\n Energy: " + FString::FromInt(MyEnergy));
		return;
	}
	M_TankEnergyComponent = FoundEnergyComp;
	M_TankEnergyComponent->InitEnergyComponent(MyEnergy);
	M_TankEnergyComponent->SetEnabled(true);
}

bool ATrackedTankMaster::GetIsValidTrackPhysicsMovement() const
{
	if (IsValid(TrackPhysicsMovement))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"TrackPhysicsMovement",
		"GetIsValidTrackPhysicsMovement",
		this);
	return false;
}

bool ATrackedTankMaster::GetIsValidAITankController()
{
	if (GetIsValidAIController())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"AITankController",
		"GetIsValidAITankController",
		this);
	return false;
}

bool ATrackedTankMaster::GetIsValidTankAnimationBP() const
{
	if (IsValid(TankAnimationBP))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"TankAnimationBP",
		"GetIsValidTankAnimationBP",
		this);
	return false;
}

bool ATrackedTankMaster::GetIsValidEngineSoundComponent() const
{
	if (IsValid(M_EngineSoundComponent))
	{
		return true;
	}
	// No error spam.
	return false;
}

bool ATrackedTankMaster::GetIsValidEngineGasSoundComponent() const
{
	if (IsValid(M_EngineGasSoundComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_EngineGasSoundComponent"),
		TEXT("GetIsValidEngineGasSoundComponent"),
		this);
	return false;
}

bool ATrackedTankMaster::GetIsValidEngineGasSound() const
{
	if (IsValid(M_EngineGasSound))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_EngineGasSound"),
		TEXT("GetIsValidEngineGasSound"),
		this);
	return false;
}

void ATrackedTankMaster::HandleVoiceLineOnLevelUp() const
{
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	if (GetIsSelected())
	{
		GetSpatialVoiceLinePlayer()->PlayVoiceLineOverRadio(ERTSVoiceLine::UnitPromoted, true, false);
		return;
	}

	GetSpatialVoiceLinePlayer()->PlaySpatialVoiceLine(ERTSVoiceLine::UnitPromoted, GetActorLocation(), true);
}

void ATrackedTankMaster::BeginPlay_SetupExperienceComponent()
{
	if (not EnsureValidExperienceComponent() || not GetIsValidRTSComponent())
	{
		return;
	}
	FTrainingOption UnitTypeAndSubType;
	UnitTypeAndSubType.UnitType = RTSComponent->GetUnitType();
	UnitTypeAndSubType.SubtypeValue = RTSComponent->GetUnitSubType();

	ExperienceComponent->InitExperienceComponent(
		this,
		UnitTypeAndSubType,
		RTSComponent->GetOwningPlayer());
}

FVector ATrackedTankMaster::MoveClearOfVehiclesWhenSpawned(const FVector& MoveTo)
{
	const UWorld* World = GetWorld();
	const URTSVehicleAvoidanceSubsystem* AvoidanceSubsystem = IsValid(World)
		                                                         ? World->GetSubsystem<URTSVehicleAvoidanceSubsystem>()
		                                                         : nullptr;
	const FRTSVehicleFootprint Footprint = RTSVehicleFootprint::BuildFromChassisMesh(ChassisMesh);
	if (not IsValid(AvoidanceSubsystem) || not Footprint.GetIsValid())
	{
		return MoveTo;
	}

	const FVector SpawnLocation = GetActorLocation();
	// Corner circle against corner circle: clear however the two hulls are turned.
	const float NeededRadius = Footprint.GetCornerRadius();
	FVector FreeLocation;
	if (AvoidanceSubsystem->GetIsLocationFreeOfVehicles(SpawnLocation, NeededRadius, this) ||
		not AvoidanceSubsystem->FindFreeLocationNear(SpawnLocation, NeededRadius, this, FreeLocation))
	{
		return MoveTo;
	}
	SetActorLocation(FreeLocation, false, nullptr, ETeleportType::ResetPhysics);
	// Sent to the spot it appeared on means "stay here"; driving back would run into the vehicle that holds it.
	const bool bWasMeantToStay = FVector::DistSquared2D(MoveTo, SpawnLocation) < FMath::Square(NeededRadius);
	return bWasMeantToStay ? FreeLocation : MoveTo;
}

void ATrackedTankMaster::OnRTSUnitSpawned_SetEnabled(const float TimeNotSelectable, const FVector InMoveTo)
{
	const FVector MoveTo = MoveClearOfVehiclesWhenSpawned(InMoveTo);
	this->SetActorHiddenInGame(false);
	this->SetActorEnableCollision(true);
	ChassisMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChassisMesh->SetSimulatePhysics(true);
	ChassisMesh->SetVisibility(true, false);
	// Calculate world rotation to the MoveTo location.
	FRotator FinalRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), MoveTo);
	MoveToLocation(MoveTo, true, FinalRotation);
	// Set after the move to command; this notifies that the unit has not received any commands from the player yet.
	SetIsSpawning(true);

	for (auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			EachTurret->SetAutoEngageTargets(false);
		}
	}
	BP_OnRTSUnitSpawned(false);
	if (TimeNotSelectable <= 0.f)
	{
		if (IsValid(SelectionComponent))
		{
			SelectionComponent->SetCanBeSelected(true);
		}
		return;
	}

	const TWeakObjectPtr<ATrackedTankMaster> WeakTrackedTankMaster = this;
	auto SetSelectableAfterDelay = [WeakTrackedTankMaster]()
	{
		if (not WeakTrackedTankMaster.IsValid())
		{
			return;
		}

		ATrackedTankMaster* StrongTrackedTankMaster = WeakTrackedTankMaster.Get();
		if (IsValid(StrongTrackedTankMaster->SelectionComponent))
		{
			StrongTrackedTankMaster->SelectionComponent->SetCanBeSelected(true);
		}
	};
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(M_OnSpawnedSelectionTimerHandle, SetSelectableAfterDelay,
		                                  TimeNotSelectable, false);
	}
}

void ATrackedTankMaster::OnRTSUnitSpawned_SetDisabled()
{
	this->SetActorHiddenInGame(true);
	this->SetActorEnableCollision(false);
	ChassisMesh->SetSimulatePhysics(false);
	if (IsValid(SelectionComponent))
	{
		SelectionComponent->SetCanBeSelected(false);
	}
	if (IsValid(RTSNavCollision))
	{
		RTSNavCollision->EnableAffectNavmesh(false);
	}
	for (auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			EachTurret->DisableTurret();
		}
	}
	BP_OnRTSUnitSpawned(true);
}

void ATrackedTankMaster::CheckFootPrintForOverlaps() const
{
	if (EnsureRTSOverlapEvasionComponent())
	{
		RTSOverlapEvasionComponent->CheckFootprintForOverlaps();
	}
}

/*
* STEER WITH ANGULAR VELOCITY
* 	float OutLeftTrack = 0.f;
	float OutRightTrack = 0.f;

	UVehicleAIFunctionLibrary::ConvertToTankSteering(m_Steering, m_Throttle, m_TurnInPlaceThreshold, m_NormalTurnMultiplier, OutLeftTrack, OutRightTrack);

	float DesiredYawRate = (OutRightTrack - OutLeftTrack) * m_ThrottleMultiplier;

	// Clamp the DesiredYawRate within a certain range
	float MaxYawRate = 10.f; // Adjust this value to control the maximum angular momentum
	DesiredYawRate = FMath::Clamp(DesiredYawRate, -MaxYawRate, MaxYawRate);

	FVector ForwardVector = m_TankMesh->GetForwardVector();
	FVector TankVelocity = (OutLeftTrack + OutRightTrack) * 0.5f * m_ThrottleMultiplier * ForwardVector;

	if (m_Break > 0)
	{
		TankVelocity *= (1 - m_Break);
	}

	m_TankMesh->SetPhysicsLinearVelocity(TankVelocity, true);

	// Get the current angular velocity
	FVector CurrentAngularVelocity = m_TankMesh->GetPhysicsAngularVelocityInDegrees();
	FVector TargetAngularVelocity = FVector(0, 0, DesiredYawRate);

	// Smoothly dampen the angular momentum as the m_Steering value approaches zero
	float DampingFactor = 5.f; // Adjust this value to control the rate at which the angular momentum dampens
	FVector NewAngularVelocity = FMath::Lerp(CurrentAngularVelocity, TargetAngularVelocity, DeltaSeconds * DampingFactor);

	m_TankMesh->SetPhysicsAngularVelocityInDegrees(NewAngularVelocity, false);
 */

// struct item
// {
// 	const int index;
// 	const float weight;
// };
// static bool CheckConstrains(TPair<float, TArray<item>> CurrentItemSet, const float MaxWeights, const item NewItem)
// {
// 	for(auto elm : CurrentItemSet.Value)
// 	{
// 		if(FMath::Abs(elm.index - NewItem.index) <= 1)
// 		{
// 			return false;
// 		}
// 	}
// 	if(CurrentItemSet.Key + NewItem.weight > MaxWeights)
// 	{
// 		return false;
// 	}
// 	return true;
// }
//
// static TPair<float, TArray<item>> Exhaustive(TArray<item> SuperSetItems, const float MaxWeights)
// {
// 	// valid sets of pairs.
// 	TArray<TPair<float, TArray<item>>> validSets;
// 	// subsets possible of superset.
// 	TArray<TArray<item>> subsets;
// 	
// 	// Generate all possible subsets we want to exhaust.
// 	int numElements = SuperSetItems.Num();
// 	for (int i = 0; i < (1 << numElements); i++)
// 	{
// 		TArray<item> subset;
// 		for (int j = 0; j < numElements; j++)
// 		{
// 			if ((i & (1 << j)) != 0)
// 			{
// 				subset.Add(SuperSetItems[j]);
// 			}
// 		}
// 		subsets.Add(subset);
// 		
// 	}
// }
