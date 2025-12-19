// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GarrisonableBuilding.h"

#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"


const int32 AGarrisonableBuilding::DamagePercentage = 50;


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
	if (not IsValid(SquadController))
	{
		return;
	}
	URTSComponent* SquadRTSComp = SquadController->FindComponentByClass<URTSComponent>();
	if (not IsValid(SquadRTSComp))
	{
		return;
	}
	HandleAlliedDamage(SquadRTSComp->GetOwningPlayer());
}

void AGarrisonableBuilding::OnCargoEmpty()
{
	if (not EnsureHealthComponentIsValid())
	{
		return;
	}
	HealthComponent->ChangeVisibilitySettings(M_CachedVisibilityPreGarrison);
	GarissonEmptyHandleAlliedDamage();
}

void AGarrisonableBuilding::BeginPlay()
{
	Super::BeginPlay();
	// Cache the visibility settings before garrisoning.
	if (EnsureHealthComponentIsValid())
	{
		M_CachedVisibilityPreGarrison = HealthComponent->GetVisibilitySettings();
	}
	// Get all static mesh components.
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents<UStaticMeshComponent>(StaticMeshComponents);
	for (UStaticMeshComponent* MeshComp : StaticMeshComponents)
	{
		if (not IsValid(MeshComp))
		{
			continue;
		}
		FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision(MeshComp, false);
	}
}

void AGarrisonableBuilding::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Initialize the health component
	HealthComponent = FindComponentByClass<UHealthComponent>();
	EnsureHealthComponentIsValid();

	// Initialize the RTS component
	RTSComponent = FindComponentByClass<URTSComponent>();
	EnsureRTSComponentIsValid();
	CargoComponent = FindComponentByClass<UCargo>();
	EnsureCargoComponentIsValid();
}

float AGarrisonableBuilding::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                        AController* EventInstigator, AActor* DamageCauser)
{
	if (not IsUnitAlive())
	{
		// Do not trigger kill on a dead unit.
		return 1.f;
	}
	if (not HealthComponent)
	{
		return 1.f;
	}
	const ERTSDamageType RtsDamageType = FRTSWeaponHelpers::GetRTSDamageType(DamageEvent);
	if (HealthComponent->TakeDamage(DamageAmount, RtsDamageType))
	{
		const ERTSDeathType DeathType = FRTSWeaponHelpers::TranslateDamageIntoDeathType(RtsDamageType);
		OnUnitDies(DeathType);
		return 0.0;
	}
	return 1.0;
}

void AGarrisonableBuilding::OnUnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		return;
	}

	OnUnitDies_HandleGarrisonState();
	if (HealthComponent)
	{
		HealthComponent->HideHealthBar();
	}
	// Sets death flag, calls bp on unit dies.
	Super::OnUnitDies(DeathType);
}

void AGarrisonableBuilding::OnUnitDies_HandleGarrisonState()
{
	if (not EnsureCargoComponentIsValid())
	{
		return;
	}
	CargoComponent->ApplyDamageToGarrisonedSquads(DamagePercentage);
}

void AGarrisonableBuilding::HandleAlliedDamage(const int32 PlayerAlliedTo)
{
	if (not IgnoreGarrisonAlliedDamage)
	{
		return;
	}
	TArray<UStaticMeshComponent*> MyMeshComponents;
	GetComponents<UStaticMeshComponent>(MyMeshComponents);
	for (const auto EachComp : MyMeshComponents)
	{
		if (not IsValid(EachComp))
		{
			continue;
		}
		// Now allied; allow tracers from this player.
		FRTS_CollisionSetup::UpdateGarrisonCollisionForNewOwner(PlayerAlliedTo, true, EachComp);
		LastAlliedToPlayer = PlayerAlliedTo;
	}
}

void AGarrisonableBuilding::GarissonEmptyHandleAlliedDamage() const
{
	if (not IgnoreGarrisonAlliedDamage)
	{
		// no collision to change.
		return;
	}
	TArray<UStaticMeshComponent*> MyMeshComponents;
	GetComponents<UStaticMeshComponent>(MyMeshComponents);
	for (const auto EachComp : MyMeshComponents)
	{
		if (not IsValid(EachComp))
		{
			continue;
		}
		// No longer allied; block tracers from this player.
		FRTS_CollisionSetup::UpdateGarrisonCollisionForNewOwner(LastAlliedToPlayer, false, EachComp);
	}
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
