// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleModuleWeaponBehaviour.h"

#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

namespace VehicleModuleWeaponConstants
{
	constexpr int32 PerfectAccuracyRating = 100;
}

UVehicleModuleWeaponBehaviour::UVehicleModuleWeaponBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::None;
	M_BuffType = EBuffDebuffType::Debuff;
	M_TitleText = TEXT("Damaged Weapon");
	M_DisplayText = TEXT("The bound gun loses accuracy and fires more slowly; a destroyed gun cannot fire.");
	BehaviourWeaponMultipliers.BaseCooldownMlt =
		VehicleModuleBalance::DefaultBehaviours::WeaponYellowFiringCycleMultiplier;
}

bool UVehicleModuleWeaponBehaviour::CheckRequirement(UWeaponState* WeaponState) const
{
	if (not IsValid(WeaponState) || GetModuleContext().Type != EVehicleModuleTypes::Weapon)
	{
		return false;
	}
	UMeshComponent* BoundMesh = GetBoundMesh();
	if (not IsValid(BoundMesh))
	{
		return false;
	}
	if (WeaponState->GetWeaponMeshComponent() == BoundMesh)
	{
		return true;
	}
	UObject* WeaponOwner = WeaponState->GetWeaponOwnerObject();
	if (const ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(WeaponOwner))
	{
		return Turret->GetModuleBindingMesh() == BoundMesh
			&& Turret->GetWeaponCount() == 1;
	}
	if (const UHullWeaponComponent* HullWeapon = Cast<UHullWeaponComponent>(WeaponOwner))
	{
		return HullWeapon->GetModuleBindingMesh() == BoundMesh
			&& HullWeapon->GetWeaponCount() == 1;
	}
	return false;
}

void UVehicleModuleWeaponBehaviour::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		return;
	}
	if (GetModuleContext().State == EVehicleModuleState::Destroyed)
	{
		WeaponState->SetModuleFireRestriction(this, true);
		return;
	}
	if (GetModuleContext().State != EVehicleModuleState::Damaged)
	{
		return;
	}
	Super::ApplyBehaviourToWeapon(WeaponState);
	const FWeaponData& WeaponData = WeaponState->GetRawWeaponData();
	const int32 BaseAccuracy = WeaponData.Accuracy - WeaponData.BehaviourAttributes.Accuracy;
	const float BaseDispersion = static_cast<float>(VehicleModuleWeaponConstants::PerfectAccuracyRating - BaseAccuracy);
	const int32 DesiredAccuracy = FMath::Clamp(FMath::RoundToInt(
		static_cast<float>(VehicleModuleWeaponConstants::PerfectAccuracyRating)
		- BaseDispersion * VehicleModuleBalance::DefaultBehaviours::WeaponYellowDispersionMultiplier),
		0, VehicleModuleWeaponConstants::PerfectAccuracyRating);
	const int32 AccuracyDelta = DesiredAccuracy - BaseAccuracy;
	FBehaviourWeaponAttributes AccuracyChange;
	AccuracyChange.Accuracy = AccuracyDelta;
	WeaponState->Upgrade(AccuracyChange);
	M_AccuracyDeltaByWeapon.Add(TWeakObjectPtr<UWeaponState>(WeaponState), AccuracyDelta);
}

void UVehicleModuleWeaponBehaviour::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		return;
	}
	WeaponState->SetModuleFireRestriction(this, false);
	const TWeakObjectPtr<UWeaponState> WeakWeapon(WeaponState);
	if (const int32* AccuracyDelta = M_AccuracyDeltaByWeapon.Find(WeakWeapon))
	{
		FBehaviourWeaponAttributes AccuracyChange;
		AccuracyChange.Accuracy = *AccuracyDelta;
		WeaponState->Upgrade(AccuracyChange, false);
		M_AccuracyDeltaByWeapon.Remove(WeakWeapon);
	}
	Super::RemoveBehaviourFromWeapon(WeaponState);
}

void UVehicleModuleWeaponBehaviour::OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext)
{
	RefreshAppliedWeaponEffects();
	Super::OnModuleContextUpdated(PreviousContext);
}
