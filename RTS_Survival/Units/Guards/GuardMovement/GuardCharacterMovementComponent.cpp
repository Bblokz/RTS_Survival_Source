// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GuardCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace GuardCharacterMovementComponentPrivate
{
	constexpr float SupportHeightTieToleranceCm = 2.0f;

	FCollisionObjectQueryParams GetGuardSurfaceObjectQueryParams()
	{
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Destructible);
		return ObjectQueryParams;
	}
}

UGuardCharacterMovementComponent::UGuardCharacterMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UGuardCharacterMovementComponent::StartGuardMove(const FVector& TargetLocation)
{
	if (not GetIsValidCharacterOwner())
	{
		return false;
	}

	M_GuardTargetLocation = TargetLocation;
	bM_IsGuardMoveActive = true;
	M_TimeWithoutProgressSeconds = 0.0f;
	StopMovementImmediately();
	SetMovementMode(MOVE_Custom, GuardStrafeCustomMode);
	return true;
}

void UGuardCharacterMovementComponent::StopGuardMove()
{
	bM_IsGuardMoveActive = false;
	M_GuardTargetLocation = FVector::ZeroVector;
	M_TimeWithoutProgressSeconds = 0.0f;
	StopMovementImmediately();
	SetMovementMode(MOVE_Walking);
	UpdateComponentVelocity();
}

void UGuardCharacterMovementComponent::PhysCustom(const float DeltaTime, const int32 Iterations)
{
	if (CustomMovementMode != GuardStrafeCustomMode)
	{
		Super::PhysCustom(DeltaTime, Iterations);
		return;
	}

	PhysGuardMove(DeltaTime);
}

void UGuardCharacterMovementComponent::PhysGuardMove(const float DeltaTime)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (not bM_IsGuardMoveActive || not GetIsValidCharacterOwner())
	{
		FinishGuardMove(false);
		return;
	}

	const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
	const FVector DeltaToTarget = M_GuardTargetLocation - CurrentLocation;
	const float DistanceToTarget2D = FVector::Dist2D(CurrentLocation, M_GuardTargetLocation);
	if (DistanceToTarget2D <= M_GuardMoveAcceptanceRadiusCm)
	{
		FinishGuardMove(true);
		return;
	}

	const FVector MoveDirection2D = FVector(DeltaToTarget.X, DeltaToTarget.Y, 0.0f).GetSafeNormal();
	if (MoveDirection2D.IsNearlyZero())
	{
		FinishGuardMove(true);
		return;
	}

	const float RequestedMoveDistance = FMath::Min3(
		MaxWalkSpeed * DeltaTime,
		DistanceToTarget2D,
		M_GuardMaxStrafeStepCm);
	if (not TryMoveTowardsGuardTarget(CurrentLocation, MoveDirection2D, RequestedMoveDistance, DeltaTime))
	{
		HandleBlockedGuardMove(DeltaTime);
		return;
	}

	if (FVector::Dist2D(UpdatedComponent->GetComponentLocation(), M_GuardTargetLocation) <= M_GuardMoveAcceptanceRadiusCm)
	{
		FinishGuardMove(true);
	}
}

void UGuardCharacterMovementComponent::FinishGuardMove(const bool bReachedDestination)
{
	if (not bM_IsGuardMoveActive)
	{
		return;
	}

	StopGuardMove();
	OnGuardMoveFinished.Broadcast(bReachedDestination);
}

void UGuardCharacterMovementComponent::HandleBlockedGuardMove(const float DeltaTime)
{
	M_TimeWithoutProgressSeconds += DeltaTime;
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();

	if (M_TimeWithoutProgressSeconds < M_MaxTimeWithoutProgressSeconds)
	{
		return;
	}

	FinishGuardMove(false);
}

void UGuardCharacterMovementComponent::RegisterGuardMoveProgress(
	const FVector& PreviousLocation,
	const FVector& NewLocation)
{
	if (FVector::DistSquared2D(PreviousLocation, NewLocation) <= FMath::Square(M_GuardMinProgressDistanceCm))
	{
		return;
	}

	M_TimeWithoutProgressSeconds = 0.0f;
}

bool UGuardCharacterMovementComponent::TryMoveTowardsGuardTarget(
	const FVector& CurrentLocation,
	const FVector& MoveDirection2D,
	float DesiredMoveDistance,
	const float DeltaTime)
{
	if (not GetIsValidCharacterOwner())
	{
		return false;
	}

	for (int32 SearchIteration = 0; SearchIteration < M_MaxGuardStepSearchIterations; SearchIteration++)
	{
		if (DesiredMoveDistance < M_GuardMinStrafeStepCm)
		{
			return false;
		}

		FVector SupportedLocation;
		if (not TryGetSupportedLocationForGuardMove(
			CurrentLocation,
			MoveDirection2D,
			DesiredMoveDistance,
			SupportedLocation))
		{
			DesiredMoveDistance *= M_GuardStepReductionFactor;
			continue;
		}

		float AdjustedMoveDistance = DesiredMoveDistance;
		if (not TryAdjustMoveDistanceForBlockingGuardGeometry(CurrentLocation, SupportedLocation, AdjustedMoveDistance))
		{
			DesiredMoveDistance *= M_GuardStepReductionFactor;
			continue;
		}

		if (AdjustedMoveDistance + M_GuardMinProgressDistanceCm < DesiredMoveDistance)
		{
			DesiredMoveDistance = AdjustedMoveDistance;
			continue;
		}

		const FVector DesiredMoveDelta = SupportedLocation - CurrentLocation;
		FHitResult MovementHit;
		MoveUpdatedComponent(DesiredMoveDelta, UpdatedComponent->GetComponentQuat(), true, &MovementHit);

		const FVector NewLocation = UpdatedComponent->GetComponentLocation();
		RegisterGuardMoveProgress(CurrentLocation, NewLocation);
		if (FVector::DistSquared2D(CurrentLocation, NewLocation) <= FMath::Square(M_GuardMinProgressDistanceCm))
		{
			DesiredMoveDistance *= M_GuardStepReductionFactor;
			continue;
		}

		Velocity = (NewLocation - CurrentLocation) / DeltaTime;
		UpdateComponentVelocity();
		return true;
	}

	return false;
}

bool UGuardCharacterMovementComponent::TryGetSupportedLocationForGuardMove(
	const FVector& CurrentLocation,
	const FVector& MoveDirection2D,
	const float DesiredMoveDistance,
	FVector& OutSupportedLocation) const
{
	OutSupportedLocation = FVector::ZeroVector;

	if (not GetIsValidCharacterOwner())
	{
		return false;
	}

	const ACharacter* MyCharacterOwner = GetCharacterOwner();
	if (not IsValid(MyCharacterOwner))
	{
		return false;
	}

	const UCapsuleComponent* CapsuleComponent = MyCharacterOwner->GetCapsuleComponent();
	if (not IsValid(CapsuleComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"CapsuleComponent",
			"TryGetSupportedLocationForGuardMove",
			this);
		return false;
	}

	const FVector CandidateCenterLocation = CurrentLocation + (MoveDirection2D * DesiredMoveDistance);
	FHitResult SurfaceHit;
	if (not TryGetWalkableGuardSurfaceHit(CandidateCenterLocation, MoveDirection2D, SurfaceHit))
	{
		return false;
	}

	OutSupportedLocation = CandidateCenterLocation;
	OutSupportedLocation.Z = SurfaceHit.ImpactPoint.Z + CapsuleComponent->GetScaledCapsuleHalfHeight() +
		M_GuardSurfaceClearanceCm;
	return true;
}

bool UGuardCharacterMovementComponent::TryGetWalkableGuardSurfaceHit(
	const FVector& CandidateCenterLocation,
	const FVector& MoveDirection2D,
	FHitResult& OutSurfaceHit) const
{
	OutSurfaceHit = FHitResult();

	if (not GetIsValidCharacterOwner())
	{
		return false;
	}

	const ACharacter* MyCharacterOwner = GetCharacterOwner();
	if (not IsValid(MyCharacterOwner))
	{
		return false;
	}

	const UCapsuleComponent* CapsuleComponent = MyCharacterOwner->GetCapsuleComponent();
	if (not IsValid(CapsuleComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"CapsuleComponent",
			"TryGetWalkableGuardSurfaceHit",
			this);
		return false;
	}

	const float CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const float SampleOffsetDistance = CapsuleRadius * M_GuardSurfaceSampleRadiusScale;
	TArray<FVector, TInlineAllocator<5>> SampleOffsets;
	BuildGuardSurfaceSampleOffsets(MoveDirection2D, SampleOffsetDistance, SampleOffsets);

	float BestSupportHeight = TNumericLimits<float>::Lowest();
	float BestSampleDistanceSquared = TNumericLimits<float>::Max();
	bool bFoundWalkableSupport = false;

	for (const FVector& SampleOffset : SampleOffsets)
	{
		FHitResult SampleHit;
		if (not TryGetWalkableGuardSurfaceHitForSample(CandidateCenterLocation + SampleOffset, SampleHit))
		{
			continue;
		}

		const float SampleDistanceSquared = SampleOffset.SizeSquared2D();
		if (not ShouldReplaceBestGuardSurfaceHit(
			SampleHit,
			SampleDistanceSquared,
			BestSupportHeight,
			BestSampleDistanceSquared))
		{
			continue;
		}

		BestSupportHeight = SampleHit.ImpactPoint.Z;
		BestSampleDistanceSquared = SampleDistanceSquared;
		OutSurfaceHit = SampleHit;
		bFoundWalkableSupport = true;
	}

	return bFoundWalkableSupport;
}

void UGuardCharacterMovementComponent::BuildGuardSurfaceSampleOffsets(
	const FVector& MoveDirection2D,
	const float SampleOffsetDistance,
	TArray<FVector, TInlineAllocator<5>>& OutSampleOffsets) const
{
	OutSampleOffsets.Reset();
	OutSampleOffsets.Add(FVector::ZeroVector);
	OutSampleOffsets.Add(MoveDirection2D * SampleOffsetDistance);
	OutSampleOffsets.Add(-MoveDirection2D * SampleOffsetDistance);

	const FVector RightVector = FVector(-MoveDirection2D.Y, MoveDirection2D.X, 0.0f).GetSafeNormal();
	OutSampleOffsets.Add(RightVector * SampleOffsetDistance);
	OutSampleOffsets.Add(-RightVector * SampleOffsetDistance);
}

bool UGuardCharacterMovementComponent::ShouldReplaceBestGuardSurfaceHit(
	const FHitResult& CandidateSurfaceHit,
	const float CandidateSampleDistanceSquared,
	const float BestSupportHeight,
	const float BestSampleDistanceSquared) const
{
	const bool bHigherSupport = CandidateSurfaceHit.ImpactPoint.Z > BestSupportHeight +
		GuardCharacterMovementComponentPrivate::SupportHeightTieToleranceCm;
	const bool bSameHeightButCloserToCenter = FMath::IsNearlyEqual(
			CandidateSurfaceHit.ImpactPoint.Z,
			BestSupportHeight,
			GuardCharacterMovementComponentPrivate::SupportHeightTieToleranceCm) &&
		CandidateSampleDistanceSquared < BestSampleDistanceSquared;
	return bHigherSupport || bSameHeightButCloserToCenter;
}

bool UGuardCharacterMovementComponent::TryGetWalkableGuardSurfaceHitForSample(
	const FVector& SampleLocation,
	FHitResult& OutSurfaceHit) const
{
	OutSurfaceHit = FHitResult();

	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"World",
			"TryGetWalkableGuardSurfaceHitForSample",
			this);
		return false;
	}

	const ACharacter* MyCharacterOwner = GetCharacterOwner();
	if (not IsValid(MyCharacterOwner))
	{
		return false;
	}

	const FVector TraceStart = SampleLocation + FVector::UpVector * M_GuardTraceStartHeightCm;
	const FVector TraceEnd = TraceStart - FVector::UpVector * M_GuardTraceLengthCm;

	FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(GuardSurfaceTrace), false, MyCharacterOwner);
	CollisionQueryParams.bTraceComplex = false;

	TArray<FHitResult> SurfaceHits;
	const bool bDidHitSurface = World->LineTraceMultiByObjectType(
		SurfaceHits,
		TraceStart,
		TraceEnd,
		GuardCharacterMovementComponentPrivate::GetGuardSurfaceObjectQueryParams(),
		CollisionQueryParams);
	if (not bDidHitSurface)
	{
		return false;
	}

	for (const FHitResult& SurfaceHit : SurfaceHits)
	{
		UPrimitiveComponent* HitComponent = SurfaceHit.GetComponent();
		if (not SurfaceHit.bBlockingHit || not IsValid(HitComponent) || not IsWalkable(SurfaceHit))
		{
			continue;
		}

		if (HitComponent->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
		{
			continue;
		}

		OutSurfaceHit = SurfaceHit;
		return true;
	}

	return false;
}

bool UGuardCharacterMovementComponent::TryAdjustMoveDistanceForBlockingGuardGeometry(
	const FVector& CurrentLocation,
	const FVector& SupportedLocation,
	float& InOutMoveDistance) const
{
	if (not GetIsValidCharacterOwner())
	{
		return false;
	}

	if (InOutMoveDistance < M_GuardMinStrafeStepCm)
	{
		return false;
	}

	FHitResult BlockingHit;
	if (not TrySweepForBlockingGuardGeometry(CurrentLocation, SupportedLocation, BlockingHit))
	{
		return true;
	}

	const float SafeMoveDistance = (InOutMoveDistance * BlockingHit.Time) - M_GuardBlockingBackoffCm;
	InOutMoveDistance = FMath::Max(0.0f, SafeMoveDistance);
	return InOutMoveDistance >= M_GuardMinStrafeStepCm;
}

bool UGuardCharacterMovementComponent::TrySweepForBlockingGuardGeometry(
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& OutBlockingHit) const
{
	OutBlockingHit = FHitResult();

	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"World",
			"TrySweepForBlockingGuardGeometry",
			this);
		return false;
	}

	const ACharacter* MyCharacterOwner = GetCharacterOwner();
	if (not IsValid(MyCharacterOwner))
	{
		return false;
	}

	const UCapsuleComponent* CapsuleComponent = MyCharacterOwner->GetCapsuleComponent();
	if (not IsValid(CapsuleComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"CapsuleComponent",
			"TrySweepForBlockingGuardGeometry",
			this);
		return false;
	}

	const float SphereRadius = CapsuleComponent->GetScaledCapsuleRadius() * M_GuardBodySweepRadiusScale;
	if (SphereRadius <= 0.0f)
	{
		return false;
	}

	FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(GuardBodySweep), false, MyCharacterOwner);
	CollisionQueryParams.bTraceComplex = false;

	return World->SweepSingleByObjectType(
		OutBlockingHit,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		GuardCharacterMovementComponentPrivate::GetGuardSurfaceObjectQueryParams(),
		FCollisionShape::MakeSphere(SphereRadius),
		CollisionQueryParams);
}

bool UGuardCharacterMovementComponent::GetIsValidCharacterOwner() const
{
	if (IsValid(GetCharacterOwner()) && IsValid(UpdatedComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"CharacterOwnerOrUpdatedComponent",
		"GetIsValidCharacterOwner",
		this);
	return false;
}
