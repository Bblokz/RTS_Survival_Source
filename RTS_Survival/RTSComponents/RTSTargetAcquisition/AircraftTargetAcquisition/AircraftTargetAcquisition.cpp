// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftTargetAcquisition.h"

#include "DrawDebugHelpers.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace AircraftTargetAcquisitionDebug
{
	constexpr float CannotAggroAirborneZOffset = 760.f;
	constexpr float DebugStringDuration = 6.f;
}

void UAircraftTargetAcquisition::BeginPlay()
{
	M_OwningAircraft = Cast<AAircraftMaster>(GetOwner());
	(void)GetIsValidOwningAircraft();
	Super::BeginPlay();
}

ETargetPreference UAircraftTargetAcquisition::GetOwnerTargetPreference() const
{
	if (not GetIsValidOwningAircraft())
	{
		return ETargetPreference::None;
	}
	return M_OwningAircraft->GetAircraftTargetPreference();
}

bool UAircraftTargetAcquisition::CanAggroEnemies() const
{
	if (not GetIsValidOwningAircraft() || not M_OwningAircraft->GetIsUnitIdle())
	{
		return false;
	}
	if (not M_OwningAircraft->GetIsAircraftAirborne())
	{
		if constexpr (DeveloperSettings::Debugging::GTargetAcquisition_Compile_DebugSymbols)
		{
			OnAirborne_Debugging();
		}
		return false;
	}
	// Separate idle movement state check.
	if (not M_OwningAircraft->GetIsIdleAndAirborne())
	{
		return false;
	}
	return Super::CanAggroEnemies();
}

float UAircraftTargetAcquisition::GetOwnerRange() const
{
	if (not GetIsValidOwningAircraft())
	{
		return 0.f;
	}
	return M_OwningAircraft->GetHighestAircraftWeaponRange();
}

bool UAircraftTargetAcquisition::GetIsValidOwningAircraft() const
{
	if (M_OwningAircraft.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid owning aircraft for aircraft target acquisition component: " + GetName());
	return false;
}

void UAircraftTargetAcquisition::OnAirborne_Debugging() const
{
	const AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	DrawDebugString(
		GetWorld(),
		OwnerActor->GetActorLocation()
			+ FVector(0.f, 0.f, AircraftTargetAcquisitionDebug::CannotAggroAirborneZOffset),
		TEXT("Cannot aggro because aircraft is not airborne"),
		nullptr,
		FColor::Orange,
		AircraftTargetAcquisitionDebug::DebugStringDuration,
		false
	);
}
