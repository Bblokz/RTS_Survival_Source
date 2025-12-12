// Copyright (C) Bas Blokzijl - All rights reserved.



#include "CPPSelfWeaponMaster.h"


// Sets default values for this component's properties
UCPPSelfWeaponMaster::UCPPSelfWeaponMaster()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCPPSelfWeaponMaster::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UCPPSelfWeaponMaster::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

float UCPPSelfWeaponMaster::CalculateDamageDealt(float AverageDamage, int DamageFluxPercentage)
{
	int lv_DmgMin = FMath::FloorToInt(AverageDamage - (AverageDamage / 100 * DamageFluxPercentage));
	int lv_DmgMax = FMath::CeilToInt(AverageDamage + (AverageDamage / 100 * DamageFluxPercentage));
    
	// Get Random number in range using Unreal Engine's FMath:
	return FMath::RandRange(lv_DmgMin, lv_DmgMax);
}


void UCPPSelfWeaponMaster::SetWeaponProperties(float averageDamage, int armorPenetration)
{
	Damage = averageDamage;
	ArmorPen = armorPenetration;
}

