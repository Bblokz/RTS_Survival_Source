// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Game/GameState/HideGameUI/RTSUIElement.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "SurrenderingTruckSubsystem.generated.h"

class AActor;
class AScavengeableObject;
class ASurrenderingCargoTruck;
class ASurrenderingCaseTruck;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurrenderingTruckWorldEvent, ASurrenderingCargoTruck*, Truck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCaseTruckWorldEvent, ASurrenderingCaseTruck*, CaseTruck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSealedCacheWorldEvent, AActor*, SealedCache);

/** A sealed cache: a scavengeable object that never expires and holds the Berlin cases. */
USTRUCT(BlueprintType)
struct FSealedCacheSettings
{
	GENERATED_BODY()

	// The scavengeable cache actor (a Blueprint child of AScavengeableObject, like the convoy cargo cache).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealed Cache")
	TSubclassOf<AScavengeableObject> SealedCacheClass = nullptr;

	// Seconds a squad needs to scavenge it (slower than a cargo cache); 0 keeps the Blueprint's time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealed Cache", meta = (ClampMin = "0"))
	float ScavengeSeconds = 30.f;

	// On: Rewards replace what the Blueprint pays on a finished scavenge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealed Cache")
	bool bOverrideRewards = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealed Cache", meta = (EditCondition = "bOverrideRewards"))
	TMap<ERTSResourceType, int32> Rewards;

	// Shown on the minimap while the cache waits (None = no icon).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealed Cache")
	EMinimapIconType MinimapIcon = EMinimapIconType::Objective;

	// A cache shot to pieces before anyone scavenged it comes back at the same spot (the cases cannot be lost).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealed Cache")
	bool bRespawnWhenDestroyed = true;
};

/** Runtime record of one sealed cache, kept until it is scavenged. */
USTRUCT()
struct FSealedCacheRecord
{
	GENERATED_BODY()

	int32 CacheId = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<AScavengeableObject> Cache;

	UPROPERTY()
	FSealedCacheSettings Settings;

	FTransform SpawnTransform;
	FName IconId = NAME_None;
	FTimerHandle SpawnTimerHandle;
	bool bScavengeCompleted = false;
	bool bResolved = false;
};

/**
 * @brief World-level hub of the surrendering convoy trucks: what outlives a single truck. Bind its events once in
 * the mission Blueprint (Get SurrenderingTruckSubsystem) to follow the case truck and its sealed caches, which spawn
 * after the truck is gone. Also keeps the once-per-world surrender line and the "hide all game UI" state.
 */
UCLASS()
class RTS_SURVIVAL_API USurrenderingTruckSubsystem : public UWorldSubsystem, public IRTSUIElement
{
	GENERATED_BODY()

public:
	static USurrenderingTruckSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	// IRTSUIElement: the "hide all game UI" toggle stores no state of its own; trucks read it here.
	virtual void OnHideAllGameUI(const bool bHide) override;

	UFUNCTION(BlueprintPure, Category = "Surrendering Truck")
	bool GetIsGameUIHidden() const { return bM_IsGameUIHidden; }

	/** @return True only for the first call in this world: that truck plays the surrender line. */
	bool TryClaimFirstSurrender();

	UFUNCTION(BlueprintPure, Category = "Surrendering Truck")
	bool GetHasAnyTruckSurrendered() const { return bM_HasAnyTruckSurrendered; }

	void BroadcastFirstTruckSurrendered(ASurrenderingCargoTruck* Truck);

	void RegisterCaseTruck(ASurrenderingCaseTruck* CaseTruck);
	void UnregisterCaseTruck(const ASurrenderingCaseTruck* CaseTruck);
	void NotifyCasesDelivered(ASurrenderingCaseTruck* CaseTruck);
	void NotifyCasesEscaped(ASurrenderingCaseTruck* CaseTruck, const FSealedCacheSettings& SealedCacheSettings);

	/** @return The first case truck in play that still carries its cases; nullptr when none. */
	UFUNCTION(BlueprintPure, Category = "Surrendering Truck|Cases")
	ASurrenderingCaseTruck* GetActiveCaseTruck() const;

	/** @return True once cases escaped with their convoy (spawn them with SpawnEscapedCasesSealedCache). */
	UFUNCTION(BlueprintPure, Category = "Surrendering Truck|Cases")
	bool GetHaveCasesEscaped() const { return bM_HaveCasesEscaped; }

	/** @return Sealed caches spawned or about to spawn that nobody has scavenged yet. */
	UFUNCTION(BlueprintPure, Category = "Surrendering Truck|Cases")
	int32 GetOpenSealedCacheCount() const;

	/**
	 * @brief Mission scripting (Base B falling when the cases escaped): a sealed cache at any spot; it reports
	 * OnSealedCacheSpawned / OnSealedCacheScavenged like one dropped by the case truck.
	 * @param SealedCacheClass Scavengeable cache Blueprint.
	 * @param SpawnTransform Where the cache stands.
	 * @param ScavengeSeconds Seconds to scavenge; 0 keeps the Blueprint's time.
	 * @return The cache, or nullptr when the class is missing or the spawn failed (reported).
	 */
	UFUNCTION(BlueprintCallable, Category = "Surrendering Truck|Cases")
	AActor* SpawnSealedCache(TSubclassOf<AScavengeableObject> SealedCacheClass, const FTransform& SpawnTransform,
	                         float ScavengeSeconds = 30.f);

	/**
	 * @brief Mission scripting: the sealed cache of the cases that escaped, with the escaped case truck's settings.
	 * @param SpawnTransform Where the cache stands (for example the Base B command post).
	 * @return The cache, or nullptr when no cases escaped or the spawn failed (reported).
	 */
	UFUNCTION(BlueprintCallable, Category = "Surrendering Truck|Cases")
	AActor* SpawnEscapedCasesSealedCache(const FTransform& SpawnTransform);

	/**
	 * @brief Spawns a sealed cache now or after a delay; the record lives here, so the source may be gone by then.
	 * @param Settings Class, scavenge time, rewards, icon, respawn rule.
	 * @param SpawnTransform Where the cache stands.
	 * @param DelaySeconds 0 spawns at once.
	 * @return The cache when it spawned at once; nullptr when delayed or failed.
	 */
	AScavengeableObject* SpawnSealedCacheWithSettings(const FSealedCacheSettings& Settings,
	                                                  const FTransform& SpawnTransform, float DelaySeconds);

	// The first truck in this world surrendered (the surrender line plays once, on that truck).
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck")
	FOnSurrenderingTruckWorldEvent OnFirstTruckSurrendered;

	// A case truck began play (convoys spawn their trucks at runtime).
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck|Cases")
	FOnCaseTruckWorldEvent OnCaseTruckSpawned;

	// A captured case truck reached its drop-off: the cases are recovered.
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck|Cases")
	FOnCaseTruckWorldEvent OnCasesDelivered;

	// The case truck left the map with its convoy (the truck is being removed while this fires).
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck|Cases")
	FOnCaseTruckWorldEvent OnCasesEscaped;

	// A sealed cache stands in the world (after the case truck was destroyed, or from SpawnSealedCache). A cache that
	// respawns after being shot to pieces fires this again.
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck|Cases")
	FOnSealedCacheWorldEvent OnSealedCacheSpawned;

	// A squad finished scavenging a sealed cache: the cases are recovered.
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck|Cases")
	FOnSealedCacheWorldEvent OnSealedCacheScavenged;

	// A sealed cache was destroyed unscavenged and does not respawn (bRespawnWhenDestroyed off).
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck|Cases")
	FOnSealedCacheWorldEvent OnSealedCacheDestroyed;

private:
	FSealedCacheRecord* FindSealedCache(int32 CacheId);
	void SpawnSealedCacheOfRecord(int32 CacheId);
	void BindSealedCache(AScavengeableObject& Cache, int32 CacheId);
	void OnSealedCacheScavengeCompleted(int32 CacheId);
	void OnSealedCacheScavengedOrDestroyed(int32 CacheId);
	void ResolveSealedCache(int32 CacheId);
	void RemoveSealedCacheIcon(FSealedCacheRecord& Record) const;

	bool bM_IsGameUIHidden = false;
	bool bM_HasAnyTruckSurrendered = false;
	bool bM_HaveCasesEscaped = false;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASurrenderingCaseTruck>> M_CaseTrucks;

	// Settings of the case truck whose cases escaped; SpawnEscapedCasesSealedCache uses them.
	UPROPERTY()
	FSealedCacheSettings M_EscapedCasesCacheSettings;

	UPROPERTY()
	TArray<FSealedCacheRecord> M_SealedCaches;

	int32 M_NextSealedCacheId = 0;
};
