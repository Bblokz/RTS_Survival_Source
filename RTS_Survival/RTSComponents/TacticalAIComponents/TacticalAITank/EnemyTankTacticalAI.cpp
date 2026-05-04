// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyTankTacticalAI.h"

#include "RTS_Survival/Units/Tanks/TankMaster.h"


// Sets default values for this component's properties
UEnemyTankTacticalAI::UEnemyTankTacticalAI()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// ...
}

void UEnemyTankTacticalAI::OnUnitInCombat()
{
	if (not GetIsValidTankOwner())
	{
		return;
	}
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - M_LastTacticalCombatActionTime < M_TacticalCombatInterval)
	{
		return;
	}
	if(IsOwnerIdle())
	{
		OnTankIdleInCombat();	
	}
	else
	{
		if(IsOwnerAttackingAndInRangeOfTarget())
		{
			// The tank is engaging an enemy with an attack command and the enemy is in range; rotate to it.
			(void)RotateTankToTurretTargetIfValid();
		}
	}
}


void UEnemyTankTacticalAI::BeginPlay()
{
	Super::BeginPlay();

	ATankMaster* TankOwner = Cast<ATankMaster>(GetOwner());
	if (IsValid(TankOwner))
	{
		M_TankOwner = TankOwner;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyTankTacticalAI: Owner is not a TankMaster!"));
	}
}

ACPPTurretsMaster* UEnemyTankTacticalAI::SetOrGetTurret()
{
	if (M_TankTurret.IsValid())
	{
		return M_TankTurret.Get();
	}
	TArray<ACPPTurretsMaster*> Turrets = M_TankOwner->GetTurrets();
	if (Turrets.IsEmpty())
	{
		return nullptr;
	}
	ACPPTurretsMaster* FirstTurret = Turrets[0];
	if (not IsValid(FirstTurret))
	{
		return nullptr;
	}
	M_TankTurret = FirstTurret;
	return FirstTurret;
}

bool UEnemyTankTacticalAI::IsOwnerIdle() const
{
	return M_TankOwner->GetCurrentActiveCommand() == EAbilityID::IdIdle;
}

bool UEnemyTankTacticalAI::IsOwnerAttackingAndInRangeOfTarget()
{
	const EAbilityID CurrentCommand = M_TankOwner->GetCurrentActiveCommand();
	if (CurrentCommand != EAbilityID::IdAttack && CurrentCommand != EAbilityID::IdAttackGround)
	{
		return false;
	}
	// We have an attack command; check if the turret is in range too.
	return GetIsTurretInRangeOfTarget();
}

bool UEnemyTankTacticalAI::GetIsTurretInRangeOfTarget()
{
	if (not SetOrGetTurret())
	{
		return false;
	}
	return M_TankTurret->BGetIsInRangeOfTarget();
	
}

void UEnemyTankTacticalAI::OnTankIdleInCombat()
{
	if(GetIsTurretInRangeOfTarget())
	{
		// vehicle is idle but turret is in range of engaging a target; rotate to it.
		(void)RotateTankToTurretTargetIfValid();
	}
}

bool UEnemyTankTacticalAI::RotateTankToTurretTargetIfValid()
{
	if(not SetOrGetTurret() || not GetIsValidTankOwner())
	{
		return false;
	}
	AActor* TurretTarget = M_TankTurret->GetCurrentTargetActor();
	if(not IsValid(TurretTarget))
	{
		return false;
	}
	FVector TankLocation = M_TankOwner->GetActorLocation();
	FVector TargetLocation = TurretTarget->GetActorLocation();
	// Find look at rotation.
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(TankLocation, TargetLocation);
	return M_TankOwner->RotateTowards(LookAtRotation, true) == ECommandQueueError::NoError;
	
	
}
