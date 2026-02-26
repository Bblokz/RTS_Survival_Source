// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeapon.h"

#include "TeamWeaponAnimationInstance.h"
#include "TeamWeaponController.h"
#include "TeamWeaponMover.h"
#include "CrewPositions/CrewPosition.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ATeamWeapon::ATeamWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATeamWeapon::BeginPlay()
{
	Super::BeginPlay();

	// Always first!
	BeginPlay_InitAnimInstance();
	BeginPlay_ApplyTeamWeaponConfig(M_TeamWeaponConfig);
	BeginPlay_ForceStartPackedState();
}

void ATeamWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	M_HealthComponent = FindComponentByClass<UHealthComponent>();
	M_RTSComponent = FindComponentByClass<URTSComponent>();
	M_TeamWeaponMover = FindComponentByClass<UTeamWeaponMover>();
	M_TeamWeaponMesh = FindComponentByClass<USkeletalMeshComponent>();

	(void)GetIsValidHealthComponent();
	(void)GetIsValidRTSComponent();
	(void)GetIsValidTeamWeaponMover();
	(void)GetIsValidTeamWeaponMesh();
}


void ATeamWeapon::BeginPlay_ApplyTeamWeaponConfig(const FTeamWeaponConfig& NewConfig)
{
	M_TeamWeaponConfig = NewConfig;
	if (GetIsValidHealthComponent())
	{
		M_HealthComponent->InitHealthAndResistance(M_TeamWeaponConfig.M_ResistanceData, 0.0f);
	}
	if (GetIsValidTeamWeaponMesh())
	{
		InitEmbeddedTurret(M_TeamWeaponMesh.Get(),
		                   M_TeamWeaponConfig.M_EmbeddedSettings.TurretHeight,
		                   true,
		                   M_TeamWeaponConfig.M_YawArc.M_MinYaw,
		                   M_TeamWeaponConfig.M_YawArc.M_MaxYaw,
		                   this,
		                   M_TeamWeaponConfig.M_EmbeddedSettings.bIsArtillery,
		                   M_TeamWeaponConfig.M_EmbeddedSettings.ArtilleryDistanceUseMaxPitch);
		if (GetIsValidAnimInstance())
		{
			M_AnimInstance->InitTeamWeaponAnimInst(M_TeamWeaponConfig.M_DeploymentTime);
		}
		InitTurretsMaster(M_TeamWeaponMesh.Get());
		InitChildTurret(
			M_TeamWeaponConfig.WeaponYawRotationSpeed,
			M_TeamWeaponConfig.M_YawArc.M_MinYaw,
			M_TeamWeaponConfig.M_YawArc.M_MaxYaw,
			EIdleRotation::Idle_Animate);
	}
}

void ATeamWeapon::SetTeamWeaponController(ATeamWeaponController* NewController)
{
	M_TeamWeaponController = NewController;
}

void ATeamWeapon::BeginPlay_ForceStartPackedState()
{
	PlayPackingMontage(false);
}

void ATeamWeapon::SetTurretOwnerActor(AActor* NewOwner)
{
	if (not IsValid(NewOwner))
	{
		RTSFunctionLibrary::ReportError("NewOwner not valid in ATeamWeapon::SetTurretOwnerActor");
		return;
	}

	InitTurretOwner(NewOwner);
	InitSelectionDelegatesOfOwner(NewOwner);
	OnSetupTurret(NewOwner);
}

void ATeamWeapon::GetCrewPositions(TArray<UCrewPosition*>& OutCrewPositions) const
{
	OutCrewPositions.Reset();
	GetComponents<UCrewPosition>(OutCrewPositions);
}


void ATeamWeapon::PlayPackingMontage(const bool bWaitForMontage) const
{
	if (not GetIsValidAnimInstance())
	{
		return;
	}

	M_AnimInstance->PlayLegsWheelsSlotMontage(ETeamWeaponMontage::PackMontage, bWaitForMontage);
}

void ATeamWeapon::PlayDeployingMontage(const bool bWaitForMontage) const
{
	if (not GetIsValidAnimInstance())
	{
		return;
	}

	M_AnimInstance->PlayLegsWheelsSlotMontage(ETeamWeaponMontage::DeployMontage, bWaitForMontage);
}

void ATeamWeapon::NotifyMoverMovementState(const bool bIsMoving, const FVector& WorldVelocity)
{
	if (not GetUsesWheelMovementMontage())
	{
		bM_IsMoverMoving = bIsMoving;
		return;
	}

	if (not GetIsValidAnimInstance())
	{
		bM_IsMoverMoving = bIsMoving;
		return;
	}

	if (not bIsMoving)
	{
		bM_IsMoverMoving = false;
		M_AnimInstance->StopMoveLoop();
		return;
	}

	if (WorldVelocity.IsNearlyZero())
	{
		bM_IsMoverMoving = false;
		M_AnimInstance->StopMoveLoop();
		return;
	}

	const FVector CurrentActorForwardVector = GetActorForwardVector();
	const float ForwardDirectionDot = FVector::DotProduct(CurrentActorForwardVector, WorldVelocity.GetSafeNormal());
	const bool bForwardMovement = ForwardDirectionDot >= 0.0f;
	if (bM_IsMoverMoving == bIsMoving && bForwardMovement == bM_LastForwardMovement)
	{
		return;
	}

	bM_IsMoverMoving = bIsMoving;
	bM_LastForwardMovement = bForwardMovement;
	M_AnimInstance->StartMoveLoop(bForwardMovement);
}

bool ATeamWeapon::GetUsesWheelMovementMontage() const
{
	return false;
}

void ATeamWeapon::SetWeaponsEnabledForTeamWeaponState(const bool bEnableWeapons)
{
	if (bEnableWeapons)
	{
		if (TargetingData.GetIsTargetValid())
		{
			SetEngageSpecificTarget(TargetingData.GetTargetActor());
			return;
		}

		SetAutoEngageTargets(true);
		return;
	}

	DisableTurret();
}

void ATeamWeapon::SetSpecificEngageTarget(AActor* TargetActor)
{
	if (not IsValid(TargetActor))
	{
		SetAutoEngageTargets(false);
		return;
	}

	SetEngageSpecificTarget(TargetActor);
}

void ATeamWeapon::SetDigInHullRotationLocked(const bool bLocked)
{
	bM_IsHullRotationLocked = bLocked;
}

float ATeamWeapon::GetCurrentTurretAngle_Implementation() const
{
    if (GetIsValidAnimInstance())
    {
        return M_AnimInstance->GetCurrentYawAngle();
    }
    return 0.0f;
}


void ATeamWeapon::SetTurretAngle_Implementation(float NewAngle)
{
	if(not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->SetYaw(NewAngle);
}

void ATeamWeapon::UpdateTargetPitch_Implementation(float NewPitch)
{
	if(not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->SetPitch(NewPitch);
}

bool ATeamWeapon::TurnBase_Implementation(float Degrees)
{
	if (bM_IsHullRotationLocked)
	{
		return false;
	}

	if (not GetIsValidTeamWeaponController())
	{
		return false;
	}

	const float DesiredYaw = GetActorRotation().Yaw + Degrees;
	const FRotator DesiredRotation = FRotator(0.0f, DesiredYaw, 0.0f);
	return M_TeamWeaponController->RequestInternalRotateTowards(DesiredRotation);
}

void ATeamWeapon::PlaySingleFireAnimation_Implementation(int32 WeaponIndex)
{
	if(not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->PlayFireAnimation();
}

void ATeamWeapon::PlayBurstFireAnimation_Implementation(int32 WeaponIndex)
{
	if(not GetIsValidAnimInstance())
	{
		return;
	}
	M_AnimInstance->PlayBurstFireAnimation();
}


bool ATeamWeapon::GetIsTargetWithinYawArc(const FVector& TargetLocation) const
{
	const float MaxYaw = M_TeamWeaponConfig.M_YawArc.M_MaxYaw;
	if (FMath::IsNearlyZero(MaxYaw))
	{
		return true;
	}

	const FVector ToTarget = TargetLocation - GetActorLocation();
	const FRotator TargetRotator = ToTarget.Rotation();
	const float LocalYaw = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, TargetRotator.Yaw);
	return LocalYaw >= M_TeamWeaponConfig.M_YawArc.M_MinYaw && LocalYaw <= MaxYaw;
}

void ATeamWeapon::SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter)
{
	M_DefaultQueryFilter = NewDefaultFilter;
}

void ATeamWeapon::GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const
{
	OutLocalOffsets.Reset();
	OutLocalOffsets.Add(FVector::ZeroVector);
}

void ATeamWeapon::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
	BP_OnHealthChanged(PercentageLeft, bIsHealing);
}

bool ATeamWeapon::GetIsValidHealthComponent() const
{
	if (IsValid(M_HealthComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_HealthComponent",
	                                                      "ATeamWeapon::GetIsValidHealthComponent", this);
	return false;
}

bool ATeamWeapon::GetIsValidRTSComponent() const
{
	if (IsValid(M_RTSComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_RTSComponent",
	                                                      "ATeamWeapon::GetIsValidRTSComponent", this);
	return false;
}

bool ATeamWeapon::GetIsValidTeamWeaponMover() const
{
	if (IsValid(M_TeamWeaponMover))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeaponMover",
	                                                      "ATeamWeapon::GetIsValidTeamWeaponMover", this);
	return false;
}

bool ATeamWeapon::GetIsValidTeamWeaponMesh() const
{
	if (not M_TeamWeaponMesh.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeaponMesh",
		                                                      "ATeamWeapon::GetIsValidTeamWeaponMesh", this);
		return false;
	}
	return true;
}

bool ATeamWeapon::GetIsValidTeamWeaponController() const
{
	if (M_TeamWeaponController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeaponController",
	                                                      "ATeamWeapon::GetIsValidTeamWeaponController", this);
	return false;
}

void ATeamWeapon::BeginPlay_InitAnimInstance()
{
	if (not GetIsValidTeamWeaponMesh())
	{
		return;
	}
	UTeamWeaponAnimationInstance* AnimInst = Cast<UTeamWeaponAnimationInstance>(M_TeamWeaponMesh->GetAnimInstance());
	M_AnimInstance = AnimInst;
	(void)GetIsValidAnimInstance();
}

bool ATeamWeapon::GetIsValidAnimInstance() const
{
	if (not M_AnimInstance.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_AnimInstance",
		                                                      "ATeamWeapon::GetIsValidAnimInstance", this);
		return false;
	}
	return true;
}
