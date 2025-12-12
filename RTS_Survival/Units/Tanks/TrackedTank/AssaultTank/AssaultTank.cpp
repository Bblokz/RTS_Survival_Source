// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "AssaultTank.h"

#include "RTS_Survival/RTSComponents/RTSComponent.h"


// Sets default values
AAssaultTank::AAssaultTank(const FObjectInitializer& ObjectInitializer)
	: ATrackedTankMaster(ObjectInitializer)
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

bool AAssaultTank::TurnBase_Implementation(const float Degrees)
{
	// 1) While dug in / digging in we do not allow hull rotation at all.
	if (bM_IsHullRotationLocked)
	{
		return false;
	}

	// 2) Preserve original movement gate.
	const EAbilityID CurrentAbility = GetActiveCommandID();
	if (CurrentAbility == EAbilityID::IdMove
		|| CurrentAbility == EAbilityID::IdPatrol
		|| CurrentAbility == EAbilityID::IdReverseMove)
	{
		// Stop rotation from possible previous turn base command, as now the vehicle is moving.
		StopRotating();
		return false;
	}

	// 3) Regular base turn.
	FRotator TankRotation = GetActorRotation();
	TankRotation.Yaw += Degrees;
	ExecuteRotateTowardsCommand(TankRotation, false);
	return true;
}


// Called when the game starts or when spawned
void AAssaultTank::BeginPlay()
{
	Super::BeginPlay();
	
}


void AAssaultTank::OnStartDigIn()
{
	// Swap abilities / disable move & rotate as in base class.
	Super::OnStartDigIn();

	// As soon as we start digging in, freeze hull rotation for the embedded assault turret.
	LockHullRotation();
}

void AAssaultTank::OnBreakCoverCompleted()
{
	// Any way the dig-in ended (cancelled, wall destroyed or via break-cover command),
	// we allow the hull to rotate again.
	UnlockHullRotation();

	Super::OnBreakCoverCompleted();
}

void AAssaultTank::LockHullRotation()
{
	if (bM_IsHullRotationLocked)
	{
		return;
	}

	bM_IsHullRotationLocked = true;

	// Ensure any in-flight rotate command is stopped so the hull stays fixed.
	StopRotating();
}

void AAssaultTank::UnlockHullRotation()
{
	bM_IsHullRotationLocked = false;
}