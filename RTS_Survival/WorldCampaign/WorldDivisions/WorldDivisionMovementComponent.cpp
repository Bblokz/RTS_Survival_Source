// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionMovementComponent.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"

namespace
{
	constexpr float MinMoveDistanceSquared = 1.f;
}

UWorldDivisionMovementComponent::UWorldDivisionMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UWorldDivisionMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	M_WorldDivisionOwner = Cast<AWorldDivisionBase>(GetOwner());
}

void UWorldDivisionMovementComponent::TickComponent(const float DeltaTime,
                                                    const ELevelTick TickType,
                                                    FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (not bM_IsMoving || not GetIsValidWorldDivisionOwner())
	{
		FinishMovement();
		return;
	}

	if (not M_PathPoints.IsValidIndex(M_TargetPathPointIndex))
	{
		FinishMovement();
		return;
	}

	AWorldDivisionBase* DivisionOwner = M_WorldDivisionOwner.Get();
	const FVector CurrentLocation = DivisionOwner->GetActorLocation();
	FVector TargetLocation = M_PathPoints[M_TargetPathPointIndex];
	TargetLocation.Z = CurrentLocation.Z;

	const FVector2D CurrentXY(CurrentLocation.X, CurrentLocation.Y);
	const FVector2D TargetXY(TargetLocation.X, TargetLocation.Y);
	const float DistanceSquared = FVector2D::DistSquared(CurrentXY, TargetXY);
	if (DistanceSquared <= MinMoveDistanceSquared)
	{
		DivisionOwner->SetActorLocation(TargetLocation);
		M_TargetPathPointIndex++;
		if (not M_PathPoints.IsValidIndex(M_TargetPathPointIndex))
		{
			FinishMovement();
		}
		return;
	}

	const float StepDistance = FMath::Max(0.f, M_Speed) * DeltaTime;
	if (StepDistance <= 0.f)
	{
		return;
	}

	const float Distance = FMath::Sqrt(DistanceSquared);
	if (StepDistance >= Distance)
	{
		DivisionOwner->SetActorLocation(TargetLocation);
		M_TargetPathPointIndex++;
		if (not M_PathPoints.IsValidIndex(M_TargetPathPointIndex))
		{
			FinishMovement();
		}
		return;
	}

	const FVector2D Direction = (TargetXY - CurrentXY).GetSafeNormal();
	const FVector2D NewXY = CurrentXY + Direction * StepDistance;
	DivisionOwner->SetActorLocation(FVector(NewXY.X, NewXY.Y, CurrentLocation.Z));
}

void UWorldDivisionMovementComponent::StartMovement(const TArray<FVector>& PathPoints, const float Speed)
{
	M_PathPoints = PathPoints;
	M_TargetPathPointIndex = 1;
	M_Speed = Speed;
	bM_IsMoving = M_PathPoints.Num() >= 2 && M_Speed > 0.f;
	SetComponentTickEnabled(bM_IsMoving);

	if (not bM_IsMoving)
	{
		FinishMovement();
	}
}

void UWorldDivisionMovementComponent::StopMovement()
{
	FinishMovement();
}

bool UWorldDivisionMovementComponent::GetIsValidWorldDivisionOwner() const
{
	if (M_WorldDivisionOwner.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldDivisionOwner"),
		TEXT("UWorldDivisionMovementComponent::GetIsValidWorldDivisionOwner"),
		this);
	return false;
}

void UWorldDivisionMovementComponent::FinishMovement()
{
	if (not bM_IsMoving)
	{
		SetComponentTickEnabled(false);
		return;
	}

	bM_IsMoving = false;
	SetComponentTickEnabled(false);
	AWorldDivisionBase* DivisionOwner = M_WorldDivisionOwner.Get();
	M_OnMovementFinished.Broadcast(DivisionOwner);
}
