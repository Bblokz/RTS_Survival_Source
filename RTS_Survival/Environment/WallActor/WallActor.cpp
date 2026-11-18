// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WallActor.h"

#include "Engine/DamageEvents.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallTankBreachComponent.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallGate/WallGateNavigation.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"


// Sets default values
AWallActor::AWallActor(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWallActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWallActor::SetupWallCollision(UStaticMeshComponent* WallMeshComponent,
	const bool bAffectNavMesh)
{
	if(not GetIsValidRTSComponent())
	{
		return;
	}
	FRTS_CollisionSetup::SetupWallActorMeshCollision(WallMeshComponent, bAffectNavMesh, RTSComponent->GetOwningPlayer());
	// Walls tanks can drive through: the setup above resets every channel, so the breach overlap goes on after it.
	if (UWallTankBreachComponent* const TankBreach = FindComponentByClass<UWallTankBreachComponent>())
	{
		TankBreach->OnWallCollisionSetUp(WallMeshComponent);
	}
}

void AWallActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	RTSComponent = FindComponentByClass<URTSComponent>();
	(void)GetIsValidRTSComponent();
	M_TankBreach = FindComponentByClass<UWallTankBreachComponent>();
	M_HoleDamageForBreach = IsValid(M_TankBreach) ? FindComponentByClass<UWallHoleDamageComponent>() : nullptr;
}

float AWallActor::TakeDamage(const float DamageAmount, FDamageEvent const& DamageEvent, AController* const EventInstigator,
                             AActor* const DamageCauser)
{
	if (IsValid(M_TankBreach)
		&& (GetIsDamageFromOwnSide(DamageCauser, EventInstigator) || GetIsShotThroughBreach(DamageEvent)))
	{
		return 0.f;
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

bool AWallActor::TryGetAttackingPlayer(const AActor* const DamageCauser, const AController* const EventInstigator,
                                       uint8& OutPlayer)
{
	if (const AProjectile* const Projectile = Cast<AProjectile>(DamageCauser))
	{
		OutPlayer = static_cast<uint8>(Projectile->GetOwningPlayer());
		return true;
	}
	const AActor* const Candidates[] = {
		DamageCauser,
		IsValid(DamageCauser) ? DamageCauser->GetOwner() : nullptr,
		IsValid(DamageCauser) ? DamageCauser->GetInstigator() : nullptr,
		IsValid(EventInstigator) ? EventInstigator->GetPawn() : nullptr
	};
	for (const AActor* const Candidate : Candidates)
	{
		const URTSComponent* const CandidateRTS = IsValid(Candidate)
			                                          ? Candidate->FindComponentByClass<URTSComponent>()
			                                          : nullptr;
		if (IsValid(CandidateRTS))
		{
			OutPlayer = CandidateRTS->GetOwningPlayer();
			return true;
		}
	}
	return false;
}

bool AWallActor::GetIsDamageFromOwnSide(const AActor* const DamageCauser, const AController* const EventInstigator) const
{
	uint8 AttackingPlayer = 0;
	if (not GetIsValidRTSComponent() || not TryGetAttackingPlayer(DamageCauser, EventInstigator, AttackingPlayer))
	{
		return false;
	}
	// The wall only takes damage from players that fight its owner. A neutral damage source keeps counting as the
	// AI side and an unowned wall can only be damaged by the human, as both did when there were two sides.
	const int32 AttackerOrAISide = FRTSPlayers::IsNeutralOwner(AttackingPlayer)
		                               ? RTSPlayers::FirstAIPlayer
		                               : static_cast<int32>(AttackingPlayer);
	return not FRTSPlayers::CanPlayerDamageOwner(AttackerOrAISide, RTSComponent->GetOwningPlayer());
}

bool AWallActor::GetIsShotThroughBreach(const FDamageEvent& DamageEvent) const
{
	if (not IsValid(M_HoleDamageForBreach) || not DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		return false;
	}
	const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
	return M_HoleDamageForBreach->GetIsWorldPointInBreach(PointEvent.HitInfo.Location);
}

bool AWallActor::GetIsValidRTSComponent() const
{
	if(not IsValid(RTSComponent))
	{
		RTSFunctionLibrary::PrintString("Invalid rts component in WallActor");
		return false;
	}
	return true;
}


