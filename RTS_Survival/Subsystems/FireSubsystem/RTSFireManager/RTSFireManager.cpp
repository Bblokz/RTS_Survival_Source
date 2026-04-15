#include "RTSFireManager.h"

#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FirePoolSettings/RTSFirePoolSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSFireManagerConstants
{
	constexpr float FireManagerTickIntervalSeconds = 1.0f;
	constexpr int32 MinimumPoolSize = 1;
}

ARTSFireManager::ARTSFireManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = RTSFireManagerConstants::FireManagerTickIntervalSeconds;

	M_RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_RootComponent);
}

void ARTSFireManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (not GetIsValidWorld())
	{
		return;
	}

	const float CurrentTimeSeconds = GetWorld()->GetTimeSeconds();
	for (auto& PoolPair : M_FirePools)
	{
		FRTSFirePool& Pool = PoolPair.Value;
		const int32 EntryCount = Pool.M_Entries.Num();
		for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
		{
			FRTSFirePoolEntry& Entry = Pool.M_Entries[EntryIndex];
			if (not Entry.bM_IsActive)
			{
				continue;
			}
			if (ShouldEntryBeDormant(Entry, CurrentTimeSeconds))
			{
				ReleaseEntryToPool(Pool, EntryIndex);
			}
		}
	}
}

void ARTSFireManager::InitFireManager(const URTSFirePoolSettings* Settings)
{
	if (not GetIsValidWorld())
	{
		return;
	}
	if (not IsValid(Settings))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::InitFireManager - settings are invalid."));
		return;
	}

	InitPoolsFromSettings(Settings);
}

int32 ARTSFireManager::ActivateFireAtLocation(const ERTSFireType FireType,
                                              const float LifeTimeSeconds,
                                              const FVector& Location,
                                              const FVector& Scale)
{
	if (not GetIsValidWorld())
	{
		return INDEX_NONE;
	}

	FRTSFirePool* Pool = M_FirePools.Find(FireType);
	if (Pool == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAtLocation - fire type not configured."));
		return INDEX_NONE;
	}

	const int32 EntryIndex = AcquireEntryIndex(*Pool);
	if (EntryIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAtLocation - failed to acquire entry."));
		return INDEX_NONE;
	}

	FRTSFirePoolEntry& Entry = Pool->M_Entries[EntryIndex];
	const int32 FireHandle = AcquireNextFireHandle();
	if (not ActivateEntryAtLocation(Entry, LifeTimeSeconds, Location, Scale))
	{
		ReleaseEntryToPool(*Pool, EntryIndex);
		return INDEX_NONE;
	}

	Entry.M_FireHandle = FireHandle;
	StartLifeTimeDelegateIfNeeded(FireType, EntryIndex, LifeTimeSeconds);
	return FireHandle;
}

int32 ARTSFireManager::ActivateFireAttached(AActor* AttachActor,
                                            const ERTSFireType FireType,
                                            const float LifeTimeSeconds,
                                            const FVector& AttachOffset,
                                            const FVector& Scale)
{
	if (not GetIsValidWorld())
	{
		return INDEX_NONE;
	}
	if (not IsValid(AttachActor))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAttached - attach actor is invalid."));
		return INDEX_NONE;
	}

	FRTSFirePool* Pool = M_FirePools.Find(FireType);
	if (Pool == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAttached - fire type not configured."));
		return INDEX_NONE;
	}

	const int32 EntryIndex = AcquireEntryIndex(*Pool);
	if (EntryIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAttached - failed to acquire entry."));
		return INDEX_NONE;
	}

	FRTSFirePoolEntry& Entry = Pool->M_Entries[EntryIndex];
	const int32 FireHandle = AcquireNextFireHandle();
	if (not ActivateEntryAttached(Entry, AttachActor, LifeTimeSeconds, AttachOffset, Scale))
	{
		ReleaseEntryToPool(*Pool, EntryIndex);
		return INDEX_NONE;
	}

	Entry.M_FireHandle = FireHandle;
	StartLifeTimeDelegateIfNeeded(FireType, EntryIndex, LifeTimeSeconds);
	return FireHandle;
}

bool ARTSFireManager::StopFireByHandle(const int32 FireHandle)
{
	for (auto& PoolPair : M_FirePools)
	{
		FRTSFirePool& FirePool = PoolPair.Value;
		const int32 EntryCount = FirePool.M_Entries.Num();
		for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
		{
			FRTSFirePoolEntry& FirePoolEntry = FirePool.M_Entries[EntryIndex];
			if (not FirePoolEntry.bM_IsActive || FirePoolEntry.M_FireHandle != FireHandle)
			{
				continue;
			}

			ReleaseEntryToPool(FirePool, EntryIndex);
			return true;
		}
	}

	return false;
}

void ARTSFireManager::InitPoolsFromSettings(const URTSFirePoolSettings* Settings)
{
	M_FirePools.Empty();

	for (const auto& Pair : Settings->FireSettingsByType)
	{
		const FFireSettingsForType& FireSettings = Pair.Value;
		UNiagaraSystem* NiagaraSystem = FireSettings.FireSystem.LoadSynchronous();
		if (not IsValid(NiagaraSystem))
		{
			RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::InitPoolsFromSettings - failed to load Niagara system."));
			continue;
		}

		CreatePoolForType(Pair.Key, NiagaraSystem, FireSettings.PoolSize);
	}
}

void ARTSFireManager::CreatePoolForType(const ERTSFireType FireType,
                                        const UNiagaraSystem* NiagaraSystem,
                                        const int32 PoolSize)
{
	if (not IsValid(NiagaraSystem))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::CreatePoolForType - Niagara system is invalid."));
		return;
	}

	const int32 AdjustedPoolSize = FMath::Max(PoolSize, RTSFireManagerConstants::MinimumPoolSize);
	FRTSFirePool& Pool = M_FirePools.FindOrAdd(FireType);
	Pool.M_FireSystem = const_cast<UNiagaraSystem*>(NiagaraSystem);
	Pool.M_Entries.Reset(AdjustedPoolSize);
	Pool.M_FreeList.Reset(AdjustedPoolSize);

	for (int32 Index = 0; Index < AdjustedPoolSize; ++Index)
	{
		UNiagaraComponent* NiagaraComponent = NewObject<UNiagaraComponent>(this);
		if (not IsValid(NiagaraComponent))
		{
			RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::CreatePoolForType - failed to create component."));
			continue;
		}

		NiagaraComponent->SetAsset(Pool.M_FireSystem);
		NiagaraComponent->bAutoActivate = false;
		NiagaraComponent->SetAutoDestroy(false);
		NiagaraComponent->SetupAttachment(M_RootComponent);
		NiagaraComponent->RegisterComponent();

		FRTSFirePoolEntry Entry;
		Entry.M_NiagaraComponent = NiagaraComponent;
		Entry.M_RequestedScale = FVector::OneVector;
		ConfigureEntryDormant(Entry);
		Pool.M_Entries.Add(Entry);
		Pool.M_FreeList.Add(Pool.M_Entries.Num() - 1);
	}
}

void ARTSFireManager::ConfigureEntryDormant(FRTSFirePoolEntry& Entry) const
{
	if (not IsValid(Entry.M_NiagaraComponent))
	{
		return;
	}

	Entry.M_NiagaraComponent->DeactivateImmediate();
	Entry.M_NiagaraComponent->SetAutoActivate(false);
	Entry.M_NiagaraComponent->SetComponentTickEnabled(false);
	Entry.M_NiagaraComponent->SetVisibility(false, true);
	Entry.M_NiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Entry.M_ActivatedAtSeconds = RTSFireConstants::InactiveTimeSeconds;
	Entry.M_TimeActiveSeconds = 0.0f;
	Entry.M_FireHandle = INDEX_NONE;
	Entry.bM_IsActive = false;
	Entry.bM_IsAttached = false;
	Entry.M_AttachedActor.Reset();
	Entry.M_AttachOffset = FVector::ZeroVector;
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(Entry.M_DormantTimerHandle);
	}
}

void ARTSFireManager::PrepareEntryForReuse(FRTSFirePoolEntry& Entry) const
{
	ConfigureEntryDormant(Entry);
}

bool ARTSFireManager::ActivateEntryAtLocation(FRTSFirePoolEntry& Entry,
                                              const float LifeTimeSeconds,
                                              const FVector& Location,
                                              const FVector& Scale) const
{
	if (not IsValid(Entry.M_NiagaraComponent))
	{
		return false;
	}

	Entry.M_NiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Entry.M_NiagaraComponent->SetAbsolute(false, false, true);
	Entry.M_NiagaraComponent->SetWorldLocation(Location);
	Entry.M_NiagaraComponent->SetWorldScale3D(Scale);
	Entry.M_NiagaraComponent->SetComponentTickEnabled(true);
	Entry.M_NiagaraComponent->SetVisibility(true, true);
	Entry.M_NiagaraComponent->ResetSystem();
	Entry.M_NiagaraComponent->Activate(true);

	Entry.M_ActivatedAtSeconds = GetWorld()->GetTimeSeconds();
	Entry.M_TimeActiveSeconds = LifeTimeSeconds;
	Entry.bM_IsActive = true;
	Entry.bM_IsAttached = false;
	Entry.M_AttachedActor.Reset();
	Entry.M_AttachOffset = FVector::ZeroVector;
	Entry.M_RequestedScale = Scale;
	return true;
}

bool ARTSFireManager::ActivateEntryAttached(FRTSFirePoolEntry& Entry,
                                            AActor* AttachActor,
                                            const float LifeTimeSeconds,
                                            const FVector& AttachOffset,
                                            const FVector& Scale) const
{
	if (not IsValid(Entry.M_NiagaraComponent) || not IsValid(AttachActor))
	{
		return false;
	}

	USceneComponent* AttachRoot = AttachActor->GetRootComponent();
	if (not IsValid(AttachRoot))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateEntryAttached - attach actor has no root."));
		return false;
	}

	const FVector WorldLocation = AttachRoot->GetComponentLocation() + AttachOffset;
	Entry.M_NiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Entry.M_NiagaraComponent->SetAbsolute(false, false, true);
	Entry.M_NiagaraComponent->SetWorldLocation(WorldLocation);
	Entry.M_NiagaraComponent->SetWorldScale3D(Scale);
	Entry.M_NiagaraComponent->AttachToComponent(AttachRoot, FAttachmentTransformRules::KeepWorldTransform);
	Entry.M_NiagaraComponent->SetRelativeLocation(AttachOffset);
	Entry.M_NiagaraComponent->SetComponentTickEnabled(true);
	Entry.M_NiagaraComponent->SetVisibility(true, true);
	Entry.M_NiagaraComponent->ResetSystem();
	Entry.M_NiagaraComponent->Activate(true);

	Entry.M_ActivatedAtSeconds = GetWorld()->GetTimeSeconds();
	Entry.M_TimeActiveSeconds = LifeTimeSeconds;
	Entry.bM_IsActive = true;
	Entry.bM_IsAttached = true;
	Entry.M_AttachedActor = AttachActor;
	Entry.M_AttachOffset = AttachOffset;
	Entry.M_RequestedScale = Scale;
	return true;
}

void ARTSFireManager::ReleaseEntryToPool(FRTSFirePool& Pool, const int32 EntryIndex)
{
	if (not Pool.M_Entries.IsValidIndex(EntryIndex))
	{
		return;
	}

	FRTSFirePoolEntry& Entry = Pool.M_Entries[EntryIndex];
	if (not Entry.bM_IsActive)
	{
		return;
	}

	ConfigureEntryDormant(Entry);
	Pool.M_FreeList.Add(EntryIndex);
}

int32 ARTSFireManager::AcquireEntryIndex(FRTSFirePool& Pool)
{
	if (Pool.M_FreeList.Num() > 0)
	{
		return Pool.M_FreeList.Pop(EAllowShrinking::No);
	}

	const int32 OldestIndex = FindOldestActiveEntryIndex(Pool);
	if (OldestIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	PrepareEntryForReuse(Pool.M_Entries[OldestIndex]);
	return OldestIndex;
}

int32 ARTSFireManager::AcquireNextFireHandle()
{
	const int32 AllocatedFireHandle = M_NextFireHandle;
	++M_NextFireHandle;

	if (M_NextFireHandle <= 0)
	{
		M_NextFireHandle = 1;
	}

	return AllocatedFireHandle;
}

int32 ARTSFireManager::FindOldestActiveEntryIndex(const FRTSFirePool& Pool) const
{
	int32 OldestIndex = INDEX_NONE;
	float OldestTime = TNumericLimits<float>::Max();

	const int32 EntryCount = Pool.M_Entries.Num();
	for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
	{
		const FRTSFirePoolEntry& Entry = Pool.M_Entries[EntryIndex];
		if (not Entry.bM_IsActive)
		{
			continue;
		}
		if (Entry.M_ActivatedAtSeconds < OldestTime)
		{
			OldestTime = Entry.M_ActivatedAtSeconds;
			OldestIndex = EntryIndex;
		}
	}

	if (OldestIndex != INDEX_NONE)
	{
		RTSFunctionLibrary::ReportWarning(TEXT("ARTSFireManager: pool exhausted, reusing oldest fire."));
	}

	return OldestIndex;
}

bool ARTSFireManager::ShouldEntryBeDormant(const FRTSFirePoolEntry& Entry, const float CurrentTimeSeconds) const
{
	if (Entry.bM_IsAttached && not Entry.M_AttachedActor.IsValid())
	{
		return true;
	}
	if (Entry.M_TimeActiveSeconds <= 0.0f)
	{
		return false;
	}
	return (CurrentTimeSeconds - Entry.M_ActivatedAtSeconds) >= Entry.M_TimeActiveSeconds;
}

void ARTSFireManager::StartLifeTimeDelegateIfNeeded(
	const ERTSFireType FireType,
	const int32 EntryIndex,
	const float LifeTimeSeconds)
{
	if (LifeTimeSeconds <= 0.0f)
	{
		return;
	}
	if (not GetIsValidWorld())
	{
		return;
	}

	FRTSFirePool* FirePool = M_FirePools.Find(FireType);
	if (FirePool == nullptr || not FirePool->M_Entries.IsValidIndex(EntryIndex))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::StartLifeTimeDelegateIfNeeded - fire entry is invalid."));
		return;
	}

	const int32 FireHandle = FirePool->M_Entries[EntryIndex].M_FireHandle;
	const TWeakObjectPtr<ARTSFireManager> WeakThis(this);
	FTimerDelegate DormantDelegate;
	DormantDelegate.BindLambda([WeakThis, FireType, EntryIndex, FireHandle]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->HandleLifeTimeExpired(FireType, EntryIndex, FireHandle);
	});

	GetWorld()->GetTimerManager().SetTimer(
		FirePool->M_Entries[EntryIndex].M_DormantTimerHandle,
		DormantDelegate,
		LifeTimeSeconds,
		false);
}

void ARTSFireManager::HandleLifeTimeExpired(
	const ERTSFireType FireType,
	const int32 EntryIndex,
	const int32 ExpectedFireHandle)
{
	FRTSFirePool* FirePool = M_FirePools.Find(FireType);
	if (FirePool == nullptr || not FirePool->M_Entries.IsValidIndex(EntryIndex))
	{
		return;
	}

	const FRTSFirePoolEntry& FirePoolEntry = FirePool->M_Entries[EntryIndex];
	if (not FirePoolEntry.bM_IsActive || FirePoolEntry.M_FireHandle != ExpectedFireHandle)
	{
		return;
	}

	ReleaseEntryToPool(*FirePool, EntryIndex);
}

bool ARTSFireManager::GetIsValidWorld() const
{
	if (not IsValid(GetWorld()))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager: World is invalid."));
		return false;
	}

	return true;
}
