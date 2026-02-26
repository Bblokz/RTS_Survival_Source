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
	// todo this should result in the team weapon having to pack up-> rotate -> deploy to engage target.
	return false;
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
