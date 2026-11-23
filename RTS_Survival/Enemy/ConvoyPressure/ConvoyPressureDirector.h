// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConvoyPressureTypes.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyBehaviourTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"

#include "ConvoyPressureDirector.generated.h"

class AConvoy;
class AEnemyConvoyRoute;
class AEnemyController;
class ADormantDefenceSlot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConvoyPressureAlertTierChanged, int32, NewTier, int32, OldTier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnConvoyPressureEscapedCargo, EConvoyCargoType, Cargo,
                                               int32, Trucks, int32, ConvoyIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConvoyPressureEscapedAmmoSeconds, float, Seconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConvoyPressureDormantDefenceActivated, AActor*, Defence);

/** Result of one try to start a tier's targeted attack. */
enum class EConvoyPressureAttackLaunch : uint8
{
	Launched,
	// No target is alive and hostile yet; tried again later.
	NoTargetYet,
	// Bad setup or no enemy controller (reported); not tried again.
	Failed
};

/**
 * @brief Placed once in a mission map to turn the convoy results of the watched routes into enemy pressure.
 * Every stopped convoy raises the alert (tiers at StoppedConvoysPerTier); each tier starts its placement actors and
 * targeted attack once. Every escaped convoy applies the response of its cargo type (placement actors, dormant
 * defence slots, ammo seconds through OnEscapedAmmoSeconds). The mission Blueprint listens to the events.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AConvoyPressureDirector : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AConvoyPressureDirector();

	// 0 = no alert yet; the highest tier is the number of StoppedConvoysPerTier entries.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Pressure")
	int32 GetAlertTier() const { return M_AlertTier; }

	// Convoys of the watched routes that lost every truck (destroyed or captured), plus AddStoppedConvoy calls.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Pressure")
	int32 GetStoppedConvoyCount() const { return M_StoppedConvoyCount; }

	// Convoys that reached the exit with at least one truck, plus ApplyEscapedCargo calls.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Pressure")
	int32 GetEscapedConvoyCount() const { return M_EscapedConvoyCount; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Convoy Pressure")
	int32 GetEscapedTruckCount() const { return M_EscapedTruckCount; }

	/**
	 * @brief For tests and scripted beats. Going up runs every skipped tier's response in order (each only once);
	 * going down only changes the tier, and later stopped convoys raise it again without repeating responses.
	 * @param NewTier Clamped to 0..highest tier.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Pressure")
	void ForceAlertTier(const int32 NewTier);

	/** @brief Counts one stopped convoy as if a watched route reported it (a convoy the mission stopped itself). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Pressure")
	void AddStoppedConvoy();

	/**
	 * @brief Applies an escaped convoy as if a watched route reported it (tests, or cargo that arrives by script).
	 * @param Cargo Picks the cargo responses.
	 * @param Trucks Escaped trucks (at least 1).
	 * @param ConvoyIndex Passed on to OnEscapedCargo; INDEX_NONE when unknown.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Convoy Pressure")
	void ApplyEscapedCargo(const EConvoyCargoType Cargo, const int32 Trucks, const int32 ConvoyIndex = -1);

	UPROPERTY(BlueprintAssignable, Category = "Convoy Pressure")
	FOnConvoyPressureAlertTierChanged OnAlertTierChanged;

	// Every escaped cargo type of every escaped convoy (also types without a response); ConvoyIndex is the route's
	// convoy index (0 = the first convoy that route started).
	UPROPERTY(BlueprintAssignable, Category = "Convoy Pressure")
	FOnConvoyPressureEscapedCargo OnEscapedCargo;

	// Forward to the artillery battery (AddSuppliedSeconds); only sent when the seconds are above 0.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Pressure")
	FOnConvoyPressureEscapedAmmoSeconds OnEscapedAmmoSeconds;

	// A dormant defence slot spawned its defence; add it to a kill mission's TargetActors here when it should count.
	UPROPERTY(BlueprintAssignable, Category = "Convoy Pressure")
	FOnConvoyPressureDormantDefenceActivated OnDormantDefenceActivated;

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FString GetPlacementKindName() const override { return TEXT("PRESSURE"); }
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual bool GetNeedsEnemyController() const override { return false; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Pressure")
	TArray<TObjectPtr<AEnemyConvoyRoute>> WatchedRoutes;

	// Stopped convoys needed for tier 1, 2, 3...; one entry per tier, ascending.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Pressure|Alert", meta = (ClampMin = "1"))
	TArray<int32> StoppedConvoysPerTier;

	// Index 0 runs on reaching tier 1, index 1 on tier 2...
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Pressure|Alert")
	TArray<FConvoyPressureTierResponse> TierResponses;

	// While a reached tier's targeted attack has no living hostile target, it is tried again this often; 0 = never.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Pressure|Alert", meta = (ClampMin = "0.0", Units = "s"))
	float TargetedAttackRetrySeconds = 20.f;

	// Every entry whose cargo type matches is applied for each escaped convoy.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Pressure|Escaped Cargo")
	TArray<FConvoyPressureCargoResponse> CargoResponses;

	// On: a convoy whose trucks were all destroyed (none captured) also counts as stopped.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy Pressure|Alert")
	bool bCountDestroyedConvoysAsStopped = true;

private:
	// The routes bound on BeginPlay (each once), unbound on EndPlay.
	UPROPERTY()
	TArray<TWeakObjectPtr<AEnemyConvoyRoute>> M_WatchedRoutes;

	// Placement actors the director started; never started twice, whichever tier or cargo list holds them.
	UPROPERTY()
	TArray<TWeakObjectPtr<AEnemyPlacementActor>> M_StartedPlacementActors;

	// Tiers (1-based) whose response ran; a tier reached again after ForceAlertTier went down does not repeat it.
	TSet<int32> M_TiersWithResponseRun;

	// Tier indices (0-based) whose targeted attack waits for a hostile target.
	TArray<int32> M_PendingTargetedAttacks;

	FTimerHandle M_TargetedAttackRetryTimerHandle;

	int32 M_AlertTier = 0;
	// The tier the stepping loop climbs to; raised by stopped convoys and ForceAlertTier while the loop runs.
	int32 M_TargetAlertTier = 0;
	int32 M_StoppedConvoyCount = 0;
	int32 M_EscapedConvoyCount = 0;
	int32 M_EscapedTruckCount = 0;

	// Set while the tier loop runs, so Blueprint handlers that raise the alert again only move the target.
	bool bM_IsSteppingAlertTier = false;

	// ----- BeginPlay -----
	void BeginPlay_BindWatchedRoutes();

	// ----- Route callbacks -----
	UFUNCTION()
	void HandleRouteConvoyCargoOutcome(AEnemyConvoyRoute* Route, AConvoy* Convoy, const FConvoyCargoOutcome& Outcome);

	void UnbindWatchedRoutes();
	bool GetIsStoppedOutcome(const EConvoyOutcome Outcome) const;

	/** @brief Counts one escaped convoy and applies the response of every cargo type that got trucks away. */
	void HandleConvoyEscaped(const FConvoyCargoOutcome& Outcome);
	static TArray<FConvoyCargoTypeOutcome> GetEscapedTrucksByCargoType(const FConvoyCargoOutcome& Outcome);
	void ApplyEscapedCargoType(const EConvoyCargoType Cargo, const int32 Trucks, const int32 ConvoyIndex);

	// ----- Alert tiers -----
	int32 GetMaxAlertTier() const;
	int32 GetTierForStoppedCount(const int32 StoppedCount) const;
	void StepAlertTierUpTo(const int32 WantedTier);
	void RunTierResponseOnce(const int32 Tier);

	// ----- Targeted attacks -----
	/**
	 * @brief Builds the wave of one tier's targeted attack with the living hostile targets as waypoints and starts it
	 * on the enemy controller of the director's enemy player.
	 * @param TierIndex 0-based index into TierResponses.
	 * @return Whether it started, has to wait for a target, or failed for good.
	 */
	EConvoyPressureAttackLaunch TryLaunchTargetedAttack(const int32 TierIndex);
	TArray<FAttackWaveElement> BuildTargetedAttackElements(const FConvoyPressureTargetedAttack& Attack) const;

	/**
	 * @brief Living targets hostile to the enemy player, ordered as a path: nearest to the start first, then nearest
	 * to the previous target.
	 * @param Attack Holds the targets.
	 * @param StartLocation Where the wave spawns (centre of its spawn points).
	 * @return Waypoints on the navmesh where possible; empty when no target qualifies.
	 */
	TArray<FVector> BuildTargetedAttackWaypoints(const FConvoyPressureTargetedAttack& Attack,
	                                             const FVector& StartLocation) const;
	static int32 GetNearestPointIndex2D(const FVector& From, const TArray<FVector>& Points);
	void FillTargetedAttackMovement(const FConvoyPressureTargetedAttack& Attack, const TArray<FVector>& Waypoints,
	                                const FVector& StartLocation, FAttackWaveRequest& OutRequest) const;
	void QueueTargetedAttackRetry(const int32 TierIndex);
	void RetryPendingTargetedAttacks();

	// ----- Escaped cargo -----
	void ApplyCargoResponse(const FConvoyPressureCargoResponse& Response, const int32 Trucks);
	int32 StartNextPlacementActors(const TArray<TObjectPtr<AEnemyPlacementActor>>& Candidates, const int32 Count);
	int32 ActivateNextDormantDefences(const TArray<TObjectPtr<ADormantDefenceSlot>>& Candidates, const int32 Count);
	/** @return True when the slot spawned its defence now (OnDormantDefenceActivated is sent). */
	bool ActivateDormantDefence(ADormantDefenceSlot& Slot);

	// ----- Editor -----
	void AddTargetedAttackShapes(const FConvoyPressureTargetedAttack& Attack, FEnemyPlacementShapes& OutSummary,
	                             FEnemyPlacementShapes& OutDetail) const;
	void AddDormantDefenceLinkShapes(const TArray<TObjectPtr<ADormantDefenceSlot>>& Slots,
	                                 FEnemyPlacementShapes& OutDetail) const;

	// ----- Placement actors -----
	/** @return True when the actor was started now; false when invalid, already started or of an unknown kind. */
	bool StartPlacementActorOnce(AEnemyPlacementActor* PlacementActor);
	bool GetWasPlacementActorStarted(const AEnemyPlacementActor* PlacementActor) const;
	bool TryStartPlacementActor(AEnemyPlacementActor& PlacementActor);
};
