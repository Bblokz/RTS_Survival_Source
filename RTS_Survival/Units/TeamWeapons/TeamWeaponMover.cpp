// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponMover.h"

#include "TeamWeapon.h"
#include "TeamWeaponController.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "NavigationPath.h"

UTeamWeaponMover::UTeamWeaponMover()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTeamWeaponMover::BeginPlay()
{
	Super::BeginPlay();



	UpdateTickEnabledState();
}

void UTeamWeaponMover::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bM_IsPushedActive)
	{
		UpdatePushedMovement(DeltaTime);
		return;
	}

	if (bM_IsLegacyFollowActive)
	{
		UpdateLegacyMovement();
	}
}

void UTeamWeaponMover::InitMover(ATeamWeapon* InTeamWeapon, ATeamWeaponController* InController)
{
	M_TeamWeapon = InTeamWeapon;
	M_TeamWeaponController = InController;

	if (GetIsValidTeamWeapon())
	{
		M_LastWeaponLocation = M_TeamWeapon->GetActorLocation();
	}
}

void UTeamWeaponMover::SetCrewMembersToFollow(const TArray<FTeamWeaponCrewMemberOffset>& CrewMembers)
{
	M_CrewMembers = CrewMembers;
}

void UTeamWeaponMover::MoveWeaponToLocation(const FVector& Destination)
{
	M_MoveDestination = Destination;
	bM_HasMoveRequest = true;
}

void UTeamWeaponMover::NotifyCrewReady(const bool bIsReady)
{
	bM_IsCrewReady = bIsReady;
}

void UTeamWeaponMover::StartLegacyFollowCrew()
{
	if (not GetUsesLegacyCrewMode())
	{
		return;
	}

	if (not bM_HasMoveRequest)
	{
		return;
	}

	if (not bM_IsCrewReady)
	{
		OnPushedMoveFailed.Broadcast(TEXT("Crew not ready for movement"));
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		OnPushedMoveFailed.Broadcast(TEXT("Team weapon missing"));
		return;
	}

	if (not GetIsCrewDataValid())
	{
		OnPushedMoveFailed.Broadcast(TEXT("Crew data invalid"));
		return;
	}

	bM_IsLegacyFollowActive = true;
	bM_IsPushedActive = false;
	M_LastWeaponLocation = M_TeamWeapon->GetActorLocation();
	UpdateTickEnabledState();
	UpdateLegacyMovement();
}

void UTeamWeaponMover::StopLegacyFollowCrew()
{
	bM_IsLegacyFollowActive = false;
	bM_HasMoveRequest = false;

	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->NotifyMoverMovementState(false, FVector::ZeroVector);
	}

	UpdateTickEnabledState();
}

void UTeamWeaponMover::StartPushedFollowPath(const FNavPathSharedPtr& InPath)
{
	StopLegacyFollowCrew();

	if (not GetIsValidTeamWeapon())
	{
		StopLegacyFollowCrew();
		OnPushedMoveFailed.Broadcast(TEXT("Team weapon missing"));
		return;
	}

	if (not GetIsValidTeamWeaponController())
	{
		OnPushedMoveFailed.Broadcast(TEXT("Team weapon controller missing"));
		return;
	}

	if (not InPath.IsValid() || InPath->GetPathPoints().Num() == 0)
	{
		OnPushedMoveFailed.Broadcast(TEXT("Invalid pushed path"));
		return;
	}

	M_PushedPath = InPath;
	M_CurrentPathPointIndex = 0;
	bM_IsPushedActive = true;
	M_LastWeaponLocation = M_TeamWeapon->GetActorLocation();
	SyncControllerTransformToWeapon();
	UpdateTickEnabledState();
}

void UTeamWeaponMover::AbortPushedFollowPath(const FString& Reason)
{
	if (not bM_IsPushedActive)
	{
		return;
	}

	StopPushedMovement();
	OnPushedMoveFailed.Broadcast(Reason);
}

bool UTeamWeaponMover::GetIsPushedFollowingPath() const
{
	return bM_IsPushedActive;
}

void UTeamWeaponMover::UpdateLegacyMovement()
{
	if (not GetUsesLegacyCrewMode())
	{
		StopLegacyFollowCrew();
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		StopLegacyFollowCrew();
		OnPushedMoveFailed.Broadcast(TEXT("Team weapon missing"));
		return;
	}

	UpdateOwnerLocationFromCrew();
	UpdateMovementAnimationState(0.0f);

	if (not HaveCrewReachedDestination())
	{
		return;
	}

	StopLegacyFollowCrew();
	OnPushedMoveArrived.Broadcast();
}

void UTeamWeaponMover::UpdatePushedMovement(const float DeltaSeconds)
{
	if (not bM_IsPushedActive)
	{
		return;
	}

	if (not GetIsValidTeamWeapon() || not GetIsValidPushedPath())
	{
		AbortPushedFollowPath(TEXT("Pushed movement dependencies became invalid"));
		return;
	}

	if (DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const TArray<FNavPathPoint>& PathPoints = M_PushedPath->GetPathPoints();
	if (M_CurrentPathPointIndex >= PathPoints.Num())
	{
		FinalizePushedArrived();
		return;
	}

	const float AcceptanceRadiusCm = M_TeamWeapon->GetPathAcceptanceRadiusCm();
	const FVector WeaponLocation = M_TeamWeapon->GetActorLocation();
	if (TryAdvancePathIndexIfReachedTarget(WeaponLocation, AcceptanceRadiusCm))
	{
		if (M_CurrentPathPointIndex >= PathPoints.Num())
		{
			FinalizePushedArrived();
			return;
		}
	}

	const FVector TargetLocation = PathPoints[M_CurrentPathPointIndex].Location;
	FVector DirectionToTarget = TargetLocation - WeaponLocation;
	DirectionToTarget.Z = 0.0f;
	const float DistanceToTarget2D = DirectionToTarget.Size();
	if (DistanceToTarget2D <= KINDA_SMALL_NUMBER)
	{
		M_CurrentPathPointIndex++;
		return;
	}

	const FVector MoveDirection = DirectionToTarget / DistanceToTarget2D;
	const float MaxStepDistance = M_TeamWeapon->GetPushedMoveSpeedCmPerSec() * DeltaSeconds;
	const float StepDistance = FMath::Min(MaxStepDistance, DistanceToTarget2D);
	FVector ProposedLocation = WeaponLocation + MoveDirection * StepDistance;

	FVector GroundAdjustedLocation = ProposedLocation;
	if (TryGetGroundAdjustedLocation(ProposedLocation, GroundAdjustedLocation))
	{
		ProposedLocation = GroundAdjustedLocation;
	}
	else
	{
		ProposedLocation.Z = WeaponLocation.Z;
	}

	ApplyYawStepTowards(MoveDirection, DeltaSeconds);

	FVector AppliedLocation = WeaponLocation;
	if (not TryMoveWeaponWithSweep(ProposedLocation, AppliedLocation))
	{
		AbortPushedFollowPath(TEXT("Blocked while pushed-following path"));
		return;
	}

	if (TryAdvancePathIndexIfReachedTarget(AppliedLocation, AcceptanceRadiusCm))
	{
		if (M_CurrentPathPointIndex >= PathPoints.Num())
		{
			FinalizePushedArrived();
			return;
		}
	}

	UpdateMovementAnimationState(DeltaSeconds);
}

void UTeamWeaponMover::FinalizePushedArrived()
{
	StopPushedMovement();
	OnPushedMoveArrived.Broadcast();
}

void UTeamWeaponMover::StopPushedMovement()
{
	bM_IsPushedActive = false;
	M_PushedPath.Reset();
	M_CurrentPathPointIndex = 0;

	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->NotifyMoverMovementState(false, FVector::ZeroVector);
	}

	UpdateTickEnabledState();
}

bool UTeamWeaponMover::GetIsCrewDataValid() const
{
	if (M_CrewMembers.Num() > 0)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_CrewMembers"),
		TEXT("UTeamWeaponMover::GetIsCrewDataValid"), GetOwner());
	return false;
}

bool UTeamWeaponMover::TryGetCrewCenterLocation(FVector& OutCenter) const
{
	int32 ValidCrewCount = 0;
	FVector AccumulatedLocation = FVector::ZeroVector;

	for (const FTeamWeaponCrewMemberOffset& CrewEntry : M_CrewMembers)
	{
		if (not CrewEntry.M_CrewMember.IsValid())
		{
			continue;
		}

		AccumulatedLocation += CrewEntry.M_CrewMember->GetActorLocation();
		ValidCrewCount++;
	}

	if (ValidCrewCount == 0)
	{
		return false;
	}

	OutCenter = AccumulatedLocation / static_cast<float>(ValidCrewCount);
	return true;
}

bool UTeamWeaponMover::HaveCrewReachedDestination() const
{
	const float ReachedAcceptanceRadius = DeveloperSettings::GamePlay::Navigation::SquadUnitAcceptanceRadius;
	const float AcceptanceDistanceSquared = FMath::Square(AcceptanceRadius);

	bool bHasValidCrewMember = false;
	for (const FTeamWeaponCrewMemberOffset& CrewEntry : M_CrewMembers)
	{
		if (not CrewEntry.M_CrewMember.IsValid())
		{
			continue;
		}

		bHasValidCrewMember = true;
		const FVector ExpectedLocation = M_MoveDestination + CrewEntry.M_OffsetFromCenter;
		const float DistanceSquared = FVector::DistSquared(CrewEntry.M_CrewMember->GetActorLocation(), ExpectedLocation);
		if (DistanceSquared > AcceptanceDistanceSquared)
		{
			return false;
		}
	}

	return bHasValidCrewMember;
}

void UTeamWeaponMover::UpdateOwnerLocationFromCrew()
{
	if (not GetIsValidTeamWeapon())
	{
		StopLegacyFollowCrew();
		OnPushedMoveFailed.Broadcast(TEXT("Team weapon missing"));
		return;
	}

	FVector CrewCenter = FVector::ZeroVector;
	if (not TryGetCrewCenterLocation(CrewCenter))
	{
		StopLegacyFollowCrew();
		OnPushedMoveFailed.Broadcast(TEXT("Crew missing"));
		return;
	}

	M_TeamWeapon->SetActorLocation(CrewCenter);
	SyncControllerTransformToWeapon();
}

void UTeamWeaponMover::UpdateMovementAnimationState(const float DeltaSeconds)
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	const FVector CurrentOwnerLocation = M_TeamWeapon->GetActorLocation();
	FVector Velocity = FVector::ZeroVector;
	if (DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		Velocity = (CurrentOwnerLocation - M_LastWeaponLocation) / DeltaSeconds;
	}
	else
	{
		Velocity = CurrentOwnerLocation - M_LastWeaponLocation;
	}

	M_LastWeaponLocation = CurrentOwnerLocation;

	const bool bCurrentlyMoving = not Velocity.IsNearlyZero();
	M_TeamWeapon->NotifyMoverMovementState(bCurrentlyMoving, Velocity);
}

bool UTeamWeaponMover::TryAdvancePathIndexIfReachedTarget(const FVector& WeaponLocation, const float AcceptanceRadiusCm)
{
	if (not GetIsValidPushedPath())
	{
		return false;
	}

	const TArray<FNavPathPoint>& PathPoints = M_PushedPath->GetPathPoints();
	if (M_CurrentPathPointIndex >= PathPoints.Num())
	{
		return true;
	}

	const FVector TargetLocation = PathPoints[M_CurrentPathPointIndex].Location;
	const FVector PlanarToTarget = FVector(TargetLocation.X - WeaponLocation.X, TargetLocation.Y - WeaponLocation.Y, 0.0f);
	if (PlanarToTarget.SizeSquared() > FMath::Square(AcceptanceRadiusCm))
	{
		return false;
	}

	M_CurrentPathPointIndex++;
	return true;
}

bool UTeamWeaponMover::TryGetGroundAdjustedLocation(const FVector& InProposedLocation, FVector& OutGroundAdjustedLocation) const
{
	if (not GetIsValidTeamWeapon())
	{
		return false;
	}

	UWorld* World = M_TeamWeapon->GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	const FVector TraceStart = InProposedLocation + FVector::UpVector * M_TeamWeapon->GetGroundTraceStartHeightCm();
	const FVector TraceEnd = TraceStart - FVector::UpVector * M_TeamWeapon->GetGroundTraceLengthCm();

	FHitResult GroundHit;
	FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(TeamWeaponGroundTrace), false, M_TeamWeapon.Get());
	for (const FTeamWeaponCrewMemberOffset& CrewMemberOffset : M_CrewMembers)
	{
		if (not CrewMemberOffset.M_CrewMember.IsValid())
		{
			continue;
		}

		CollisionQueryParams.AddIgnoredActor(CrewMemberOffset.M_CrewMember.Get());
	}

	const bool bDidHit = World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		CollisionQueryParams);
	if (not bDidHit)
	{
		return false;
	}

	OutGroundAdjustedLocation = InProposedLocation;
	OutGroundAdjustedLocation.Z = GroundHit.ImpactPoint.Z;
	return true;
}

void UTeamWeaponMover::ApplyYawStepTowards(const FVector& MovementDirection, const float DeltaSeconds) const
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	const float DesiredYaw = MovementDirection.Rotation().Yaw;
	const float CurrentYaw = M_TeamWeapon->GetActorRotation().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);
	const float MaxYawStep = M_TeamWeapon->GetTurnSpeedYaw() * DeltaSeconds;
	const float AppliedYawStep = FMath::Clamp(DeltaYaw, -MaxYawStep, MaxYawStep);
	const FRotator NewRotation(0.0f, CurrentYaw + AppliedYawStep, 0.0f);
	M_TeamWeapon->SetActorRotation(NewRotation);
	SyncControllerTransformToWeapon();
}

bool UTeamWeaponMover::TryMoveWeaponWithSweep(const FVector& ProposedLocation, FVector& OutAppliedLocation) const
{
	if (not GetIsValidTeamWeapon())
	{
		return false;
	}

	FHitResult HitResult;
	const bool bSweep = true;
	const bool bMoved = M_TeamWeapon->SetActorLocation(ProposedLocation, bSweep, &HitResult, ETeleportType::None);
	OutAppliedLocation = M_TeamWeapon->GetActorLocation();
	if (not bMoved)
	{
		return false;
	}

	SyncControllerTransformToWeapon();
	return true;
}

void UTeamWeaponMover::SyncControllerTransformToWeapon() const
{
	if (not GetIsValidTeamWeaponController())
	{
		return;
	}

	M_TeamWeaponController->SetActorLocationAndRotation(
		M_TeamWeapon->GetActorLocation(),
		M_TeamWeapon->GetActorRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

bool UTeamWeaponMover::GetUsesLegacyCrewMode() const
{
	if (not GetIsValidTeamWeapon())
	{
		return true;
	}

	return M_TeamWeapon->GetMovementType() == ETeamWeaponMovementType::CrewFollowsWeapon;
}

bool UTeamWeaponMover::GetIsValidTeamWeapon() const
{
	if (M_TeamWeapon.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_TeamWeapon"),
		TEXT("UTeamWeaponMover::GetIsValidTeamWeapon"), GetOwner());
	return false;
}

bool UTeamWeaponMover::GetIsValidTeamWeaponController() const
{
	if (M_TeamWeaponController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_TeamWeaponController"),
		TEXT("UTeamWeaponMover::GetIsValidTeamWeaponController"), GetOwner());
	return false;
}

bool UTeamWeaponMover::GetIsValidPushedPath() const
{
	if (M_PushedPath.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PushedPath"),
		TEXT("UTeamWeaponMover::GetIsValidPushedPath"), GetOwner());
	return false;
}

void UTeamWeaponMover::UpdateTickEnabledState()
{
	SetComponentTickEnabled(bM_IsLegacyFollowActive || bM_IsPushedActive);
}
