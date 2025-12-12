// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "HPActorObjectsMaster.h"


#include "RTS_Survival/Collapse/FRTS_Collapse/FRTS_Collapse.h"
#include "RTS_Survival/Weapons/CPPWeaponsMaster.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

AHPActorObjectsMaster::AHPActorObjectsMaster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), HealthComponent(nullptr), RTSComponent(nullptr)
{
}

void AHPActorObjectsMaster::TriggerDestroyActor(const ERTSDeathType DeathType)
{
	UnitDies(DeathType);
}

float AHPActorObjectsMaster::TakeDamage(
	float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (not IsUnitAlive())
	{
		// Do not trigger kill on a dead unit.
		return 1.f;
	}
	if (IsValid(HealthComponent))
	{
		GotHit(DamageEvent);

		const ERTSDamageType RtsDamageType = FRTSWeaponHelpers::GetRTSDamageType(DamageEvent);

		if (HealthComponent->TakeDamage(DamageAmount, RtsDamageType))
		{
			const ERTSDeathType DeathType = FRTSWeaponHelpers::TranslateDamageIntoDeathType(RtsDamageType);
			UnitDies(DeathType);
			return 0.0;
		}
		return 1.0;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this,
	                                             "HealthComponent",
	                                             "AHPActorObjectsMaster::TakeDamage");
	return float();
}

void AHPActorObjectsMaster::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
	Bp_OnHealthChanged(PercentageLeft);
}

bool AHPActorObjectsMaster::GetIsValidHealthComponent() const
{
	if (IsValid(HealthComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid hp component for actor: " + GetName() +
		"AHPActorObjectsMaster::GetIsValidHealthComponent");
	return false;
}

bool AHPActorObjectsMaster::GetIsValidRTSComponent() const
{
	if (IsValid(RTSComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid RTS component for actor: " + GetName() +
		"AHPActorObjectsMaster::GetIsValidRTSComponent");
	return false;
}

void AHPActorObjectsMaster::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (not bDoNotUseRTSComponent)
	{
		RTSComponent = FindComponentByClass<URTSComponent>();
		if (!IsValid(RTSComponent))
		{
			RTSFunctionLibrary::ReportNullErrorComponent(this, "RTSComponent ",
			                                             "AHPActorObjectsMaster::PostInitializeComponents");
		}
	}
	HealthComponent = FindComponentByClass<UHealthComponent>();
	if (!IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "HealthComponent",
		                                             "AHPActorObjectsMaster::PostInitializeComponents");
	}
}

void AHPActorObjectsMaster::DestroyAndSpawnActors(const FDestroySpawnActorsParameters& SpawnParams,
                                                  FCollapseFX CollapseFX)
{
	FRTS_Collapse::OnDestroySpawnActors(this,
	                                    SpawnParams,
	                                    CollapseFX);
}
