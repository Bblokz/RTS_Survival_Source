// Copyright (C) Bas Blokzijl - All rights reserved.


#include "HpPawnMaster.h"

#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Collapse/FRTS_Collapse/FRTS_Collapse.h"
#include "Engine/DamageEvents.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"


// Sets default values
AHpPawnMaster::AHpPawnMaster(const FObjectInitializer& ObjectInitializer)
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void AHpPawnMaster::TakeFatalDamage()
{
	if (not GetIsValidHealthComponent())
	{
		return;
	}
	FDamageEvent DamageEvent;
	DamageEvent.DamageTypeClass = FRTSWeaponHelpers::GetDamageTypeClass(ERTSDamageType::Kinetic);
	TakeDamage(HealthComponent->GetCurrentHealth() * 100, DamageEvent, nullptr, nullptr);
}

void AHpPawnMaster::TriggerDestroyActor(const ERTSDeathType DeathType)
{
	UnitDies(DeathType);
}

// Called when the game starts or when spawned
void AHpPawnMaster::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AHpPawnMaster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AHpPawnMaster::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AHpPawnMaster::CheckReferences()
{
	if (!HealthComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("HealthComponent not set in %s"), *GetName());
	}
	if (!RTSComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("RTSComponent not set in %s"), *GetName());
	}
}


float AHpPawnMaster::TakeDamage(
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
	if (HealthComponent)
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
	                                             "AHpPawnMaster::TakeDamage");
	return float();
}

void AHpPawnMaster::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
	Bp_OnHealthChanged(PercentageLeft);
}


void AHpPawnMaster::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Initialize the health component
	HealthComponent = FindComponentByClass<UHealthComponent>();
	if (!IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "HealthComponent",
		                                             "AHpPawnMaster::PostInitializeComponents");
	}

	// Initialize the RTS component
	RTSComponent = FindComponentByClass<URTSComponent>();
	if (!IsValid(RTSComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "RTSComponent", "AHpPawnMaster::PostInitializeComponents");
	}
}

void AHpPawnMaster::DestroyAndSpawnActors(const FDestroySpawnActorsParameters& SpawnParams,
                                          const FCollapseFX CollapseFX)
{
	FRTS_Collapse::OnDestroySpawnActors(this,
	                                    SpawnParams,
	                                    CollapseFX);
}

bool AHpPawnMaster::GetIsValidHealthComponent() const
{
	if (IsValid(HealthComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("HealthComponent not set in HpPawnMaster"
		"\n  actor: " + GetName());
	return false;
}

bool AHpPawnMaster::GetIsValidRTSComponent() const
{
	if (IsValid(RTSComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("RTSComponent not set in HpPawnMaster"
		"\n  actor: " + GetName());
	return false;
}
