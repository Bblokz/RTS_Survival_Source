// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurrenderingTruckSubsystem.h"

#include "TimerManager.h"
#include "Engine/World.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/Enemy/Convoy/SurrenderingCargoTruck/SurrenderingCaseTruck.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

USurrenderingTruckSubsystem* USurrenderingTruckSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<USurrenderingTruckSubsystem>() : nullptr;
}

void USurrenderingTruckSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		for (FSealedCacheRecord& Record : M_SealedCaches)
		{
			World->GetTimerManager().ClearTimer(Record.SpawnTimerHandle);
		}
	}
	M_SealedCaches.Empty();
	M_CaseTrucks.Empty();
	Super::Deinitialize();
}

void USurrenderingTruckSubsystem::OnHideAllGameUI(const bool bHide)
{
	bM_IsGameUIHidden = bHide;
}

bool USurrenderingTruckSubsystem::TryClaimFirstSurrender()
{
	// Set at once: several trucks may surrender in the same frame, only the first one claims the line.
	if (bM_HasAnyTruckSurrendered)
	{
		return false;
	}
	bM_HasAnyTruckSurrendered = true;
	return true;
}

void USurrenderingTruckSubsystem::BroadcastFirstTruckSurrendered(ASurrenderingCargoTruck* Truck)
{
	OnFirstTruckSurrendered.Broadcast(Truck);
}

// ------------------------------------------------------------------
// Case trucks
// ------------------------------------------------------------------

void USurrenderingTruckSubsystem::RegisterCaseTruck(ASurrenderingCaseTruck* CaseTruck)
{
	if (not IsValid(CaseTruck))
	{
		return;
	}
	M_CaseTrucks.RemoveAll([](const TWeakObjectPtr<ASurrenderingCaseTruck>& Entry)
	{
		return not Entry.IsValid();
	});
	M_CaseTrucks.AddUnique(CaseTruck);
	OnCaseTruckSpawned.Broadcast(CaseTruck);
}

void USurrenderingTruckSubsystem::UnregisterCaseTruck(const ASurrenderingCaseTruck* CaseTruck)
{
	M_CaseTrucks.RemoveAll([CaseTruck](const TWeakObjectPtr<ASurrenderingCaseTruck>& Entry)
	{
		return not Entry.IsValid() or Entry.Get() == CaseTruck;
	});
}

void USurrenderingTruckSubsystem::NotifyCasesDelivered(ASurrenderingCaseTruck* CaseTruck)
{
	OnCasesDelivered.Broadcast(CaseTruck);
}

void USurrenderingTruckSubsystem::NotifyCasesEscaped(ASurrenderingCaseTruck* CaseTruck,
                                                     const FSealedCacheSettings& SealedCacheSettings)
{
	bM_HaveCasesEscaped = true;
	M_EscapedCasesCacheSettings = SealedCacheSettings;
	OnCasesEscaped.Broadcast(CaseTruck);
}

ASurrenderingCaseTruck* USurrenderingTruckSubsystem::GetActiveCaseTruck() const
{
	for (const TWeakObjectPtr<ASurrenderingCaseTruck>& Entry : M_CaseTrucks)
	{
		ASurrenderingCaseTruck* CaseTruck = Entry.Get();
		if (IsValid(CaseTruck) and CaseTruck->GetIsCarryingCases())
		{
			return CaseTruck;
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------
// Sealed caches
// ------------------------------------------------------------------

int32 USurrenderingTruckSubsystem::GetOpenSealedCacheCount() const
{
	int32 OpenCount = 0;
	for (const FSealedCacheRecord& Record : M_SealedCaches)
	{
		if (not Record.bResolved)
		{
			++OpenCount;
		}
	}
	return OpenCount;
}

AActor* USurrenderingTruckSubsystem::SpawnSealedCache(const TSubclassOf<AScavengeableObject> SealedCacheClass,
                                                      const FTransform& SpawnTransform, const float ScavengeSeconds)
{
	FSealedCacheSettings Settings;
	Settings.SealedCacheClass = SealedCacheClass;
	Settings.ScavengeSeconds = ScavengeSeconds;
	constexpr float SpawnNow = 0.f;
	return SpawnSealedCacheWithSettings(Settings, SpawnTransform, SpawnNow);
}

AActor* USurrenderingTruckSubsystem::SpawnEscapedCasesSealedCache(const FTransform& SpawnTransform)
{
	if (not bM_HaveCasesEscaped)
	{
		RTSFunctionLibrary::ReportError("SpawnEscapedCasesSealedCache: no cases escaped in this world; use "
			"SpawnSealedCache with a class instead.");
		return nullptr;
	}
	constexpr float SpawnNow = 0.f;
	return SpawnSealedCacheWithSettings(M_EscapedCasesCacheSettings, SpawnTransform, SpawnNow);
}

AScavengeableObject* USurrenderingTruckSubsystem::SpawnSealedCacheWithSettings(
	const FSealedCacheSettings& Settings, const FTransform& SpawnTransform, const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	if (Settings.SealedCacheClass == nullptr)
	{
		RTSFunctionLibrary::ReportError("Sealed cache has no SealedCacheClass; the cases have nowhere to go. Set it "
			"on the case truck Blueprint (Case Truck > Sealed Cache) or pass a class to SpawnSealedCache.");
		return nullptr;
	}
	FSealedCacheRecord& NewRecord = M_SealedCaches.AddDefaulted_GetRef();
	NewRecord.CacheId = M_NextSealedCacheId++;
	NewRecord.Settings = Settings;
	NewRecord.SpawnTransform = SpawnTransform;
	const int32 CacheId = NewRecord.CacheId;
	if (DelaySeconds <= 0.f)
	{
		SpawnSealedCacheOfRecord(CacheId);
		const FSealedCacheRecord* SpawnedRecord = FindSealedCache(CacheId);
		return SpawnedRecord != nullptr ? SpawnedRecord->Cache.Get() : nullptr;
	}
	TWeakObjectPtr<USurrenderingTruckSubsystem> WeakThis(this);
	World->GetTimerManager().SetTimer(NewRecord.SpawnTimerHandle, FTimerDelegate::CreateLambda([WeakThis, CacheId]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->SpawnSealedCacheOfRecord(CacheId);
		}
	}), DelaySeconds, false);
	return nullptr;
}

FSealedCacheRecord* USurrenderingTruckSubsystem::FindSealedCache(const int32 CacheId)
{
	return M_SealedCaches.FindByPredicate([CacheId](const FSealedCacheRecord& Record)
	{
		return Record.CacheId == CacheId;
	});
}

void USurrenderingTruckSubsystem::SpawnSealedCacheOfRecord(const int32 CacheId)
{
	UWorld* World = GetWorld();
	const FSealedCacheRecord* RecordBeforeSpawn = FindSealedCache(CacheId);
	if (World == nullptr or RecordBeforeSpawn == nullptr or RecordBeforeSpawn->bResolved)
	{
		return;
	}
	// Copied out: the cache's construction script and BeginPlay may spawn another cache, which moves the records.
	UClass* CacheClass = RecordBeforeSpawn->Settings.SealedCacheClass.Get();
	const FTransform SpawnTransform = RecordBeforeSpawn->SpawnTransform;
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AScavengeableObject* Cache = World->SpawnActor<AScavengeableObject>(CacheClass, SpawnTransform, Parameters);
	FSealedCacheRecord* Record = FindSealedCache(CacheId);
	if (Record == nullptr)
	{
		return;
	}
	if (not IsValid(Cache))
	{
		Record->bResolved = true;
		RTSFunctionLibrary::ReportError("Sealed cache failed to spawn (class "
			+ GetNameSafe(Record->Settings.SealedCacheClass.Get()) + "); the cases are lost.");
		return;
	}
	if (Record->Settings.bOverrideRewards)
	{
		Cache->SetFixedScavengeRewards(Record->Settings.Rewards, Record->Settings.ScavengeSeconds);
	}
	else if (Record->Settings.ScavengeSeconds > 0.f)
	{
		// Keeps the Blueprint's rewards; only the time changes.
		TMap<ERTSResourceType, int32> BlueprintRewards;
		const TMap<ERTSResourceType, float> ExpectedRewards = Cache->GetExpectedScavengeRewards();
		for (const TPair<ERTSResourceType, float>& Expected : ExpectedRewards)
		{
			BlueprintRewards.Add(Expected.Key, FMath::RoundToInt(Expected.Value));
		}
		Cache->SetFixedScavengeRewards(BlueprintRewards, Record->Settings.ScavengeSeconds);
	}
	Record->Cache = Cache;
	if (Record->Settings.MinimapIcon != EMinimapIconType::None)
	{
		constexpr bool bUseStaticRotation = true;
		Record->IconId = URTSBlueprintFunctionLibrary::BP_AddCustomMiniMapIconAttachedToActor(
			this, FName(*FString::Printf(TEXT("SealedCache_%d"), CacheId)), Record->Settings.MinimapIcon,
			Cache->GetActorLocation(), Cache, FRotator::ZeroRotator, bUseStaticRotation, FMinimapIconTextPayload());
	}
	BindSealedCache(*Cache, CacheId);
	UE_LOG(LogConvoy, Log, TEXT("Sealed cache %d spawned: %s at %s."), CacheId, *Cache->GetName(),
	       *Cache->GetActorLocation().ToCompactString());
	// Last: a listener may spawn another cache, which moves the records.
	OnSealedCacheSpawned.Broadcast(Cache);
}

void USurrenderingTruckSubsystem::BindSealedCache(AScavengeableObject& Cache, const int32 CacheId)
{
	Cache.OnScavenged.AddUObject(this, &USurrenderingTruckSubsystem::OnSealedCacheScavengeCompleted, CacheId);
	Cache.OnScavengedOrDestroyed.AddUObject(this, &USurrenderingTruckSubsystem::OnSealedCacheScavengedOrDestroyed,
	                                        CacheId);
}

void USurrenderingTruckSubsystem::OnSealedCacheScavengeCompleted(const int32 CacheId)
{
	if (FSealedCacheRecord* Record = FindSealedCache(CacheId))
	{
		Record->bScavengeCompleted = true;
	}
}

void USurrenderingTruckSubsystem::OnSealedCacheScavengedOrDestroyed(const int32 CacheId)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// The scavenge object announces "gone" before "scavenged" in the same frame; decide on the next tick.
	TWeakObjectPtr<USurrenderingTruckSubsystem> WeakThis(this);
	World->GetTimerManager().SetTimerForNextTick([WeakThis, CacheId]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ResolveSealedCache(CacheId);
		}
	});
}

void USurrenderingTruckSubsystem::ResolveSealedCache(const int32 CacheId)
{
	FSealedCacheRecord* Record = FindSealedCache(CacheId);
	if (Record == nullptr or Record->bResolved)
	{
		return;
	}
	Record->bResolved = true;
	RemoveSealedCacheIcon(*Record);
	AScavengeableObject* Cache = Record->Cache.Get();
	if (Record->bScavengeCompleted)
	{
		UE_LOG(LogConvoy, Log, TEXT("Sealed cache %d scavenged: the cases are recovered."), CacheId);
		OnSealedCacheScavenged.Broadcast(Cache);
		return;
	}
	if (not Record->Settings.bRespawnWhenDestroyed)
	{
		UE_LOG(LogConvoy, Warning, TEXT("Sealed cache %d destroyed before it was scavenged."), CacheId);
		OnSealedCacheDestroyed.Broadcast(Cache);
		return;
	}
	// Copied out: the new record may move the array.
	const FSealedCacheSettings Settings = Record->Settings;
	const FTransform SpawnTransform = Record->SpawnTransform;
	UE_LOG(LogConvoy, Log, TEXT("Sealed cache %d destroyed before it was scavenged; it comes back."), CacheId);
	constexpr float SpawnNow = 0.f;
	SpawnSealedCacheWithSettings(Settings, SpawnTransform, SpawnNow);
}

void USurrenderingTruckSubsystem::RemoveSealedCacheIcon(FSealedCacheRecord& Record) const
{
	if (Record.IconId.IsNone())
	{
		return;
	}
	URTSBlueprintFunctionLibrary::BP_RemoveCustomMiniMapIcon(this, Record.IconId);
	Record.IconId = NAME_None;
}
