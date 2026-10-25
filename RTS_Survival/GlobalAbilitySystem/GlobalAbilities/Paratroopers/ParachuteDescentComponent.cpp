// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ParachuteDescentComponent.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

namespace ParachuteDescentConstants
{
	constexpr float MinDescentSeconds = 1.0f;
}

UParachuteDescentComponent::UParachuteDescentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UParachuteDescentComponent::StartDescent(
	const FParatrooperDescentSettings& Settings,
	const FVector& StartLocation,
	const FVector& LandingLocation,
	const float FacingYaw)
{
	M_SquadUnit = Cast<ASquadUnit>(GetOwner());
	if (not GetIsValidSquadUnit())
	{
		OnDescentFinished.ExecuteIfBound(nullptr, false);
		return;
	}

	M_Settings = Settings;
	M_DescentState.StartLocation = StartLocation;
	M_DescentState.LandingLocation = LandingLocation;
	M_DescentState.FacingYaw = FacingYaw;
	M_DescentState.ElapsedSeconds = 0.0f;
	M_DescentState.DurationSeconds = FMath::Max(
		ParachuteDescentConstants::MinDescentSeconds,
		FVector::Dist(StartLocation, LandingLocation) / FMath::Max(1.0f, Settings.DescentSpeed));
	// Every soldier swings in his own direction and rhythm so a stick of paratroopers does not move in lockstep.
	const float SwayAxisYaw = FMath::FRandRange(0.0f, 360.0f);
	M_DescentState.SwayAxis = FRotator(0.0f, SwayAxisYaw, 0.0f).Vector();
	M_DescentState.SwayPhaseRadians = FMath::FRandRange(0.0f, UE_TWO_PI);
	M_DescentState.bM_IsDescending = true;

	StartDescent_SuspendUnit();
	StartDescent_AttachParachute();
	const FTransform StartTransform = GetSwayedTransform(StartLocation);
	M_SquadUnit->SetActorLocationAndRotation(
		StartTransform.GetLocation(),
		StartTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetComponentTickEnabled(true);
}

void UParachuteDescentComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (not M_DescentState.bM_IsDescending)
	{
		return;
	}
	if (not M_SquadUnit.IsValid() || not M_SquadUnit->IsUnitAlive())
	{
		FinishDescent(false);
		return;
	}
	TickDescent(DeltaTime);
}

void UParachuteDescentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (M_DescentState.bM_IsDescending)
	{
		if (EndPlayReason != EEndPlayReason::Destroyed)
		{
			// The world is going away (PIE stop, level change): nobody needs to release the squad.
			OnDescentFinished.Unbind();
		}
		FinishDescent(false, false);
	}
	Super::EndPlay(EndPlayReason);
}

bool UParachuteDescentComponent::GetIsValidSquadUnit() const
{
	if (M_SquadUnit.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_SquadUnit"),
		TEXT("UParachuteDescentComponent::GetIsValidSquadUnit"),
		this);
	return false;
}

void UParachuteDescentComponent::StartDescent_SuspendUnit()
{
	ASquadUnit* SquadUnit = M_SquadUnit.Get();
	SquadUnit->CancelPendingSpawnPlacement();
	SquadUnit->SetActorHiddenInGame(false);
	SquadUnit->SetActorEnableCollision(false);
	if (UCharacterMovementComponent* CharacterMovement = SquadUnit->GetCharacterMovement())
	{
		CharacterMovement->StopMovementImmediately();
		CharacterMovement->DisableMovement();
	}
	if (USelectionComponent* SelectionComponent = SquadUnit->GetSelectionComponent())
	{
		SelectionComponent->SetCanBeSelected(false);
	}
	if (AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon())
	{
		InfantryWeapon->DisableWeaponSearch(true, true);
		// Keeps the weapon silent even if target acquisition engages a target while the soldier is in the air.
		InfantryWeapon->SetCoverFireBlocked(this, true);
	}
	if (USquadUnitAnimInstance* AnimInstance = SquadUnit->GetAnimBP_SquadUnit())
	{
		AnimInstance->StartParachuteDescentAnimation(M_Settings.ParachuteMontage);
	}
}

void UParachuteDescentComponent::StartDescent_AttachParachute()
{
	if (not IsValid(M_Settings.ParachuteMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("UParachuteDescentComponent has no parachute mesh configured."));
		return;
	}
	USkeletalMeshComponent* SoldierMesh = M_SquadUnit->GetMesh();
	if (not IsValid(SoldierMesh))
	{
		return;
	}

	M_ParachuteMeshComponent = NewObject<UStaticMeshComponent>(M_SquadUnit.Get());
	M_ParachuteMeshComponent->SetStaticMesh(M_Settings.ParachuteMesh);
	M_ParachuteMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_ParachuteMeshComponent->SetCanEverAffectNavigation(false);
	M_ParachuteMeshComponent->SetupAttachment(SoldierMesh);
	M_ParachuteMeshComponent->RegisterComponent();
}

void UParachuteDescentComponent::TickDescent(const float DeltaTime)
{
	M_DescentState.ElapsedSeconds += DeltaTime;
	const float Alpha = FMath::Clamp(M_DescentState.ElapsedSeconds / M_DescentState.DurationSeconds, 0.0f, 1.0f);
	if (Alpha >= 1.0f)
	{
		FinishDescent(true);
		return;
	}

	const FVector PathLocation = FMath::Lerp(M_DescentState.StartLocation, M_DescentState.LandingLocation, Alpha);
	const FTransform SwayedTransform = GetSwayedTransform(PathLocation);
	M_SquadUnit->SetActorLocationAndRotation(
		SwayedTransform.GetLocation(),
		SwayedTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

FTransform UParachuteDescentComponent::GetSwayedTransform(const FVector& PathLocation) const
{
	const float RemainingSeconds = M_DescentState.DurationSeconds - M_DescentState.ElapsedSeconds;
	const float SettleFade = M_Settings.SwaySettleSeconds > KINDA_SMALL_NUMBER
		                         ? FMath::Clamp(RemainingSeconds / M_Settings.SwaySettleSeconds, 0.0f, 1.0f)
		                         : 1.0f;
	const float SwayCycle = UE_TWO_PI * M_DescentState.ElapsedSeconds / M_Settings.SwayPeriodSeconds;
	const float SwayDegrees = M_Settings.SwayAmplitudeDegrees * SettleFade *
		FMath::Sin(SwayCycle + M_DescentState.SwayPhaseRadians);
	const FQuat SwayRotation(M_DescentState.SwayAxis, FMath::DegreesToRadians(SwayDegrees));

	// Pendulum below the canopy: the pivot follows the path, the soldier swings around it.
	const FVector PivotOffset(0.0f, 0.0f, M_Settings.SwayPivotHeight);
	const FVector SwayedLocation = PathLocation + PivotOffset - SwayRotation.RotateVector(PivotOffset);
	const FQuat FacingRotation = FRotator(0.0f, M_DescentState.FacingYaw, 0.0f).Quaternion();
	return FTransform(SwayRotation * FacingRotation, SwayedLocation);
}

void UParachuteDescentComponent::FinishDescent(const bool bLanded, const bool bDestroySelf)
{
	if (not M_DescentState.bM_IsDescending)
	{
		return;
	}
	M_DescentState.bM_IsDescending = false;
	SetComponentTickEnabled(false);
	RemoveParachute();
	if (bLanded && M_SquadUnit.IsValid())
	{
		FinishDescent_RestoreUnit();
	}

	ASquadUnit* SquadUnit = M_SquadUnit.Get();
	// Copy first: the listener may destroy this component.
	const FOnParatrooperDescentFinished FinishedDelegate = OnDescentFinished;
	OnDescentFinished.Unbind();
	FinishedDelegate.ExecuteIfBound(SquadUnit, bLanded);
	if (bDestroySelf && IsValid(this) && not IsBeingDestroyed())
	{
		DestroyComponent();
	}
}

void UParachuteDescentComponent::FinishDescent_RestoreUnit()
{
	ASquadUnit* SquadUnit = M_SquadUnit.Get();
	SquadUnit->SetActorLocationAndRotation(
		M_DescentState.LandingLocation,
		FRotator(0.0f, M_DescentState.FacingYaw, 0.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SquadUnit->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* CharacterMovement = SquadUnit->GetCharacterMovement())
	{
		CharacterMovement->SetMovementMode(MOVE_Walking);
	}
	if (USelectionComponent* SelectionComponent = SquadUnit->GetSelectionComponent())
	{
		SelectionComponent->SetCanBeSelected(true);
	}
	if (USquadUnitAnimInstance* AnimInstance = SquadUnit->GetAnimBP_SquadUnit())
	{
		AnimInstance->StopParachuteDescentAnimation();
	}
	if (AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon())
	{
		InfantryWeapon->SetCoverFireBlocked(this, false);
		InfantryWeapon->SetAutoEngageTargets(false);
	}
}

void UParachuteDescentComponent::RemoveParachute()
{
	if (not IsValid(M_ParachuteMeshComponent))
	{
		return;
	}
	M_ParachuteMeshComponent->DestroyComponent();
	M_ParachuteMeshComponent = nullptr;
}
