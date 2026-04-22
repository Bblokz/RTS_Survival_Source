#include "TrainUnit.h"

#include "Components/MeshComponent.h"
#include "Components/SplineComponent.h"
#include "RTS_Survival/Environment/Splines/RoadSplineActor.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TrainUnit/TrainSplineMovementComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ATrainUnit::ATrainUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_TrainSplineMovementComponent = CreateDefaultSubobject<UTrainSplineMovementComponent>(TEXT("TrainSplineMovementComponent"));
}

void ATrainUnit::SetupTrainMeshCollision(const TArray<UMeshComponent*>& MeshComponents)
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}

	const bool bOwnedByPlayer1 = RTSComponent->GetOwningPlayer() == 1;
	FRTS_CollisionSetup::SetupCollisionForTrainMeshes(MeshComponents, bOwnedByPlayer1);
}

void ATrainUnit::SetupTrainComposition(UMeshComponent* InLocomotiveMesh, const TArray<UMeshComponent*>& InCartMeshes)
{
	if (not TryCacheRoadSplineComponentFromAssignedRoad())
	{
		return;
	}

	if (not IsValid(InLocomotiveMesh))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"InLocomotiveMesh",
			"ATrainUnit::SetupTrainComposition",
			this);
		return;
	}

	M_LocomotiveMesh = InLocomotiveMesh;
	M_TrainCartMeshes.Empty();
	M_CartDistanceOffsets.Empty();
	M_TrainCartMeshes.Reserve(InCartMeshes.Num());
	for (UMeshComponent* CartMesh : InCartMeshes)
	{
		if (not IsValid(CartMesh))
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(
				this,
				"InCartMeshes",
				"ATrainUnit::SetupTrainComposition",
				this);
			M_LocomotiveMesh = nullptr;
			M_TrainCartMeshes.Empty();
			return;
		}
		M_TrainCartMeshes.Add(CartMesh);
	}

	TArray<float> InferredOffsets;
	if (not TryBuildCartDistanceOffsetsFromCurrentPlacement(InLocomotiveMesh, InCartMeshes, InferredOffsets))
	{
		M_LocomotiveMesh = nullptr;
		M_TrainCartMeshes.Empty();
		M_CartDistanceOffsets.Empty();
		return;
	}

	M_CartDistanceOffsets = InferredOffsets;
}

void ATrainUnit::BeginPlay()
{
	Super::BeginPlay();
	if (not TryCacheRoadSplineComponentFromAssignedRoad() || not GetIsValidTrainSplineMovementComponent())
	{
		return;
	}

	M_TrainSplineMovementComponent->SetAssignedRoadSpline(AssignedRoadSpline.Get());

	float StartDistanceAlongSpline = 0.0f;
	if (not TryGetLocomotiveStartDistance(StartDistanceAlongSpline))
	{
		return;
	}

	M_TrainSplineMovementComponent->SetCurrentDistanceAlongSpline(StartDistanceAlongSpline);
	ApplyTrainTransforms(StartDistanceAlongSpline);
}

void ATrainUnit::Tick(float DeltaSeconds)
{
	ASelectablePawnMaster::Tick(DeltaSeconds);
}

void ATrainUnit::ExecuteMoveCommand(const FVector MoveToLocation)
{
	SetTurretsToAutoEngage(true);
	bM_IsReversing = false;
	bM_LastMoveCommandWasReverse = false;

	float TargetDistanceAlongSpline = 0.0f;
	if (not TryConvertWorldLocationToSplineDistance(MoveToLocation, TargetDistanceAlongSpline))
	{
		DoneExecutingCommand(EAbilityID::IdMove);
		return;
	}

	StartSplineMovementToDistance(TargetDistanceAlongSpline);
}

void ATrainUnit::TerminateMoveCommand()
{
	if (not GetIsValidTrainSplineMovementComponent())
	{
		return;
	}

	M_TrainSplineMovementComponent->StopMovement();
}

void ATrainUnit::ExecuteReverseCommand(const FVector ReverseToLocation)
{
	SetTurretsToAutoEngage(true);
	bM_IsReversing = true;
	bM_LastMoveCommandWasReverse = true;

	float TargetDistanceAlongSpline = 0.0f;
	if (not TryConvertWorldLocationToSplineDistance(ReverseToLocation, TargetDistanceAlongSpline))
	{
		DoneExecutingCommand(EAbilityID::IdReverseMove);
		return;
	}

	StartSplineMovementToDistance(TargetDistanceAlongSpline);
}

void ATrainUnit::TerminateReverseCommand()
{
	if (not GetIsValidTrainSplineMovementComponent())
	{
		return;
	}

	M_TrainSplineMovementComponent->StopMovement();
}

void ATrainUnit::ExecuteStopCommand()
{
	SetTurretsToAutoEngage(false);
	if (not GetIsValidTrainSplineMovementComponent())
	{
		return;
	}

	M_TrainSplineMovementComponent->StopMovement();
}

void ATrainUnit::ExecuteAttackCommand(AActor* Target)
{
	Super::ExecuteAttackCommand(Target);
}

void ATrainUnit::ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand)
{
	(void)RotateToRotator;

	if (not IsQueueCommand)
	{
		return;
	}

	DoneExecutingCommand(EAbilityID::IdRotateTowards);
}

void ATrainUnit::TerminateRotateTowardsCommand()
{
}

void ATrainUnit::ApplyRotateTowardsStep(const float TurnAmountDegrees, const float DeltaSeconds)
{
	(void)TurnAmountDegrees;
	(void)DeltaSeconds;
}

void ATrainUnit::OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret)
{
	(void)TargetLocation;
	(void)CallingTurret;
}

void ATrainUnit::OnTurretInRange(ACPPTurretsMaster* CallingTurret)
{
	(void)CallingTurret;
}

void ATrainUnit::OnUnitIdleAndNoNewCommands()
{
	ASelectablePawnMaster::OnUnitIdleAndNoNewCommands();
}

bool ATrainUnit::GetIsValidAssignedRoadSpline() const
{
	if (AssignedRoadSpline.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"AssignedRoadSpline",
		"ATrainUnit::GetIsValidAssignedRoadSpline",
		this);
	return false;
}

bool ATrainUnit::TryCacheRoadSplineComponentFromAssignedRoad()
{
	if (GetIsValidRoadSplineComponent())
	{
		return true;
	}

	if (not GetIsValidAssignedRoadSpline())
	{
		return false;
	}

	if (not IsValid(AssignedRoadSpline->RoadSpline))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"RoadSpline",
			"ATrainUnit::TryCacheRoadSplineComponentFromAssignedRoad",
			this);
		return false;
	}

	M_RoadSplineComponent = AssignedRoadSpline->RoadSpline;
	return true;
}

bool ATrainUnit::GetIsValidTrainSplineMovementComponent() const
{
	if (IsValid(M_TrainSplineMovementComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_TrainSplineMovementComponent",
		"ATrainUnit::GetIsValidTrainSplineMovementComponent",
		this);
	return false;
}

bool ATrainUnit::GetIsValidRoadSplineComponent() const
{
	if (M_RoadSplineComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_RoadSplineComponent",
		"ATrainUnit::GetIsValidRoadSplineComponent",
		this);
	return false;
}

bool ATrainUnit::TryConvertWorldLocationToSplineDistance(const FVector& WorldLocation, float& OutDistanceAlongSpline) const
{
	return TryGetSplineDistanceAtWorldLocation(WorldLocation, OutDistanceAlongSpline);
}

bool ATrainUnit::TryGetSplineDistanceAtWorldLocation(const FVector& WorldLocation, float& OutDistanceAlongSpline) const
{
	if (not GetIsValidAssignedRoadSpline())
	{
		return false;
	}

	if (not GetIsValidRoadSplineComponent())
	{
		return false;
	}

	const float SplineInputKey = M_RoadSplineComponent->FindInputKeyClosestToWorldLocation(WorldLocation);
	const float DistanceAlongSpline = M_RoadSplineComponent->GetDistanceAlongSplineAtSplineInputKey(SplineInputKey);
	const float SplineLength = M_RoadSplineComponent->GetSplineLength();
	OutDistanceAlongSpline = FMath::Clamp(DistanceAlongSpline, 0.0f, SplineLength);
	return true;
}

void ATrainUnit::StartSplineMovementToDistance(const float TargetDistanceAlongSpline)
{
	if (not GetIsValidTrainSplineMovementComponent())
	{
		return;
	}

	M_TrainSplineMovementComponent->SetTargetDistanceAlongSpline(TargetDistanceAlongSpline);
}

void ATrainUnit::OnReachedSplineMoveTarget()
{
	if (bM_LastMoveCommandWasReverse)
	{
		DoneExecutingCommand(EAbilityID::IdReverseMove);
		return;
	}

	DoneExecutingCommand(EAbilityID::IdMove);
}

bool ATrainUnit::GetIsValidLocomotiveMesh() const
{
	if (IsValid(M_LocomotiveMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_LocomotiveMesh",
		"ATrainUnit::GetIsValidLocomotiveMesh",
		this);
	return false;
}

bool ATrainUnit::GetHasValidTrainComposition() const
{
	if (not GetIsValidLocomotiveMesh())
	{
		return false;
	}

	if (M_TrainCartMeshes.Num() != M_CartDistanceOffsets.Num())
	{
		RTSFunctionLibrary::ReportError(
			"Train cart mesh and offset arrays are not aligned"
			"\n at function: ATrainUnit::GetHasValidTrainComposition"
			"\n for train: " + GetName());
		return false;
	}

	for (int32 CartIndex = 0; CartIndex < M_TrainCartMeshes.Num(); ++CartIndex)
	{
		if (not IsValid(M_TrainCartMeshes[CartIndex]))
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(
				this,
				"M_TrainCartMeshes",
				"ATrainUnit::GetHasValidTrainComposition",
				this);
			return false;
		}

		if (M_CartDistanceOffsets[CartIndex] < 0.0f)
		{
			RTSFunctionLibrary::ReportError(
				"Train cart offset is negative which would place a cart ahead of the locomotive"
				"\n at function: ATrainUnit::GetHasValidTrainComposition"
				"\n for train: " + GetName());
			return false;
		}
	}

	return true;
}

bool ATrainUnit::TryBuildCartDistanceOffsetsFromCurrentPlacement(
	const UMeshComponent* InLocomotiveMesh,
	const TArray<UMeshComponent*>& InCartMeshes,
	TArray<float>& OutCartDistanceOffsets) const
{
	if (not IsValid(InLocomotiveMesh))
	{
		return false;
	}

	float LocomotiveDistanceAlongSpline = 0.0f;
	if (not TryGetSplineDistanceAtWorldLocation(InLocomotiveMesh->GetComponentLocation(), LocomotiveDistanceAlongSpline))
	{
		return false;
	}

	OutCartDistanceOffsets.Empty();
	OutCartDistanceOffsets.Reserve(InCartMeshes.Num());
	for (UMeshComponent* CartMesh : InCartMeshes)
	{
		if (not IsValid(CartMesh))
		{
			return false;
		}

		float CartDistanceAlongSpline = 0.0f;
		if (not TryGetSplineDistanceAtWorldLocation(CartMesh->GetComponentLocation(), CartDistanceAlongSpline))
		{
			return false;
		}

		const float CartDistanceOffset = LocomotiveDistanceAlongSpline - CartDistanceAlongSpline;
		if (CartDistanceOffset < 0.0f)
		{
			RTSFunctionLibrary::ReportError(
				"Invalid train composition: cart was authored ahead of locomotive"
				"\n at function: ATrainUnit::TryBuildCartDistanceOffsetsFromCurrentPlacement"
				"\n for train: " + GetName());
			OutCartDistanceOffsets.Empty();
			return false;
		}

		OutCartDistanceOffsets.Add(CartDistanceOffset);
	}

	return true;
}

bool ATrainUnit::TryGetLocomotiveStartDistance(float& OutDistanceAlongSpline) const
{
	if (GetIsValidLocomotiveMesh())
	{
		return TryGetSplineDistanceAtWorldLocation(M_LocomotiveMesh->GetComponentLocation(), OutDistanceAlongSpline);
	}

	return TryGetSplineDistanceAtWorldLocation(GetActorLocation(), OutDistanceAlongSpline);
}

float ATrainUnit::GetMaxCartDistanceOffset() const
{
	float MaxCartDistanceOffset = 0.0f;
	for (const float CartDistanceOffset : M_CartDistanceOffsets)
	{
		MaxCartDistanceOffset = FMath::Max(MaxCartDistanceOffset, CartDistanceOffset);
	}
	return MaxCartDistanceOffset;
}

FRotator ATrainUnit::GetSplineRotationForCurrentMovementDirection(const float DistanceAlongSpline) const
{
	FRotator SplineRotation = M_RoadSplineComponent->GetRotationAtDistanceAlongSpline(
		DistanceAlongSpline,
		ESplineCoordinateSpace::World);
	if (not bM_IsReversing)
	{
		return SplineRotation;
	}

	constexpr float ReverseYawFlipDegrees = 180.0f;
	SplineRotation.Yaw += ReverseYawFlipDegrees;
	SplineRotation.Normalize();
	return SplineRotation;
}

FVector ATrainUnit::GetSplineWorldLocationWithMovementOffset(const float DistanceAlongSpline) const
{
	FVector SplineLocation = M_RoadSplineComponent->GetLocationAtDistanceAlongSpline(
		DistanceAlongSpline,
		ESplineCoordinateSpace::World);
	if (not GetIsValidTrainSplineMovementComponent())
	{
		return SplineLocation;
	}

	SplineLocation.Z += M_TrainSplineMovementComponent->GetSplineWorldZOffset();
	return SplineLocation;
}

void ATrainUnit::ApplyActorTransformFromSplineDistance(const float DistanceAlongSpline)
{
	const FVector ActorLocation = GetSplineWorldLocationWithMovementOffset(DistanceAlongSpline);
	const FRotator ActorRotation = GetSplineRotationForCurrentMovementDirection(DistanceAlongSpline);
	SetActorLocationAndRotation(ActorLocation, ActorRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ATrainUnit::ApplyTrainTransforms_Forward(const float ClampedLocomotiveDistance, const float SplineLength)
{
	ApplyActorTransformFromSplineDistance(ClampedLocomotiveDistance);

	const FVector LocomotiveLocation = GetSplineWorldLocationWithMovementOffset(ClampedLocomotiveDistance);
	const FRotator LocomotiveRotation = GetSplineRotationForCurrentMovementDirection(ClampedLocomotiveDistance);
	M_LocomotiveMesh->SetWorldLocationAndRotation(LocomotiveLocation, LocomotiveRotation);

	for (int32 CartIndex = 0; CartIndex < M_TrainCartMeshes.Num(); ++CartIndex)
	{
		UMeshComponent* CartMesh = M_TrainCartMeshes[CartIndex];
		if (not IsValid(CartMesh))
		{
			continue;
		}

		const float StoredOffset = M_CartDistanceOffsets[CartIndex];
		const float CartDistance = FMath::Clamp(ClampedLocomotiveDistance - StoredOffset, 0.0f, SplineLength);
		const FVector CartLocation = GetSplineWorldLocationWithMovementOffset(CartDistance);
		const FRotator CartRotation = GetSplineRotationForCurrentMovementDirection(CartDistance);
		CartMesh->SetWorldLocationAndRotation(CartLocation, CartRotation);
	}
}

void ATrainUnit::ApplyTrainTransforms_Reverse(const float ClampedLeaderDistance, const float SplineLength)
{
	ApplyActorTransformFromSplineDistance(ClampedLeaderDistance);

	const float MaxCartOffset = GetMaxCartDistanceOffset();
	const float ClampedLocomotiveDistance = FMath::Clamp(ClampedLeaderDistance - MaxCartOffset, 0.0f, SplineLength);
	const FVector LocomotiveLocation = GetSplineWorldLocationWithMovementOffset(ClampedLocomotiveDistance);
	const FRotator LocomotiveRotation = GetSplineRotationForCurrentMovementDirection(ClampedLocomotiveDistance);
	M_LocomotiveMesh->SetWorldLocationAndRotation(LocomotiveLocation, LocomotiveRotation);

	for (int32 CartIndex = 0; CartIndex < M_TrainCartMeshes.Num(); ++CartIndex)
	{
		UMeshComponent* CartMesh = M_TrainCartMeshes[CartIndex];
		if (not IsValid(CartMesh))
		{
			continue;
		}

		const float StoredOffset = M_CartDistanceOffsets[CartIndex];
		const float CartDistance = FMath::Clamp(ClampedLeaderDistance - (MaxCartOffset - StoredOffset), 0.0f, SplineLength);
		const FVector CartLocation = GetSplineWorldLocationWithMovementOffset(CartDistance);
		const FRotator CartRotation = GetSplineRotationForCurrentMovementDirection(CartDistance);
		CartMesh->SetWorldLocationAndRotation(CartLocation, CartRotation);
	}
}

void ATrainUnit::ApplyTrainTransforms(float LocomotiveDistance)
{
	if (not GetIsValidRoadSplineComponent())
	{
		return;
	}

	if (not GetHasValidTrainComposition())
	{
		return;
	}

	const float SplineLength = M_RoadSplineComponent->GetSplineLength();
	const float ClampedLocomotiveDistance = FMath::Clamp(LocomotiveDistance, 0.0f, SplineLength);
	if (not bM_IsReversing)
	{
		ApplyTrainTransforms_Forward(ClampedLocomotiveDistance, SplineLength);
		return;
	}

	ApplyTrainTransforms_Reverse(ClampedLocomotiveDistance, SplineLength);
}

void ATrainUnit::OnSplineMovementReachedTarget()
{
	OnReachedSplineMoveTarget();
}
