// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UT_TurretTarget.h"
#include "RTS_Survival/UnitTests/TestTurrets/UT_TurretManager/UT_TurretManager.h"
#include "Engine/World.h"

AUT_TurretTarget::AUT_TurretTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AUT_TurretTarget::Init(AUT_TurretManager* UtManager)
{
	this->M_UtManager = UtManager;
}

void AUT_TurretTarget::BeginPlay()
{
	Super::BeginPlay();
}

void AUT_TurretTarget::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AUT_TurretTarget::UnitDies(const ERTSDeathType DeathType)
{
	// If we are being destroyed and have a valid manager, let it know 
	if (IsValid(M_UtManager))
	{
		M_UtManager->OnTargetDestroyed(this);
	}
	
	Super::UnitDies(DeathType);
}

