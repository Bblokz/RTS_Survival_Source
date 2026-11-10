// Copyright (C) Bas Blokzijl - All rights reserved.


#include "DestructableActorHealth.h"

#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"


ADestructableActorHealth::ADestructableActorHealth(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled= false;
}

void ADestructableActorHealth::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
	BpOnHealthChanged(PercentageLeft, bIsHealing);
}

float ADestructableActorHealth::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (not IsUnitAlive())
	{
		// Do not trigger kill on a dead unit.
		return 1.f;
	}
	if (IsValid(HealthComponent))
	{
		const ERTSDamageType RtsDamageType = FRTSWeaponHelpers::GetRTSDamageType(DamageEvent);
		const float HealthBefore = HealthComponent->GetCurrentHealth();
		const bool bDied = HealthComponent->TakeDamage(DamageAmount, RtsDamageType);
		// Before the death: a surviving actor shows shell holes, a dying one keeps its own death.
		ForwardDamageToHoleComponents(HealthBefore - HealthComponent->GetCurrentHealth(), DamageEvent, DamageCauser,
		                              HealthBefore, bDied ? 0.f : HealthComponent->GetCurrentHealth());
		if (bDied)
		{
			const ERTSDeathType DeathType = FRTSWeaponHelpers::TranslateDamageIntoDeathType(RtsDamageType);
		OnUnitDies(DeathType);
			return 0.0;
		}
		return 1.0;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this,
	                                             "HealthComponent",
	                                             "AHPActorObjectsMaster::TakeDamage");
	return float();
}



// Called when the game starts or when spawned
void ADestructableActorHealth::BeginPlay()
{
	Super::BeginPlay();
	// The hole component caps the removed area by the share of health lost; its fallback max is the base class Health
	// field, which this class does not use.
	UWallHoleDamageComponent* const WallHoleDamage = FindComponentByClass<UWallHoleDamageComponent>();
	if (IsValid(WallHoleDamage) && IsValid(HealthComponent))
	{
		WallHoleDamage->SetOwnerMaxHealth(HealthComponent->GetMaxHealth());
	}
}

void ADestructableActorHealth::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	HealthComponent = FindComponentByClass<UHealthComponent>();
	if (!IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "HealthComponent",
		                                             "AHPActorObjectsMaster::PostInitializeComponents");
	}
}

