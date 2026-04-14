#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTSFireManager.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class URTSFirePoolSettings;
class USceneComponent;

namespace RTSFireConstants
{
	constexpr float InactiveTimeSeconds = -1.0f;
}

USTRUCT()
struct FRTSFirePoolEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_NiagaraComponent = nullptr;

	float M_ActivatedAtSeconds = RTSFireConstants::InactiveTimeSeconds;
	float M_TimeActiveSeconds = 0.0f;
	int32 M_FireHandle = INDEX_NONE;
	bool bM_IsActive = false;
	bool bM_IsAttached = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_AttachedActor;

	FVector M_AttachOffset = FVector::ZeroVector;
	FVector M_RequestedScale = FVector::OneVector;
	FTimerHandle M_DormantTimerHandle;
};

USTRUCT()
struct FRTSFirePool
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_FireSystem = nullptr;

	UPROPERTY()
	TArray<FRTSFirePoolEntry> M_Entries;

	TArray<int32> M_FreeList;
};

/**
 * @brief Spawned by the fire subsystem to pool, activate, and recycle fire Niagara components.
 * Ticks every second to reclaim expired pooled fire instances.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSFireManager : public AActor
{
	GENERATED_BODY()

public:
	ARTSFireManager();

	virtual void Tick(float DeltaSeconds) override;

	void InitFireManager(const URTSFirePoolSettings* Settings);

	/**
	 * @brief Activate pooled fire at a world location and scale for a limited duration.
	 * @param FireType Fire pool to use for the effect.
	 * @param LifeTimeSeconds Seconds to keep active; <= 0 keeps it active until manually reclaimed.
	 * @param Location World location to place the fire.
	 * @param Scale World scale to apply to the fire.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	int32 ActivateFireAtLocation(ERTSFireType FireType,
	                            float LifeTimeSeconds,
	                            const FVector& Location,
	                            const FVector& Scale);

	/**
	 * @brief Activate pooled fire attached to an actor for a limited duration.
	 * @param AttachActor Actor to attach the fire to.
	 * @param FireType Fire pool to use for the effect.
	 * @param LifeTimeSeconds Seconds to keep active; <= 0 keeps it active until manually reclaimed.
	 * @param AttachOffset Offset from the actor root while attached.
	 * @param Scale World scale to apply to the fire (does not inherit actor scale).
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	int32 ActivateFireAttached(AActor* AttachActor,
	                          ERTSFireType FireType,
	                          float LifeTimeSeconds,
	                          const FVector& AttachOffset,
	                          const FVector& Scale);

	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	bool StopFireByHandle(int32 FireHandle);

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_RootComponent = nullptr;

	// Pools keyed by fire type for quick lookup during activation and ticking.
	UPROPERTY()
	TMap<ERTSFireType, FRTSFirePool> M_FirePools;
	int32 M_NextFireHandle = 1;

	void InitPoolsFromSettings(const URTSFirePoolSettings* Settings);
	void CreatePoolForType(ERTSFireType FireType, const UNiagaraSystem* NiagaraSystem, int32 PoolSize);
	void ConfigureEntryDormant(FRTSFirePoolEntry& Entry) const;
	void PrepareEntryForReuse(FRTSFirePoolEntry& Entry) const;
	bool ActivateEntryAtLocation(FRTSFirePoolEntry& Entry,
	                             float LifeTimeSeconds,
	                             const FVector& Location,
	                             const FVector& Scale) const;
	bool ActivateEntryAttached(FRTSFirePoolEntry& Entry,
	                           AActor* AttachActor,
	                           float LifeTimeSeconds,
	                           const FVector& AttachOffset,
	                           const FVector& Scale) const;
	void StartLifeTimeDelegateIfNeeded(ERTSFireType FireType, int32 EntryIndex, float LifeTimeSeconds);
	void HandleLifeTimeExpired(ERTSFireType FireType, int32 EntryIndex, int32 ExpectedFireHandle);
	void ReleaseEntryToPool(FRTSFirePool& Pool, int32 EntryIndex);
	int32 AcquireEntryIndex(FRTSFirePool& Pool);
	int32 AcquireNextFireHandle();
	int32 FindOldestActiveEntryIndex(const FRTSFirePool& Pool) const;
	bool ShouldEntryBeDormant(const FRTSFirePoolEntry& Entry, float CurrentTimeSeconds) const;
	bool GetIsValidWorld() const;
};
