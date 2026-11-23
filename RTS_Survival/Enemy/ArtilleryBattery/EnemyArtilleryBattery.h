// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"

#include "EnemyArtilleryBattery.generated.h"

class AEnemyArtilleryBattery;
class AConvoy;
class ACPPTurretsMaster;
class ATankMaster;
class UGameUnitManager;
class UWeaponState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyArtilleryBatteryEvent, AEnemyArtilleryBattery*, Battery);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyArtilleryBatterySupplyAdded, AEnemyArtilleryBattery*, Battery,
                                               float, SecondsAdded, float, SuppliedSecondsLeft);

/** Which weapon pits make up the battery. */
USTRUCT(BlueprintType)
struct FEnemyArtilleryBatteryPitSettings
{
	GENERATED_BODY()

	// Placed pits (building expansions, team weapons or any actor with turrets attached) that belong to the battery.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Pits")
	TArray<TObjectPtr<AActor>> PitActors;

	// On: building expansions and team weapons of the battery's player within AutoCollectRadiusCm join as well.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits")
	bool bAutoCollectPits = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits",
		meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bAutoCollectPits", EditConditionHides))
	float AutoCollectRadiusCm = 6000.f;

	// Turrets whose largest gun is smaller than this (mm) keep their normal range and never barrage (20 leaves MGs out).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pits", meta = (ClampMin = "0.0"))
	float MinimumCalibreMm = 0.f;
};

/** How long the battery stays supplied and how far it reaches while it is. */
USTRUCT(BlueprintType)
struct FEnemyArtilleryBatterySupplySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply", meta = (ClampMin = "0.0", Units = "s"))
	float SecondsPerAmmoTruck = 180.f;

	// Supplied time stacks up to this many seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply", meta = (ClampMin = "1.0", Units = "s"))
	float MaxSuppliedSeconds = 540.f;

	// While supplied every weapon of the battery reaches at least this far; the barrage looks for targets this far
	// from the battery actor, so place the actor in the middle of the pits.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply", meta = (ClampMin = "0.0", Units = "cm"))
	float SuppliedRangeCm = 40000.f;
};

/** What the battery shoots at while supplied. */
USTRUCT(BlueprintType)
struct FEnemyArtilleryBatteryBarrageSettings
{
	GENERATED_BODY()

	// Every interval each pit gets a new aim point; between two intervals it keeps firing at its point.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrage", meta = (ClampMin = "1.0", Units = "s"))
	float BarrageIntervalSeconds = 10.f;

	// Aim points are scattered up to this far around the picked target.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrage", meta = (ClampMin = "0.0", Units = "cm"))
	float AimSpreadRadiusCm = 900.f;

	// Chance per pit to shell a building (deployed base vehicle or building expansion) instead of a unit, when both
	// are in range.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseTargetPreference = 0.75f;

	// A pit with a hostile this close fights it with its own targeting instead of joining the barrage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrage", meta = (ClampMin = "0.0", Units = "cm"))
	float SelfDefenceRadiusCm = 5000.f;

	// On: only targets the battery's player can see are shelled. Off: the battery knows where everything is.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrage")
	bool bOnlyVisibleTargets = false;
};

/** Ammo trucks of a running convoy that reach the battery supply it without a mission call. */
USTRUCT(BlueprintType)
struct FEnemyArtilleryBatteryTruckDetection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo Trucks")
	bool bAutoDetectAmmoTrucks = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo Trucks",
		meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bAutoDetectAmmoTrucks", EditConditionHides))
	float ArrivalRadiusCm = 2500.f;

	// On: only trucks of a convoy whose cargo is Ammo count. Off: any convoy truck that arrives counts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo Trucks",
		meta = (EditCondition = "bAutoDetectAmmoTrucks", EditConditionHides))
	bool bRequireAmmoCargo = true;
};

/** One weapon whose range the battery raised; the exact amount is taken off again when the supply ends. */
USTRUCT()
struct FEnemyArtilleryBatteryRangeBoost
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UWeaponState> Weapon = nullptr;

	UPROPERTY()
	TWeakObjectPtr<ACPPTurretsMaster> Turret = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AActor> Pit = nullptr;

	float AddedRangeCm = 0.f;
};

/** A turret the battery ordered to fire at the ground; it goes back to its own targeting afterwards. */
USTRUCT()
struct FEnemyArtilleryBatteryBarrageTurret
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ACPPTurretsMaster> Turret = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AActor> Pit = nullptr;
};

/** A turret of a pit that can fight for the battery right now. */
struct FEnemyArtilleryBatteryActiveTurret
{
	ACPPTurretsMaster* Turret = nullptr;
	AActor* Pit = nullptr;
};

/** Hostile locations in reach of the supplied battery, split so the barrage can prefer the base. */
struct FEnemyArtilleryBatteryTargets
{
	TArray<FVector> BaseLocations;
	TArray<FVector> UnitLocations;

	bool GetIsEmpty() const { return BaseLocations.IsEmpty() && UnitLocations.IsEmpty(); }
};

/**
 * @brief Placed in a mission map next to existing weapon pits to make them an artillery battery that ammo trucks
 * supply. Each truck (NotifyAmmoTruckArrived, or auto-detected convoy ammo trucks) or AddSuppliedSeconds adds timed
 * supply; while supplied the pits reach SuppliedRangeCm and barrage hostile buildings and units in that range.
 * Missions find it with FindEnemyArtilleryBattery(BatteryId) and listen to OnSupplied / OnSupplyEnded / OnBatteryDestroyed.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AEnemyArtilleryBattery : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AEnemyArtilleryBattery();

	/**
	 * @brief Supplies the battery for more seconds (escaped ammo cargo, scripted events); stacks up to MaxSuppliedSeconds.
	 * @param Seconds Seconds to add; zero or less does nothing.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Artillery Battery")
	void AddSuppliedSeconds(const float Seconds);

	/**
	 * @brief An ammo truck reached the battery: adds SecondsPerAmmoTruck once per truck (auto-detection and route
	 * events may both report the same truck).
	 * @param Truck The arrived truck; nullptr adds the time without the once-per-truck guard.
	 * @return True when the time was added, false for a truck that already counted or a destroyed battery.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Artillery Battery")
	bool NotifyAmmoTruckArrived(AActor* Truck);

	/** @brief Adds a pit placed or spawned after the battery started (e.g. a rebuilt pit). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Artillery Battery")
	void AddPitActor(AActor* PitActor);

	/** @brief Ends the supplied state now: normal ranges are restored and the barrage stops. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Artillery Battery")
	void EndSupplyNow();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Artillery Battery")
	bool GetIsSupplied() const { return bM_IsSupplied; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Artillery Battery")
	float GetSuppliedSecondsLeft() const;

	// True once every pit is dead or captured.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Artillery Battery")
	bool GetIsBatteryDestroyed() const { return bM_IsDestroyed; }

	// Pits that are alive and still the battery's (an abandoned team weapon counts: it can be crewed again).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Artillery Battery")
	int32 GetRemainingPitCount() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Artillery Battery")
	FName GetBatteryId() const { return BatteryId; }

	/** @return The placed battery with this BatteryId, or null (reports an error) when there is none. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Artillery Battery",
		meta = (WorldContext = "WorldContextObject"))
	static AEnemyArtilleryBattery* FindEnemyArtilleryBattery(const UObject* WorldContextObject,
	                                                         const FName BatteryIdToFind);

	// The battery went from unsupplied to supplied.
	UPROPERTY(BlueprintAssignable, Category = "Artillery Battery")
	FOnEnemyArtilleryBatteryEvent OnSupplied;

	// Supplied time was added (also on the first supply); SecondsAdded is after the cap.
	UPROPERTY(BlueprintAssignable, Category = "Artillery Battery")
	FOnEnemyArtilleryBatterySupplyAdded OnSuppliedTimeAdded;

	// The supplied time ran out (or EndSupplyNow, or the battery was destroyed while supplied).
	UPROPERTY(BlueprintAssignable, Category = "Artillery Battery")
	FOnEnemyArtilleryBatteryEvent OnSupplyEnded;

	// Every pit is dead or captured.
	UPROPERTY(BlueprintAssignable, Category = "Artillery Battery")
	FOnEnemyArtilleryBatteryEvent OnBatteryDestroyed;

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FString GetPlacementKindName() const override { return TEXT("BATTERY"); }
	virtual FName GetPlacementId() const override { return BatteryId; }
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual bool GetNeedsEnemyController() const override { return false; }

	// Name missions use to find this battery (FindEnemyArtilleryBattery); also shown above it in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artillery Battery")
	FName BatteryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artillery Battery|Pits")
	FEnemyArtilleryBatteryPitSettings Pits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artillery Battery|Supply")
	FEnemyArtilleryBatterySupplySettings Supply;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artillery Battery|Barrage")
	FEnemyArtilleryBatteryBarrageSettings Barrage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artillery Battery|Ammo Trucks")
	FEnemyArtilleryBatteryTruckDetection TruckDetection;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_Pits;

	// Weapons that carry the supplied range; each entry is taken off exactly once.
	UPROPERTY()
	TArray<FEnemyArtilleryBatteryRangeBoost> M_RangeBoosts;

	UPROPERTY()
	TArray<FEnemyArtilleryBatteryBarrageTurret> M_BarrageTurrets;

	// Trucks that already supplied the battery, so a truck that waits at the battery counts once.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_CreditedTrucks;

	FTimerHandle M_StartTimerHandle;
	FTimerHandle M_UpdateTimerHandle;

	// World time (seconds) at which the supply runs out; only meaningful while bM_IsSupplied.
	double M_SuppliedUntilSeconds = 0.0;
	double M_NextBarrageSeconds = 0.0;

	bool bM_IsSupplied = false;
	bool bM_IsDestroyed = false;
	bool bM_HasStarted = false;

	// ----- Lifecycle -----
	void BeginPlay_ScheduleStart();
	void StartBattery();
	void UpdateBattery();
	void StopBatteryTimers();
	double GetWorldSeconds() const;

	// ----- Pits -----
	void CollectPits();
	void CollectPits_AutoCollect();
	void TryAddPit(AActor* PitActor);
	bool GetIsPitOfBatteryPlayer(AActor* PitActor) const;
	bool GetIsPitLost(AActor* PitActor) const;
	bool GetIsPitActive(AActor* PitActor) const;
	void GatherTurretsOfPit(AActor* PitActor, TArray<ACPPTurretsMaster*>& OutTurrets) const;
	bool GetIsTurretBarrageCapable(const ACPPTurretsMaster* Turret) const;
	void GatherActiveTurrets(TArray<FEnemyArtilleryBatteryActiveTurret>& OutTurrets) const;
	void AddBarrageCapableTurretsOfPit(AActor* PitActor, TArray<FEnemyArtilleryBatteryActiveTurret>& OutTurrets) const;
	bool CheckBatteryDestroyed();
	void HandleBatteryDestroyed();

	// ----- Supply -----
	void StartSupply();
	void EndSupply();

	// ----- Range -----
	void RefreshRangeBoosts();
	void ApplyRangeBoostToTurret(ACPPTurretsMaster* Turret, AActor* PitActor);
	bool GetHasRangeBoost(const UWeaponState* Weapon) const;
	void RemoveRangeBoostsOfInactiveTurrets(const TArray<FEnemyArtilleryBatteryActiveTurret>& ActiveTurrets);
	void RemoveRangeBoostAt(const int32 BoostIndex);
	void RemoveAllRangeBoosts();

	// ----- Barrage -----
	void FireBarrage();
	void GatherBarrageTargets(FEnemyArtilleryBatteryTargets& OutTargets) const;
	void GatherBarrageTargetsOfPlayer(const UGameUnitManager& GameUnitManager, const uint8 Player,
	                                  FEnemyArtilleryBatteryTargets& OutTargets) const;
	void TryAddBarrageTarget(AActor* Target, const int32 TargetOwner, const bool bIsBaseTarget,
	                         FEnemyArtilleryBatteryTargets& OutTargets) const;
	bool GetHasTargetNear(const FVector& Location, const FEnemyArtilleryBatteryTargets& Targets,
	                      const float Radius) const;

	/**
	 * @brief Picks a scattered aim point the turret can reach, preferring the base per BaseTargetPreference; the
	 * shared pick index makes the pits spread over different targets.
	 * @param Turret The turret that will fire.
	 * @param Targets Candidate locations in range of the battery.
	 * @param InOutPickIndex Advances with every pick.
	 * @param OutAimPoint Receives the aim point.
	 * @return False when no candidate is in this turret's range and arc.
	 */
	bool TryPickAimPoint(ACPPTurretsMaster& Turret, const FEnemyArtilleryBatteryTargets& Targets,
	                     int32& InOutPickIndex, FVector& OutAimPoint) const;

	bool TryPickAimPointFrom(ACPPTurretsMaster& Turret, const TArray<FVector>& Candidates, int32& InOutPickIndex,
	                         FVector& OutAimPoint) const;
	bool GetCanTurretReach(ACPPTurretsMaster& Turret, const FVector& AimPoint) const;
	FVector GetScatteredAimPoint(const FVector& TargetLocation) const;
	void SetTurretOnBarrage(ACPPTurretsMaster* Turret, AActor* PitActor, const FVector& AimPoint);
	void ReturnTurretToOwnTargeting(ACPPTurretsMaster* Turret);
	void ReturnAllBarrageTurretsToOwnTargeting();

	// ----- Ammo trucks -----
	void DetectArrivingAmmoTrucks();
	bool GetIsArrivingAmmoTruck(ATankMaster* Tank) const;
	bool GetIsAmmoCargoTruck(const AConvoy& Convoy, const ATankMaster* Tank) const;
	bool GetIsTruckCredited(const AActor* Truck) const;
};
