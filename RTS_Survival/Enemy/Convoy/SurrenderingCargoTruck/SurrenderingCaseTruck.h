// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/Convoy/SurrenderingCargoTruck/SurrenderingCargoTruck.h"
#include "RTS_Survival/Enemy/Convoy/SurrenderingCargoTruck/SurrenderingTruckSubsystem.h"

#include "SurrenderingCaseTruck.generated.h"

class AScavengeableObject;
class ASurrenderingCaseTruck;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCaseTruckCasesEvent, ASurrenderingCaseTruck*, CaseTruck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCaseTruckCasesDropped, ASurrenderingCaseTruck*, CaseTruck,
                                             const FTransform&, SealedCacheTransform);

/** Where the cases of a case truck are. */
UENUM(BlueprintType)
enum class ECaseTruckCasesState : uint8
{
	// On the truck: in the convoy, in its scuttle countdown, or captured and driving to the drop-off.
	OnTruck,
	// The captured truck reached its drop-off: the cases are recovered.
	Delivered,
	// The truck left the map with its convoy.
	Escaped,
	// The truck is gone (destroyed, scuttled, removed); the cases wait in a sealed cache at the wreck.
	InSealedCache
};

USTRUCT(BlueprintType)
struct FSurrenderingCaseTruckSettings
{
	GENERATED_BODY()

	// Multiplies every hit the truck takes after armour decided the penetration: an armoured truck that is very hard
	// to destroy, so capturing it is the intended way. Tune health and armour in the Blueprint as well.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case Truck", meta = (ClampMin = "0", ClampMax = "1"))
	float DamageTakenMultiplier = 0.25f;

	// What the cases become when the truck is destroyed (also by its scuttle, or after it was captured).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case Truck")
	FSealedCacheSettings SealedCache;

	// Delay after the death so the wreck spawns first; the sealed cache lands beside it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case Truck", meta = (ClampMin = "0"))
	float SealedCacheSpawnDelaySeconds = 1.5f;

	// Distance (cm) of the sealed cache to the right of the truck's last spot (0 = on the spot).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case Truck", meta = (ClampMin = "0"))
	float SealedCacheSideOffset = 650.f;
};

/**
 * @brief The armoured case truck of the Ore Mountains case convoy: point one FConvoyTruckSpec::TruckClass at a
 * Blueprint child. It surrenders and pays like any surrendering cargo truck, but also carries the Berlin cases: they
 * are recovered when the captured truck reaches its drop-off, survive in a sealed cache (no expiry) when the truck is
 * destroyed, or leave with the convoy. The convoy drops no normal cargo cache for it. Follow it from the mission
 * through USurrenderingTruckSubsystem (the truck is gone before most of its outcomes resolve).
 * @note SealedCache.SealedCacheClass: set in the Blueprint child, or a destroyed truck loses the cases (reported).
 * @note InitTrackedTank: call in the Blueprint child, as for any tracked tank master.
 * @note SetupTurret: call in the Blueprint child for each turret, as for any tracked tank master.
 */
UCLASS()
class RTS_SURVIVAL_API ASurrenderingCaseTruck : public ASurrenderingCargoTruck
{
	GENERATED_BODY()

public:
	ASurrenderingCaseTruck(const FObjectInitializer& ObjectInitializer);

	/** @return The case truck in play that still carries its cases; nullptr when none (yet). */
	UFUNCTION(BlueprintPure, Category = "Case Truck", meta = (WorldContext = "WorldContextObject"))
	static ASurrenderingCaseTruck* GetActiveCaseTruck(const UObject* WorldContextObject);

	/**
	 * @brief Mission scripting (Base B falling when the cases escaped): a sealed cache at any spot that reports
	 * through USurrenderingTruckSubsystem like one dropped by the truck.
	 * @param WorldContextObject Any object in the world.
	 * @param SealedCacheClass Scavengeable cache Blueprint.
	 * @param SpawnTransform Where the cache stands.
	 * @param ScavengeSeconds Seconds to scavenge; 0 keeps the Blueprint's time.
	 * @return The cache, or nullptr when it could not spawn (reported).
	 */
	UFUNCTION(BlueprintCallable, Category = "Case Truck", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnSealedCacheAt(const UObject* WorldContextObject,
	                                  TSubclassOf<AScavengeableObject> SealedCacheClass,
	                                  const FTransform& SpawnTransform, float ScavengeSeconds = 30.f);

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Case Truck")
	ECaseTruckCasesState GetCasesState() const { return M_CasesState; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Case Truck")
	bool GetIsCarryingCases() const { return M_CasesState == ECaseTruckCasesState::OnTruck; }

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	// The sealed cache replaces the convoy's cargo cache.
	virtual bool GetDropsConvoyCargoCache() const override { return false; }

	// The captured truck reached its drop-off with the cases (fired before the truck is removed).
	UPROPERTY(BlueprintAssignable, Category = "Case Truck")
	FOnCaseTruckCasesEvent OnCasesDelivered;

	// The truck leaves the map with its convoy (fired while it is being removed).
	UPROPERTY(BlueprintAssignable, Category = "Case Truck")
	FOnCaseTruckCasesEvent OnCasesEscaped;

	// The truck is gone without delivering: the sealed cache spawns at SealedCacheTransform shortly after
	// (USurrenderingTruckSubsystem::OnSealedCacheSpawned, then OnSealedCacheScavenged).
	UPROPERTY(BlueprintAssignable, Category = "Case Truck")
	FOnCaseTruckCasesDropped OnCasesDropped;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnitDies(const ERTSDeathType DeathType) override;
	virtual void HandleCapturedCargoDelivered() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Case Truck")
	void BP_OnCasesDelivered();

	UFUNCTION(BlueprintImplementableEvent, Category = "Case Truck")
	void BP_OnCasesEscaped();

	UFUNCTION(BlueprintImplementableEvent, Category = "Case Truck")
	void BP_OnCasesDropped(const FTransform& SealedCacheTransform);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case Truck")
	FSurrenderingCaseTruckSettings CaseTruckSettings;

private:
	/** @return True when the truck is being removed because its convoy escaped with it. */
	bool GetIsLeavingWithEscapedConvoy() const;
	FTransform GetSealedCacheTransform() const;
	void MarkCasesEscaped();
	void DropCasesInSealedCache(const FTransform& SealedCacheTransform);

	ECaseTruckCasesState M_CasesState = ECaseTruckCasesState::OnTruck;
};
