#include "TrainSplineMovementComponent.h"

#include "Components/SplineComponent.h"
#include "RTS_Survival/Environment/Splines/RoadSplineActor.h"
#include "RTS_Survival/Units/Tanks/TrainUnit/TrainUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UTrainSplineMovementComponent::UTrainSplineMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTrainSplineMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTrainSplineMovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (not bM_IsMoving || DeltaTime <= 0.0f)
	{
		return;
	}

	if (not GetHasValidCachedSplineComponentNoReport() && not TryCacheSplineComponent())
	{
		StopMovement();
		return;
	}

	const float DistanceToTarget = M_TargetDistanceAlongSpline - M_CurrentDistanceAlongSpline;
	if (GetHasReachedTargetDistance(DistanceToTarget))
	{
		HandleReachedTargetDistance();
		return;
	}

	Tick_UpdateSpeed(DeltaTime, DistanceToTarget);
	if (M_CurrentSpeed <= KINDA_SMALL_NUMBER)
	{
		HandleReachedTargetDistance();
		return;
	}

	const float PreviousDistanceAlongSpline = M_CurrentDistanceAlongSpline;
	const float MovementDirectionSign = FMath::Sign(DistanceToTarget);
	const float MovementDeltaDistance = MovementDirectionSign * M_CurrentSpeed * DeltaTime;
	const float NewDistanceAlongSpline = PreviousDistanceAlongSpline + MovementDeltaDistance;

	if (GetHasCrossedTargetDistance(PreviousDistanceAlongSpline, NewDistanceAlongSpline))
	{
		HandleReachedTargetDistance();
		return;
	}

	M_CurrentDistanceAlongSpline = ClampDistanceToSplineLength(NewDistanceAlongSpline);
	Tick_ApplyTrainTransformsAtCurrentDistance();
}

void UTrainSplineMovementComponent::SetAssignedRoadSpline(ARoadSplineActor* InAssignedRoadSpline)
{
	if (M_AssignedRoadSplineActor.Get() == InAssignedRoadSpline && GetHasValidCachedSplineComponentNoReport())
	{
		return;
	}

	M_AssignedRoadSplineActor = InAssignedRoadSpline;
	if (not TryCacheSplineComponent())
	{
		return;
	}

	M_CurrentDistanceAlongSpline = ClampDistanceToSplineLength(M_CurrentDistanceAlongSpline);
	M_TargetDistanceAlongSpline = ClampDistanceToSplineLength(M_TargetDistanceAlongSpline);
}

void UTrainSplineMovementComponent::SetCurrentDistanceAlongSpline(const float InCurrentDistanceAlongSpline)
{
	if (not GetIsValidCachedSplineComponent())
	{
		return;
	}

	M_CurrentDistanceAlongSpline = ClampDistanceToSplineLength(InCurrentDistanceAlongSpline);
	Tick_ApplyTrainTransformsAtCurrentDistance();
}

void UTrainSplineMovementComponent::SetTargetDistanceAlongSpline(const float InTargetDistanceAlongSpline)
{
	if (not GetIsValidCachedSplineComponent())
	{
		return;
	}

	M_TargetDistanceAlongSpline = ClampDistanceToSplineLength(InTargetDistanceAlongSpline);
	const float DistanceToTarget = M_TargetDistanceAlongSpline - M_CurrentDistanceAlongSpline;
	if (GetHasReachedTargetDistance(DistanceToTarget))
	{
		HandleReachedTargetDistance();
		return;
	}

	bM_IsMoving = true;
}

void UTrainSplineMovementComponent::StopMovement()
{
	bM_IsMoving = false;
	M_CurrentSpeed = 0.0f;
	M_CurrentDistanceAlongSpline = ClampDistanceToSplineLength(M_CurrentDistanceAlongSpline);
	Tick_ApplyTrainTransformsAtCurrentDistance();
}

bool UTrainSplineMovementComponent::GetHasValidAssignedRoadSplineActorNoReport() const
{
	return M_AssignedRoadSplineActor.IsValid();
}

bool UTrainSplineMovementComponent::GetHasValidCachedSplineComponentNoReport() const
{
	return M_CachedSplineComponent.IsValid();
}

bool UTrainSplineMovementComponent::GetIsValidAssignedRoadSplineActor() const
{
	if (GetHasValidAssignedRoadSplineActorNoReport())
	{
		bM_HasLoggedInvalidAssignedRoadSplineActor = false;
		return true;
	}

	if (bM_HasLoggedInvalidAssignedRoadSplineActor)
	{
		return false;
	}

	bM_HasLoggedInvalidAssignedRoadSplineActor = true;
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_AssignedRoadSplineActor",
		"UTrainSplineMovementComponent::GetIsValidAssignedRoadSplineActor",
		this);
	return false;
}

bool UTrainSplineMovementComponent::GetIsValidCachedSplineComponent() const
{
	if (GetHasValidCachedSplineComponentNoReport())
	{
		bM_HasLoggedInvalidCachedSplineComponent = false;
		return true;
	}

	if (bM_HasLoggedInvalidCachedSplineComponent)
	{
		return false;
	}

	bM_HasLoggedInvalidCachedSplineComponent = true;
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_CachedSplineComponent",
		"UTrainSplineMovementComponent::GetIsValidCachedSplineComponent",
		this);
	return false;
}

bool UTrainSplineMovementComponent::GetIsValidTrainOwner() const
{
	if (IsValid(Cast<ATrainUnit>(GetOwner())))
	{
		bM_HasLoggedInvalidTrainOwner = false;
		return true;
	}

	if (bM_HasLoggedInvalidTrainOwner)
	{
		return false;
	}

	bM_HasLoggedInvalidTrainOwner = true;
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"TrainOwner",
		"UTrainSplineMovementComponent::GetIsValidTrainOwner",
		this);
	return false;
}

bool UTrainSplineMovementComponent::TryCacheSplineComponent()
{
	if (not GetIsValidAssignedRoadSplineActor())
	{
		M_CachedSplineComponent = nullptr;
		return false;
	}

	if (not IsValid(M_AssignedRoadSplineActor->RoadSpline))
	{
		M_CachedSplineComponent = nullptr;
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			M_AssignedRoadSplineActor.Get(),
			"RoadSpline",
			"UTrainSplineMovementComponent::TryCacheSplineComponent",
			M_AssignedRoadSplineActor.Get());
		return false;
	}

	M_CachedSplineComponent = M_AssignedRoadSplineActor->RoadSpline;
	bM_HasLoggedInvalidCachedSplineComponent = false;
	return true;
}

float UTrainSplineMovementComponent::ClampDistanceToSplineLength(const float InDistance) const
{
	if (not GetIsValidCachedSplineComponent())
	{
		return 0.0f;
	}

	const float SplineLength = M_CachedSplineComponent->GetSplineLength();
	return FMath::Clamp(InDistance, 0.0f, SplineLength);
}

float UTrainSplineMovementComponent::GetValidatedAcceleration() const
{
	if (M_Acceleration > KINDA_SMALL_NUMBER)
	{
		bM_HasLoggedInvalidAcceleration = false;
		return M_Acceleration;
	}

	if (not bM_HasLoggedInvalidAcceleration)
	{
		bM_HasLoggedInvalidAcceleration = true;
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_Acceleration",
			"UTrainSplineMovementComponent::GetValidatedAcceleration",
			this);
	}

	constexpr float SafeAccelerationFallback = 1.0f;
	return SafeAccelerationFallback;
}

bool UTrainSplineMovementComponent::GetHasReachedTargetDistance(const float DistanceToTarget) const
{
	return FMath::Abs(DistanceToTarget) <= M_TargetDistanceTolerance;
}

bool UTrainSplineMovementComponent::GetHasCrossedTargetDistance(
	const float PreviousDistanceAlongSpline,
	const float NewDistanceAlongSpline) const
{
	const float PreviousTargetDelta = M_TargetDistanceAlongSpline - PreviousDistanceAlongSpline;
	const float NewTargetDelta = M_TargetDistanceAlongSpline - NewDistanceAlongSpline;
	return PreviousTargetDelta * NewTargetDelta <= 0.0f;
}

void UTrainSplineMovementComponent::HandleReachedTargetDistance()
{
	M_CurrentDistanceAlongSpline = ClampDistanceToSplineLength(M_TargetDistanceAlongSpline);
	StopMovement();

	ATrainUnit* TrainOwner = Cast<ATrainUnit>(GetOwner());
	if (not IsValid(TrainOwner))
	{
		(void)GetIsValidTrainOwner();
		return;
	}

	TrainOwner->OnSplineMovementReachedTarget();
}

void UTrainSplineMovementComponent::Tick_UpdateSpeed(const float DeltaTime, const float DistanceToTarget)
{
	const float SafeAcceleration = GetValidatedAcceleration();
	const float AbsoluteDistanceToTarget = FMath::Abs(DistanceToTarget);
	const float PhysicallyRequiredBrakingDistance = M_CurrentSpeed * M_CurrentSpeed / (2.0f * SafeAcceleration);
	const float DecelerationDistance = FMath::Max3(
		M_TargetDistanceTolerance,
		M_MinBrakingDistance,
		PhysicallyRequiredBrakingDistance);
	if (AbsoluteDistanceToTarget <= M_TargetDistanceTolerance)
	{
		M_CurrentSpeed = 0.0f;
		return;
	}

	if (FMath::Abs(DistanceToTarget) <= DecelerationDistance)
	{
		M_CurrentSpeed = FMath::Max(0.0f, M_CurrentSpeed - SafeAcceleration * DeltaTime);
		return;
	}

	M_CurrentSpeed = FMath::Min(M_MaxSpeed, M_CurrentSpeed + SafeAcceleration * DeltaTime);
}

void UTrainSplineMovementComponent::Tick_ApplyTrainTransformsAtCurrentDistance() const
{
	ATrainUnit* TrainOwner = Cast<ATrainUnit>(GetOwner());
	if (not IsValid(TrainOwner))
	{
		(void)GetIsValidTrainOwner();
		return;
	}

	TrainOwner->ApplyTrainTransforms(M_CurrentDistanceAlongSpline);
}
