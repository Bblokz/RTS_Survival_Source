// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyConvoyBattleTracker.h"

#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyBehaviourTypes.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRouteActor/EnemyConvoyRoute.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UEnemyConvoyBattleTracker::UEnemyConvoyBattleTracker()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UEnemyConvoyBattleTracker::StartConvoyBattle(AEnemyConvoyRoute* Route, const int32 TruckCount)
{
	if (bM_HasStartedBattle)
	{
		return false;
	}

	bM_HasStartedBattle = true;
	if (not IsValid(Route))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Convoy battle of %s has no convoy route to start."), *GetNameSafe(GetOwner())));
		M_BattleState.bHasRouteFailed = true;
		MarkBattleReady();
		return false;
	}

	// Bound before the start: a route that cannot build its drive path finishes inside the first launch.
	M_Route = Route;
	BindRoute(*Route);
	if (Route->StartSingleConvoy(TruckCount))
	{
		return true;
	}

	UnbindRoute();
	M_BattleState.bHasRouteFailed = true;
	MarkBattleReady();
	return false;
}

FEnemyConvoyBattleState UEnemyConvoyBattleTracker::GetBattleState() const
{
	FEnemyConvoyBattleState BattleState = M_BattleState;
	if (not BattleState.bHasConvoyFinished)
	{
		AddLiveTruckFates(BattleState);
	}

	return BattleState;
}

void UEnemyConvoyBattleTracker::AddLiveTruckFates(FEnemyConvoyBattleState& InOutBattleState) const
{
	const AConvoy* Convoy = M_Convoy.Get();
	if (not IsValid(Convoy))
	{
		return;
	}

	InOutBattleState.bHasConvoySpawned = InOutBattleState.bHasConvoySpawned
		|| Convoy->GetConvoyState() != EConvoyState::Spawning;
	InOutBattleState.EscapedTruckCount = FMath::Max(InOutBattleState.EscapedTruckCount,
	                                                Convoy->GetEscapedTruckCount());
	InOutBattleState.BranchDeliveredTruckCount = FMath::Max(InOutBattleState.BranchDeliveredTruckCount,
	                                                        Convoy->GetBranchDeliveredTruckCount());
}

TArray<AActor*> UEnemyConvoyBattleTracker::GetLivingTrucks() const
{
	TArray<AActor*> LivingTrucks;
	const AConvoy* Convoy = M_Convoy.Get();
	if (not IsValid(Convoy))
	{
		return LivingTrucks;
	}

	TArray<FConvoyMemberSnapshot> MemberSnapshots;
	Convoy->GetMemberSnapshots(MemberSnapshots);
	for (const FConvoyMemberSnapshot& MemberSnapshot : MemberSnapshots)
	{
		AActor* Truck = MemberSnapshot.Actor.Get();
		if (MemberSnapshot.Role != EConvoyUnitRole::Truck || not RTSFunctionLibrary::RTSIsValid(Truck))
		{
			continue;
		}

		// A surrendered truck belongs to its captor: it is stopped, not a target.
		if (FRTSPlayers::IsActorHostileToPlayer(Truck, RTSPlayers::HumanPlayer))
		{
			LivingTrucks.Add(Truck);
		}
	}

	return LivingTrucks;
}

int32 UEnemyConvoyBattleTracker::GetSpawnedTruckCount() const
{
	return M_BattleState.bHasConvoyFinished ? M_BattleState.SpawnedTruckCount : GetLivingTrucks().Num();
}

void UEnemyConvoyBattleTracker::HandleConvoyStarted(AEnemyConvoyRoute* Route, AConvoy* Convoy)
{
	if (M_BattleState.bHasConvoyStarted || not IsValid(Convoy))
	{
		return;
	}

	M_BattleState.bHasConvoyStarted = true;
	M_Convoy = Convoy;
	M_ConvoyEventHandle = Convoy->OnConvoyEventNative.AddUObject(this, &UEnemyConvoyBattleTracker::HandleConvoyEvent);
}

void UEnemyConvoyBattleTracker::HandleConvoyEvent(const FConvoyEvent& Event)
{
	if (Event.Type != EConvoyEventType::StateChanged || M_BattleState.bHasConvoySpawned)
	{
		return;
	}

	const AConvoy* Convoy = M_Convoy.Get();
	if (not IsValid(Convoy) || Convoy->GetConvoyState() == EConvoyState::Spawning)
	{
		return;
	}

	M_BattleState.bHasConvoySpawned = true;
	MarkBattleReady();
}

void UEnemyConvoyBattleTracker::HandleConvoyCargoOutcome(AEnemyConvoyRoute* Route, AConvoy* Convoy,
                                                         const FConvoyCargoOutcome& Outcome)
{
	if (Convoy != M_Convoy.Get() || M_BattleState.bHasConvoyFinished)
	{
		return;
	}

	M_BattleState.bHasConvoyFinished = true;
	M_BattleState.Outcome = Outcome.Outcome;
	M_BattleState.SpawnedTruckCount = Outcome.TruckCount;
	M_BattleState.EscapedTruckCount = Outcome.EscapedTrucks;
	M_BattleState.BranchDeliveredTruckCount = Outcome.BranchDeliveredTrucks;
	UE_LOG(LogTemp, Display,
	       TEXT("RTS_ENEMY_CONVOY_BATTLE finished owner=%s outcome=%d trucks=%d escaped=%d branchDelivered=%d"),
	       *GetNameSafe(GetOwner()), static_cast<int32>(Outcome.Outcome), Outcome.TruckCount, Outcome.EscapedTrucks,
	       Outcome.BranchDeliveredTrucks);
	UnbindConvoy();
	MarkBattleReady();
}

void UEnemyConvoyBattleTracker::HandleRouteFinished(AEnemyConvoyRoute* Route)
{
	if (not M_BattleState.bHasConvoyStarted)
	{
		// LaunchConvoy reported why (destroyed owning building, no drivable path, broken definition).
		M_BattleState.bHasRouteFailed = true;
	}

	UnbindRoute();
	MarkBattleReady();
}

void UEnemyConvoyBattleTracker::MarkBattleReady()
{
	if (bM_IsBattleReady)
	{
		return;
	}

	bM_IsBattleReady = true;
	OnBattleReady.Broadcast();
}

void UEnemyConvoyBattleTracker::BindRoute(AEnemyConvoyRoute& Route)
{
	Route.OnConvoyStarted.AddUniqueDynamic(this, &UEnemyConvoyBattleTracker::HandleConvoyStarted);
	Route.OnConvoyCargoOutcome.AddUniqueDynamic(this, &UEnemyConvoyBattleTracker::HandleConvoyCargoOutcome);
	Route.OnRouteFinished.AddUniqueDynamic(this, &UEnemyConvoyBattleTracker::HandleRouteFinished);
}

void UEnemyConvoyBattleTracker::UnbindRoute()
{
	AEnemyConvoyRoute* Route = M_Route.Get();
	if (not IsValid(Route))
	{
		return;
	}

	Route->OnConvoyStarted.RemoveDynamic(this, &UEnemyConvoyBattleTracker::HandleConvoyStarted);
	Route->OnConvoyCargoOutcome.RemoveDynamic(this, &UEnemyConvoyBattleTracker::HandleConvoyCargoOutcome);
	Route->OnRouteFinished.RemoveDynamic(this, &UEnemyConvoyBattleTracker::HandleRouteFinished);
}

void UEnemyConvoyBattleTracker::UnbindConvoy()
{
	AConvoy* Convoy = M_Convoy.Get();
	if (IsValid(Convoy))
	{
		Convoy->OnConvoyEventNative.Remove(M_ConvoyEventHandle);
	}

	M_ConvoyEventHandle.Reset();
}

void UEnemyConvoyBattleTracker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindConvoy();
	UnbindRoute();
	Super::EndPlay(EndPlayReason);
}
