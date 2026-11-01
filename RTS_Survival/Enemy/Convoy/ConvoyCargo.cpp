// Copyright (C) Bas Blokzijl - All rights reserved.

// Members of AConvoy that handle deaths, the cargo caches destroyed trucks leave, releasing units and the end.

#include "Convoy.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

namespace ConvoyCargoConstants
{
	// The convoy actor lingers this long after its last cache resolved, so late events still find it.
	constexpr float SelfDestroyDelaySeconds = 2.f;
}

void AConvoy::UpdateDeaths()
{
	bool bAnyDied = false;
	for (FConvoyMember& Member : M_Members)
	{
		const bool bWasActive = Member.State != EConvoyMemberState::Dead and Member.State != EConvoyMemberState::Released
			and Member.State != EConvoyMemberState::Spawning;
		if (bWasActive and not Member.GetIsAlive())
		{
			OnMemberDied(Member.MemberId);
			bAnyDied = true;
		}
	}
	if (bAnyDied and M_Outcome == EConvoyOutcome::None and M_State != EConvoyState::TrucksLost)
	{
		AssignSlots();
		Broadcast(EConvoyEventType::SlotReassigned, nullptr, FString::Printf(TEXT("trucks=%d"), GetAliveTruckCount()));
	}
}

void AConvoy::OnMemberDied(const int32 MemberId)
{
	FConvoyMember* Member = FindMember(MemberId);
	if (Member == nullptr or Member->State == EConvoyMemberState::Dead or Member->State == EConvoyMemberState::Released)
	{
		return;
	}
	Member->State = EConvoyMemberState::Dead;
	if (Member->Tank.IsValid() and Member->DeathHandle.IsValid())
	{
		Member->Tank->OnUnitDies.Remove(Member->DeathHandle);
	}
	Member->DeathHandle.Reset();
	if (Member->Role != EConvoyUnitRole::Truck)
	{
		Broadcast(EConvoyEventType::EscortDestroyed, Member, Member->UnitName);
		return;
	}
	HandleTruckLost(*Member);
}

void AConvoy::HandleTruckLost(FConvoyMember& Member)
{
	Broadcast(EConvoyEventType::TruckDestroyed, &Member,
	          FString::Printf(TEXT("%s trucks_left=%d"), *Member.UnitName, GetAliveTruckCount()));
	if (M_Outcome == EConvoyOutcome::None)
	{
		ScheduleCargoCache(Member);
	}
	if (GetAliveTruckCount() > 0 or M_State == EConvoyState::TrucksLost or M_Outcome != EConvoyOutcome::None)
	{
		return;
	}
	// The last truck is gone: the escorts stop keeping station and are no longer slowed.
	for (FConvoyMember& Escort : M_Members)
	{
		if (not Escort.GetIsAlive())
		{
			continue;
		}
		if (Escort.Tank.IsValid())
		{
			Escort.Tank->ClearMobilityRestriction(this);
			Escort.AppliedSpeedFraction = 1.f;
		}
		if (Escort.Squad.IsValid())
		{
			Escort.Squad->SetMarchSpeedOverride(0.f);
			Escort.AppliedMarchSpeed = 0.f;
		}
		Escort.bHasOrder = false;
	}
	SetState(EConvoyState::TrucksLost, TEXT("every truck destroyed"));
}

void AConvoy::ScheduleCargoCache(const FConvoyMember& Truck)
{
	FConvoyCargoCache& Cache = M_Caches.AddDefaulted_GetRef();
	Cache.CacheId = M_NextCacheId++;
	Cache.TruckMemberId = Truck.MemberId;
	Cache.Value = M_CargoPerTruck;
	// Beside the wreck, on the side away from the route centre line.
	const float Side = Truck.LateralOffset >= 0.f ? 1.f : -1.f;
	Cache.Location = M_Polyline.GetOffsetPoint(Truck.RouteDistance,
	                                           Truck.LateralOffset + Side * M_Definition->Cargo.CacheSideOffset);
	Cache.Location.Z = Truck.Location.Z;
	UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), Cache.Location, Cache.Location);
	const int32 CacheId = Cache.CacheId;
	TWeakObjectPtr<AConvoy> WeakThis(this);
	FTimerHandle SpawnTimer;
	GetWorld()->GetTimerManager().SetTimer(SpawnTimer, [WeakThis, CacheId]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->SpawnCargoCache(CacheId);
		}
	}, FMath::Max(KINDA_SMALL_NUMBER, M_Definition->Cargo.CacheSpawnDelaySeconds), false);
}

void AConvoy::SpawnCargoCache(const int32 CacheId)
{
	if (not GetIsValidDefinition() or FindCache(CacheId) == nullptr)
	{
		return;
	}
	TWeakObjectPtr<AConvoy> WeakThis(this);
	const FSoftObjectPath ClassPath = M_Definition->Cargo.CacheClass.ToSoftObjectPath();
	UAssetManager::GetStreamableManager().RequestAsyncLoad(ClassPath, [WeakThis, CacheId, ClassPath]()
	{
		AConvoy* Convoy = WeakThis.Get();
		FConvoyCargoCache* Cache = IsValid(Convoy) ? Convoy->FindCache(CacheId) : nullptr;
		UClass* CacheClass = Cast<UClass>(ClassPath.ResolveObject());
		if (Cache == nullptr)
		{
			return;
		}
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AScavengeableObject* Spawned = IsValid(CacheClass)
			                               ? Convoy->GetWorld()->SpawnActor<AScavengeableObject>(
				                               CacheClass, Cache->Location, FRotator::ZeroRotator, Parameters)
			                               : nullptr;
		if (not IsValid(Spawned))
		{
			Cache->bSpawnFailed = true;
			Convoy->Broadcast(EConvoyEventType::CargoSpawned, nullptr,
			                  FString::Printf(TEXT("cache=%d FAILED class=%s"), CacheId, *ClassPath.ToString()),
			                  Cache->Location);
			Convoy->TryDestroySelfWhenCachesResolved();
			return;
		}
		Spawned->SetFixedScavengeRewards(Cache->Value.Resources, Convoy->M_Definition->Cargo.CacheScavengeSeconds);
		Spawned->OnScavenged.AddUObject(Convoy, &AConvoy::OnCacheScavenged, CacheId);
		Spawned->OnScavengedOrDestroyed.AddUObject(Convoy, &AConvoy::OnCacheScavengedOrDestroyed, CacheId);
		Cache->Cache = Spawned;
		Cache->SpawnTime = Convoy->GetNow();
		Convoy->GetWorld()->GetTimerManager().SetTimer(
			Cache->ExpiryTimer, FTimerDelegate::CreateUObject(Convoy, &AConvoy::OnCacheExpired, CacheId),
			Convoy->M_Definition->Cargo.CacheLifetimeSeconds, false);
		if (Convoy->M_Params.bShowOnMinimap)
		{
			Cache->IconId = URTSBlueprintFunctionLibrary::BP_AddCustomMiniMapIconAttachedToActor(
				Convoy, FName(*FString::Printf(TEXT("ConvoyCache_%d_%d"), Convoy->M_ConvoyId, CacheId)),
				Convoy->M_Definition->CacheMinimapIcon, Cache->Location, Spawned, FRotator::ZeroRotator, true,
				FMinimapIconTextPayload());
		}
		FString Rewards;
		for (const TPair<ERTSResourceType, int32>& Pair : Cache->Value.Resources)
		{
			Rewards += FString::Printf(TEXT("%s:%d "), *Global_GetResourceTypeAsString(Pair.Key), Pair.Value);
		}
		Convoy->Broadcast(EConvoyEventType::CargoSpawned, nullptr,
		                  FString::Printf(TEXT("cache=%d actor=%s %s"), CacheId, *Spawned->GetName(), *Rewards),
		                  Cache->Location);
	});
}

void AConvoy::OnCacheScavenged(const int32 CacheId)
{
	if (FConvoyCargoCache* Cache = FindCache(CacheId))
	{
		Cache->bScavengeCompleted = true;
	}
}

void AConvoy::OnCacheScavengedOrDestroyed(const int32 CacheId)
{
	// The scavenge object announces "gone" before "scavenged" in the same frame; decide on the next tick.
	TWeakObjectPtr<AConvoy> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis, CacheId]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ResolveCache(CacheId);
		}
	});
}

void AConvoy::ResolveCache(const int32 CacheId)
{
	FConvoyCargoCache* Cache = FindCache(CacheId);
	if (Cache == nullptr or Cache->bExpired or Cache->bScavenged)
	{
		return;
	}
	Cache->bScavenged = true;
	GetWorld()->GetTimerManager().ClearTimer(Cache->ExpiryTimer);
	if (Cache->IconId != NAME_None)
	{
		URTSBlueprintFunctionLibrary::BP_RemoveCustomMiniMapIcon(this, Cache->IconId);
		Cache->IconId = NAME_None;
	}
	// Only a finished scavenge pays (the scavenge object paid the player); a cache shot to pieces pays nothing.
	Broadcast(EConvoyEventType::CargoScavenged, nullptr,
	          FString::Printf(TEXT("cache=%d total=%d scavenged=%d"), CacheId,
	                          Cache->bScavengeCompleted ? Cache->Value.GetTotal() : 0,
	                          Cache->bScavengeCompleted ? 1 : 0), Cache->Location);
	TryDestroySelfWhenCachesResolved();
}

void AConvoy::OnCacheExpired(const int32 CacheId)
{
	FConvoyCargoCache* Cache = FindCache(CacheId);
	if (Cache == nullptr or Cache->bScavenged or Cache->bExpired)
	{
		return;
	}
	Cache->bExpired = true;
	if (Cache->IconId != NAME_None)
	{
		URTSBlueprintFunctionLibrary::BP_RemoveCustomMiniMapIcon(this, Cache->IconId);
		Cache->IconId = NAME_None;
	}
	if (Cache->Cache.IsValid())
	{
		Cache->Cache->OnScavengedOrDestroyed.RemoveAll(this);
		Cache->Cache->OnScavenged.RemoveAll(this);
		Cache->Cache->Destroy();
	}
	Broadcast(EConvoyEventType::CargoExpired, nullptr, FString::Printf(TEXT("cache=%d"), CacheId), Cache->Location);
	TryDestroySelfWhenCachesResolved();
}

void AConvoy::ReleaseMember(FConvoyMember& Member, const bool bDespawn)
{
	if (Member.State == EConvoyMemberState::Released or Member.State == EConvoyMemberState::Dead)
	{
		return;
	}
	if (Member.Tank.IsValid())
	{
		if (Member.DeathHandle.IsValid())
		{
			Member.Tank->OnUnitDies.Remove(Member.DeathHandle);
		}
		Member.Tank->ClearMobilityRestriction(this);
	}
	Member.DeathHandle.Reset();
	if (Member.Squad.IsValid())
	{
		Member.Squad->SetMarchSpeedOverride(0.f);
	}
	Member.State = EConvoyMemberState::Released;
	Broadcast(EConvoyEventType::RestrictionCleared, &Member, bDespawn ? TEXT("despawn") : TEXT("released alive"));
	if (not bDespawn)
	{
		return;
	}
	if (Member.Squad.IsValid())
	{
		for (ASquadUnit* Soldier : Member.Squad->GetSquadUnitsChecked())
		{
			if (IsValid(Soldier))
			{
				Soldier->Destroy();
			}
		}
	}
	if (Member.Actor.IsValid())
	{
		Member.Actor->Destroy();
	}
	Broadcast(EConvoyEventType::Despawned, &Member, Member.UnitName);
}

void AConvoy::FinishConvoy(const EConvoyOutcome Outcome, const bool bDespawnUnits)
{
	if (M_Outcome != EConvoyOutcome::None)
	{
		return;
	}
	M_Outcome = Outcome;
	GetWorld()->GetTimerManager().ClearTimer(M_TickTimer);
	for (FConvoyMember& Member : M_Members)
	{
		if (Member.State == EConvoyMemberState::Spawning and Member.Actor.IsValid())
		{
			Member.Actor->Destroy();
			Member.State = EConvoyMemberState::Dead;
			continue;
		}
		if (Member.GetIsAlive())
		{
			ReleaseMember(Member, bDespawnUnits);
		}
	}
	if (M_ConvoyIconId != NAME_None)
	{
		URTSBlueprintFunctionLibrary::BP_RemoveCustomMiniMapIcon(this, M_ConvoyIconId);
		M_ConvoyIconId = NAME_None;
	}
	SetState(EConvoyState::Finished, ConvoyNames::GetOutcomeName(Outcome));
	Broadcast(EConvoyEventType::Finished, nullptr,
	          FString::Printf(TEXT("outcome=%s open_caches=%d"), *ConvoyNames::GetOutcomeName(Outcome),
	                          GetOpenCacheCount()));
	OnConvoyFinished.Broadcast(this, Outcome);
	TryDestroySelfWhenCachesResolved();
}

void AConvoy::TryDestroySelfWhenCachesResolved()
{
	if (M_Outcome == EConvoyOutcome::None)
	{
		return;
	}
	for (const FConvoyCargoCache& Cache : M_Caches)
	{
		const bool bPendingSpawn = not Cache.Cache.IsValid() and not Cache.bSpawnFailed and not Cache.bExpired
			and not Cache.bScavenged;
		if (bPendingSpawn or (not Cache.bScavenged and not Cache.bExpired and not Cache.bSpawnFailed))
		{
			return;
		}
	}
	TWeakObjectPtr<AConvoy> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(M_SelfDestroyTimer, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Destroy();
		}
	}, ConvoyCargoConstants::SelfDestroyDelaySeconds, false);
}

void AConvoy::RemoveAllMinimapIcons()
{
	if (M_ConvoyIconId != NAME_None)
	{
		URTSBlueprintFunctionLibrary::BP_RemoveCustomMiniMapIcon(this, M_ConvoyIconId);
		M_ConvoyIconId = NAME_None;
	}
	for (FConvoyCargoCache& Cache : M_Caches)
	{
		if (Cache.IconId != NAME_None)
		{
			URTSBlueprintFunctionLibrary::BP_RemoveCustomMiniMapIcon(this, Cache.IconId);
			Cache.IconId = NAME_None;
		}
	}
}
