// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadTargetAcquisition.h"

#include "DrawDebugHelpers.h"

#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

namespace SquadTargetAcquisitionDebug
{
	constexpr float CannotAggroCargoZOffset = 760.f;
	constexpr float DebugStringDuration = 6.f;
}

void USquadTargetAcquisition::BeginPlay()
{
	M_OwningSquad = Cast<ASquadController>(GetOwner());
	(void)GetIsValidOwningSquad();
	Super::BeginPlay();
}

ETargetPreference USquadTargetAcquisition::GetOwnerTargetPreference() const
{
	if (not GetIsValidOwningSquad())
	{
		return ETargetPreference::None;
	}

	const TArray<ASquadUnit*> SquadUnits = M_OwningSquad->GetSquadUnitsChecked();
	if (SquadUnits.IsEmpty() || not IsValid(SquadUnits[0]) || not IsValid(SquadUnits[0]->GetInfantryWeapon()))
	{
		return ETargetPreference::None;
	}
	return SquadUnits[0]->GetInfantryWeapon()->GetTargetPreference();
}

bool USquadTargetAcquisition::CanAggroEnemies() const
{
	if (not GetIsValidOwningSquad())
	{
		return false;
	}
	const UCargoSquad* CargoSquad = M_OwningSquad->FindComponentByClass<UCargoSquad>();
	if (IsValid(CargoSquad) && CargoSquad->GetIsInsideCargo())
	{
		if constexpr (DeveloperSettings::Debugging::GTargetAcquisition_Compile_DebugSymbols)
		{
			OnCargo_Debugging();
		}
		return false;
	}
	return Super::CanAggroEnemies();
}

float USquadTargetAcquisition::GetOwnerRange() const
{
	if (not GetIsValidOwningSquad())
	{
		return 0.f;
	}
	return M_OwningSquad->GetSquadRange();
}

bool USquadTargetAcquisition::GetIsValidOwningSquad() const
{
	if (M_OwningSquad.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid owning squad for squad target acquisition component: " + GetName());
	return false;
}

void USquadTargetAcquisition::OnCargo_Debugging() const
{
	const AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	DrawDebugString(
		GetWorld(),
		OwnerActor->GetActorLocation()
			+ FVector(0.f, 0.f, SquadTargetAcquisitionDebug::CannotAggroCargoZOffset),
		TEXT("Cannot aggro due to cargo"),
		nullptr,
		FColor::Orange,
		SquadTargetAcquisitionDebug::DebugStringDuration,
		false
	);
}
