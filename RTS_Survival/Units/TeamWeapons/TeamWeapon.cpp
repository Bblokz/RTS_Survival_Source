// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeapon.h"

#include "TeamWeaponController.h"
#include "TeamWeaponMover.h"
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

	if (GetIsValidHealthComponent())
	{
		M_HealthComponent->InitHealthAndResistance(M_TeamWeaponConfig.M_ResistanceData, 0.0f);
	}
}

void ATeamWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	M_HealthComponent = FindComponentByClass<UHealthComponent>();
	M_RTSComponent = FindComponentByClass<URTSComponent>();
	M_TeamWeaponMover = FindComponentByClass<UTeamWeaponMover>();

	(void)GetIsValidHealthComponent();
	(void)GetIsValidRTSComponent();
	(void)GetIsValidTeamWeaponMover();
}

void ATeamWeapon::ApplyTeamWeaponConfig(const FTeamWeaponConfig& NewConfig)
{
	M_TeamWeaponConfig = NewConfig;
	if (GetIsValidHealthComponent())
	{
		M_HealthComponent->InitHealthAndResistance(M_TeamWeaponConfig.M_ResistanceData, 0.0f);
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
	OnSetupTurret(NewOwner);
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
