// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SoloInfantryUnit.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "TimerManager.h"

namespace SoloInfantryUnitPrivate
{
	constexpr float CoverGenerationPollIntervalSeconds = 0.25f;
}

ASoloInfantryUnit::ASoloInfantryUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

bool ASoloInfantryUnit::GetIsUnitIdle() const
{
	return not GetIsPathFollowingActive();
}

void ASoloInfantryUnit::OnSpecificTargetOutOfRange(const FVector& TargetLocation)
{
	static_cast<void>(TargetLocation);
	// Hold-position infantry never walks closer to a weapon target.
}

void ASoloInfantryUnit::OnSpecificTargetDestroyedOrInvisible()
{
	SetWeaponToAutoEngageTargets(false);
}

void ASoloInfantryUnit::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupWeaponForSoloUse();
	BeginPlay_StartConfiguredAction();
}

void ASoloInfantryUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_StartCoverTimer);
	}
	ReleaseCargoSeat();
	Super::EndPlay(EndPlayReason);
}

void ASoloInfantryUnit::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAnimationStateFromVelocity();
}

bool ASoloInfantryUnit::GetIsValidSquadController() const
{
	// A solo infantry unit intentionally has no squad controller.
	return false;
}

bool ASoloInfantryUnit::GetCanUseAutomaticCover() const
{
	// Startup cover is a single explicit query; tactical updates must never start another search.
	return false;
}

bool ASoloInfantryUnit::GetMayKeepAutomaticCover() const
{
	return true;
}

bool ASoloInfantryUnit::GetMayRelocateFromCoverForStress() const
{
	// Stress must not turn the one startup query into a later cover search.
	return false;
}

bool ASoloInfantryUnit::GetIsSquadOnHoldPosition() const
{
	return true;
}

void ASoloInfantryUnit::BeginPlay_SetupWeaponForSoloUse()
{
	AInfantryWeaponMaster* InfantryWeapon = GetInfantryWeapon();
	if (not IsValid(InfantryWeapon))
	{
		return;
	}

	InfantryWeapon->SetupOwner(this);
	if (const URTSComponent* SoloRTSComponent = GetRTSComponent())
	{
		InfantryWeapon->SetOwningPlayer(SoloRTSComponent->GetOwningPlayer());
	}
	SetWeaponToAutoEngageTargets(false);
}

void ASoloInfantryUnit::BeginPlay_StartConfiguredAction()
{
	bM_HasStartCargoRequest = M_EnterCargoActor.Get() != nullptr;
	if (bM_HasStartCargoRequest)
	{
		UWorld* World = GetWorld();
		if (not IsValid(World))
		{
			return;
		}

		TWeakObjectPtr<ASoloInfantryUnit> WeakThis(this);
		FTimerDelegate EnterCargoDelegate;
		EnterCargoDelegate.BindLambda([WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->TryEnterStartCargo();
		});
		World->GetTimerManager().SetTimerForNextTick(EnterCargoDelegate);
		return;
	}

	BeginPlay_WaitForCoverGeneration();
}

void ASoloInfantryUnit::BeginPlay_WaitForCoverGeneration()
{
	if (not bM_HideInNearestFreeCover)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	TWeakObjectPtr<ASoloInfantryUnit> WeakThis(this);
	FTimerDelegate CoverReadyDelegate;
	CoverReadyDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->TryStartGameCover();
	});
	World->GetTimerManager().SetTimer(
		M_StartCoverTimer,
		CoverReadyDelegate,
		SoloInfantryUnitPrivate::CoverGenerationPollIntervalSeconds,
		true,
		0.0f);
}

void ASoloInfantryUnit::TryStartGameCover()
{
	if (bM_HasAttemptedStartingCover)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const URTSCoverFinderWorldSubsystem* CoverSubsystem =
		World->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	if (not IsValid(CoverSubsystem) || not CoverSubsystem->GetHasCompletedFullScan())
	{
		return;
	}

	bM_HasAttemptedStartingCover = true;
	World->GetTimerManager().ClearTimer(M_StartCoverTimer);
	TryEnterNearestCover();
}

void ASoloInfantryUnit::TryEnterStartCargo()
{
	if (not bM_HasStartCargoRequest || not GetIsValidEnterCargoActor())
	{
		return;
	}

	AActor* CargoActor = M_EnterCargoActor.Get();
	UCargo* CargoComponent = CargoActor->FindComponentByClass<UCargo>();
	if (not IsValid(CargoComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			CargoActor,
			"CargoComponent",
			"ASoloInfantryUnit::TryEnterStartCargo");
		return;
	}

	FName SeatSocketName = NAME_None;
	if (not CargoComponent->TryAssignSoloUnitSeat(this, SeatSocketName))
	{
		RTSFunctionLibrary::ReportError(
			"Solo infantry unit could not occupy a cargo seat at game start."
			"\n Unit: " + GetName() +
			"\n Cargo actor: " + CargoActor->GetName());
		return;
	}

	if (not AttachToCargoSeat(*CargoComponent, SeatSocketName))
	{
		CargoComponent->ReleaseSoloUnitSeat(this);
		return;
	}

	M_CurrentCargo = CargoComponent;
	bM_IsInsideCargo = true;
	OnUnitEnteredLeftCargo(CargoComponent, true);
}

bool ASoloInfantryUnit::AttachToCargoSeat(UCargo& CargoComponent, const FName SeatSocketName)
{
	UPrimitiveComponent* CargoMesh = CargoComponent.GetCargoMesh();
	if (not IsValid(CargoMesh))
	{
		return false;
	}

	const FTransform SocketWorldTransform = CargoComponent.GetSocketWorldTransform(SeatSocketName);
	const FVector SeatOffset = CargoComponent.GetSeatOffset();
	const FVector OriginalScale = GetActorScale3D();
	if (not GetIsFiniteTransform(SocketWorldTransform) || not GetIsFiniteVector(SeatOffset) ||
		not GetIsFiniteVector(OriginalScale))
	{
		RTSFunctionLibrary::ReportError(
			"Solo infantry cargo placement received a non-finite transform."
			"\n Unit: " + GetName());
		return false;
	}

	Force_TerminateMovement();
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	FTransform DesiredWorldTransform = SocketWorldTransform;
	DesiredWorldTransform.SetLocation(SocketWorldTransform.GetLocation() + SeatOffset);
	DesiredWorldTransform.SetScale3D(OriginalScale);
	if (not GetIsFiniteTransform(DesiredWorldTransform))
	{
		return false;
	}

	SetActorTransform(DesiredWorldTransform);
	AttachToComponent(CargoMesh, FAttachmentTransformRules::KeepWorldTransform, SeatSocketName);
	return true;
}

void ASoloInfantryUnit::ReleaseCargoSeat()
{
	if (not bM_IsInsideCargo)
	{
		return;
	}

	if (GetIsValidCurrentCargo())
	{
		UCargo* CargoComponent = M_CurrentCargo.Get();
		CargoComponent->ReleaseSoloUnitSeat(this);
		OnUnitEnteredLeftCargo(CargoComponent, false);
	}

	bM_IsInsideCargo = false;
	M_CurrentCargo.Reset();
}

bool ASoloInfantryUnit::GetIsValidEnterCargoActor() const
{
	if (M_EnterCargoActor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_EnterCargoActor",
		"ASoloInfantryUnit::GetIsValidEnterCargoActor",
		this);
	return false;
}

bool ASoloInfantryUnit::GetIsValidCurrentCargo() const
{
	if (M_CurrentCargo.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_CurrentCargo",
		"ASoloInfantryUnit::GetIsValidCurrentCargo",
		this);
	return false;
}

bool ASoloInfantryUnit::GetIsFiniteVector(const FVector& Vector)
{
	return FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y) && FMath::IsFinite(Vector.Z);
}

bool ASoloInfantryUnit::GetIsFiniteRotator(const FRotator& Rotator)
{
	return FMath::IsFinite(Rotator.Pitch) && FMath::IsFinite(Rotator.Yaw) && FMath::IsFinite(Rotator.Roll);
}

bool ASoloInfantryUnit::GetIsFiniteTransform(const FTransform& Transform)
{
	return GetIsFiniteVector(Transform.GetLocation()) &&
		GetIsFiniteRotator(Transform.GetRotation().Rotator()) &&
		GetIsFiniteVector(Transform.GetScale3D());
}
