// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GarrisonableBuilding.h"

#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"


AGarrisonableBuilding::AGarrisonableBuilding(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AGarrisonableBuilding::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
	BP_OnHealthChanged(PercentageLeft, bIsHealing);
}

void AGarrisonableBuilding::OnSquadRegistered(ASquadController* SquadController)
{
	if (not EnsureHealthComponentIsValid())
	{
		return;
	}
	FHealthBarVisibilitySettings NewSettings = M_CachedVisibilityPreGarrison;
	NewSettings.bAlwaysDisplay = true;
	HealthComponent->ChangeVisibilitySettings(NewSettings);
}

void AGarrisonableBuilding::OnCargoEmpty()
{
	if (not EnsureHealthComponentIsValid())
	{
		return;
	}
	HealthComponent->ChangeVisibilitySettings(M_CachedVisibilityPreGarrison);
}

void AGarrisonableBuilding::BeginPlay()
{
	Super::BeginPlay();
	// Cache the visibility settings before garrisoning.
	if (EnsureHealthComponentIsValid())
	{
		M_CachedVisibilityPreGarrison = HealthComponent->GetVisibilitySettings();
	}
}

void AGarrisonableBuilding::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Initialize the health component
	HealthComponent = FindComponentByClass<UHealthComponent>();
	if (!IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "HealthComponent",
		                                             "AGarrisonableBuilding::PostInitializeComponents");
	}

	// Initialize the RTS component
	RTSComponent = FindComponentByClass<URTSComponent>();
	if (!IsValid(RTSComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "RTSComponent",
		                                             "AGarrisonableBuilding::PostInitializeComponents");
	}
	CargoComponent = FindComponentByClass<UCargo>();
	if (!IsValid(CargoComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "CargoComponent",
		                                             "AGarrisonableBuilding::PostInitializeComponents");
	}
}

float AGarrisonableBuilding::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                        AController* EventInstigator, AActor* DamageCauser)
{
	if (not IsUnitAlive())
	{
		// Do not trigger kill on a dead unit.
		return 1.f;
	}
	if (HealthComponent)
	{
		const ERTSDamageType RtsDamageType = FRTSWeaponHelpers::GetRTSDamageType(DamageEvent);
		if (HealthComponent->TakeDamage(DamageAmount, RtsDamageType))
		{
			const ERTSDeathType DeathType = FRTSWeaponHelpers::TranslateDamageIntoDeathType(RtsDamageType);
			OnUnitDies(DeathType);
			return 0.0;
		}
		return 1.0;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this,
	                                             "HealthComponent",
	                                             "AGarrisonableBuilding::TakeDamage");
	return float();
}

void AGarrisonableBuilding::OnUnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		return;
	}
	// Sets death flag, calls bp on unit dies.
	Super::OnUnitDies(DeathType);
}

bool AGarrisonableBuilding::EnsureHealthComponentIsValid() const
{
	if (not IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "HealthComponent",
		                                             "AGarrisonableBuilding::EnsureHealthComponentIsValid");
		return false;
	}
	return true;
}

bool AGarrisonableBuilding::EnsureRTSComponentIsValid() const
{
	if (not IsValid(RTSComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "RTSComponent",
		                                             "AGarrisonableBuilding::EnsureRTSComponentIsValid");
		return false;
	}
	return true;
}

bool AGarrisonableBuilding::EnsureCargoComponentIsValid() const
{
	if (not IsValid(CargoComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "CargoComponent",
		                                             "AGarrisonableBuilding::EnsureCargoComponentIsValid");
		return false;
	}
	return true;
}
