// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TankTargetAcquisition.h"

#include "DrawDebugHelpers.h"

#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace TankTargetAcquisitionDebug
{
	constexpr float CannotAggroDigInZOffset = 760.f;
	constexpr float DebugStringDuration = 6.f;
}


// Sets default values for this component's properties
UTankTargetAcquisition::UTankTargetAcquisition()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void UTankTargetAcquisition::BeginPlay()
{
	M_OwningTank = Cast<ATrackedTankMaster>(GetOwner());
	(void)EnsureIsValidOwningTank();
	Super::BeginPlay();
}

void UTankTargetAcquisition::IssueAttackClosestVisibleTargetInAggroRange(AActor* TargetActor)
{
	Super::IssueAttackClosestVisibleTargetInAggroRange(TargetActor);
}

ETargetPreference UTankTargetAcquisition::GetOwnerTargetPreference() const
{
	if (not EnsureIsValidOwningTank())
	{
		return ETargetPreference::None;
	}

	const TArray<ACPPTurretsMaster*> Turrets = M_OwningTank->GetTurrets();
	if (Turrets.IsEmpty() || not IsValid(Turrets[0]))
	{
		return ETargetPreference::None;
	}

	return Turrets[0]->GetTargetPreference();
}

float UTankTargetAcquisition::GetOwnerRange() const
{
	if (not EnsureIsValidOwningTank())
	{
		return 0.f;
	}

	return FMath::Max(0.f, M_OwningTank->GetVehicleHighestWeaponRange());
}

bool UTankTargetAcquisition::CanAggroEnemies() const
{
	if (not EnsureIsValidOwningTank() || not M_OwningTank->GetIsUnitIdle()
		|| not M_OwningTank->DoesVehicleHaveAnyWeapons())
	{
		return false;
	}
	UDigInComponent* DigIn = M_OwningTank->GetDigInComponent();
	if (not IsValid(DigIn))
	{
		return false;
	}
	if (not DigIn->GetIsMovable())
	{
		if constexpr (DeveloperSettings::Debugging::GTargetAcquisition_Compile_DebugSymbols)
		{
			OnDigIn_Debugging();
		}
		return false;
	}
	// Check for enum state == Aggressive.
	return Super::CanAggroEnemies();
}

bool UTankTargetAcquisition::EnsureIsValidOwningTank() const
{
	if (not M_OwningTank.IsValid())
	{
		RTSFunctionLibrary::ReportError("no valid owning tank for target acquistion tank component: " + GetName());
		return false;
	}
	return true;
}

void UTankTargetAcquisition::OnDigIn_Debugging() const
{
	const AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	DrawDebugString(
		GetWorld(),
		OwnerActor->GetActorLocation()
		+ FVector(0.f, 0.f, TankTargetAcquisitionDebug::CannotAggroDigInZOffset),
		TEXT("Cannot aggro due to dig in"),
		nullptr,
		FColor::Orange,
		TankTargetAcquisitionDebug::DebugStringDuration,
		false
	);
}
