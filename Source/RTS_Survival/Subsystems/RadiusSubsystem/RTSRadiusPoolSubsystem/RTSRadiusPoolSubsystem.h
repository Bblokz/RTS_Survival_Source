#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSRadiusPoolSubsystem.generated.h"

class APooledRadiusActor;
class UMaterialInterface;
class URadiusPoolSettings;
class UStaticMesh;
class AActor;

/**
 * @brief World subsystem that owns a pool of APooledRadiusActor instances.
 * Loads settings on init, spawns the pool, and serves Create/Hide/Attach API for gameplay code.
 */
UCLASS()
class RTS_SURVIVAL_API URTSRadiusPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * @brief Show a radius ring.
	 * @param Location World-space center for the ring.
	 * @param Radius   Radius in units.
	 * @param Type     Which logical type (drives the material).
	 * @param LifeTime If > 0, auto-hide after this many seconds.
	 * @return Unique runtime id for later HideRTSRadiusById; negative on error.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|RadiusPool")
	int32 CreateRTSRadius(FVector Location, float Radius, ERTSRadiusType Type, float LifeTime = 0.0f);

	/**
	 * @brief Hide and return the pooled actor with a given id to the pool.
	 * @param ID The id returned by CreateRTSRadius.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|RadiusPool")
	void HideRTSRadiusById(int32 ID);

	/**
	 * @brief Attach an active pooled radius actor to another actor with a relative offset.
	 * @param ID           The id returned by CreateRTSRadius.
	 * @param TargetActor  Actor to attach to.
	 * @param RelativeOffset Offset applied as relative location while attached.
	 *
	 * @note The pooled actor is detached automatically when hidden or reclaimed.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|RadiusPool")
	void AttachRTSRadiusToActor(int32 ID, AActor* TargetActor, FVector RelativeOffset);

private:
	// -------- Settings cache (loaded once at Initialize) --------
	UPROPERTY()
	TObjectPtr<UStaticMesh> M_RadiusMesh = nullptr;

	float M_DefaultStartingRadius = 0.0f;
	float M_UnitsPerScale = 1.0f;
	float M_ZScale = 1.0f;
	float M_RenderHeight = 0.0f;
	int32 M_DefaultPoolSize = 0;

	// Materials resolved from settings.
	UPROPERTY()
	TMap<ERTSRadiusType, TObjectPtr<UMaterialInterface>> M_TypeToMaterial;

	// -------- Pool state --------
	UPROPERTY()
	TArray<TObjectPtr<APooledRadiusActor>> M_Pool;

	// Active ids mapped to actors.
	UPROPERTY()
	TMap<int32, TWeakObjectPtr<APooledRadiusActor>> M_IdToActor;

	// Lifetime timers per active id.
	TMap<int32, FTimerHandle> M_LifetimeTimers;

	// Monotonic id counter.
	int32 M_NextId = 1;

private:
	// --- Initialize helpers ---
	void Initialize_LoadSettings();
	void Initialize_LoadMaterials(const URadiusPoolSettings* Settings);
	void Initialize_SpawnPool();

	// --- Acquire/Release ---
	APooledRadiusActor* AcquireFreeOrLRU();
	void ReleaseById_Internal(int32 Id, bool bSilentIfMissing);

	// --- Lookup ---
	APooledRadiusActor* FindPooledActorById(int32 Id) const;

	// --- Validators ---
	bool GetIsValidWorld() const;
	bool GetIsValidMesh() const;

	// --- Lifetime callback ---
	UFUNCTION()
	void OnLifetimeExpired(int32 ExpiredId);
};
