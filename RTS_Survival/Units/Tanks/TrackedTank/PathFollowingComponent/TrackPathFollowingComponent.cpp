// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TrackPathFollowingComponent.h"

#include "AIConfig.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Navigation/RTSNavAgentRegistery/RTSNavAgentRegistery.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/AI/AITrackTank.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/Utils/VehicleAIFunctionLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/Navigator/RTSNavigator.h"

namespace TrackFollowingEvasion
{
	// Angle used on both sides of the movement direction to get the cone in which to check for other moving
	// overlapping allied actors.
	constexpr float MovementDirectionConeHalfAngle = 40.f;
	constexpr bool bDebugTrackEvasion = false;
	constexpr float DebugConeLength = 400.f;

	//  Minimal speed to consider a valid direction (cm/s). Below this we fall back to forward.
	constexpr float MinSpeedForDirection = 10.f;

	// Distance under which we ignore AoA checks to avoid jitter (cm).
	constexpr float MinSeparationToConsider = 30.f;

	//  If the other actor's "angle-of-attack" against our movement exceeds this, we treat as critical.
	constexpr float CriticalAngleOfAttackDeg = 10.f;
}

namespace TrackFollowingOverlapCleanup
{
	constexpr int32 CleanupTickInterval = 256;
}

int32 UTrackPathFollowingComponent::M_TicksCountCheckOverlappers = TrackFollowingOverlapCleanup::CleanupTickInterval;
float UTrackPathFollowingComponent::M_TimeTillDiscardOverlap = 3.f;


/**
 * If Resolve Collisions is set in the project settings on the crowd controller then
 * ApplyCrowdAgentPosition will be called to resolve issues, however this causes glitches with physics based vehicles.
 * It calls UpdateStepMove on DetourCrowd in UCrowdManager::Tick which at the end of the function executes
 * dtVadd(ag->npos, ag->npos, ag->disp); which adds the displacement to the position of the agent.
 *
 * 
 */
// Sets default values for this component's properties
UTrackPathFollowingComponent::UTrackPathFollowingComponent()
	: UVehiclePathFollowingComponent(),
	  MaxAngleDontSlow(0),
	  M_TimeWantDesiredSpeed(0),
	  M_CurrentSpeed(0),
	  M_ThrottleIncreaseForDesiredSpeed(0)
{
	// SimulationState = ECrowdSimulationState::Disabled;
	// bAffectFallingVelocity = false;
	// bRotateToVelocity = true;
	// bSuspendCrowdSimulation = false;
	// bEnableSimulationReplanOnResume = true;
	// bRegisteredWithCrowdSimulation = false;
	// bCanCheckMovingTooFar = true;
	// bCanUpdatePathPartInTick = true;
	//
	// // Looks at the coming two corner points and smooths the steering between them.
	// bEnableAnticipateTurns = true;
	// bEnableObstacleAvoidance = true;
	// bEnableSeparation = true;
	// bEnableOptimizeVisibility = false;
	// // The topology optimization process in dtCrowd::updateTopologyOptimization primarily focuses
	// // on the navigation mesh and does not directly consider other agents as obstacles during the optimization of the path topology. bEnableOptimizeTopology = false;
	// bEnablePathOffset = false;
	//
	PrimaryComponentTick.bCanEverTick = true;
	// /* Leave on false will do the following if true:
	// * Defines the radius within which to start slowing down:
	// * const dtReal slowDownRadius = ag->params.radius * 2;
	// * Calculates the speed scale based on distance to the goal:
	//   speedScale = getDistanceToGoal(ag, slowDownRadius) / slowDownRadius; 
	//  */
	// bEnableSlowdownAtGoal = false;
	//
	// SeparationWeight = 2.0f;
	// CollisionQueryRange = 400.0f; // approx: radius * 12.0f
	// PathOptimizationRange = 1000.0f; // approx: radius * 30.0f
	// AvoidanceQuality = ECrowdAvoidanceQuality::Low;
	// AvoidanceRangeMultiplier = 1.0f;
}


#if WITH_EDITOR
void UTrackPathFollowingComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// To set desired speed.
	Super::PostEditChangeProperty(PropertyChangedEvent);
	DesiredReverseSpeed = UVehicleAIFunctionLibrary::ConvertVelocityByUnit(
		DesiredReverseSpeedKmph, ESpeedUnits::KilometersPerHour, ESpeedUnits::CentimetersPerSecond);
	M_MinimalSlowdownSpeed = FMath::Max(DesiredSpeed / 5, 300.f);
	SetDesiredSpeed(StartingDesiredSpeed, StartSpeedUnit);
}

#endif

float UTrackPathFollowingComponent::GetCurrentWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (World)
	{
		return World->GetTimeSeconds();
	}

	return 0.f;
}

bool UTrackPathFollowingComponent::ContainsOverlapActor(
	const TArray<FOverlapActorData>& TargetArray,
	const AActor* InActor) const
{
	if (not IsValid(InActor))
	{
		return false;
	}

	for (const FOverlapActorData& OverlapData : TargetArray)
	{
		if (OverlapData.Actor.Get() == InActor)
		{
			return true;
		}
	}

	return false;
}

void UTrackPathFollowingComponent::AddOverlapActorData(
	TArray<FOverlapActorData>& TargetArray,
	AActor* InActor)
{
	if (not IsValid(InActor))
	{
		return;
	}

	const float CurrentTimeSeconds = GetCurrentWorldTimeSeconds();
	for (FOverlapActorData& OverlapData : TargetArray)
	{
		if (OverlapData.Actor.Get() == InActor)
		{
			OverlapData.TimeRegisteredSeconds = CurrentTimeSeconds;
			return;
		}
	}

	FOverlapActorData NewOverlapData;
	NewOverlapData.Actor = InActor;
	NewOverlapData.TimeRegisteredSeconds = CurrentTimeSeconds;
	TargetArray.Add(MoveTemp(NewOverlapData));
}

void UTrackPathFollowingComponent::RemoveOverlapActorFromArray(
	TArray<FOverlapActorData>& TargetArray,
	AActor* InActor)
{
	if (not IsValid(InActor))
	{
		return;
	}

	for (int32 Index = TargetArray.Num() - 1; Index >= 0; --Index)
	{
		if (TargetArray[Index].Actor.Get() == InActor)
		{
			TargetArray.RemoveAt(Index);
		}
	}
}

void UTrackPathFollowingComponent::RemoveInvalidOverlaps(TArray<FOverlapActorData>& TargetArray) const
{
	for (int32 Index = TargetArray.Num() - 1; Index >= 0; --Index)
	{
		if (not TargetArray[Index].Actor.IsValid())
		{
			TargetArray.RemoveAt(Index);
		}
	}
}

void UTrackPathFollowingComponent::DebugRemovedOverlapActor(
	const FString& Reason,
	const FOverlapActorData& OverlapData) const
{
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		const FString ActorName = OverlapData.Actor.IsValid() ? OverlapData.Actor->GetName() : TEXT("Invalid");
		const FString DebugMessage = FString::Printf(TEXT("%s: %s"), *Reason, *ActorName);
		RTSFunctionLibrary::PrintString(DebugMessage, FColor::Yellow);
	}
}

void UTrackPathFollowingComponent::RemoveExpiredOverlaps(
	const float CurrentTimeSeconds,
	TArray<FOverlapActorData>& TargetArray)
{
	for (int32 Index = TargetArray.Num() - 1; Index >= 0; --Index)
	{
		const FOverlapActorData& OverlapData = TargetArray[Index];

		if (not OverlapData.Actor.IsValid())
		{
			TargetArray.RemoveAt(Index);
			continue;
		}

		const float Elapsed = CurrentTimeSeconds - OverlapData.TimeRegisteredSeconds;
		if (Elapsed < M_TimeTillDiscardOverlap)
		{
			continue;
		}

		DebugRemovedOverlapActor(TEXT("Removing stale overlap"), OverlapData);
		TargetArray.RemoveAt(Index);
	}
}

void UTrackPathFollowingComponent::ResetOverlapBlockingActors()
{
	M_IdleAlliedBlockingActors.Reset();
	M_MovingBlockingOverlapActors.Reset();
}

void UTrackPathFollowingComponent::ResetOverlapBlockingActorsForCommand()
{
	ResetOverlapBlockingActors();
	M_TicksCountCheckOverlappers = TrackFollowingOverlapCleanup::CleanupTickInterval;
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(TEXT("Cleared overlap blockers for new move command."), FColor::Emerald);
	}
}

void UTrackPathFollowingComponent::RegisterIdleBlockingActor(AActor* InActor)
{
	if (not IsValid(InActor))
	{
		return;
	}
	AddOverlapActorData(M_IdleAlliedBlockingActors, InActor);
	RemoveOverlapActorFromArray(M_MovingBlockingOverlapActors, InActor);
}

void UTrackPathFollowingComponent::RegisterMovingOverlapBlockingActor(AActor* InActor)
{
	if (not IsValid(InActor))
	{
		return;
	}
	if (ContainsOverlapActor(M_IdleAlliedBlockingActors, InActor))
	{
		return;
	}
	AddOverlapActorData(M_MovingBlockingOverlapActors, InActor);
}

void UTrackPathFollowingComponent::DeregisterOverlapBlockingActor(AActor* InActor)
{
	// This actor is no longer overlapping with us and can be ignored.
	if (not IsValid(InActor))
	{
		return;
	}
	RemoveOverlapActorFromArray(M_IdleAlliedBlockingActors, InActor);
	RemoveOverlapActorFromArray(M_MovingBlockingOverlapActors, InActor);
}


bool UTrackPathFollowingComponent::HasBlockingOverlaps() const
{
	for (const FOverlapActorData& OverlapData : M_IdleAlliedBlockingActors)
	{
		if (OverlapData.Actor.IsValid())
		{
			return true;
		}
	}
	for (const FOverlapActorData& OverlapData : M_MovingBlockingOverlapActors)
	{
		if (OverlapData.Actor.IsValid())
		{
			return true;
		}
	}
	return false;
}


void UTrackPathFollowingComponent::SetReverse(bool Reverse)
{
	bWantsReverse = Reverse;
}

bool UTrackPathFollowingComponent::IsReversing()
{
	return bReversing;
}


APawn* UTrackPathFollowingComponent::GetValidControlledPawn()
{
	if (IsValid(ControlledPawn))
	{
		return ControlledPawn;
	}
	// Use the NavMovementInterface to access the owner
	if (NavMovementInterface.IsValid())
	{
		// Retrieve the object implementing the interface
		UObject* OwnerAsObject = NavMovementInterface->GetOwnerAsObject();
		if (OwnerAsObject)
		{
			// Cast the object to a Pawn and return if valid
			if (APawn* PawnOwner = Cast<APawn>(OwnerAsObject))
			{
				ControlledPawn = PawnOwner;
				return PawnOwner;
			}
		}
	}
	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "None";
	const FString OwnerClass = GetOwner() ? GetOwner()->GetClass()->GetName() : "None";
	RTSFunctionLibrary::ReportError("The UVehiclePathFollowingComponent is not able to find the controlled pawn!"
		"\n Component name: " + GetName() + "Owner: " + OwnerName + "Owner Class: " + OwnerClass);
	return nullptr;
}


void UTrackPathFollowingComponent::SetDesiredReverseSpeed(const float NewSpeed, const ESpeedUnits SpeedUnit)
{
	DesiredReverseSpeed = UVehicleAIFunctionLibrary::ConvertVelocityByUnit(
		NewSpeed, SpeedUnit, ESpeedUnits::CentimetersPerSecond);
}

void UTrackPathFollowingComponent::SetDesiredSpeed(float NewSpeed, ESpeedUnits Unit)
{
	DesiredSpeed = UVehicleAIFunctionLibrary::ConvertVelocityByUnit(NewSpeed, Unit, ESpeedUnits::CentimetersPerSecond);
}

void UTrackPathFollowingComponent::BP_SetNewVehicleSpeed(float NewSpeed, ESpeedUnits Unit)
{
	DesiredSpeed = UVehicleAIFunctionLibrary::ConvertVelocityByUnit(NewSpeed, Unit, ESpeedUnits::CentimetersPerSecond);
}


bool UTrackPathFollowingComponent::IsStuck(const float DeltaTime)
{
	// Minimal Movement Detection
	if (bUseStuckDetection && StuckLocationSamples.Num() == StuckDetectionSampleCount && StuckDetectionSampleCount > 0)
	{
		bool bHasMoved = false;

		for (const FVector& StuckSample : StuckLocationSamples)
		{
			const float TestDistanceSq = FVector::DistSquared(StuckSample, GetAgentLocation());
			if (TestDistanceSq > FMath::Square(StuckDetectionDistance))
			{
				bHasMoved = true;
				break;
			}
		}

		if (!bHasMoved)
		{
			// Vehicle hasn't moved significantly over the sampled locations
			bIsStuck = true;
			if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("Vehicle stuck with samples!"
				                                "\n at time: " + FString::SanitizeFloat(GetWorld()->GetTimeSeconds()),
				                                FColor::Purple);
			}
			return true;
		}
	}

	return false;
}

void UTrackPathFollowingComponent::ClearOverlapsForNewMovementCommand()
{
	ResetOverlapBlockingActorsForCommand();
}


// Called when the game starts
void UTrackPathFollowingComponent::BeginPlay()
{
	Super::BeginPlay();
	M_MinimalSlowdownSpeed = FMath::Max(DesiredSpeed / 5, 300.f);
	MinAgentRadiusPct = DestinationVehicleRadiusMultiplier;
}

void UTrackPathFollowingComponent::BeginDestroy()
{
	Super::BeginDestroy();
}

void UTrackPathFollowingComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	M_TicksCountCheckOverlappers -= 1;
	if (M_TicksCountCheckOverlappers > 0)
	{
		return;
	}

	M_TicksCountCheckOverlappers = TrackFollowingOverlapCleanup::CleanupTickInterval;

	const float CurrentTimeSeconds = GetCurrentWorldTimeSeconds();
	RemoveExpiredOverlaps(CurrentTimeSeconds, M_IdleAlliedBlockingActors);
	RemoveExpiredOverlaps(CurrentTimeSeconds, M_MovingBlockingOverlapActors);
}


void UTrackPathFollowingComponent::OnPathUpdated()
{
	Super::OnPathUpdated();

	// Reset stuck status
	ResetStuck();

	// Debug message
	if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Path Updated: bIsStuck and timers reset!");
	}
}

bool UTrackPathFollowingComponent::CheckOverlapIdleAllies(const float DeltaTime)
{
	RemoveInvalidOverlaps(M_IdleAlliedBlockingActors);

	if (M_IdleAlliedBlockingActors.Num() == 0 || not IsValid(ControlledPawn))
	{
		return false;
	}
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		DebugIdleOverlappingActors();
	}

	// Keep steering as before, set throttle to zero while we’re overlapping.
	const float CurrentSpeed = ControlledPawn->GetVelocity().Size2D();
	UpdateVehicle(/*Throttle*/0.f, /*CurrentSpeed*/CurrentSpeed, DeltaTime, /*Brake*/0.f, /*Steering*/
	                          M_LastSteeringInput);
	return true;
}

bool UTrackPathFollowingComponent::CheckOverlapMovingAllies(float DeltaTime)
{
	RemoveInvalidOverlaps(M_MovingBlockingOverlapActors);

	if (M_MovingBlockingOverlapActors.Num() == 0 || not IsValid(ControlledPawn))
	{
		return false;
	}
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		DebugMovingOverlappingActors(DeltaTime);
	}


	// --- Compute our movement direction (world space, 2D) -----------------------------
	const FVector SelfLoc = IsValid(ControlledPawn) ? ControlledPawn->GetActorLocation() : FVector::ZeroVector;
	const FVector Vel = IsValid(ControlledPawn) ? ControlledPawn->GetVelocity() : FVector::ZeroVector;

	FVector MovementDirection = Vel.GetSafeNormal2D();
	if (MovementDirection.IsNearlyZero())
	{
		// If velocity is too small, use facing (forward) so the cone is still meaningful.
		const FRotator SelfRot = IsValid(ControlledPawn) ? ControlledPawn->GetActorRotation() : FRotator::ZeroRotator;
		MovementDirection = SelfRot.Vector().GetSafeNormal2D();
	}
	// Guard against pathological cases.
	if (MovementDirection.IsNearlyZero())
	{
		return false;
	}

	if (TrackFollowingEvasion::bDebugTrackEvasion)
	{
		const FVector Tip = SelfLoc + MovementDirection * TrackFollowingEvasion::DebugConeLength;
		DrawDebugDirectionalArrow(GetWorld(), SelfLoc, Tip, /*ArrowSize*/20.f, FColor::Cyan, /*bPersistentLines*/false,
		                          /*LifeTime*/0.f, /*Depth*/0, /*Thickness*/2.f);

		// Cone sides: rotate around +Z by ±half-angle.
		const float HalfAngleRad = FMath::DegreesToRadians(TrackFollowingEvasion::MovementDirectionConeHalfAngle);
		const FVector LeftDir = FQuat(FVector::UpVector, +HalfAngleRad).RotateVector(MovementDirection);
		const FVector RightDir = FQuat(FVector::UpVector, -HalfAngleRad).RotateVector(MovementDirection);

		DrawDebugLine(GetWorld(), SelfLoc, SelfLoc + LeftDir * TrackFollowingEvasion::DebugConeLength, FColor::Cyan,
		              false, 0.f, 0, 1.5f);
		DrawDebugLine(GetWorld(), SelfLoc, SelfLoc + RightDir * TrackFollowingEvasion::DebugConeLength, FColor::Cyan,
		              false, 0.f, 0, 1.5f);
	}

	EEvasionAction FinalAction = EEvasionAction::MoveNormally;
	for (const FOverlapActorData& OverlapData : M_MovingBlockingOverlapActors)
	{
		AActor* Other = OverlapData.Actor.Get();
		if (not IsValid(Other))
		{
			continue;
		}
		const EEvasionAction Action = DetermineOverlapWithinMovementDirection(MovementDirection, Other, DeltaTime);
		if (Action == EEvasionAction::Wait)
		{
			FinalAction = EEvasionAction::Wait;
			break;
		}
		if (Action == EEvasionAction::EvadeRight)
		{
			FinalAction = EEvasionAction::EvadeRight;
		}
	}

	if (TrackFollowingEvasion::bDebugTrackEvasion)
	{
		// Debug draw our action 400 units above the vehicle.
		FColor ActionColor = FColor::Green;
		if (FinalAction == EEvasionAction::Wait)
		{
			ActionColor = FColor::Red;
		}
		else if (FinalAction == EEvasionAction::EvadeRight)
		{
			ActionColor = FColor::Yellow;
		}
		FString ActionText = UEnum::GetValueAsString(FinalAction);
		RTSFunctionLibrary::DrawDebugAtLocation(this, GetAgentLocation() + FVector(0, 0, 400.f),
		                                        ActionText, ActionColor, DeltaTime);
	}
	return ExecuteEvasiveAction(FinalAction, DeltaTime);
}

void UTrackPathFollowingComponent::UpdateDriving(FVector Destination, float DeltaTime)
{
	// .65 ms for 100 vehicles.
	// TRACE_CPUPROFILER_EVENT_SCOPE(Tracks_UpdateDriving);
	if (!M_TrackPhysicsMovement)
	{
		return;
	}
	VehicleCurrentDestination = Destination;

	// Make sure we have an associated move compnent and the interface is implemented
	if (not bImplementsInterface || not IsValid(ControlledPawn))
	{
		return;
	}

	// Pause throttle while overlapping idle allies being pushed aside; keep last steering.
	if (CheckOverlapIdleAllies(DeltaTime))
	{
		return;
	}
	if (CheckOverlapMovingAllies(DeltaTime))
	{
		return;
	}
	// [-180 , +180]
	const float SignedTargetAngle = CalculateDestinationAngle(Destination);
	AbsoluteTargetAngle = FMath::Abs(SignedTargetAngle);

	M_CurrentSpeed = ControlledPawn->GetVelocity().Size2D();

	// Use target location instead of move focus for calculating distance so it works with the crowd simulation
	// Crowd simulation uses direction vector offset by about 500 units, so the destination would always be constant if using move focus
	// However if we're stuck, use the normal destination instead, as this will be used as a reverse target
	DestinationDistance = CalculateDestinationDistance(bIsStuck ? Destination : GetCurrentTargetLocation());

	FinalDestinationDistance = GetPathLengthRemaining();

	// Adjust the desired speed depending on distance to final dest.
	// and uses the AbsoluteTargetAngle to see if the MaxAngleDontSlow is exceeded.
	AdjustedDesiredSpeed = TrackGetSlowdownSpeed(DestinationDistance, Destination);

	// .58 ms for 100 vehicles.
	// TRACE_CPUPROFILER_EVENT_SCOPE(Tracks_CoreCalculation);

	float SpeedDifference = 0;
	float Steering = 0;
	bool bIsInReverseDeadzone = false;
	// Needs TargetAngle to be in absolute value.
	float ThrottleIncreaseValue;
	bReversing = ShouldTrackVehicleReverse(AbsoluteTargetAngle, DestinationDistance);
	if (bReversing)
	{
		bIsInReverseDeadzone = AbsoluteTargetAngle <= (180 - DeadZoneAngle);
		const float AbsoluteTurnAngle = AbsoluteTargetAngle > 90 ? 180 - AbsoluteTargetAngle : AbsoluteTargetAngle;
		// Steer maximally if the angle is greater than the threshold, otherwise steer based on the angle.
		Steering = AbsoluteTurnAngle > MaxAngleDontSlow ? 1 : AbsoluteTurnAngle / MaxAngleDontSlow;
		Steering *= -FMath::Sign(SignedTargetAngle);
		// Adjust speed difference with wanted reverse speed.
		SpeedDifference = FMath::GetMappedRangeValueClamped(
			FVector2D(-ReverseSpeedPIDThreshold, ReverseSpeedPIDThreshold), FVector2D(-1.f, 1.f),
			DesiredReverseSpeed - M_CurrentSpeed);
		ThrottleIncreaseValue = M_ReverseThrottleIncreaseForDesiredSpeed;
		// At the timer check current speed vs max reverse speed.
		if (M_CurrentSpeed < DesiredReverseSpeed)
		{
			M_TimeWantDesiredSpeed += DeltaTime;
		}
		else
		{
			M_TimeWantDesiredSpeed = 0;
		}
	}
	else
	{
		// Steer maximally if the angle is greater than the threshold, otherwise steer based on the angle.
		Steering = AbsoluteTargetAngle > MaxAngleDontSlow ? 1 : AbsoluteTargetAngle / MaxAngleDontSlow;
		Steering *= FMath::Sign(SignedTargetAngle);
		// Make sure the speed difference is mapped to [-1, 1] as that is needed for the throttle input and hence the PID controller part.
		SpeedDifference = FMath::GetMappedRangeValueClamped(
			FVector2D(-DesiredSpeedThrottleThreshold, DesiredSpeedThrottleThreshold), FVector2D(-1.f, 1.f),
			AdjustedDesiredSpeed - M_CurrentSpeed);
		ThrottleIncreaseValue = M_ThrottleIncreaseForDesiredSpeed;
		// At the timer check the current speed vs the max forward speed.
		if (AdjustedDesiredSpeed == DesiredSpeed && M_CurrentSpeed < DesiredSpeed)
		{
			M_TimeWantDesiredSpeed += DeltaTime;
		}
		else
		{
			M_TimeWantDesiredSpeed = 0;
		}
	}

	ThrottlePID.AddMeasuredValue(SpeedDifference, DeltaTime);
	CalculatedThrottleValue = ThrottlePID.GetPIDValue();
	if (M_TimeWantDesiredSpeed > 1.5)
	{
		CalculatedThrottleValue += FMath::GetMappedRangeValueClamped(
			FVector2D(1.5, 4), FVector2D(0, ThrottleIncreaseValue), M_TimeWantDesiredSpeed);
	}

	CalculatedThrottleValue = FMath::Clamp(CalculatedThrottleValue, 0, 1);
	CalculatedThrottleValue = bReversing ? CalculatedThrottleValue * -1.f : CalculatedThrottleValue;

	// Apply smoothing
	SmoothedTargetAngle = FMath::Lerp(SmoothedTargetAngle, AbsoluteTargetAngle, SmoothingFactor);
	SmoothedDestinationDistance = FMath::Lerp(SmoothedDestinationDistance, DestinationDistance, SmoothingFactor);

	// Use smoothed values in deadzone calculations to avoid jittering.
	if (SmoothedDestinationDistance < DeadZoneDistance &&
		((!bReversing && SmoothedTargetAngle >= DeadZoneAngle) || bIsInReverseDeadzone))
	{
		CalculatedThrottleValue = 0.00;

		// Max steering.
		Steering = FMath::Sign(SignedTargetAngle);
		if (bDebugSlowDown && !bIsInReverseDeadzone)
		{
			RTSFunctionLibrary::PrintString("Frontal deadzone, only steering!", FColor::Black);
			DrawDebugLine(GetWorld(), GetAgentLocation(), Destination, FColor::Black, false, 0.1f, 1, 3.0f);
		}
		if (bDebugSlowDown && bIsInReverseDeadzone)
		{
			RTSFunctionLibrary::PrintString("Reverse deadzone, only steering!", FColor::Black);
			DrawDebugLine(GetWorld(), GetAgentLocation(), Destination, FColor::White, false, 0.1f, 1, 3.0f);
		}
	}
	M_LastSteeringInput = Steering;
	M_LastThrottleInput = CalculatedThrottleValue;
	UpdateVehicle(CalculatedThrottleValue,
	              M_CurrentSpeed,
	              DeltaTime,
	              0,
	              Steering);

	if constexpr (DeveloperSettings::Debugging::GRTSNavAgents_Compile_DebugSymbols)
	{
		const bool bShouldDebugNavAgents = bDebug && ControlledPawn != nullptr;
		if (bShouldDebugNavAgents)
		{
			// flush all debug messages

			FNavAgentProperties Properties = ControlledPawn->GetNavAgentPropertiesRef();
			auto NavData = Properties.PreferredNavData;

			FString InWorldDebug = "";
			InWorldDebug += "Cur Vs Des speed: " + FString::SanitizeFloat(M_CurrentSpeed) + " / " +
				FString::SanitizeFloat(DesiredSpeed) + "\n";
			InWorldDebug += "Agent Radius: " + FString::SanitizeFloat(Properties.AgentRadius) + "\n";
			const FString NavDataStr = NavData.IsNull() ? "Any" : NavData.ToString();
			InWorldDebug += "NavData: " + NavDataStr + "\n";
			// Add steering:
			InWorldDebug += "Steering Input: " + FString::SanitizeFloat(M_LastSteeringInput) + "\n";

			// NEW: resolve agent name using our registry (match by radius/height/class)
			const URTSNavAgentRegistry* NavAgentRegistry = URTSNavAgentRegistry::Get(this);
			const FName AgentName = NavAgentRegistry
				                        ? NavAgentRegistry->GetAgentNameForProps(GetWorld(), Properties, 0.5f)
				                        : NAME_None;
			const FString AgentNameStr = AgentName.IsNone() ? TEXT("UNKNOWN") : AgentName.ToString();
			InWorldDebug += "RTSAgent: " + AgentNameStr + "\n";

			const FVector DebugLoc = ControlledPawn->GetActorLocation() + FVector(0, 0, 500);
			DrawDebugString(GetWorld(), DebugLoc, InWorldDebug, nullptr, FColor::Blue, DeltaTime, false, 1.3f);
		}
	}


	// RTSFunctionLibrary::PrintString("Destination distance: " + FString::SanitizeFloat(DestinationDistance),
	//                                 FColor::Red);
	// RTSFunctionLibrary::PrintString(" frontal desired speed: " + FString::SanitizeFloat(DesiredSpeed),
	//                                 FColor::Red);
	// RTSFunctionLibrary::PrintString(
	// 	" frontaladjusted Desired speed: " + FString::SanitizeFloat(AdjustedDesiredSpeed), FColor::Purple);
	// RTSFunctionLibrary::PrintString("current speed: " + FString::SanitizeFloat(M_CurrentSpeed), FColor::Green);
	// RTSFunctionLibrary::PrintString("Desired Reverse Speed: " + FString::SanitizeFloat(DesiredReverseSpeed),
	//                                 FColor::Yellow);
	// TArray<FString> DebugMessages;
	// DebugMessages.Add(FString::Printf(TEXT("Stuck Detection")));
	// DebugMessages.Add(FString::Printf(TEXT("Is Stuck : %s"), bIsStuck ? TEXT("true") : TEXT("false")));
	//
	// DebugMessages.Add(FString::Printf(TEXT("")));
	//
	// DebugMessages.Add(
	// 	FString::Printf(TEXT("Last Path Point : %s"), bIsLastPathPoint ? TEXT("true") : TEXT("false")));
	//
	// float AgentRadius, AgentHalfHeight;
	// AActor* MovingAgent = ControlledPawn;
	// MovingAgent->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);
	// AgentRadius *= MinAgentRadiusPct;
	// DebugMessages.Add(FString::Printf(TEXT("Acceptance Radius: %0.3f"), AcceptanceRadius));
	// DebugMessages.Add(FString::Printf(TEXT("Agent Radius: %0.3f"), AgentRadius));
	// DebugMessages.Add(FString::Printf(TEXT("Agent Radius multi: %0.3f"), MinAgentRadiusPct));
	// for (int i = 0; i < DebugMessages.Num(); i++)
	// {
	// 	if (i % 2 == 0)
	// 	{
	// 		RTSFunctionLibrary::PrintString(DebugMessages[i], FColor::Blue);
	// 	}
	// 	else
	// 	{
	// 		RTSFunctionLibrary::PrintString(DebugMessages[i], FColor::Purple);
	// 	}
	// }
}

void UTrackPathFollowingComponent::FollowPathSegment(float DeltaTime)
{
	UVehiclePathFollowingComponent::FollowPathSegment(DeltaTime);
	TRACE_CPUPROFILER_EVENT_SCOPE(VehiclePathFollowing_FollowSegment);

	if (not IsValid(ControlledPawn))
	{
		return;
	}

	UpdateStuckSamples();

	FVector Destination = GetCurrentTargetLocation();

	// sphere at the destination
	if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
	{
		DrawDebugSphere(GetWorld(), Destination, 50.f, 12, FColor::Green, false, 0.1f);
		// Draw debug sphere at unstuck destination
		DrawDebugSphere(GetWorld(), UnStuckDestination, 50.f, 12, FColor::Red, false, 0.1f);
	}

	// If we're stuck try to unstuck
	if (bIsStuck && !bWalkingNavLinkStart)
	{
		UpdateDriving(UnStuckDestination, DeltaTime);
		float Distance = FVector::Dist(GetAgentLocation(), UnStuckDestination);

		if (ControlledPawn->GetVelocity().Size2D() <= DeveloperSettings::GamePlay::Navigation::StuckSpeedThreshold)
		{
			M_TimeAtLowSpeed += DeltaTime;
			if (M_TimeAtLowSpeed >= 2 * StuckTimeLowSpeedScale)
			{
				if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
				{
					RTSFunctionLibrary::PrintString("Vehicle stuck while navigating to unstuck location!"
					                                "\n" + FString::SanitizeFloat(GetWorld()->GetTimeSeconds()),
					                                FColor::Red);
				}
				// Get new unstuck location.
				UnStuckDestination = TeleportOrCalculateUnstuckDestination(Destination);
				M_TimeAtLowSpeed = 0;
			}
		}
		if (Distance < StuckAcceptanceRadius)
		{
			// if the distance is less than the acceptance radius, we can stop our stuck logic
			ResetStuck();
			if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("Vehicle no longer stuck as within acceptance radius!", FColor::Green);
			}
		}
	}
	else
	{
		// if we're not currently stuck lets check for the next frame
		bIsStuck = IsStuck(DeltaTime);

		// If stuck, calculate a destination for unstuck for use next frame
		if (bIsStuck)
		{
			UnStuckDestination = TeleportOrCalculateUnstuckDestination(Destination);
			return;
		}

		// Call custom driving method
		UpdateDriving(Destination, DeltaTime);
	}
}


void UTrackPathFollowingComponent::UpdatePathSegment()
{
	UVehiclePathFollowingComponent::UpdatePathSegment();

	if (GetValidControlledPawn() && Path.IsValid())
	{
		// Setup the slowdown angle which is used in the slowdown calculation
		float Distance;

		SlowdownAngle = FMath::Abs(CalculateLargestAngleWithSamples(Distance));

		AverageAheadAngle = FMath::Abs(CalculateAverageAngleWithSamples(Distance));

		const float DistanceLeft = (*Path->GetPathPointLocation(Path->GetPathPoints().Num() - 1) - ControlledPawn->
			GetActorLocation()).Size();
		if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
		{
			if (bDebugAcceptanceRadius)
			{
				RTSFunctionLibrary::PrintString("Distance left: " + FString::SanitizeFloat(DistanceLeft),
				                                FColor::Green);
				RTSFunctionLibrary::PrintString("Acceptance Radius: " + FString::SanitizeFloat(AcceptanceRadius),
				                                FColor::Green);
			}
		}

		if (DistanceLeft < AcceptanceRadius)
		{
			OnSegmentFinished();

			if (bSnapToSpline && !bIsLastPathPoint)
			{
				return;
			}
			OnPathFinished(FPathFollowingResult(FPathFollowingResultFlags::Success));
			//OnPathFinished(EPathFollowingResult::Success);
			if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
			{
				if (bDebug)
				{
					RTSFunctionLibrary::PrintString("SUCCESSFULLY FINISHED PATH");
				}
			}
		}
	}
}

void UTrackPathFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{
	int32 EndSegmentIndex = SegmentStartIndex + 1;
	FNavigationPath* PathInstance = Path.Get();

	if (PathInstance != nullptr && PathInstance->GetPathPoints().IsValidIndex(SegmentStartIndex) && PathInstance->
		GetPathPoints().IsValidIndex(EndSegmentIndex))
	{
		// Allow the AI Controller to modify the MoveSegment Location
		AVehicleAIController* VehicleController = Cast<AVehicleAIController>(GetOwner());

		FVector SegmentDestination = PathInstance->GetPathPoints()[EndSegmentIndex].Location;
		FNavPathPoint DestinationPoint = PathInstance->GetPathPoints()[EndSegmentIndex];


		const ARecastNavMesh* RecastNavMesh = Cast<const ARecastNavMesh>(PathInstance->GetNavigationDataUsed());
		if (RecastNavMesh)
		{
			FNavMeshNodeFlags NodeFlags(PathInstance->GetPathPoints()[EndSegmentIndex].Flags);
			const UClass* AreaClass = RecastNavMesh->GetAreaClass(NodeFlags.Area);

			FNavMeshNodeFlags StartNodeFlags(PathInstance->GetPathPoints()[SegmentStartIndex].Flags);

			bool bOnNavLink = StartNodeFlags.IsNavLink() || NodeFlags.IsNavLink();
			bOnNavLink = bWalkingNavLinkStart;

			if (VehicleController)
			{
				FVector MoveSegmentLocation = SegmentDestination;

				if (PathInstance->GetPathPoints().IsValidIndex(MoveSegmentStartIndex))
				{
					MoveSegmentLocation = PathInstance->GetPathPoints()[MoveSegmentStartIndex].Location;
				}

				SegmentDestination = VehicleController->OnNewPathSegment(
					EndSegmentIndex, MoveSegmentLocation, SegmentDestination, AreaClass, bOnNavLink);
			}
		}

		DestinationPoint.Location = SegmentDestination;

		PathInstance->GetPathPoints()[EndSegmentIndex] = DestinationPoint;
	}
	Super::SetMoveSegment(SegmentStartIndex);

	// Adjust the acceptance radius based on the user created acceptance radius

	if (PathInstance != nullptr && PathInstance->GetPathPoints().IsValidIndex(SegmentStartIndex) && PathInstance->
		GetPathPoints().IsValidIndex(EndSegmentIndex))
	{
		if (PathInstance->GetPathPoints().Num() == (EndSegmentIndex + 1))
		{
			bIsLastPathPoint = true;
		}
		else
		{
			bIsLastPathPoint = false;
		}

		const FNavPathPoint& PathPt0 = PathInstance->GetPathPoints()[SegmentStartIndex];
		const FNavPathPoint& PathPt1 = PathInstance->GetPathPoints()[EndSegmentIndex];

		CurrentAcceptanceRadius = (PathInstance->GetPathPoints().Num() == (EndSegmentIndex + 1))
			                          ? VehicleGoalAcceptanceRadius
			                          // pick appropriate value base on whether we're going to nav link or not
			                          : VehiclePathPointAcceptanceRadius;
		AcceptanceRadius = CurrentAcceptanceRadius;
		if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
		{
			if (bDebugAcceptanceRadius)
			{
				RTSFunctionLibrary::PrintString(
					"vehicleGoalAcceptanceRadius: " + FString::SanitizeFloat(VehicleGoalAcceptanceRadius),
					FColor::Red);
				RTSFunctionLibrary::PrintString(
					"vehiclePathPointAcceptanceRadius: " + FString::SanitizeFloat(VehiclePathPointAcceptanceRadius),
					FColor::Red);
				RTSFunctionLibrary::PrintString(
					"CurrentAcceptanceRadius: " + FString::SanitizeFloat(CurrentAcceptanceRadius), FColor::Green);
			}
		}
	}
}

void UTrackPathFollowingComponent::OnPathFinished(const FPathFollowingResult& Result)
{
	// When the path ends, set all the steering and throttle to nothing, and apply the brakes
	UpdateVehicle(0.f, 0.f, 1.f, 0, 0);
	if (IsValid(ControlledPawn))
	{
		IVehicleAIInterface* VehicleAI = Cast<IVehicleAIInterface>(ControlledPawn);
		if (VehicleAI)
		{
			VehicleAI->OnFinishedPathFollowing();
		}
	}

	Super::OnPathFinished(Result);
}


void UTrackPathFollowingComponent::UpdateVehicle(float Throttle, float CurrentSpeed, float DeltaTime,
                                                 float BreakAmount, float Steering)
{
	if (bImplementsInterface)
	{
		IVehicleAIInterface::Execute_UpdateVehicle(ControlledPawn, AbsoluteTargetAngle, DestinationDistance,
		                                           AdjustedDesiredSpeed, Throttle, CurrentSpeed, DeltaTime, BreakAmount,
		                                           Steering);
	}
}


void UTrackPathFollowingComponent::Initialize()
{
	Super::Initialize();

	ThrottlePID.ResetController(ThrottlePIDSetup);

	bImplementsInterface = DoesControlledPawnImplementInterface();

	if (!bImplementsInterface)
	{
		RTSFunctionLibrary::PrintString("Interface is not implemented on your vehicle, AI will not work!");
	}

	// // Messy solution to allow blueprint enum to be used with the crowd simulation enum
	// SetVehicleCrowdSimulation(DetourCrowdSimulationState);
	//
	// // Setup crowd properties
	// SetCrowdCollisionQueryRange(CrowdQueryRange, true);
	// //SetCrowdObstacleAvoidance()
	// SetCrowdPathOptimizationRange(CrowdCollisionRadius * 30, true);
	// SetCrowdAvoidanceRangeMultiplier(CrowdCollisionRadius * 12, true);
}

bool UTrackPathFollowingComponent::DoesControlledPawnImplementInterface()
{
	if (GetValidControlledPawn())
	{
		return GetValidControlledPawn()->GetClass()->ImplementsInterface(UVehicleAIInterface::StaticClass());
	}
	RTSFunctionLibrary::ReportError(
		"No valid controlled pawn found for UVehiclePathFollowingComponent::DoesControlledPawnImplementInterface");
	return false;
}


FVector UTrackPathFollowingComponent::GetMoveFocus(bool bAllowStrafe) const
{
	FVector MoveFocus = FVector::ZeroVector;
	if (bAllowStrafe && DestinationActor.IsValid())
	{
		MoveFocus = DestinationActor->GetActorLocation();
	}
	else
	{
		const FVector CurrentMoveDirection = GetCurrentDirection();
		MoveFocus = *CurrentDestination + (CurrentMoveDirection * FAIConfig::Navigation::FocalPointDistance);
	}

	return MoveFocus;
}

void UTrackPathFollowingComponent::OnPathfindingQuery(FPathFindingQuery& Query)
{
	// to not disable stringpulling.
	return;
}


float UTrackPathFollowingComponent::CalculateTrackStoppingDistance(const float Velocity)
{
	// Mapping from speed difference range [0, max(Velocity, DesiredSpeed)] to a range from SlowdownDistance to 100
	// Based on the absolute difference between desired speed and current velocity
	return FMath::GetMappedRangeValueClamped(
		FVector2D(0.f, FMath::Max(DesiredSpeed, Velocity)),
		FVector2D(SlowdownDistance, 100.f),
		FMath::Abs(DesiredSpeed - Velocity)
	);
}


void UTrackPathFollowingComponent::DebugIdleOverlappingActors()
{
	FString DebugMessage = "overlapping: ";
	for (const FOverlapActorData& OverlapData : M_IdleAlliedBlockingActors)
	{
		if (OverlapData.Actor.IsValid())
		{
			DebugMessage += OverlapData.Actor->GetName() + "\n";
		}
	}
	RTSFunctionLibrary::DrawDebugAtLocation(this, GetAgentLocation() + FVector(0, 0, 300),
	                                        DebugMessage, FColor::White, 5.f);
}

void UTrackPathFollowingComponent::DebugMovingOverlappingActors(const float DeltaTime)
{
	FString DebugMessage = "overlapping moving: ";
	for (const FOverlapActorData& OverlapData : M_MovingBlockingOverlapActors)
	{
		if (OverlapData.Actor.IsValid())
		{
			DebugMessage += OverlapData.Actor->GetName() + "\n";
		}
	}
	RTSFunctionLibrary::DrawDebugAtLocation(this, GetAgentLocation() + FVector(0, 0, 600),
	                                        DebugMessage, FColor::Yellow, DeltaTime);
}

void UTrackPathFollowingComponent::DrawNavLinks(const FColor Color)
{
	// Get the path points to check for NavLinks
	if (Path.IsValid())
	{
		const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
		for (int i = 0; i < Points.Num(); ++i)
		{
			if (FNavMeshNodeFlags(Points[i].Flags).IsNavLink())
			{
				// Draw red spheres at NavLink locations
				DrawDebugSphere(
					GetWorld(),
					Points[i].Location,
					30.0f, // Sphere radius
					12, // Sphere segments
					Color,
					false, // Persistent (true if you want it to stay for more than one frame)
					0.05, // Life time in seconds
					0, // Depth priority
					1.5f // Thickness of lines
				);
			}
		}
	}
}


float UTrackPathFollowingComponent::CalculateDestinationAngle(const FVector& Destination)
{
	FVector LocDifference = Destination - GetAgentLocation();
	float SignAngle = GetVectorAngle(LocDifference, ControlledPawn->GetActorRightVector());
	SignAngle = (SignAngle < 90.0f) ? 1.0f : -1.0f;
	return GetVectorAngle(LocDifference, ControlledPawn->GetActorForwardVector()) * SignAngle;
}

float UTrackPathFollowingComponent::CalculateDestinationDistance(const FVector& Destination)
{
	return FVector::Dist(GetAgentLocation(), Destination);
}

float UTrackPathFollowingComponent::GetPathLengthRemaining()
{
	if (Path.IsValid())
	{
		return Path.Get()->GetLengthFromPosition(GetAgentLocation(), MoveSegmentEndIndex);
	}

	return 0.f;
}


FVector UTrackPathFollowingComponent::GetUnstuckLocation(const FVector& MoveFocus)
{
	if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finding unstuck location");
	}
	// Cache agent location and directional vectors to avoid redundant calls
	const FVector AgentLocation = GetAgentLocation();
	const FVector RightVector = ControlledPawn->GetActorRightVector();
	const FVector ForwardVector = ControlledPawn->GetActorForwardVector();

	// Determine if the destination is more to the right or left
	const bool bDestinationOnTheRight = CalculateDestinationAngle(MoveFocus) > 0.f;

	// Define potential unstuck locations based on destination direction
	FVector FirstCheckLocation;
	FVector SecondCheckLocation;

	if (bDestinationOnTheRight)
	{
		FirstCheckLocation = AgentLocation + (RightVector * UnStuckTraceDistance);
		SecondCheckLocation = AgentLocation - (RightVector * UnStuckTraceDistance);
	}
	else
	{
		FirstCheckLocation = AgentLocation - (RightVector * UnStuckTraceDistance);
		SecondCheckLocation = AgentLocation + (RightVector * UnStuckTraceDistance);
	}

	// Perform visibility trace on the first preferred side
	if (!VisibilityTrace(AgentLocation, FirstCheckLocation))
	{
		DrawDebugSphere(GetWorld(), FirstCheckLocation, 50.f, 12, FColor::Purple, false, 1.f);
		return FirstCheckLocation;
	}

	// If first trace fails, perform visibility trace on the opposite side
	if (!VisibilityTrace(AgentLocation, SecondCheckLocation))
	{
		DrawDebugSphere(GetWorld(), SecondCheckLocation, 50.f, 12, FColor::Purple, false, 1.f);
		return SecondCheckLocation;
	}

	// Fallback: Move backward if both sides are blocked
	return AgentLocation - (ForwardVector * UnStuckDistance);
}

bool UTrackPathFollowingComponent::VisibilityTrace(const FVector& Start, const FVector& End) const
{
	if (UWorld* World = GetWorld())
	{
		if (bDebug)
		{
			DrawDebugLine(World, Start, End, FColor::Red, false, 4.1f, 0, 1.f);
		}
		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(ControlledPawn);
		// Trace from the start to the end
		return World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);
	}
	return false;
}

FVector UTrackPathFollowingComponent::TeleportOrCalculateUnstuckDestination(const FVector& MoveFocus)
{
	if (not bM_TeleportedLastStuck)
	{
		if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Attempting to teleport vehicle to get unstuck!");
		}
		// Attempt to find a teleport location
		FVector TeleportLocation = FindTeleportLocation();

		if (not TeleportLocation.IsNearlyZero())
		{
			// Teleport the vehicle to the new location
			ControlledPawn->SetActorLocation(TeleportLocation, false, nullptr, ETeleportType::TeleportPhysics);

			// Draw a debug sphere at the teleport location
			if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
			{
				DrawDebugSphere(GetWorld(), TeleportLocation, 50.f, 12, FColor::Red, false, 5.f);
			}

			// Vehicle is no longer stuck
			ResetStuck();

			bM_TeleportedLastStuck = not bM_TeleportedLastStuck;

			// Return the current location as we've already teleported
			return ControlledPawn->GetActorLocation();
		}
	}
	bM_TeleportedLastStuck = not bM_TeleportedLastStuck;
	// Teleportation either failed or we teleported last time.
	return GetUnstuckLocation(MoveFocus);
}

FVector UTrackPathFollowingComponent::FindTeleportLocation()
{
	const FVector StartLocation = ControlledPawn->GetActorLocation();
	// Calculate direction vector to the destination
	const FVector Direction = (VehicleCurrentDestination - StartLocation).GetSafeNormal();
	const FVector EndLocation = StartLocation + (Direction * TeleportDistance);

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(ControlledPawn);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility,
	                                                 CollisionParams);

	if (not bHit)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (NavSys)
		{
			FNavLocation NavLocation;
			if (NavSys->ProjectPointToNavigation(EndLocation, NavLocation, FVector(100.f, 100.f, 100.f)))
			{
				return NavLocation.Location;
			}
		}
	}

	// Teleportation not possible, return zero vector
	return FVector::ZeroVector;
}

void UTrackPathFollowingComponent::ResetStuck()
{
	bIsStuck = false;
	// Reset the timer for low speed and the flag that indicates if we did a force rotation to the unstuck location.
	M_TimeAtLowSpeed = 0;
	StuckLocationSamples.Empty();
	NextStuckSampleIdx = 0;
}


float UTrackPathFollowingComponent::TrackGetSlowdownSpeed(
	const float DistanceToNextPoint,
	const FVector& Destination)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Tracjs_GetSlowDownSpeed);

	if (!IsValid(M_TrackPhysicsMovement))
	{
		RTSFunctionLibrary::ReportError("No TrackPhysics movement component"
			"\n On component:" + GetName()
			+ "\n ");
		return DesiredSpeed;
	}
	const float Alpha = FMath::GetMappedRangeValueClamped(FVector2D(SlowdownDistance, 0.f), FVector2D(0.f, 1.f),
	                                                      DistanceToNextPoint);
	if (FinalDestinationDistance < SlowdownDistance)
	{
		if (M_TrackPhysicsMovement->GetInclineAngle() >= DeveloperSettings::GamePlay::Navigation::TrackVehicleIncline ||
			M_TrackPhysicsMovement->GetInclineAngle() < -KINDA_SMALL_NUMBER)
		{
			if (bDebugSlowDown)
			{
				RTSFunctionLibrary::PrintString("Inclin"
				                                "e to destination: " + FString::SanitizeFloat(
					                                M_TrackPhysicsMovement->GetInclineAngle()),
				                                FColor::Orange);
				DrawDebugLine(GetWorld(), ControlledPawn->GetActorLocation(), Destination, FColor::Orange, false,
				              0.1f, 0, 1.0f);
			}
			return DesiredSpeed;
		}

		// If we're at the final destination point, slow down
		if (bDebugSlowDown)
		{
			DrawDebugLine(GetWorld(), ControlledPawn->GetActorLocation(), Destination, FColor::Purple, false, 0.1f,
			              0, 1.0f);
		}
		return FMath::Lerp(DesiredSpeed, M_MinimalSlowdownSpeed, Alpha);
	}
	if (AbsoluteTargetAngle > MaxAngleDontSlow)
	{
		if (bDebugSlowDown)
		{
			DrawDebugLine(GetWorld(), ControlledPawn->GetActorLocation(), Destination, FColor::Red, false, 0.1f, 0,
			              1.0f);
		}
		return FMath::Lerp(0.f, CalculateTrackMaximumSpeedWithSamples(), CornerSpeedControlPercentage);
	}
	if (bDebugSlowDown)
	{
		DrawDebugLine(GetWorld(), ControlledPawn->GetActorLocation(), Destination, FColor::Green, false, 0.1f, 0,
		              1.0f);
	}
	return DesiredSpeed;
}


float UTrackPathFollowingComponent::CalculateTrackMaximumSpeedWithSamples()
{
	TArray<FNavPathPoint> Points = Path.Get()->GetPathPoints();

	// Clamp the max index
	int MaxIndex = GetCurrentPathIndex() + CornerSlowdownSamples + 1;
	MaxIndex = FMath::Clamp(MaxIndex, 0, (Points.Max() - 1));

	float MaximumCorneringSpeed = DesiredSpeed;
	const float TempSpeed = DesiredSpeed;

	// Calculate the stopping distance at this desired speed, we don't need to check further ahead than this as we would already be able to stop
	const float StoppingDistance = CalculateTrackStoppingDistance(M_CurrentSpeed);

	// We work out the number of samples (capped at the maximum slowdown samples for perf reasons) based on the stopping distance and not an arbitary value
	float Distance = 0.f;
	int Samples = 1;
	float CurveRadius = 0.f;

	for (int i = GetCurrentPathIndex(); i <= MaxIndex; i++)
	{
		if (Points.IsValidIndex(i) && Points.IsValidIndex(i + 1))
		{
			if (Distance > StoppingDistance)
			{
				break;
			}
			CurveRadius = CurveRadius + UKismetMathLibrary::FindLookAtRotation(
				Points[i].Location, Points[i + 1].Location).Yaw;

			if (TempSpeed < MaximumCorneringSpeed)
			{
				MaximumCorneringSpeed = TempSpeed;
			}

			SlowdownSampleIndex = i;

			Samples++;

			// Update the distance we've traversed here
			Distance = Distance + FVector::Dist(Points[i].Location, Points[i + 1].Location);
		}
		else
		{
			break;
		}
	}

	CurveRadius = CalculateAverageAngleWithSamples(Distance);


	// Get the angle of the curve in radians
	const float CurveAngle = FMath::Abs(((Distance / 100) / (CurveRadius * 0.0174533)));

	const float TempCalculatedMaxSpeed = GetMaximumCorneringSpeed(CurveAngle);

	if (TempCalculatedMaxSpeed > DesiredSpeed)
	{
		return DesiredSpeed;
	}
	return TempCalculatedMaxSpeed;
}

float UTrackPathFollowingComponent::GetMaximumCorneringSpeed(float CurveRadius)
{
	float TireRoadFrictionCoefficient = 1.f; // GetFrictionCoefficientAtLocation(Get);
	//VehicleDownforce = 0.3f;

	return FMath::Sqrt(TireRoadFrictionCoefficient * CurveRadius * (WorldGravity + VehicleMass / VehicleDownforce));
}


bool UTrackPathFollowingComponent::IsPathClear(FVector Start, FVector End)
{
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	Params.bTraceComplex = true; // Consider using complex collision for more accuracy.
	Params.bReturnPhysicalMaterial = false; // Set to true if you need material data.

	// Perform the line trace
	bool bHasObstacle = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, COLLISION_TRACE_LANDSCAPE, Params);

	// Debug draw the line trace
	if (bDebug)
	{
		FColor TraceColor = bHasObstacle ? FColor::Red : FColor::Green;
		DrawDebugLine(GetWorld(), Start, End, TraceColor, false, 5.0f, 0, 5.0f);
		if (bHasObstacle)
		{
			DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 10.0f, 32, FColor::Red, false, 5.0f);
		}
	}

	return !bHasObstacle;
}

bool UTrackPathFollowingComponent::ShouldTrackVehicleReverse(const float TurnAngle, const float TargetDistance)
{
	// If we manually want to tell this vehicle to reverse
	if (bWantsReverse)
	{
		return true;
	}
	const bool bIsFinalPointFar = TargetDistance >= FaceFirstPointIfEndDistanceGreaterThan;
	if (GetCurrentPathIndex() == 0 && bIsFinalPointFar)
	{
		return false;
	}
	if (bReversing)
	{
		// If we're already revesing, include the threshold for deciding whether we should reverse or not
		// this prevents the vehicle constantly trying to reverse and move forward while its near the ShouldReverse angle
		/*
		 * If the vehicle is already reversing (bReversing),
		 * it checks whether the TurnAngle is greater than a slightly reduced reverse angle (ShouldReverseAngle - ReverseThreshold).
		 * This hysteresis (using ReverseThreshold)
		 * prevents the control system from oscillating between moving forward and reversing when the vehicle is close to the threshold angle.
		 */
		return (TurnAngle > (ShouldReverseAngle - ReverseThreshold) && TargetDistance < ReverseMaxDistance);
	}
	return (TurnAngle > ShouldReverseAngle && TargetDistance < ReverseMaxDistance);
}


void UTrackPathFollowingComponent::RegisterWithRTSNavigator()
{
	if (UWorld* World = GetWorld())
	{
		APlayerController* PlayerController = Cast<APlayerController>(UGameplayStatics::GetPlayerController(World, 0));
		if (PlayerController)
		{
			if (ACPPController* CPPController = Cast<ACPPController>(PlayerController))
			{
				if (ARTSNavigator* Navigator = CPPController->GetRTSNavigator())
				{
					Navigator->RegisterWithNavigator(this);
				}
			}
		}
	}
}

bool UTrackPathFollowingComponent::UpdateStuckSamples()
{
	const float GameTime = GetWorld()->GetTimeSeconds();
	if (bUseStuckDetection && GameTime > (LastStuckSampleTime + StuckDetectionInterval) && StuckDetectionSampleCount >
		0)
	{
		LastStuckSampleTime = GameTime;

		if (StuckLocationSamples.Num() < StuckDetectionSampleCount)
		{
			StuckLocationSamples.Add(GetAgentLocation());
		}
		else
		{
			// RotateTowards the samples to maintain a fixed size
			StuckLocationSamples.RemoveAt(0);
			StuckLocationSamples.Add(GetAgentLocation());
		}
		return true;
	}

	return false;
}


float UTrackPathFollowingComponent::CalculateAverageAngleWithSamples(float& OutDistance)
{
	TArray<FNavPathPoint> Points = Path.Get()->GetPathPoints();

	// Clamp the max index
	int MaxIndex = GetCurrentPathIndex() + CornerSlowdownSamples + 1;
	MaxIndex = FMath::Clamp(MaxIndex, 0, (Points.Max() - 1));

	float Angles = 0.f;
	int Samples = 0;
	float Distance = 0.f;

	FVector ForwardVector = GetCurrentDirection();

	for (int i = GetCurrentPathIndex(); i <= MaxIndex; i++)
	{
		if (Points.IsValidIndex(i) && Points.IsValidIndex(i + 1))
		{
			if (Distance > SlowdownDistance)
			{
				break;
			}
			Angles = Angles + (UKismetMathLibrary::FindLookAtRotation(Points[i].Location, Points[i + 1].Location).Yaw -
				ForwardVector.Rotation().Yaw);

			ForwardVector = UKismetMathLibrary::FindLookAtRotation(Points[i].Location, Points[i + 1].Location).Vector();

			SlowdownSampleIndex = i;

			Samples++;

			Distance = Distance + FVector::Dist(Points[i].Location, Points[i + 1].Location);
		}
		else
		{
			break;
		}
	}

	// Finds the length of the Chord
	float ChordLength = FVector::Dist(Points[GetCurrentPathIndex()].Location, Points[GetCurrentPathIndex() + 1]);

	OutDistance = Distance;

	return Angles / Samples;
}


float UTrackPathFollowingComponent::CalculateLargestAngleWithSamples(float& OutDistance)
{
	TArray<FNavPathPoint> Points = Path.Get()->GetPathPoints();

	// Clamp the max index
	int MaxIndex = GetCurrentPathIndex() + CornerSlowdownSamples + 1;
	MaxIndex = FMath::Clamp(MaxIndex, 0, (Points.Max() - 1));

	float MaxAngle = 0.f;
	float Distance = 0.f;

	for (int i = GetCurrentPathIndex(); i <= MaxIndex; i++)
	{
		if (Points.IsValidIndex(i) && Points.IsValidIndex(i + 1))
		{
			if (Distance > SlowdownDistance)
			{
				break;
			}

			MaxAngle = MaxAngle + UKismetMathLibrary::FindLookAtRotation(Points[i].Location, Points[i + 1].Location).
				Yaw;
			SlowdownSampleIndex = i;

			Distance = Distance + FVector::Dist(Points[i].Location, Points[i + 1].Location);
		}
		else
		{
			break;
		}
	}

	OutDistance = Distance;

	return MaxAngle;
}


FVector UTrackPathFollowingComponent::GetAgentLocation() const
{
	if (NavMovementInterface.IsValid())
	{
		// Use the GetLocation method from INavMovementInterface
		return NavMovementInterface->GetLocation();
	}

	RTSFunctionLibrary::ReportError("nav movement interface not valid, unable to get crowd agent location");
	return FVector::ZeroVector;
}

float UTrackPathFollowingComponent::GetVectorAngle(FVector A, FVector B)
{
	A = A.GetSafeNormal();
	B = B.GetSafeNormal();
	return UKismetMathLibrary::DegAcos(FVector::DotProduct(A, B) / (A.Size() * B.Size()));
}

EEvasionAction UTrackPathFollowingComponent::DetermineOverlapWithinMovementDirection(
	const FVector& MovementDirection, AActor* OverlapActor, const float DeltaTime)
{
	if (not IsValid(OverlapActor) || MovementDirection.IsNearlyZero())
	{
		return EEvasionAction::MoveNormally;
	}

	const FVector SelfLoc = ControlledPawn->GetActorLocation();
	const FVector OtherLoc = OverlapActor->GetActorLocation();

	// Relative vector to the other actor (2D ground-plane).
	const FVector ToOther2D = (OtherLoc - SelfLoc);
	const float Dist2D = ToOther2D.Size2D();
	if (Dist2D < TrackFollowingEvasion::MinSeparationToConsider)
	{
		// Too close to make a stable prediction; keep moving normally to avoid oscillation.
		return EEvasionAction::MoveNormally;
	}

	const FVector DirToOther = ToOther2D.GetSafeNormal2D();
	const float cosInCone = FVector::DotProduct(MovementDirection, DirToOther);

	// Check if the other actor lies inside our forward cone.
	const float minCosCone = FMath::Cos(FMath::DegreesToRadians(TrackFollowingEvasion::MovementDirectionConeHalfAngle));
	if (cosInCone <= minCosCone)
	{
		// Outside our cone -> not a forward conflict case.
		return EEvasionAction::MoveNormally;
	}

	// Angle-of-attack: compare the other's velocity direction against our movement vector.
	// Intuition:
	// - If the other is headed "into us", its direction aligns with -MovementDirection and also roughly with -DirToOther.
	// - Using -MovementDirection is robust when we’re fast and the other is fast too (classic head-on geometry).
	const FVector OtherVel = OverlapActor->GetVelocity();
	FVector OtherDir = OtherVel.GetSafeNormal2D();

	// If they have no meaningful velocity, they’re not a “moving evader” case.
	if (OtherDir.IsNearlyZero())
	{
		return EEvasionAction::MoveNormally;
	}

	// Compute AoA in degrees against our movement (reversed, i.e., toward us).
	const float cosAoA = FMath::Clamp(FVector::DotProduct(OtherDir, -MovementDirection), -1.f, 1.f);
	const float AoADeg = FMath::RadiansToDegrees(FMath::Acos(cosAoA));
	const bool bCriticalAoA = (AoADeg <= TrackFollowingEvasion::CriticalAngleOfAttackDeg);

	// Additional gating: the other should be traveling *toward us*, not parallel-away.
	// Check that it's generally aimed toward us using vector from other to us.
	const FVector ToUs2D = (SelfLoc - OtherLoc).GetSafeNormal2D();
	const float cosTowardUs = FVector::DotProduct(OtherDir, ToUs2D);
	const bool bEnteringCone = cosTowardUs > 0.f;

	// Debug readouts over the other actor.
	if (TrackFollowingEvasion::bDebugTrackEvasion)
	{
		const FVector TextLoc = OtherLoc + FVector(0.f, 0.f, 300.f);
		DrawDebugString(GetWorld(), TextLoc, FString::Printf(TEXT("AoA: %.1f deg"), AoADeg), nullptr, FColor::White,
		                DeltaTime, true);
	}

	// Pick color & action based on classification and draw their motion arrow.
	EEvasionAction Result = EEvasionAction::MoveNormally;
	FColor ArrowCol = FColor::Green;

	if (bEnteringCone && bCriticalAoA)
	{
		Result = EEvasionAction::EvadeRight;
		ArrowCol = FColor::Black;
	}
	else if (bEnteringCone /*inside our cone by cosInCone check*/ && not bCriticalAoA)
	{
		Result = EEvasionAction::Wait;
		ArrowCol = FColor::Orange;
	}
	else
	{
		Result = EEvasionAction::MoveNormally;
		ArrowCol = FColor::Green;
	}

	if (TrackFollowingEvasion::bDebugTrackEvasion)
	{
		const FVector OtherTip = OtherLoc + OtherDir * TrackFollowingEvasion::DebugConeLength;
		DrawDebugDirectionalArrow(GetWorld(), OtherLoc, OtherTip, /*ArrowSize*/18.f, ArrowCol, false, DeltaTime, 0,
		                          2.f);
	}

	return Result;
}

bool UTrackPathFollowingComponent::ExecuteEvasiveAction(const EEvasionAction Action, const float DeltaTime)
{
	const float CurrentSpeed = ControlledPawn->GetVelocity().Size2D();
	// If we go backwards then the right is reversed
	const float RightSteeringDir = bReversing ? -1.f : 1.f;
	switch (Action)
	{
	case EEvasionAction::MoveNormally:
		// Not blocked by allies
		return false;
	case EEvasionAction::Wait:
		UpdateVehicle(/*Throttle*/0.f, CurrentSpeed, DeltaTime, /*Brake*/0.f, /*Steering*/
		                          M_LastSteeringInput);
		return true;
	case EEvasionAction::EvadeRight:
		// Move slower to the right.
		UpdateVehicle(M_LastThrottleInput * 0.66, CurrentSpeed, DeltaTime, /*Brake*/0.f,
		              RightSteeringDir);
		return true;
	}
	return false;
}
