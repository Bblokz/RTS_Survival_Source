// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleModuleDamageEvent.h"

#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"

FVehicleModuleDamageEvent FVehicleModuleDamageEvent::MakeBallisticEvent(
	const FVehicleModuleBallisticHit& BallisticHit,
	const TSubclassOf<UDamageType> InDamageTypeClass)
{
	FVehicleModuleDamageEvent DamageEvent;
	DamageEvent.DamageTypeClass = InDamageTypeClass;
	DamageEvent.HitInfo.Location = BallisticHit.HitLocation;
	DamageEvent.ArmorCalculation = BallisticHit.ArmorCalculation;
	DamageEvent.PlateHit = BallisticHit.PlateHit;
	DamageEvent.EffectiveArmor = BallisticHit.EffectiveArmor;
	DamageEvent.ProjectileBaseDamage = BallisticHit.ProjectileBaseDamage;
	DamageEvent.ProjectileCalibre = BallisticHit.ProjectileCalibre;
	DamageEvent.bPenetrated = BallisticHit.bPenetrated;
	if (IsValid(BallisticHit.ArmorCalculation))
	{
		DamageEvent.HitContext = BallisticHit.ArmorCalculation->MakeHitContextForWorldHit(BallisticHit.HitLocation);
	}
	DamageEvent.HitContext.StablePlateRegistrationId = BallisticHit.PlateRegistrationId;
	DamageEvent.HitContext.ShellType = BallisticHit.ShellType;
	DamageEvent.HitContext.DamageType = ERTSDamageType::Kinetic;
	DamageEvent.HitContext.DeliveryType = EVehicleModuleDelivery::Ballistic;
	DamageEvent.HitContext.bOverpenetrating = BallisticHit.bOverpenetrating;
	DamageEvent.HitContext.ShotActivationId = BallisticHit.ShotActivationId;
	DamageEvent.HitContext.ImpactOrdinal = BallisticHit.ImpactOrdinal;
	return DamageEvent;
}

FVehicleModuleDamageEvent FVehicleModuleDamageEvent::MakeExplosionEvent(
	UArmorCalculation* Armor,
	const EVehicleModuleDelivery Delivery,
	const FVector& InExplosionLocation,
	const float SourceDamage,
	const uint32 ShotActivationId,
	const TSubclassOf<UDamageType> InDamageTypeClass,
	const EWeaponShellType DamageShellType)
{
	FVehicleModuleDamageEvent DamageEvent;
	DamageEvent.DamageTypeClass = InDamageTypeClass;
	DamageEvent.HitInfo.Location = InExplosionLocation;
	DamageEvent.ArmorCalculation = Armor;
	DamageEvent.ExplosionLocation = InExplosionLocation;
	DamageEvent.ProjectileBaseDamage = SourceDamage;
	if (IsValid(Armor))
	{
		DamageEvent.HitContext = Armor->MakeHitContextForWorldHit(InExplosionLocation);
	}
	DamageEvent.HitContext.DeliveryType = Delivery;
	DamageEvent.HitContext.DamageType = ERTSDamageType::Kinetic;
	DamageEvent.HitContext.ShellType = DamageShellType;
	DamageEvent.HitContext.ShotActivationId = ShotActivationId;
	return DamageEvent;
}

void FVehicleModuleDamageEvent::ApplyModuleDamageAfterHullDamage(const float AppliedHullDamage) const
{
	if (not FMath::IsFinite(AppliedHullDamage) || AppliedHullDamage <= 0.f)
	{
		return;
	}
	ResolveModuleDamage(AppliedHullDamage);
}

void FVehicleModuleDamageEvent::ApplyModuleDamageWithoutHullDamage() const
{
	if (bPenetrated)
	{
		return;
	}
	ResolveModuleDamage(0.f);
}

void FVehicleModuleDamageEvent::ResolveModuleDamage(const float AppliedHullDamage) const
{
	UArmorCalculation* Armor = ArmorCalculation.Get();
	if (not IsValid(Armor))
	{
		return;
	}

	switch (HitContext.DeliveryType)
	{
	case EVehicleModuleDelivery::Mine:
		Armor->CalculateMineModuleDamage(ExplosionLocation, ProjectileBaseDamage, HitContext);
		break;
	case EVehicleModuleDelivery::Splash:
		Armor->CalculateSplashModuleDamage(ExplosionLocation, ProjectileBaseDamage, HitContext);
		break;
	case EVehicleModuleDelivery::Ballistic:
		Armor->CalculateModuleDamage(PlateHit, EffectiveArmor, bPenetrated, AppliedHullDamage,
		                             ProjectileBaseDamage, ProjectileCalibre, HitContext);
		break;
	}
}
