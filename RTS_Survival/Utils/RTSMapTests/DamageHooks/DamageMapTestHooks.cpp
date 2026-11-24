// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DamageMapTestHooks.h"

#if RTS_WITH_MAP_TESTS

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace DamageMapTestHooksPrivate
{
	FOnMapTestDamageApplied GOnDamageApplied;
	int32 GCurrentAttacker = INDEX_NONE;
	TWeakObjectPtr<AActor> GCurrentAttackerActor;
}

FOnMapTestDamageApplied& FDamageMapTestHooks::OnDamageApplied()
{
	return DamageMapTestHooksPrivate::GOnDamageApplied;
}

bool FDamageMapTestHooks::GetHasListeners()
{
	return DamageMapTestHooksPrivate::GOnDamageApplied.IsBound();
}

int32 FDamageMapTestHooks::GetCurrentAttacker()
{
	return DamageMapTestHooksPrivate::GCurrentAttacker;
}

AActor* FDamageMapTestHooks::GetCurrentAttackerActor()
{
	return DamageMapTestHooksPrivate::GCurrentAttackerActor.Get();
}

AActor* FDamageMapTestHooks::GetFiringActorOfWeaponState(const UObject* WeaponState)
{
	// Only resolved while a map test listens: the scope costs nothing in a normal game.
	if (not GetHasListeners())
	{
		return nullptr;
	}
	const UWeaponState* const State = Cast<UWeaponState>(WeaponState);
	if (not IsValid(State))
	{
		return nullptr;
	}
	const UMeshComponent* const WeaponMesh = State->GetWeaponMeshComponent();
	return IsValid(WeaponMesh) ? WeaponMesh->GetOwner() : nullptr;
}

void FDamageMapTestHooks::ReportDamageApplied(AActor* Victim, const float AppliedDamage, const bool bKilled)
{
	if (not GetHasListeners() || not IsValid(Victim))
	{
		return;
	}
	FMapTestDamageReport Report;
	Report.AttackingPlayer = GetCurrentAttacker();
	Report.AttackingActor = DamageMapTestHooksPrivate::GCurrentAttackerActor;
	Report.Victim = Victim;
	Report.VictimOwner = FRTSPlayers::GetOwningPlayerOfActor(Victim);
	Report.AppliedDamage = AppliedDamage;
	Report.bKilled = bKilled;
	DamageMapTestHooksPrivate::GOnDamageApplied.Broadcast(Report);
}

FDamageMapTestHooks::FAttackerScope::FAttackerScope(const int32 AttackingPlayer, AActor* AttackingActor)
	: M_OuterAttacker(DamageMapTestHooksPrivate::GCurrentAttacker)
	, M_OuterAttackerActor(DamageMapTestHooksPrivate::GCurrentAttackerActor)
{
	DamageMapTestHooksPrivate::GCurrentAttacker = AttackingPlayer;
	DamageMapTestHooksPrivate::GCurrentAttackerActor = AttackingActor;
}

FDamageMapTestHooks::FAttackerScope::~FAttackerScope()
{
	DamageMapTestHooksPrivate::GCurrentAttacker = M_OuterAttacker;
	DamageMapTestHooksPrivate::GCurrentAttackerActor = M_OuterAttackerActor;
}

#endif
