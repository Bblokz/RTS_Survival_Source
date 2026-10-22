#include "RTSFireManager.h"

#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FirePoolSettings/RTSFirePoolSettings.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSFireManagerConstants
{
	constexpr float FireManagerTickIntervalSeconds = 1.0f;
	constexpr int32 MinimumPoolSize = 1;
	// Below this much damage a tick sends no overlap query (e.g. a tick right after the area started).
	constexpr float MinimumGroundFireDamagePerTick = 0.5f;
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

	TickGroundFires(CurrentTimeSeconds);
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

	InitGroundFireSettings(Settings);
	InitPoolsFromSettings(Settings);
}

int32 ARTSFireManager::ActivateFireAtLocation(const ERTSFireType FireType,
                                              const float LifeTimeSeconds,
                                              const FVector& Location,
                                              const FVector& Scale,
                                              const FRTSFireEffectParams& EffectParams)
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
	if (not ActivateEntryAtLocation(Entry, LifeTimeSeconds, Location, Scale, EffectParams))
	{
		RestoreFailedEntryToPool(*Pool, EntryIndex);
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
                                            const FVector& Scale,
                                            const FRTSFireEffectParams& EffectParams)
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
	if (not ActivateEntryAttached(Entry, AttachActor, LifeTimeSeconds, AttachOffset, Scale, EffectParams))
	{
		RestoreFailedEntryToPool(*Pool, EntryIndex);
		return INDEX_NONE;
	}

	Entry.M_FireHandle = FireHandle;
	StartLifeTimeDelegateIfNeeded(FireType, EntryIndex, LifeTimeSeconds);
	return FireHandle;
}

int32 ARTSFireManager::ActivateFireAttachedToComponent(AActor* AttachActor,
	USceneComponent* AttachComponent,
	const FName SocketName,
	const FVector& RelativeOffset,
	const ERTSFireType FireType,
	const float LifeTimeSeconds,
	const FVector& Scale,
	const FRTSFireEffectParams& EffectParams)
{
	if (not GetIsValidWorld())
	{
		return INDEX_NONE;
	}
	if (not IsValid(AttachActor) || not IsValid(AttachComponent) || AttachComponent->GetOwner() != AttachActor)
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAttachedToComponent - attachment is invalid."));
		return INDEX_NONE;
	}
	if (SocketName != NAME_None && not AttachComponent->DoesSocketExist(SocketName))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAttachedToComponent - socket does not exist."));
		return INDEX_NONE;
	}

	FRTSFirePool* Pool = M_FirePools.Find(FireType);
	if (Pool == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAttachedToComponent - fire type not configured."));
		return INDEX_NONE;
	}

	const int32 EntryIndex = AcquireEntryIndex(*Pool);
	if (EntryIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateFireAttachedToComponent - failed to acquire entry."));
		return INDEX_NONE;
	}

	FRTSFirePoolEntry& Entry = Pool->M_Entries[EntryIndex];
	if (not ActivateEntryAttachedToComponent(Entry, AttachActor, AttachComponent, SocketName, RelativeOffset,
		LifeTimeSeconds, Scale, EffectParams))
	{
		RestoreFailedEntryToPool(*Pool, EntryIndex);
		return INDEX_NONE;
	}

	const int32 FireHandle = AcquireNextFireHandle();
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

FRTSGroundFireRequestResult ARTSFireManager::RequestGroundFire(const FRTSGroundFireSettings& Settings,
                                                               const FVector& GroundLocation,
                                                               const int32 OwningPlayer)
{
	if (not GetIsValidWorld())
	{
		return FRTSGroundFireRequestResult();
	}

	return RequestGroundFireAtTime(Settings, GroundLocation, OwningPlayer, GetWorld()->GetTimeSeconds());
}

int32 ARTSFireManager::GetActiveFireCount(const ERTSFireType FireType) const
{
	const FRTSFirePool* Pool = M_FirePools.Find(FireType);
	if (Pool == nullptr)
	{
		return 0;
	}

	int32 ActiveCount = 0;
	for (const FRTSFirePoolEntry& Entry : Pool->M_Entries)
	{
		ActiveCount += Entry.bM_IsActive ? 1 : 0;
	}
	return ActiveCount;
}

int32 ARTSFireManager::GetFreeFireCount(const ERTSFireType FireType) const
{
	const FRTSFirePool* Pool = M_FirePools.Find(FireType);
	return Pool == nullptr ? 0 : Pool->M_FreeList.Num();
}

FRTSGroundFireRequestResult ARTSFireManager::RequestGroundFireAtTime(const FRTSGroundFireSettings& Settings,
                                                                     const FVector& GroundLocation,
                                                                     const int32 OwningPlayer,
                                                                     const float CurrentTimeSeconds)
{
	FRTSGroundFireRequestResult Result;
	if (not Settings.GetIsEnabled())
	{
		return Result;
	}

	// Three rockets of one salvo landing together keep one area burning instead of three.
	const int32 MergeIndex = FindMergeableGroundFireIndex(Settings, GroundLocation, OwningPlayer);
	if (MergeIndex != INDEX_NONE)
	{
		FRTSGroundFireArea& Area = M_GroundFireAreas[MergeIndex];
		MergeIntoGroundFire(Area, Settings, CurrentTimeSeconds);
		Result.AreaHandle = Area.AreaHandle;
		Result.bMergedIntoExistingArea = true;
		return Result;
	}

	if (M_GroundFireAreas.Num() >= M_MaxActiveGroundFires)
	{
		const int32 ReplaceIndex = FindGroundFireClosestToBurningOut();
		if (ReplaceIndex != INDEX_NONE)
		{
			PutOutGroundFireAt(ReplaceIndex);
			Result.bReplacedOldestArea = true;
		}
	}

	FRTSGroundFireArea NewArea;
	NewArea.Settings = Settings;
	NewArea.Location = GroundLocation;
	NewArea.EndTimeSeconds = CurrentTimeSeconds + Settings.DurationSeconds;
	NewArea.LastDamageTimeSeconds = CurrentTimeSeconds;
	NewArea.OwningPlayer = OwningPlayer;
	NewArea.AreaHandle = M_NextGroundFireAreaHandle;
	M_NextGroundFireAreaHandle = M_NextGroundFireAreaHandle >= MAX_int32 - 1 ? 1 : M_NextGroundFireAreaHandle + 1;
	// The area may burn without flames when the pool is missing; gameplay does not depend on the VFX asset.
	NewArea.VfxFireHandle = StartGroundFireVfx(Settings, GroundLocation);
	M_GroundFireAreas.Add(NewArea);

	Result.AreaHandle = NewArea.AreaHandle;
	return Result;
}

void ARTSFireManager::TickGroundFires(const float CurrentTimeSeconds)
{
	// Reverse order: PutOutGroundFireAt swaps the last (already processed) area into the removed slot.
	for (int32 AreaIndex = M_GroundFireAreas.Num() - 1; AreaIndex >= 0; --AreaIndex)
	{
		FRTSGroundFireArea& Area = M_GroundFireAreas[AreaIndex];
		const float BurnUntilSeconds = FMath::Min(CurrentTimeSeconds, Area.EndTimeSeconds);
		const float BurnSeconds = BurnUntilSeconds - Area.LastDamageTimeSeconds;
		if (BurnSeconds > 0.f && Area.Settings.DamagePerSecond * BurnSeconds >=
			RTSFireManagerConstants::MinimumGroundFireDamagePerTick)
		{
			DealGroundFireDamage(Area, BurnSeconds);
			Area.LastDamageTimeSeconds = BurnUntilSeconds;
		}

		if (CurrentTimeSeconds >= Area.EndTimeSeconds)
		{
			PutOutGroundFireAt(AreaIndex);
		}
	}
}

int32 ARTSFireManager::FindMergeableGroundFireIndex(const FRTSGroundFireSettings& Settings,
                                                    const FVector& GroundLocation,
                                                    const int32 OwningPlayer) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const int32 AreaCount = M_GroundFireAreas.Num();
	for (int32 AreaIndex = 0; AreaIndex < AreaCount; ++AreaIndex)
	{
		const FRTSGroundFireArea& Area = M_GroundFireAreas[AreaIndex];
		if (Area.OwningPlayer != OwningPlayer)
		{
			continue;
		}

		const float MergeDistance = M_GroundFireMergeDistanceFraction
			* FMath::Max(Area.Settings.RadiusCm, Settings.RadiusCm);
		const float DistanceSquared = FVector::DistSquared2D(Area.Location, GroundLocation);
		if (DistanceSquared <= FMath::Square(MergeDistance) && DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestIndex = AreaIndex;
		}
	}
	return BestIndex;
}

int32 ARTSFireManager::FindGroundFireClosestToBurningOut() const
{
	int32 BestIndex = INDEX_NONE;
	float EarliestEndTime = TNumericLimits<float>::Max();
	const int32 AreaCount = M_GroundFireAreas.Num();
	for (int32 AreaIndex = 0; AreaIndex < AreaCount; ++AreaIndex)
	{
		if (M_GroundFireAreas[AreaIndex].EndTimeSeconds < EarliestEndTime)
		{
			EarliestEndTime = M_GroundFireAreas[AreaIndex].EndTimeSeconds;
			BestIndex = AreaIndex;
		}
	}
	return BestIndex;
}

void ARTSFireManager::MergeIntoGroundFire(FRTSGroundFireArea& Area,
                                          const FRTSGroundFireSettings& Settings,
                                          const float CurrentTimeSeconds) const
{
	// Refresh, never stack: the area burns as long and as hard as the strongest fire that fed it.
	Area.EndTimeSeconds = FMath::Max(Area.EndTimeSeconds, CurrentTimeSeconds + Settings.DurationSeconds);
	Area.Settings.DurationSeconds = FMath::Max(Area.Settings.DurationSeconds, Settings.DurationSeconds);
	Area.Settings.RadiusCm = FMath::Max(Area.Settings.RadiusCm, Settings.RadiusCm);
	Area.Settings.DamagePerSecond = FMath::Max(Area.Settings.DamagePerSecond, Settings.DamagePerSecond);
	Area.Settings.FullArmorPen = FMath::Max(Area.Settings.FullArmorPen, Settings.FullArmorPen);
	Area.Settings.MaxArmorPen = FMath::Max(Area.Settings.MaxArmorPen, Settings.MaxArmorPen);
}

int32 ARTSFireManager::StartGroundFireVfx(const FRTSGroundFireSettings& Settings, const FVector& GroundLocation)
{
	if (not M_FirePools.Contains(ERTSFireType::GroundFire))
	{
		if (not bM_HasReportedMissingGroundFirePool)
		{
			bM_HasReportedMissingGroundFirePool = true;
			RTSFunctionLibrary::ReportWarning(TEXT("ARTSFireManager: no GroundFire pool in RTSFirePoolSettings; "
				"ground fires burn without flames."));
		}
		return INDEX_NONE;
	}

	// Spread the flames over the radius in X/Y only; the Niagara Scale parameter sizes the flames themselves.
	const float SpreadScale = Settings.RadiusCm / FMath::Max(M_GroundFireVfxBaseRadiusCm, 1.f);
	const FVector ComponentScale(SpreadScale, SpreadScale, 1.f);
	FRTSFireEffectParams EffectParams;
	EffectParams.Scale = Settings.VfxScale;
	// Lifetime 0: no dormant timer per fire; the area puts its flames out when it burns out.
	constexpr float KeepActiveUntilStopped = 0.f;
	return ActivateFireAtLocation(ERTSFireType::GroundFire, KeepActiveUntilStopped, GroundLocation, ComponentScale,
	                              EffectParams);
}

void ARTSFireManager::PutOutGroundFireAt(const int32 AreaIndex)
{
	if (not M_GroundFireAreas.IsValidIndex(AreaIndex))
	{
		return;
	}

	const int32 VfxFireHandle = M_GroundFireAreas[AreaIndex].VfxFireHandle;
	if (VfxFireHandle != INDEX_NONE)
	{
		StopFireByHandle(VfxFireHandle);
	}
	M_GroundFireAreas.RemoveAtSwap(AreaIndex, 1, EAllowShrinking::No);
}

void ARTSFireManager::DealGroundFireDamage(const FRTSGroundFireArea& Area, const float BurnSeconds)
{
	const ETriggerOverlapLogic OverlapLogic = Area.OwningPlayer == 1
		                                          ? ETriggerOverlapLogic::OverlapEnemy
		                                          : ETriggerOverlapLogic::OverlapPlayer;
	const TArray<TWeakObjectPtr<AActor>> NoActorsToIgnore;
	// One async overlap per area per tick; measured to each victim's hull like the molotov burn.
	FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
		this,
		Area.Location,
		Area.Settings.RadiusCm,
		Area.Settings.DamagePerSecond * BurnSeconds,
		Area.Settings.DamageFalloffExponent,
		Area.Settings.FullArmorPen,
		Area.Settings.ArmorPenFallOff,
		Area.Settings.MaxArmorPen,
		ERTSDamageType::Fire,
		OverlapLogic,
		EShieldDamageSource::Flamethrower,
		NoActorsToIgnore,
		EVehicleModuleSplashPolicy::Ignore,
		static_cast<EWeaponShellType>(0),
		EAoeFalloffDistance::ClosestCollision);
}

void ARTSFireManager::InitGroundFireSettings(const URTSFirePoolSettings* Settings)
{
	M_MaxActiveGroundFires = FMath::Max(Settings->MaxActiveGroundFires, 1);
	M_GroundFireMergeDistanceFraction = FMath::Max(Settings->GroundFireMergeDistanceFraction, 0.f);
	M_GroundFireVfxBaseRadiusCm = FMath::Max(Settings->GroundFireVfxBaseRadiusCm, 1.f);
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
                                              const FVector& Scale,
                                              const FRTSFireEffectParams& EffectParams) const
{
	if (not IsValid(Entry.M_NiagaraComponent))
	{
		return false;
	}

	Entry.M_NiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Entry.M_NiagaraComponent->SetAbsolute(false, false, true);
	Entry.M_NiagaraComponent->SetWorldLocation(Location);
	Entry.M_NiagaraComponent->SetWorldScale3D(Scale);
	SetFireEffectParameters(Entry.M_NiagaraComponent, EffectParams);
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
                                            const FVector& Scale,
                                            const FRTSFireEffectParams& EffectParams) const
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
	SetFireEffectParameters(Entry.M_NiagaraComponent, EffectParams);
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

bool ARTSFireManager::ActivateEntryAttachedToComponent(FRTSFirePoolEntry& Entry,
	AActor* AttachActor,
	USceneComponent* AttachComponent,
	const FName SocketName,
	const FVector& RelativeOffset,
	const float LifeTimeSeconds,
	const FVector& Scale,
	const FRTSFireEffectParams& EffectParams) const
{
	if (not IsValid(Entry.M_NiagaraComponent) || not IsValid(AttachActor) || not IsValid(AttachComponent))
	{
		return false;
	}

	UNiagaraComponent* NiagaraComponent = Entry.M_NiagaraComponent;
	NiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	NiagaraComponent->SetAbsolute(false, false, true);
	if (not NiagaraComponent->AttachToComponent(AttachComponent, FAttachmentTransformRules::KeepRelativeTransform, SocketName))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::ActivateEntryAttachedToComponent - attachment failed."));
		return false;
	}
	NiagaraComponent->SetRelativeLocation(RelativeOffset);
	NiagaraComponent->SetRelativeRotation(FRotator::ZeroRotator);
	NiagaraComponent->SetWorldScale3D(Scale);
	SetFireEffectParameters(NiagaraComponent, EffectParams);
	NiagaraComponent->SetComponentTickEnabled(true);
	NiagaraComponent->SetVisibility(true, true);
	NiagaraComponent->ResetSystem();
	NiagaraComponent->Activate(true);

	Entry.M_ActivatedAtSeconds = GetWorld()->GetTimeSeconds();
	Entry.M_TimeActiveSeconds = LifeTimeSeconds;
	Entry.bM_IsActive = true;
	Entry.bM_IsAttached = true;
	Entry.M_AttachedActor = AttachActor;
	Entry.M_AttachOffset = RelativeOffset;
	Entry.M_RequestedScale = Scale;
	return true;
}

void ARTSFireManager::SetFireEffectParameters(UNiagaraComponent* NiagaraComponent,
                                              const FRTSFireEffectParams& EffectParams) const
{
	if (not IsValid(NiagaraComponent))
	{
		return;
	}

	const URTSFirePoolSettings* Settings = GetDefault<URTSFirePoolSettings>();
	if (not IsValid(Settings))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSFireManager::SetFireEffectParameters - fire pool settings are invalid."));
		return;
	}

	NiagaraComponent->SetVariableFloat(Settings->ScaleMltUserParamName, EffectParams.Scale);
	NiagaraComponent->SetVariableVec3(Settings->Fire_RGB_MltUserParamName, EffectParams.FireColorMlt);
	NiagaraComponent->SetVariableVec3(Settings->Smoke_RGB_MltUserParamName, EffectParams.SmokeColorMlt);
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

void ARTSFireManager::RestoreFailedEntryToPool(FRTSFirePool& Pool, const int32 EntryIndex)
{
	if (not Pool.M_Entries.IsValidIndex(EntryIndex))
	{
		return;
	}

	ConfigureEntryDormant(Pool.M_Entries[EntryIndex]);
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
