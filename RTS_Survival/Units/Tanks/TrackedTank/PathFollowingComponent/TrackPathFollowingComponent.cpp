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
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/AI/AITrackTank.h"
#include "RTS_Survival/Units/Tanks/FRTSOverlapEvasion/RTSOverlapEvasionComponent.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/Utils/VehicleAIFunctionLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/Navigator/RTSNavigator.h"

namespace TrackFollowingEvasion
{
	/**
	 * @brief Vertical offset for normal overlap debug text. Increasing it raises all red/yellow/orange messages;
	 * decreasing it places them closer to the vehicle and can make them intersect the mesh.
	 */
	constexpr float ZOffsetDebug = 400.f;

	/**
	 * @brief Initial reserved capacity for simultaneously moving overlap contacts. Increasing it uses more memory per vehicle
	 * but avoids reallocations in unusually dense traffic; decreasing it saves memory but may allocate sooner.
	 */
	constexpr int32 ExpectedMovingOverlapCount = 4;

	/**
	 * @brief Initial reserved capacity for stationary idle contacts steered around in orange mode. Increasing it reduces
	 * reallocations in dense formations; decreasing it reduces reserved memory when idle overlaps are rare.
	 */
	constexpr int32 ExpectedIdleOverlapCount = 4;

	/**
	 * @brief Minimum direction dot product for two moving vehicles to use yellow following instead of orange avoidance.
	 * Increasing it requires more parallel routes; decreasing it classifies wider angular differences as following.
	 */
	constexpr float SameDirectionDotThreshold = 0.8f;

	/**
	 * @brief Multiplier applied to the leading vehicle's speed for the yellow follower speed cap. Increasing it lets followers
	 * stay closer to leader speed; decreasing it opens gaps faster but makes dependency queues progress more slowly.
	 */
	constexpr float SameDirectionLeaderSpeedScale = 0.85f;

	/**
	 * @brief Additional throttle multiplier used only by normal yellow following. Increasing it retains more engine input;
	 * decreasing it softens closing speed more aggressively. This does not affect orange steering avoidance.
	 */
	constexpr float SameDirectionThrottleScale = 0.85f;

	/**
	 * @brief Throttle retained during orange steering avoidance. Keep in [0, 1]: increasing it preserves more speed through
	 * avoidance turns, while decreasing it makes orange contacts slower and less forceful.
	 */
	constexpr float ConflictingDirectionThrottleScale = 1.33f;

	/**
	 * @brief Blend weight from normal path steering toward the latched orange passing side. Increasing it overrides the route
	 * more strongly and starts a larger avoidance turn sooner; decreasing it keeps closer to normal path steering.
	 * (was 0.85)
	 */
	constexpr float ConflictingDirectionSteeringOverrideStrength = 0.4f;

	/**
	 * @brief Minimum absolute steering input enforced during orange avoidance. Increasing it produces harder, wider lateral
	 * clearance manoeuvres; decreasing it allows gentler curves that may take longer to clear the overlap.
	 */
	constexpr float ConflictingDirectionMinimumSteeringMagnitude = 0.45f;

	/**
	 * @brief Minimum direction dot product for displacing an idle vehicle and waiting instead of steering around it.
	 * Increasing it narrows the forward obstruction cone and preserves more formations; decreasing it sends more idle
	 * vehicles away. Values near 1 reserve displacement for vehicles almost exactly in the driving direction.
	 */
	constexpr float IdleDisplacementDirectionDotThreshold = 0.9f;

	/**
	 * @brief Longitudinal tolerance used to identify side-by-side moving vehicles and ignore contacts already behind us.
	 * Increasing it uses the deterministic tie-break over a wider band; decreasing it assigns front/back responsibility
	 * sooner.
	 */
	constexpr float SideBySideDistanceTolerance = 5.f;

	/**
	 * @brief Lateral tolerance within which a new contact defaults to passing right instead of choosing the open side.
	 * Increasing it makes right-side defaults more common; decreasing it uses relative left/right placement more often.
	 */
	constexpr float CenteredPassingSideTolerance = 5.f;

	/**
	 * @brief Squared 2D vector length below which destination or velocity direction is considered unusable. Increasing it falls
	 * back to velocity/facing sooner; decreasing it trusts smaller, potentially noisy direction vectors.
	 */
	constexpr float MinimumTravelDirectionSquared = 1.f;

	/**
	 * @brief Maximum yellow dependency links inspected when searching for a cycle. Increasing it detects larger cycles at a
	 * slightly higher worst-case cost; decreasing it bounds work more tightly but can miss unusually large deadlocks.
	 */
	constexpr int32 DeadlockDependencyTraversalLimit = 32;

	/**
	 * @brief Extra height applied only to purple deadlock-resolution text. Increasing it separates purple text further from
	 * normal overlap messages; decreasing it keeps the messages closer together.
	 */
	constexpr float DeadlockDebugAdditionalZOffset = 100.f;

	/**
	 * @brief Lifetime in seconds for purple deadlock-resolution text. Increasing it makes rare resolutions easier to notice;
	 * decreasing it clears the message sooner and reduces persistent debug clutter.
	 */
	constexpr float DeadlockDebugLifetimeSeconds = 1.f;
}

namespace TrackFollowingOverlapCleanup
{
	constexpr int32 CleanupTickInterval = 256;
}

namespace TrackFollowingReverseDeadzone
{
	constexpr float ReverseDeadzoneDistanceBufferPercent = 0.15f;
}

int32 UTrackPathFollowingComponent::M_TicksCountCheckOverlappers = TrackFollowingOverlapCleanup::CleanupTickInterval;
float UTrackPathFollowingComponent::M_TimeTillDiscardOverlap = 3.f;


namespace TrackFollowingBlockDetection
{
	// Vehicles accelerate slowly and can be flagged as blocked too early with default thresholds.
	// These values control when UE decides a move is "blocked" and aborts the path request.
	// DistanceThresholdCentimeters:
	// - Minimum distance the agent must advance during a single block-detection interval.
	// - Increase this if vehicles should more aggressively give up when they barely move.
	// - Decrease this if slow heavy vehicles are incorrectly marked as blocked while still moving.
	// IntervalSeconds:
	// - How often UE samples position deltas for blocked detection.
	// - Increase this to give heavy vehicles more time to build momentum before being evaluated.
	// - Decrease this to make blocked detection react faster when responsiveness is more important.
	// SampleCount:
	// - Number of recent samples UE considers before declaring the move blocked.
	// - Increase this to require a longer "no progress" window (less false positives, slower abort).
	// - Decrease this to allow quicker aborts when the vehicle is truly stuck.
	constexpr float DefaultDistanceThresholdCentimeters = 150.f;
	constexpr float DistanceThresholdCentimeters_Min = 50.f;
	constexpr float RTSRadius_MinThreshold = 500.f;
	constexpr float DistanceThresholdCentimeters_Max = 200.f;
	constexpr float RTSRadius_MaxThreshold = 1000.f;
	constexpr float IntervalSeconds = 1.0f;
	constexpr int32 SampleCount = 16;
}

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
	BlockDetectionDistance = TrackFollowingBlockDetection::DefaultDistanceThresholdCentimeters;
	BlockDetectionInterval = TrackFollowingBlockDetection::IntervalSeconds;
	BlockDetectionSampleCount = TrackFollowingBlockDetection::SampleCount;
	M_MovingAlliedOverlapActors.Reserve(TrackFollowingEvasion::ExpectedMovingOverlapCount);
	M_IdleAlliedAvoidanceActors.Reserve(TrackFollowingEvasion::ExpectedIdleOverlapCount);
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

void UTrackPathFollowingComponent::RegisterMovingOverlappingActor(
	AActor* InActor,
	UTrackPathFollowingComponent* InPathFollowingComponent,
	const float ClearanceDistance)
{
	if (not IsValid(InActor) || not IsValid(InPathFollowingComponent) || not IsValid(ControlledPawn))
	{
		return;
	}

	// An ally that was idle when first touched may now be executing the evasion move we gave it.
	// Keep that contact in the stronger wait-until-clear flow instead of reclassifying its evasion as normal driving.
	if (ContainsOverlapActor(M_IdleAlliedBlockingActors, InActor))
	{
		return;
	}
	RemoveIdleSteeringAvoidanceActor(InActor);

	const float CurrentTimeSeconds = GetCurrentWorldTimeSeconds();
	for (FMovingAlliedOverlapData& OverlapData : M_MovingAlliedOverlapActors)
	{
		if (OverlapData.Actor.Get() != InActor)
		{
			continue;
		}

		OverlapData.PathFollowingComponent = InPathFollowingComponent;
		OverlapData.LastConfirmedCloseTimeSeconds = CurrentTimeSeconds;
		OverlapData.ClearanceDistanceSquared = FMath::Square(ClearanceDistance);
		return;
	}

	FMovingAlliedOverlapData NewOverlapData;
	NewOverlapData.Actor = InActor;
	NewOverlapData.PathFollowingComponent = InPathFollowingComponent;
	NewOverlapData.LastConfirmedCloseTimeSeconds = CurrentTimeSeconds;
	NewOverlapData.ClearanceDistanceSquared = FMath::Square(ClearanceDistance);
	NewOverlapData.PassingSide = CalculateOverlapPassingSide(InActor);
	NewOverlapData.bYieldWhenSideBySide = ControlledPawn->GetUniqueID() > InActor->GetUniqueID();
	M_MovingAlliedOverlapActors.Add(MoveTemp(NewOverlapData));
}

void UTrackPathFollowingComponent::RegisterIdleSteeringAvoidanceActor(
	AActor* InActor,
	const float ClearanceDistance)
{
	if (not IsValid(InActor) || not IsValid(ControlledPawn))
	{
		return;
	}
	if (ContainsOverlapActor(M_IdleAlliedBlockingActors, InActor))
	{
		return;
	}

	RemoveMovingOverlapActor(InActor);
	const float CurrentTimeSeconds = GetCurrentWorldTimeSeconds();
	for (FIdleAlliedOverlapAvoidanceData& OverlapData : M_IdleAlliedAvoidanceActors)
	{
		if (OverlapData.Actor.Get() != InActor)
		{
			continue;
		}

		OverlapData.LastConfirmedCloseTimeSeconds = CurrentTimeSeconds;
		OverlapData.ClearanceDistanceSquared = FMath::Square(ClearanceDistance);
		return;
	}

	FIdleAlliedOverlapAvoidanceData NewOverlapData;
	NewOverlapData.Actor = InActor;
	NewOverlapData.LastConfirmedCloseTimeSeconds = CurrentTimeSeconds;
	NewOverlapData.ClearanceDistanceSquared = FMath::Square(ClearanceDistance);
	NewOverlapData.PassingSide = CalculateOverlapPassingSide(InActor);
	M_IdleAlliedAvoidanceActors.Add(MoveTemp(NewOverlapData));
}

EIdleOverlapResponse UTrackPathFollowingComponent::GetIdleOverlapResponse(const AActor* InActor) const
{
	if (not IsValid(InActor) || not IsValid(ControlledPawn) || not bM_HasCurrentDrivingDestination ||
		GetStatus() != EPathFollowingStatus::Moving)
	{
		return EIdleOverlapResponse::SteerAround;
	}

	FVector TravelDirection = VehicleCurrentDestination - ControlledPawn->GetActorLocation();
	TravelDirection.Z = 0.f;
	if (TravelDirection.SizeSquared2D() <= TrackFollowingEvasion::MinimumTravelDirectionSquared)
	{
		TravelDirection = ControlledPawn->GetVelocity();
		TravelDirection.Z = 0.f;
	}
	if (TravelDirection.SizeSquared2D() <= TrackFollowingEvasion::MinimumTravelDirectionSquared)
	{
		return EIdleOverlapResponse::SteerAround;
	}

	FVector DirectionToIdleActor = InActor->GetActorLocation() - ControlledPawn->GetActorLocation();
	DirectionToIdleActor.Z = 0.f;
	if (DirectionToIdleActor.SizeSquared2D() <= TrackFollowingEvasion::MinimumTravelDirectionSquared)
	{
		return EIdleOverlapResponse::DisplaceAndWait;
	}

	const float DirectionDot = FVector::DotProduct(
		TravelDirection.GetSafeNormal2D(),
		DirectionToIdleActor.GetSafeNormal2D());
	return DirectionDot >= TrackFollowingEvasion::IdleDisplacementDirectionDotThreshold
		       ? EIdleOverlapResponse::DisplaceAndWait
		       : EIdleOverlapResponse::SteerAround;
}

void UTrackPathFollowingComponent::SetRTSOverlapEvasionComponent(
	URTSOverlapEvasionComponent* InOverlapEvasionComponent)
{
	if (not IsValid(InOverlapEvasionComponent))
	{
		return;
	}
	M_RTSOverlapEvasionComponent = InOverlapEvasionComponent;
}

bool UTrackPathFollowingComponent::GetIsValidRTSOverlapEvasionComponent() const
{
	if (M_RTSOverlapEvasionComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_RTSOverlapEvasionComponent"),
		TEXT("UTrackPathFollowingComponent::GetIsValidRTSOverlapEvasionComponent"),
		this);
	return false;
}

EOverlapPassingSide UTrackPathFollowingComponent::CalculateOverlapPassingSide(const AActor* InActor) const
{
	if (not IsValid(InActor) || not IsValid(ControlledPawn))
	{
		return EOverlapPassingSide::Right;
	}

	const FVector RelativeLocation = InActor->GetActorLocation() - ControlledPawn->GetActorLocation();
	const float OtherRightDistance = FVector::DotProduct(RelativeLocation, ControlledPawn->GetActorRightVector());
	const bool bOtherIsCentered = FMath::Abs(OtherRightDistance) <=
		TrackFollowingEvasion::CenteredPassingSideTolerance;
	return bOtherIsCentered || OtherRightDistance < 0.f
		       ? EOverlapPassingSide::Right
		       : EOverlapPassingSide::Left;
}

TArray<FOverlapActorData>& UTrackPathFollowingComponent::GetOverlapBlockingActorsArray()
{
	return M_IdleAlliedBlockingActors;
}

void UTrackPathFollowingComponent::UpdateBlockDetectionDistanceFromRTSRadius(const float RTSRadius)
{
	using namespace TrackFollowingBlockDetection;
	// If thresholds are misconfigured (avoid divide-by-zero / inverted ranges), fall back safely.
	if (RTSRadius_MaxThreshold <= RTSRadius_MinThreshold)
	{
		BlockDetectionDistance = DistanceThresholdCentimeters_Min;
		return;
	}

	// Map RTSRadius from [MinThreshold .. MaxThreshold] -> [MinDistance .. MaxDistance], clamped.
	const float RadiusAlpha01 =
		FMath::Clamp((RTSRadius - RTSRadius_MinThreshold) / (RTSRadius_MaxThreshold - RTSRadius_MinThreshold), 0.0f,
		             1.0f);

	BlockDetectionDistance =
		FMath::Lerp(DistanceThresholdCentimeters_Min, DistanceThresholdCentimeters_Max, RadiusAlpha01);
}

float UTrackPathFollowingComponent::GetStuckDetectionDist() const
{
	return BlockDetectionDistance;
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

void UTrackPathFollowingComponent::RemoveMovingOverlapActor(AActor* InActor)
{
	if (not IsValid(InActor))
	{
		return;
	}

	for (int32 Index = M_MovingAlliedOverlapActors.Num() - 1; Index >= 0; --Index)
	{
		if (M_MovingAlliedOverlapActors[Index].Actor.Get() == InActor)
		{
			if (M_MovingOverlapYieldDependency.Get() ==
				M_MovingAlliedOverlapActors[Index].PathFollowingComponent.Get())
			{
				ResetMovingOverlapYieldDependency();
			}
			M_MovingAlliedOverlapActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

void UTrackPathFollowingComponent::RemoveIdleSteeringAvoidanceActor(AActor* InActor)
{
	if (not IsValid(InActor))
	{
		return;
	}

	for (int32 Index = M_IdleAlliedAvoidanceActors.Num() - 1; Index >= 0; --Index)
	{
		if (M_IdleAlliedAvoidanceActors[Index].Actor.Get() == InActor)
		{
			M_IdleAlliedAvoidanceActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
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

void UTrackPathFollowingComponent::RemoveStaleMovingOverlaps(const float CurrentTimeSeconds)
{
	if (not IsValid(ControlledPawn))
	{
		M_MovingAlliedOverlapActors.Reset();
		ResetMovingOverlapYieldDependency();
		return;
	}

	const FVector OwnerLocation = ControlledPawn->GetActorLocation();
	for (int32 Index = M_MovingAlliedOverlapActors.Num() - 1; Index >= 0; --Index)
	{
		FMovingAlliedOverlapData& OverlapData = M_MovingAlliedOverlapActors[Index];
		AActor* const OtherActor = OverlapData.Actor.Get();
		if (not IsValid(OtherActor) || not OverlapData.PathFollowingComponent.IsValid())
		{
			if (M_MovingOverlapYieldDependency == OverlapData.PathFollowingComponent)
			{
				ResetMovingOverlapYieldDependency();
			}
			M_MovingAlliedOverlapActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(OwnerLocation, OtherActor->GetActorLocation());
		if (DistanceSquared <= OverlapData.ClearanceDistanceSquared)
		{
			OverlapData.LastConfirmedCloseTimeSeconds = CurrentTimeSeconds;
			continue;
		}

		const float TimeOutsideClearance = CurrentTimeSeconds - OverlapData.LastConfirmedCloseTimeSeconds;
		if (TimeOutsideClearance < M_TimeTillDiscardOverlap)
		{
			continue;
		}

		FOverlapActorData DebugOverlapData;
		DebugOverlapData.Actor = OverlapData.Actor;
		DebugOverlapData.TimeRegisteredSeconds = OverlapData.LastConfirmedCloseTimeSeconds;
		DebugRemovedOverlapActor(TEXT("Removing stale moving overlap"), DebugOverlapData);
		if (M_MovingOverlapYieldDependency == OverlapData.PathFollowingComponent)
		{
			ResetMovingOverlapYieldDependency();
		}
		M_MovingAlliedOverlapActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}
}

void UTrackPathFollowingComponent::RemoveStaleIdleAvoidanceOverlaps(const float CurrentTimeSeconds)
{
	if (not IsValid(ControlledPawn))
	{
		M_IdleAlliedAvoidanceActors.Reset();
		return;
	}

	const FVector OwnerLocation = ControlledPawn->GetActorLocation();
	for (int32 Index = M_IdleAlliedAvoidanceActors.Num() - 1; Index >= 0; --Index)
	{
		FIdleAlliedOverlapAvoidanceData& OverlapData = M_IdleAlliedAvoidanceActors[Index];
		AActor* const OtherActor = OverlapData.Actor.Get();
		if (not IsValid(OtherActor))
		{
			M_IdleAlliedAvoidanceActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(OwnerLocation, OtherActor->GetActorLocation());
		if (DistanceSquared <= OverlapData.ClearanceDistanceSquared)
		{
			OverlapData.LastConfirmedCloseTimeSeconds = CurrentTimeSeconds;
			continue;
		}

		const float TimeOutsideClearance = CurrentTimeSeconds - OverlapData.LastConfirmedCloseTimeSeconds;
		if (TimeOutsideClearance < M_TimeTillDiscardOverlap)
		{
			continue;
		}

		FOverlapActorData DebugOverlapData;
		DebugOverlapData.Actor = OverlapData.Actor;
		DebugOverlapData.TimeRegisteredSeconds = OverlapData.LastConfirmedCloseTimeSeconds;
		DebugRemovedOverlapActor(TEXT("Removing stale idle avoidance overlap"), DebugOverlapData);
		M_IdleAlliedAvoidanceActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}
}

void UTrackPathFollowingComponent::ResetOverlapBlockingActors()
{
	M_IdleAlliedBlockingActors.Reset();
}

void UTrackPathFollowingComponent::ResetOverlapBlockingActorsForCommand()
{
	ResetOverlapBlockingActors();
	M_TicksCountCheckOverlappers = TrackFollowingOverlapCleanup::CleanupTickInterval;
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(TEXT("Cleared idle overlap blockers for new move command."), FColor::Emerald);
	}
}

void UTrackPathFollowingComponent::RegisterIdleBlockingActor(AActor* InActor)
{
	if (not IsValid(InActor))
	{
		return;
	}
	RemoveMovingOverlapActor(InActor);
	RemoveIdleSteeringAvoidanceActor(InActor);
	AddOverlapActorData(M_IdleAlliedBlockingActors, InActor);
}


void UTrackPathFollowingComponent::DeregisterOverlapBlockingActor(AActor* InActor)
{
	// This actor is no longer overlapping with us and can be ignored.
	if (not IsValid(InActor))
	{
		return;
	}
	RemoveOverlapActorFromArray(M_IdleAlliedBlockingActors, InActor);
	RemoveMovingOverlapActor(InActor);
	RemoveIdleSteeringAvoidanceActor(InActor);
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
	for (const FMovingAlliedOverlapData& OverlapData : M_MovingAlliedOverlapActors)
	{
		if (OverlapData.Actor.IsValid() && OverlapData.PathFollowingComponent.IsValid())
		{
			return true;
		}
	}
	for (const FIdleAlliedOverlapAvoidanceData& OverlapData : M_IdleAlliedAvoidanceActors)
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
	if (not bWantsReverse)
	{
		bM_ReverseToPositionInDeadzone = false;
	}
}

bool UTrackPathFollowingComponent::IsReversing()
{
	return bReversing;
}


APawn* UTrackPathFollowingComponent::GetValidControlledPawn()
{
	if (not IsValid(ControlledPawn))
	{
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

	return ControlledPawn;
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
	RemoveStaleMovingOverlaps(CurrentTimeSeconds);
	RemoveStaleIdleAvoidanceOverlaps(CurrentTimeSeconds);
}


void UTrackPathFollowingComponent::OnPathUpdated()
{
	Super::OnPathUpdated();

	bM_HasCurrentDrivingDestination = false;
	ResetMovingOverlapYieldDependency();
	// Reset stuck status
	ResetStuck();
	bM_IsInDeadzone = false;
	bM_IsInReverseDeadzone = false;
	UpdateReverseDeadzoneStateForNewPath();
}

bool UTrackPathFollowingComponent::CheckOverlapIdleAllies(const float DeltaTime)
{
	RemoveInvalidOverlaps(M_IdleAlliedBlockingActors);

	const bool bHasIdleBlockers = (M_IdleAlliedBlockingActors.Num() > 0);
	const bool bCanWait = bHasIdleBlockers && IsValid(ControlledPawn);

	if (not bCanWait)
	{
		return false;
	}

	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		DebugWaitingForIdleOverlappingActors(DeltaTime);
	}

	// Keep steering as before, set throttle to zero while we’re overlapping.
	const float CurrentSpeed = ControlledPawn->GetVelocity().Size2D();
	UpdateVehicle(/*Throttle*/0.f, /*CurrentSpeed*/CurrentSpeed, DeltaTime, /*Brake*/0.f, /*Steering*/
	                          M_LastSteeringInput);
	return true;
}

FVector UTrackPathFollowingComponent::GetMovingOverlapTravelDirection(
	const AActor* VehicleActor,
	const UTrackPathFollowingComponent* VehiclePathFollowingComponent) const
{
	if (not IsValid(VehicleActor) || not IsValid(VehiclePathFollowingComponent))
	{
		return FVector::ZeroVector;
	}

	FVector TravelDirection = VehiclePathFollowingComponent->GetCurrentDrivingDestination() -
		VehicleActor->GetActorLocation();
	TravelDirection.Z = 0.f;
	if (TravelDirection.SizeSquared2D() <= TrackFollowingEvasion::MinimumTravelDirectionSquared)
	{
		TravelDirection = VehicleActor->GetVelocity();
		TravelDirection.Z = 0.f;
	}
	if (TravelDirection.SizeSquared2D() <= TrackFollowingEvasion::MinimumTravelDirectionSquared)
	{
		TravelDirection = VehicleActor->GetActorForwardVector();
		TravelDirection.Z = 0.f;
	}

	return TravelDirection.GetSafeNormal2D();
}

void UTrackPathFollowingComponent::TryEscalateIdleAvoidanceContacts()
{
	if (M_IdleAlliedAvoidanceActors.IsEmpty() || not GetIsValidRTSOverlapEvasionComponent())
	{
		return;
	}

	AActor* IdleActorToDisplace = nullptr;
	for (int32 Index = M_IdleAlliedAvoidanceActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* const OtherActor = M_IdleAlliedAvoidanceActors[Index].Actor.Get();
		if (not IsValid(OtherActor))
		{
			M_IdleAlliedAvoidanceActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		if (GetIdleOverlapResponse(OtherActor) != EIdleOverlapResponse::DisplaceAndWait)
		{
			continue;
		}

		IdleActorToDisplace = OtherActor;
		break;
	}

	if (not IsValid(IdleActorToDisplace) ||
		not M_RTSOverlapEvasionComponent->TryDisplaceIdleOverlappingVehicle(IdleActorToDisplace))
	{
		return;
	}

	RemoveIdleSteeringAvoidanceActor(IdleActorToDisplace);
	AddOverlapActorData(M_IdleAlliedBlockingActors, IdleActorToDisplace);
}

void UTrackPathFollowingComponent::ApplySteeringAvoidanceContact(
	const FVector& RelativeLocation,
	const EOverlapPassingSide PassingSide,
	AActor* OtherActor,
	UTrackPathFollowingComponent* OtherPathFollowingComponent,
	const EOverlapAdjustmentReason Reason,
	float& ClosestDistanceSquared,
	FOverlapAdjustment& Adjustment) const
{
	Adjustment.ThrottleScale = FMath::Min(
		Adjustment.ThrottleScale,
		TrackFollowingEvasion::ConflictingDirectionThrottleScale);
	Adjustment.bIsLimitingMovement = true;

	const float DistanceSquared = RelativeLocation.SizeSquared2D();
	if (DistanceSquared >= ClosestDistanceSquared)
	{
		return;
	}

	ClosestDistanceSquared = DistanceSquared;
	Adjustment.SteeringDirection = PassingSide == EOverlapPassingSide::Right ? 1.f : -1.f;
	Adjustment.bShouldAdjustSteering = true;
	Adjustment.Reason = Reason;
	Adjustment.PrimaryActor = OtherActor;
	Adjustment.PrimaryPathFollowingComponent = OtherPathFollowingComponent;
}

FOverlapAdjustment UTrackPathFollowingComponent::CalculateOverlapAdjustment(const FVector& Destination)
{
	FOverlapAdjustment Adjustment;
	if ((M_MovingAlliedOverlapActors.IsEmpty() && M_IdleAlliedAvoidanceActors.IsEmpty()) ||
		not IsValid(ControlledPawn))
	{
		ResetMovingOverlapYieldDependency();
		return Adjustment;
	}

	const FVector OwnerLocation = ControlledPawn->GetActorLocation();
	FVector OwnerTravelDirection = Destination - OwnerLocation;
	OwnerTravelDirection.Z = 0.f;
	if (OwnerTravelDirection.SizeSquared2D() <= TrackFollowingEvasion::MinimumTravelDirectionSquared)
	{
		OwnerTravelDirection = ControlledPawn->GetVelocity();
		OwnerTravelDirection.Z = 0.f;
	}
	if (OwnerTravelDirection.SizeSquared2D() <= TrackFollowingEvasion::MinimumTravelDirectionSquared)
	{
		OwnerTravelDirection = ControlledPawn->GetActorForwardVector();
		OwnerTravelDirection.Z = 0.f;
	}
	OwnerTravelDirection = OwnerTravelDirection.GetSafeNormal2D();

	float ClosestConflictingDistanceSquared = MAX_flt;
	for (int32 Index = M_MovingAlliedOverlapActors.Num() - 1; Index >= 0; --Index)
	{
		const FMovingAlliedOverlapData& OverlapData = M_MovingAlliedOverlapActors[Index];
		AActor* const OtherActor = OverlapData.Actor.Get();
		UTrackPathFollowingComponent* const OtherPathFollowingComponent =
			OverlapData.PathFollowingComponent.Get();
		if (not IsValid(OtherActor) || not IsValid(OtherPathFollowingComponent))
		{
			M_MovingAlliedOverlapActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		FVector RelativeLocation = OtherActor->GetActorLocation() - OwnerLocation;
		RelativeLocation.Z = 0.f;
		const float LongitudinalDistance = FVector::DotProduct(RelativeLocation, OwnerTravelDirection);
		const FVector OtherTravelDirection = GetMovingOverlapTravelDirection(
			OtherActor,
			OtherPathFollowingComponent);
		const float DirectionDot = FVector::DotProduct(OwnerTravelDirection, OtherTravelDirection);

		if (DirectionDot >= TrackFollowingEvasion::SameDirectionDotThreshold)
		{
			const bool bIsSideBySide = FMath::Abs(LongitudinalDistance) <=
				TrackFollowingEvasion::SideBySideDistanceTolerance;
			const bool bShouldYield = LongitudinalDistance > TrackFollowingEvasion::SideBySideDistanceTolerance ||
				(bIsSideBySide && OverlapData.bYieldWhenSideBySide);
			if (not bShouldYield)
			{
				continue;
			}

			const float LeaderSpeed = OtherActor->GetVelocity().Size2D();
			const float LeaderSpeedCap = LeaderSpeed * TrackFollowingEvasion::SameDirectionLeaderSpeedScale;
			const bool bIsStrongestSameDirectionLimit = LeaderSpeedCap < Adjustment.DesiredSpeedCap;
			Adjustment.DesiredSpeedCap = FMath::Min(Adjustment.DesiredSpeedCap, LeaderSpeedCap);
			Adjustment.ThrottleScale = FMath::Min(
				Adjustment.ThrottleScale,
				TrackFollowingEvasion::SameDirectionThrottleScale);
			Adjustment.bIsLimitingMovement = true;
			Adjustment.bHasSameDirectionSpeedLimit = true;
			if (bIsStrongestSameDirectionLimit &&
				Adjustment.Reason != EOverlapAdjustmentReason::AvoidingConflictingDirection &&
				Adjustment.Reason != EOverlapAdjustmentReason::AvoidingIdleVehicle)
			{
				Adjustment.Reason = EOverlapAdjustmentReason::FollowingSameDirection;
				Adjustment.PrimaryActor = OtherActor;
				Adjustment.PrimaryPathFollowingComponent = OtherPathFollowingComponent;
			}
			continue;
		}

		if (LongitudinalDistance < -TrackFollowingEvasion::SideBySideDistanceTolerance)
		{
			// The other vehicle is behind us and is responsible for avoiding this contact.
			continue;
		}

		ApplySteeringAvoidanceContact(
			RelativeLocation,
			OverlapData.PassingSide,
			OtherActor,
			OtherPathFollowingComponent,
			EOverlapAdjustmentReason::AvoidingConflictingDirection,
			ClosestConflictingDistanceSquared,
			Adjustment);
	}

	for (int32 Index = M_IdleAlliedAvoidanceActors.Num() - 1; Index >= 0; --Index)
	{
		const FIdleAlliedOverlapAvoidanceData& OverlapData = M_IdleAlliedAvoidanceActors[Index];
		AActor* const OtherActor = OverlapData.Actor.Get();
		if (not IsValid(OtherActor))
		{
			M_IdleAlliedAvoidanceActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		FVector RelativeLocation = OtherActor->GetActorLocation() - OwnerLocation;
		RelativeLocation.Z = 0.f;
		const float LongitudinalDistance = FVector::DotProduct(RelativeLocation, OwnerTravelDirection);
		if (LongitudinalDistance < -TrackFollowingEvasion::SideBySideDistanceTolerance)
		{
			continue;
		}

		ApplySteeringAvoidanceContact(
			RelativeLocation,
			OverlapData.PassingSide,
			OtherActor,
			nullptr,
			EOverlapAdjustmentReason::AvoidingIdleVehicle,
			ClosestConflictingDistanceSquared,
			Adjustment);
	}

	ResolveMovingOverlapDeadlock(Adjustment);
	return Adjustment;
}

void UTrackPathFollowingComponent::ResolveMovingOverlapDeadlock(FOverlapAdjustment& Adjustment)
{
	ResetMovingOverlapYieldDependency();
	if (Adjustment.Reason != EOverlapAdjustmentReason::FollowingSameDirection ||
		not IsValid(ControlledPawn))
	{
		return;
	}

	UTrackPathFollowingComponent* Dependency = Adjustment.PrimaryPathFollowingComponent.Get();
	if (not IsValid(Dependency))
	{
		return;
	}
	M_MovingOverlapYieldDependency = Dependency;

	const uint32 OwnerPriority = ControlledPawn->GetUniqueID();
	uint32 WinningPriority = OwnerPriority;
	for (int32 DependencyDepth = 0;
		 DependencyDepth < TrackFollowingEvasion::DeadlockDependencyTraversalLimit;
		 ++DependencyDepth)
	{
		if (Dependency == this)
		{
			if (OwnerPriority == WinningPriority)
			{
				Adjustment.DesiredSpeedCap = MAX_flt;
				Adjustment.ThrottleScale = 1.f;
				Adjustment.bIsLimitingMovement = false;
				Adjustment.bHasSameDirectionSpeedLimit = false;
				Adjustment.Reason = EOverlapAdjustmentReason::ResolvingDeadlock;
				ResetMovingOverlapYieldDependency();
			}
			return;
		}

		if (not IsValid(Dependency) || not IsValid(Dependency->ControlledPawn))
		{
			return;
		}

		WinningPriority = FMath::Min(WinningPriority, Dependency->ControlledPawn->GetUniqueID());
		Dependency = Dependency->M_MovingOverlapYieldDependency.Get();
	}
}

void UTrackPathFollowingComponent::ResetMovingOverlapYieldDependency()
{
	M_MovingOverlapYieldDependency.Reset();
}


void UTrackPathFollowingComponent::UpdateDriving(FVector Destination, float DeltaTime)
{
	// .65 ms for 100 vehicles.
	// TRACE_CPUPROFILER_EVENT_SCOPE(Tracks_UpdateDriving);
	if (not M_TrackPhysicsMovement)
	{
		bM_HasCurrentDrivingDestination = false;
		ResetMovingOverlapYieldDependency();
		return;
	}

	if (not bImplementsInterface || not IsValid(ControlledPawn))
	{
		bM_HasCurrentDrivingDestination = false;
		ResetMovingOverlapYieldDependency();
		// Make sure we never "stick" in suppressed mode if something becomes invalid.
		UpdateEngineBlockDetectionForOverlapResponse(false);
		return;
	}
	VehicleCurrentDestination = Destination;
	bM_HasCurrentDrivingDestination = true;

	TryEscalateIdleAvoidanceContacts();

	// Pause throttle while overlapping idle allies being pushed aside; keep last steering.
	// Disables engine block detection while waiting.
	if (CheckOverlapIdleAllies(DeltaTime))
	{
		ResetMovingOverlapYieldDependency();
		UpdateEngineBlockDetectionForOverlapResponse(true);
		return;
	}
	// [-180 , +180]
	const float SignedTargetAngle = CalculateDestinationAngle(Destination);
	AbsoluteTargetAngle = FMath::Abs(SignedTargetAngle);

	M_CurrentSpeed = ControlledPawn->GetVelocity().Size2D();

	if (bM_ReverseToPositionInDeadzone && bWantsReverse)
	{
		const FVector AgentLocation = GetAgentLocation();
		const FVector ReverseFacingDestination = AgentLocation - (Destination - AgentLocation);
		const float SignedReverseFacingAngle = CalculateDestinationAngle(ReverseFacingDestination);
		const float AbsoluteReverseFacingAngle = FMath::Abs(SignedReverseFacingAngle);

		if (AbsoluteReverseFacingAngle <= DeadZoneExitAngleOffset)
		{
			bM_ReverseToPositionInDeadzone = false;
		}
		else
		{
			ResetMovingOverlapYieldDependency();
			UpdateEngineBlockDetectionForOverlapResponse(false);
			const float SteeringScale = AbsoluteReverseFacingAngle > MaxAngleDontSlow
				                            ? 1.f
				                            : AbsoluteReverseFacingAngle / MaxAngleDontSlow;
			const float Steering = SteeringScale * FMath::Sign(SignedReverseFacingAngle);
			M_LastSteeringInput = Steering;
			M_LastThrottleInput = 0.f;
			UpdateVehicle(/*Throttle*/0.f, /*CurrentSpeed*/M_CurrentSpeed, DeltaTime, /*Brake*/0.f, /*Steering*/
			                          Steering);
			return;
		}
	}

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
	// Needs TargetAngle to be in absolute value.
	float ThrottleIncreaseValue;
	bReversing = ShouldTrackVehicleReverse(AbsoluteTargetAngle, DestinationDistance);
	const FOverlapAdjustment OverlapAdjustment =
		M_MovingAlliedOverlapActors.IsEmpty() && M_IdleAlliedAvoidanceActors.IsEmpty()
			? FOverlapAdjustment()
			: CalculateOverlapAdjustment(Destination);
	const bool bHasSameDirectionSpeedLimit = OverlapAdjustment.bHasSameDirectionSpeedLimit;
	if (bReversing)
	{
		const float AbsoluteTurnAngle = AbsoluteTargetAngle > 90 ? 180 - AbsoluteTargetAngle : AbsoluteTargetAngle;
		// Steer maximally if the angle is greater than the threshold, otherwise steer based on the angle.
		Steering = AbsoluteTurnAngle > MaxAngleDontSlow ? 1 : AbsoluteTurnAngle / MaxAngleDontSlow;
		Steering *= -FMath::Sign(SignedTargetAngle);
		// Adjust speed difference with wanted reverse speed.
		const float OverlapLimitedReverseSpeed = FMath::Min(
			DesiredReverseSpeed,
			OverlapAdjustment.DesiredSpeedCap);
		SpeedDifference = FMath::GetMappedRangeValueClamped(
			FVector2D(-ReverseSpeedPIDThreshold, ReverseSpeedPIDThreshold), FVector2D(-1.f, 1.f),
			OverlapLimitedReverseSpeed - M_CurrentSpeed);
		ThrottleIncreaseValue = M_ReverseThrottleIncreaseForDesiredSpeed;
		// At the timer check current speed vs max reverse speed.
		if (not bHasSameDirectionSpeedLimit && M_CurrentSpeed < DesiredReverseSpeed)
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
		const float OverlapLimitedDesiredSpeed = FMath::Min(
			AdjustedDesiredSpeed,
			OverlapAdjustment.DesiredSpeedCap);
		SpeedDifference = FMath::GetMappedRangeValueClamped(
			FVector2D(-DesiredSpeedThrottleThreshold, DesiredSpeedThrottleThreshold), FVector2D(-1.f, 1.f),
			OverlapLimitedDesiredSpeed - M_CurrentSpeed);
		ThrottleIncreaseValue = M_ThrottleIncreaseForDesiredSpeed;
		// At the timer check the current speed vs the max forward speed.
		if (not bHasSameDirectionSpeedLimit && AdjustedDesiredSpeed == DesiredSpeed &&
			M_CurrentSpeed < DesiredSpeed)
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

	const bool bIsInDeadzone = UpdateDeadzoneHysteresis(AbsoluteTargetAngle, DestinationDistance, bReversing);
	if (bIsInDeadzone)
	{
		CalculatedThrottleValue = 0.f;

		// Max steering.
		Steering = FMath::Sign(SignedTargetAngle);
		if (bDebugSlowDown && not bM_IsInReverseDeadzone)
		{
			RTSFunctionLibrary::PrintString("Frontal deadzone, only steering!", FColor::Black);
			DrawDebugLine(GetWorld(), GetAgentLocation(), Destination, FColor::Black, false, 0.1f, 1, 3.0f);
		}
		if (bDebugSlowDown && bM_IsInReverseDeadzone)
		{
			RTSFunctionLibrary::PrintString("Reverse deadzone, only steering!", FColor::Black);
			DrawDebugLine(GetWorld(), GetAgentLocation(), Destination, FColor::White, false, 0.1f, 1, 3.0f);
		}
	}
	if (OverlapAdjustment.bShouldAdjustSteering)
	{
		const float AvoidanceSteeringDirection = bReversing
			                                         ? -OverlapAdjustment.SteeringDirection
			                                         : OverlapAdjustment.SteeringDirection;
		const float BlendedAvoidanceSteering = FMath::Lerp(
			Steering,
			AvoidanceSteeringDirection,
			TrackFollowingEvasion::ConflictingDirectionSteeringOverrideStrength);
		const float SteeringAlongAvoidanceDirection =
			BlendedAvoidanceSteering * AvoidanceSteeringDirection;
		const float AvoidanceSteeringMagnitude = FMath::Max(
			SteeringAlongAvoidanceDirection,
			TrackFollowingEvasion::ConflictingDirectionMinimumSteeringMagnitude);
		Steering = FMath::Clamp(
			AvoidanceSteeringDirection * AvoidanceSteeringMagnitude,
			-1.f,
			1.f);
	}
	CalculatedThrottleValue *= OverlapAdjustment.ThrottleScale;
	if (bHasSameDirectionSpeedLimit)
	{
		M_TimeWantDesiredSpeed = 0.f;
	}
	UpdateEngineBlockDetectionForOverlapResponse(OverlapAdjustment.bIsLimitingMovement);

	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		DebugOverlapAdjustment(
			OverlapAdjustment,
			CalculatedThrottleValue,
			Steering,
			DeltaTime);
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
			// Add acceptance radius (current)
			InWorldDebug += "Acceptance Radius: " + FString::SanitizeFloat(AcceptanceRadius) + "\n";
			InWorldDebug += "Goal Radius: " + FString::SanitizeFloat(VehicleGoalAcceptanceRadius) + "\n";
			// Add steering:
			InWorldDebug += "Steering Input: " + FString::SanitizeFloat(M_LastSteeringInput) + "\n";
			// Add throttle:
			InWorldDebug += "Throttle Input: " + FString::SanitizeFloat(M_LastThrottleInput) + "\n";

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

bool UTrackPathFollowingComponent::UpdateDeadzoneHysteresis(
	const float TargetAngle,
	const float TargetDistance,
	const bool bIsReversing)
{
	const float DeadzoneExitAngle = FMath::Max(0.f, DeadZoneAngle - DeadZoneExitAngleOffset);
	const float DeadzoneExitDistance = DeadZoneDistance * FMath::Max(DeadZoneExitDistanceMultiplier, 1.f);

	if (bM_IsInDeadzone && bM_IsInReverseDeadzone != bIsReversing)
	{
		bM_IsInDeadzone = false;
		bM_IsInReverseDeadzone = false;
	}

	if (bM_IsInDeadzone)
	{
		const float ReverseExitAngle = 180.f - DeadzoneExitAngle;
		const bool bShouldExitForwardDeadzone = not bM_IsInReverseDeadzone &&
			(TargetDistance > DeadzoneExitDistance || TargetAngle < DeadzoneExitAngle);
		const bool bShouldExitReverseDeadzone = bM_IsInReverseDeadzone &&
			(TargetDistance > DeadzoneExitDistance || TargetAngle > ReverseExitAngle);

		if (bShouldExitForwardDeadzone || bShouldExitReverseDeadzone)
		{
			bM_IsInDeadzone = false;
			bM_IsInReverseDeadzone = false;
		}

		return bM_IsInDeadzone;
	}

	const bool bShouldEnterForwardDeadzone = not bIsReversing &&
		TargetDistance < DeadZoneDistance &&
		TargetAngle >= DeadZoneAngle;
	const bool bShouldEnterReverseDeadzone = bIsReversing &&
		TargetDistance < DeadZoneDistance &&
		TargetAngle <= (180.f - DeadZoneAngle);

	if (bShouldEnterForwardDeadzone)
	{
		bM_IsInDeadzone = true;
		bM_IsInReverseDeadzone = false;
		return true;
	}

	if (bShouldEnterReverseDeadzone)
	{
		bM_IsInDeadzone = true;
		bM_IsInReverseDeadzone = true;
		return true;
	}

	return false;
}

void UTrackPathFollowingComponent::UpdateReverseDeadzoneStateForNewPath()
{
	bM_ReverseToPositionInDeadzone = false;
	if (not bWantsReverse)
	{
		return;
	}

	const APawn* const PawnOwner = GetValidControlledPawn();
	if (not IsValid(PawnOwner))
	{
		return;
	}

	if (not Path.IsValid())
	{
		return;
	}

	const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();
	if (PathPoints.IsEmpty())
	{
		return;
	}

	const float ReverseDeadzoneRange = DeadZoneDistance *
		(1.0f + TrackFollowingReverseDeadzone::ReverseDeadzoneDistanceBufferPercent);
	const float DistanceToDestination = FVector::Dist(GetAgentLocation(), PathPoints.Last().Location);
	if (DistanceToDestination > ReverseDeadzoneRange)
	{
		return;
	}

	bM_ReverseToPositionInDeadzone = true;
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
		DrawDebugSphere(GetWorld(), Destination, 50.f, 12, FColor::Green, false, DeltaTime);
		// Draw debug sphere at unstuck destination
		DrawDebugSphere(GetWorld(), UnStuckDestination, 50.f, 12, FColor::Red, false, DeltaTime);
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
					RTSFunctionLibrary::DrawDebugAtLocation(this, GetAgentLocation() + FVector(0, 0, 300),
					                                        "Vehicle stuck while navigating to unstuck location!"
					                                        "\n" + FString::SanitizeFloat(GetWorld()->GetTimeSeconds()),
					                                        FColor::Red, 3);
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
				const FVector DebugLocation = GetAgentLocation() + FVector(0, 0, 200);
				RTSFunctionLibrary::DrawDebugAtLocation(this,
				                                        DebugLocation,
				                                        "Distance left: " + FString::SanitizeFloat(DistanceLeft),
				                                        FColor::White,
				                                        0.167

				);
				RTSFunctionLibrary::DrawDebugAtLocation(this,
				                                        DebugLocation,
				                                        "Acceptance Radius: " +
				                                        FString::SanitizeFloat(AcceptanceRadius),
				                                        FColor::Green,
				                                        0.167
				);
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
	bM_HasCurrentDrivingDestination = false;
	ResetMovingOverlapYieldDependency();
	SetEngineBlockDetectionSuppressed(false);
	// // When the path ends, set all the steering and throttle to nothing, and apply the brakes
	// UpdateVehicle(0.f, 0.f, 1.f, 0, 0);
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


void UTrackPathFollowingComponent::DebugWaitingForIdleOverlappingActors(const float DeltaTime) const
{
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		FString DebugMessage = TEXT("Overlap WAIT: direct idle obstruction moving aside\n");
		for (const FOverlapActorData& OverlapData : M_IdleAlliedBlockingActors)
		{
			if (OverlapData.Actor.IsValid())
			{
				bool bValidName = false;
				const URTSComponent* const OtherRTSComp =
					OverlapData.Actor->FindComponentByClass<URTSComponent>();
				const FString NameOfOther = OtherRTSComp
					                            ? OtherRTSComp->GetDisplayName(bValidName)
					                            : OverlapData.Actor->GetName();
				DebugMessage += NameOfOther + TEXT(" ");
			}
		}
		RTSFunctionLibrary::DrawDebugAtLocation(
			this,
			GetAgentLocation() + FVector(0.f, 0.f, TrackFollowingEvasion::ZOffsetDebug),
			DebugMessage,
			FColor::Red,
			DeltaTime);
	}
}

void UTrackPathFollowingComponent::DebugOverlapAdjustment(
	const FOverlapAdjustment& Adjustment,
	const float FinalThrottle,
	const float FinalSteering,
	const float DeltaTime) const
{
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		FString OtherName = TEXT("unknown ally");
		if (Adjustment.PrimaryActor.IsValid())
		{
			bool bValidName = false;
			const URTSComponent* const OtherRTSComponent =
				Adjustment.PrimaryActor->FindComponentByClass<URTSComponent>();
			OtherName = OtherRTSComponent
				            ? OtherRTSComponent->GetDisplayName(bValidName)
				            : Adjustment.PrimaryActor->GetName();
		}
		FString DebugMessage;
		FColor DebugColor = FColor::Yellow;
		float DebugLifetime = DeltaTime;
		float DebugZOffset = TrackFollowingEvasion::ZOffsetDebug;
		if (Adjustment.Reason == EOverlapAdjustmentReason::ResolvingDeadlock)
		{
			DebugMessage = FString::Printf(
				TEXT("Overlap DEADLOCK RESOLVED\nPriority vehicle proceeding past %s"),
				*OtherName);
			DebugColor = FColor::Purple;
			DebugLifetime = TrackFollowingEvasion::DeadlockDebugLifetimeSeconds;
			DebugZOffset += TrackFollowingEvasion::DeadlockDebugAdditionalZOffset;
		}
		else if (not Adjustment.bIsLimitingMovement)
		{
			return;
		}
		else if (Adjustment.Reason == EOverlapAdjustmentReason::FollowingSameDirection)
		{
			DebugMessage = FString::Printf(
				TEXT("Overlap FOLLOW: slowing behind %s\nThrottle %.2f"),
				*OtherName,
				FinalThrottle);
		}
		else if (Adjustment.Reason == EOverlapAdjustmentReason::AvoidingConflictingDirection ||
			Adjustment.Reason == EOverlapAdjustmentReason::AvoidingIdleVehicle)
		{
			const float AppliedPassingDirection = bReversing
				                                      ? -Adjustment.SteeringDirection
				                                      : Adjustment.SteeringDirection;
			const TCHAR* PassingSide = AppliedPassingDirection >= 0.f ? TEXT("RIGHT") : TEXT("LEFT");
			const bool bAvoidingIdleVehicle =
				Adjustment.Reason == EOverlapAdjustmentReason::AvoidingIdleVehicle;
			if (bAvoidingIdleVehicle)
			{
				DebugMessage = FString::Printf(
					TEXT("Overlap AVOID IDLE: pass %s around %s\nThrottle %.2f | Steering %.2f"),
					PassingSide,
					*OtherName,
					FinalThrottle,
					FinalSteering);
			}
			else
			{
				DebugMessage = FString::Printf(
					TEXT("Overlap AVOID: pass %s around %s\nThrottle %.2f | Steering %.2f"),
					PassingSide,
					*OtherName,
					FinalThrottle,
					FinalSteering);
			}
			DebugColor = FColor::Orange;
		}

		RTSFunctionLibrary::DrawDebugAtLocation(
			this,
			GetAgentLocation() + FVector(0.f, 0.f, DebugZOffset),
			DebugMessage,
			DebugColor,
			DebugLifetime);
	}
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
	if constexpr (DeveloperSettings::Debugging::GPathFollowing_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Attempting to teleport vehicle to get unstuck!");
	}

	// Attempt to find a teleport location
	const FVector TeleportLocation = FindTeleportLocation();

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

		// Return the current location as we've already teleported
		return ControlledPawn->GetActorLocation();
	}

	// Teleportation failed, so keep the previous side/back movement fallback.
	return GetUnstuckLocation(MoveFocus);
}

FVector UTrackPathFollowingComponent::FindTeleportLocation()
{
	constexpr float TeleportUnstuckZOffset = 80.f;

	const FVector StartLocation = ControlledPawn->GetActorLocation();
	// Calculate direction vector to the destination
	const FVector Direction = (VehicleCurrentDestination - StartLocation).GetSafeNormal();
	const FVector EndLocation = StartLocation + (Direction * TeleportDistance);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		FNavLocation NavLocation;
		if (NavSys->ProjectPointToNavigation(EndLocation, NavLocation, FVector(500.f, 500.f, 500.f)))
		{
			return NavLocation.Location + FVector(0.f, 0.f, TeleportUnstuckZOffset);
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

void UTrackPathFollowingComponent::SetEngineBlockDetectionSuppressed(const bool bShouldBeSuppressed)
{
	if (bM_IsEngineBlockDetectionSuppressionLocked && not bShouldBeSuppressed)
	{
		if (not bM_IsEngineBlockDetectionSuppressed)
		{
			bM_IsEngineBlockDetectionSuppressed = true;
			SetBlockDetectionState(false);
			ResetBlockDetectionData();
		}
		return;
	}

	if (bM_IsEngineBlockDetectionSuppressed == bShouldBeSuppressed)
	{
		return;
	}

	bM_IsEngineBlockDetectionSuppressed = bShouldBeSuppressed;

	if (bM_IsEngineBlockDetectionSuppressed)
	{
		// We are intentionally stationary: don't let UE classify this as "Blocked".
		SetBlockDetectionState(false);
		ResetBlockDetectionData();
		return;
	}

	// We're moving again: re-enable with a clean slate so stale samples don't instantly fire.
	ResetBlockDetectionData();
	SetBlockDetectionState(true);
	ForceBlockDetectionUpdate();
}

void UTrackPathFollowingComponent::SetEngineBlockDetectionSuppressionLock(const bool bShouldLock)
{
	if (bM_IsEngineBlockDetectionSuppressionLocked == bShouldLock)
	{
		return;
	}

	bM_IsEngineBlockDetectionSuppressionLocked = bShouldLock;

	if (bM_IsEngineBlockDetectionSuppressionLocked)
	{
		SetEngineBlockDetectionSuppressed(true);
		return;
	}

	SetEngineBlockDetectionSuppressed(false);
}

void UTrackPathFollowingComponent::UpdateEngineBlockDetectionForOverlapResponse(
	const bool bShouldSuppressForOverlap)
{
	SetEngineBlockDetectionSuppressed(bShouldSuppressForOverlap);
}
