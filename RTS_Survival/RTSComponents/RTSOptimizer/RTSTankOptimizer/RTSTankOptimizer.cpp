// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSTankOptimizer.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"


URTSTankOptimizer::URTSTankOptimizer()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void URTSTankOptimizer::BeginPlay()
{
	Super::BeginPlay();
}

void URTSTankOptimizer::DetermineChildActorCompOptimization(UChildActorComponent* ChildActorComponent)
{
	using DeveloperSettings::Optimization::Turrets::OutOfFOVClose_TurretTickInterval;
	using DeveloperSettings::Optimization::Turrets::OutOfFOVFar_TurretTickInterval;
	// Setup the skeletons, primitives and tick components as we usually would for optimization for this child actor.
	Super::DetermineChildActorCompOptimization(ChildActorComponent);

	if (not ChildActorComponent)
	{
		return;
	}
	ACPPTurretsMaster* TurretMaster = Cast<ACPPTurretsMaster>(ChildActorComponent->GetChildActor());
	if (not TurretMaster)
	{
		return;
	}
	// Setup special turret tickrates.
	FRTSActorTickOptimizationSettings TurretTickSettings;
	TurretTickSettings.TickingActor = TurretMaster;
	TurretTickSettings.BaseTickInterval = TurretMaster->GetActorTickInterval();
	TurretTickSettings.CloseOutFovInterval = OutOfFOVClose_TurretTickInterval;
	TurretTickSettings.FarOutFovInterval = OutOfFOVFar_TurretTickInterval;
	M_TickingActors.Add(TurretTickSettings);
}

void URTSTankOptimizer::InFOVUpdateComponents()
{
	Super::InFOVUpdateComponents();
	for (auto EachTurret : M_TickingActors)
	{
		if (EachTurret.TickingActor)
		{
			EachTurret.TickingActor->SetActorTickInterval(EachTurret.BaseTickInterval);
		}
	}
}

void URTSTankOptimizer::OutFovCloseUpdateComponents()
{
	Super::OutFovCloseUpdateComponents();
	for (auto EachTurret : M_TickingActors)
	{
		if (EachTurret.TickingActor)
		{
			EachTurret.TickingActor->SetActorTickInterval(EachTurret.CloseOutFovInterval);
		}
	}
}

void URTSTankOptimizer::OutFovFarUpdateComponents()
{
	Super::OutFovFarUpdateComponents();
	for (auto EachTurret : M_TickingActors)
	{
		if (EachTurret.TickingActor)
		{
			EachTurret.TickingActor->SetActorTickInterval(EachTurret.FarOutFovInterval);
		}
	}
}

void URTSTankOptimizer::DetermineOwnerOptimization(AActor* ValidOwner)
{
	using DeveloperSettings::Optimization::Tank::OutOfFOVFar_TankTickInterval;
	FRTSActorTickOptimizationSettings ActorTickSettings;
	ActorTickSettings.TickingActor = ValidOwner;
	ActorTickSettings.BaseTickInterval = ValidOwner->GetActorTickInterval();
	ActorTickSettings.CloseOutFovInterval = ValidOwner->GetActorTickInterval();
	ActorTickSettings.FarOutFovInterval = OutOfFOVFar_TankTickInterval;
	M_TickingActors.Add(ActorTickSettings);
}
