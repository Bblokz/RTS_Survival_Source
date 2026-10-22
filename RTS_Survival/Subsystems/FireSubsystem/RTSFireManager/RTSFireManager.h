#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FRTSFireEffectParams.h"
#include "RTS_Survival/Subsystems/FireSubsystem/GroundFire/RTSGroundFireSettings.h"
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

/** One burning patch of ground: its damage is dealt by the fire manager's tick, its flames are a pooled GroundFire. */
struct FRTSGroundFireArea
{
	FRTSGroundFireSettings Settings;
	FVector Location = FVector::ZeroVector;
	float EndTimeSeconds = 0.f;
	float LastDamageTimeSeconds = 0.f;
	int32 OwningPlayer = 0;
	int32 AreaHandle = INDEX_NONE;
	// Pooled GroundFire Niagara of this area, or INDEX_NONE when the pool is not configured (the area still burns).
	int32 VfxFireHandle = INDEX_NONE;
};

/** Outcome of a ground fire request. */
struct FRTSGroundFireRequestResult
{
	// Handle of the area that burns for this request (new or refreshed), INDEX_NONE when nothing burns.
	int32 AreaHandle = INDEX_NONE;
	// True when the request refreshed an overlapping area of the same player instead of starting a new one.
	bool bMergedIntoExistingArea = false;
	// True when the cap was reached and the area closest to burning out was put out for this one.
	bool bReplacedOldestArea = false;
};

/**
 * @brief Spawned by the fire subsystem to pool, activate, and recycle fire Niagara components.
 * Ticks every second to reclaim expired pooled fire instances and to burn the ground fire areas.
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
	 * @param EffectParams Niagara scale and color multipliers for this activation.
	 * @return Handle for the active fire, or INDEX_NONE if activation fails.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	int32 ActivateFireAtLocation(ERTSFireType FireType,
	                            float LifeTimeSeconds,
	                            const FVector& Location,
	                            const FVector& Scale,
	                            const FRTSFireEffectParams& EffectParams);

	/**
	 * @brief Activate pooled fire attached to an actor for a limited duration.
	 * @param AttachActor Actor to attach the fire to.
	 * @param FireType Fire pool to use for the effect.
	 * @param LifeTimeSeconds Seconds to keep active; <= 0 keeps it active until manually reclaimed.
	 * @param AttachOffset Offset from the actor root while attached.
	 * @param Scale World scale to apply to the fire (does not inherit actor scale).
	 * @param EffectParams Niagara scale and color multipliers for this activation.
	 * @return Handle for the active fire, or INDEX_NONE if activation fails.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	int32 ActivateFireAttached(AActor* AttachActor,
	                          ERTSFireType FireType,
	                          float LifeTimeSeconds,
	                          const FVector& AttachOffset,
	                          const FVector& Scale,
	                          const FRTSFireEffectParams& EffectParams);

	/**
	 * @brief Activate pooled fire on a particular component and optional socket.
	 * @param AttachActor Actor that owns the component and fire lifetime.
	 * @param AttachComponent Component the fire follows.
	 * @param SocketName Socket to follow, or NAME_None for the component origin.
	 * @param RelativeOffset Offset from that socket or component origin.
	 * @param FireType Fire pool to use.
	 * @param LifeTimeSeconds Seconds before recycling the fire.
	 * @param Scale Component world scale for the fire.
	 * @param EffectParams Niagara scale and color multipliers.
	 * @return Handle for the active fire, or INDEX_NONE if activation fails.
	 */
	int32 ActivateFireAttachedToComponent(AActor* AttachActor,
	                                    USceneComponent* AttachComponent,
	                                    FName SocketName,
	                                    const FVector& RelativeOffset,
	                                    ERTSFireType FireType,
	                                    float LifeTimeSeconds,
	                                    const FVector& Scale,
	                                    const FRTSFireEffectParams& EffectParams);

	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	bool StopFireByHandle(int32 FireHandle);

	/**
	 * @brief Start (or refresh) a burning ground area that damages the other side every manager tick.
	 * Overlapping requests of the same player merge into one area; at the cap the area closest to burning out is
	 * replaced. The flames are one pooled GroundFire Niagara per area, stopped when the area burns out.
	 * @param Settings Radius, duration, damage per second and armor pen of the burn; ignored when not enabled.
	 * @param GroundLocation Centre of the burning area on the ground.
	 * @param OwningPlayer Player that caused the fire; it damages the units of the other side.
	 * @return Which area burns for this request and whether it merged or replaced one.
	 */
	FRTSGroundFireRequestResult RequestGroundFire(const FRTSGroundFireSettings& Settings,
	                                              const FVector& GroundLocation,
	                                              int32 OwningPlayer);

	/** @return Number of ground fire areas currently burning. */
	int32 GetActiveGroundFireCount() const { return M_GroundFireAreas.Num(); }

	/** @return Number of pooled fire effects of this type that are currently active. */
	int32 GetActiveFireCount(ERTSFireType FireType) const;

	/** @return Number of pooled fire effects of this type that are free for reuse. */
	int32 GetFreeFireCount(ERTSFireType FireType) const;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FRTSGroundFireTestAccess;
#endif

	// Burning ground areas; at most M_MaxActiveGroundFires, damaged from Tick (no timer or tick per fire).
	TArray<FRTSGroundFireArea> M_GroundFireAreas;
	int32 M_NextGroundFireAreaHandle = 1;
	int32 M_MaxActiveGroundFires = 8;
	float M_GroundFireMergeDistanceFraction = 0.75f;
	float M_GroundFireVfxBaseRadiusCm = 100.f;
	bool bM_HasReportedMissingGroundFirePool = false;

	FRTSGroundFireRequestResult RequestGroundFireAtTime(const FRTSGroundFireSettings& Settings,
	                                                    const FVector& GroundLocation,
	                                                    int32 OwningPlayer,
	                                                    float CurrentTimeSeconds);
	/** @brief Deal the burn damage owed since the last tick and put out the areas that burnt out. */
	void TickGroundFires(float CurrentTimeSeconds);
	int32 FindMergeableGroundFireIndex(const FRTSGroundFireSettings& Settings,
	                                   const FVector& GroundLocation,
	                                   int32 OwningPlayer) const;
	int32 FindGroundFireClosestToBurningOut() const;
	void MergeIntoGroundFire(FRTSGroundFireArea& Area, const FRTSGroundFireSettings& Settings,
	                         float CurrentTimeSeconds) const;
	int32 StartGroundFireVfx(const FRTSGroundFireSettings& Settings, const FVector& GroundLocation);
	void PutOutGroundFireAt(int32 AreaIndex);
	void DealGroundFireDamage(const FRTSGroundFireArea& Area, float BurnSeconds);
	void InitGroundFireSettings(const URTSFirePoolSettings* Settings);

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
	                             const FVector& Scale,
	                             const FRTSFireEffectParams& EffectParams) const;
	bool ActivateEntryAttached(FRTSFirePoolEntry& Entry,
	                           AActor* AttachActor,
	                           float LifeTimeSeconds,
	                           const FVector& AttachOffset,
	                           const FVector& Scale,
	                           const FRTSFireEffectParams& EffectParams) const;
	/**
	 * @brief Position a reused component against the requested hull attachment before activation.
	 * @param Entry Pool entry being activated.
	 * @param AttachActor Actor whose lifetime owns this fire.
	 * @param AttachComponent Component the fire follows.
	 * @param SocketName Optional component socket.
	 * @param RelativeOffset Offset from the socket or component origin.
	 * @param LifeTimeSeconds Seconds before recycling the fire.
	 * @param Scale Component world scale.
	 * @param EffectParams Niagara scale and color multipliers.
	 * @return True when the component was activated.
	 */
	bool ActivateEntryAttachedToComponent(FRTSFirePoolEntry& Entry,
	                                    AActor* AttachActor,
	                                    USceneComponent* AttachComponent,
	                                    FName SocketName,
	                                    const FVector& RelativeOffset,
	                                    float LifeTimeSeconds,
	                                    const FVector& Scale,
	                                    const FRTSFireEffectParams& EffectParams) const;
	/**
	 * @brief Apply per-activation values before the pooled Niagara component restarts.
	 * @param NiagaraComponent Component being activated.
	 * @param EffectParams User parameter values for this fire.
	 */
	void SetFireEffectParameters(UNiagaraComponent* NiagaraComponent,
	                             const FRTSFireEffectParams& EffectParams) const;
	void StartLifeTimeDelegateIfNeeded(ERTSFireType FireType, int32 EntryIndex, float LifeTimeSeconds);
	void HandleLifeTimeExpired(ERTSFireType FireType, int32 EntryIndex, int32 ExpectedFireHandle);
	void ReleaseEntryToPool(FRTSFirePool& Pool, int32 EntryIndex);
	void RestoreFailedEntryToPool(FRTSFirePool& Pool, int32 EntryIndex);
	int32 AcquireEntryIndex(FRTSFirePool& Pool);
	int32 AcquireNextFireHandle();
	int32 FindOldestActiveEntryIndex(const FRTSFirePool& Pool) const;
	bool ShouldEntryBeDormant(const FRTSFirePoolEntry& Entry, float CurrentTimeSeconds) const;
	bool GetIsValidWorld() const;
};
