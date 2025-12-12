// Copyright (C) Bas Blokzijl - All rights reserved.


#include "HpCharacterObjectsMaster.h"

#include "GameFramework/GameState.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Weapons/CPPWeaponsMaster.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"


// Sets default values
ACharacterObjectsMaster::ACharacterObjectsMaster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void ACharacterObjectsMaster::TriggerDestroyActor(const ERTSDeathType DeathType)
{
	UnitDies(DeathType);
}


void ACharacterObjectsMaster::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	HealthComponent = FindComponentByClass<USquadUnitHealthComponent>();
	if (!IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             " squad unit HealthComponent",
		                                             "ACharacterObjectsMaster::PostInitializeComponents");
	}
	RTSComponent = FindComponentByClass<URTSComponent>();
	if (!IsValid(RTSComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "RTSComponent",
		                                             "ACharacterObjectsMaster::PostInitializeComponents");
	}
	SelectionComponent = FindComponentByClass<USelectionComponent>();
	if (!IsValid(SelectionComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "SelectionComponent",
		                                             "ACharacterObjectsMaster::PostInitializeComponents");
	}
}

bool ACharacterObjectsMaster::GetIsValidRTSComponent() const
{
	if (IsValid(RTSComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportNullErrorInitialisation(
		this,
		"RTSComponent",
		"ACharacterObjectsMaster::GetIsValidRTSComponent");
	return false;
}

bool ACharacterObjectsMaster::GetIsValidSelectionComponent() const
{
	if (IsValid(SelectionComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportNullErrorInitialisation(
		this,
		"SelectionComponent",
		"ACharacterObjectsMaster::GetIsValidSelectionComponent");
	return false;
}

bool ACharacterObjectsMaster::GetIsSelected() const
{
	if (not GetIsValidSelectionComponent())
	{
		return false;
	}
	return SelectionComponent->GetIsSelected();
}

bool ACharacterObjectsMaster::GetIsValidHealthComponent() const
{
	if (IsValid(HealthComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportNullErrorInitialisation(
		this,
		"HealthComponent",
		"ACharacterObjectsMaster::GetIsValidHealthComponent");
	return false;
}

float ACharacterObjectsMaster::TakeDamage(
	float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if(not IsUnitAlive())
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
	                                             "ACharacterObjectsMaster::TakeDamage");
	return float();
}

void ACharacterObjectsMaster::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
	Bp_OnHealthChanged(PercentageLeft);
}
