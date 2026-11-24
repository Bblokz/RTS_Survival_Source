// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Missions/MissionProps/PropSinkDeath/MissionPropSinkDeath.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "FuelDeliveryDepotComponent.generated.h"

class AActor;
class AMissionFuelCrane;
class UFuelDeliveryNodeComponent;
class UStaticMesh;
enum class EFuelDeliveryNodeRole : uint8;

/** Where the depot's truck is in its round trip. */
UENUM(BlueprintType)
enum class EFuelDeliveryState : uint8
{
	// No truck in the world: waiting for the (re)fill or respawn timer.
	WaitingForTruck,
	// Async spawn requested.
	SpawningTruck,
	// Lining up behind the park node so the truck rolls in straight.
	DrivingToApproach,
	DrivingToPark,
	// Parked; the crane lifts the cargo off.
	Unloading,
	// Empty (or turned back): driving to the spawn node, where it disappears.
	DrivingHome,
	// The depot building or its crane is gone: no more trucks.
	Stopped
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFuelDeliveryTruckEvent, AActor*, Truck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFuelDeliveryUnloaded, AActor*, Truck, int32, TrainingPointsGranted);

/**
 * @brief Self-contained fuel supply line for an AI-owned building (e.g. Rus_BXP_LongCamoBunker_FuelDepot).
 * At BeginPlay it spawns a fuel crane at the Crane node. Then, in a loop: a full fuel truck spawns at the TruckSpawn node
 * (owned by the building's player), drives to the TruckPark node, the crane lifts all its cargo off, the building's AI
 * player gets TrainingPointsPerDelivery, the truck drives back to the spawn node and disappears, and after
 * RefillSeconds a new full truck sets off. A destroyed truck is replaced after RespawnAfterDestroyedSeconds.
 * When the building or the crane is destroyed, deliveries stop (a truck on the way turns back and disappears).
 * @note Needs three UFuelDeliveryNodeComponents on the same actor, one per EFuelDeliveryNodeRole.
 * @note A truck still out when the building actor is removed drives home on a timer bound to the truck and is removed.
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UFuelDeliveryDepotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFuelDeliveryDepotComponent();

	UFUNCTION(BlueprintPure, Category="FuelDelivery")
	EFuelDeliveryState GetDeliveryState() const { return M_State; }

	UFUNCTION(BlueprintPure, Category="FuelDelivery")
	AActor* GetTruck() const { return M_Truck.Get(); }

	UFUNCTION(BlueprintPure, Category="FuelDelivery")
	AMissionFuelCrane* GetCrane() const;

	/** @return Trucks fully unloaded since BeginPlay. */
	UFUNCTION(BlueprintPure, Category="FuelDelivery")
	int32 GetCompletedDeliveries() const { return M_CompletedDeliveries; }

	/** @return Training points this depot gave its AI player since BeginPlay. */
	UFUNCTION(BlueprintPure, Category="FuelDelivery")
	int32 GetTotalTrainingPointsGranted() const { return M_TotalTrainingPointsGranted; }

	UPROPERTY(BlueprintAssignable, Category="FuelDelivery")
	FOnFuelDeliveryTruckEvent OnTruckSpawned;

	UPROPERTY(BlueprintAssignable, Category="FuelDelivery")
	FOnFuelDeliveryUnloaded OnTruckUnloaded;

	// The truck reached the spawn node and was removed (a new full one comes after RefillSeconds).
	UPROPERTY(BlueprintAssignable, Category="FuelDelivery")
	FOnFuelDeliveryTruckEvent OnTruckDespawned;

	// Truck may already be null: the truck actor can be gone by the time the depot notices.
	UPROPERTY(BlueprintAssignable, Category="FuelDelivery")
	FOnFuelDeliveryTruckEvent OnTruckDestroyed;

#if RTS_WITH_MAP_TESTS
	/** Shortens the waits for -FuelDepotValidate; a truck not yet spawned comes after FirstTruckDelay from now. */
	void MapTest_OverrideTimings(float FirstTruckDelay, float Refill, float RespawnAfterDestroyed);
	int32 MapTest_GetTrainingPointsPerDelivery() const { return TrainingPointsPerDelivery; }
	const UFuelDeliveryNodeComponent* MapTest_GetNode(EFuelDeliveryNodeRole Role) const;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ------------------------------------------------------------------ general
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery")
	bool bEnabled = true;

	// Training points the building's AI player gets each time a truck is fully unloaded.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery", meta=(ClampMin="0"))
	int32 TrainingPointsPerDelivery = 3;

	// ------------------------------------------------------------------ timing
	// Seconds after BeginPlay before the first truck spawns.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Timing", meta=(ClampMin="0"))
	float FirstTruckDelaySeconds = 10.f;

	// Seconds a truck stays away after it disappeared at the spawn node ("being refilled") before it comes again.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Timing", meta=(ClampMin="0"))
	float RefillSeconds = 120.f;

	// Seconds before a destroyed truck is replaced by a new full one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Timing", meta=(ClampMin="0"))
	float RespawnAfterDestroyedSeconds = 180.f;

	// A parked truck that is not empty after this long (crane stalled) drives home without granting points.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Timing", meta=(ClampMin="10"))
	float MaxUnloadSeconds = 240.f;

	// A truck that makes no headway for this long is removed and replaced after RefillSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Timing", meta=(ClampMin="10"))
	float StuckSecondsBeforeRecycle = 90.f;

	// One drive (to the approach point, to the park node, or home) taking longer than this recycles the truck, also
	// when it keeps moving (e.g. another system ordered it elsewhere).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Timing", meta=(ClampMin="30"))
	float MaxLegSeconds = 300.f;

	// ------------------------------------------------------------------ truck
	// Set on the Blueprint (not a native default) so the cooker follows the reference.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck")
	TSoftClassPtr<AActor> TruckClass;

	// The truck first drives to a point this far behind the park node (against its arrow), then straight in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck", meta=(ClampMin="0"))
	float ApproachDistance = 1500.f;

	// Vehicles stop ~2 m short of a move goal, so the park order aims this far past the park node.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck", meta=(ClampMin="0"))
	float ParkStopShortDistance = 230.f;

	// The stopped truck counts as parked within this distance (2D) of the park node.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck", meta=(ClampMin="50"))
	float ParkTolerance = 300.f;

	// After a few orders a truck that stopped this close to the park node is accepted anyway.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck", meta=(ClampMin="50"))
	float LooseParkTolerance = 700.f;

	// The truck disappears once it is this close (2D) to the spawn node.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck", meta=(ClampMin="50"))
	float HomeArrivalTolerance = 600.f;

	// Copy of the truck's hull with one padded collision box (shells otherwise pass through the open bed).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck")
	TSoftObjectPtr<UStaticMesh> TruckHullHitboxMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck")
	bool bSinkTruckWreck = true;

	// What happens to a destroyed truck's wreck: smoke, sink, scorch + embers (no scavengable wreck stays behind).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Truck", meta=(EditCondition="bSinkTruckWreck"))
	FMissionPropSinkDeathSettings TruckWreckDeath;

	// ------------------------------------------------------------------ crane
	// Spawned at the Crane node at BeginPlay. Its rig, stack and timing settings live on this Blueprint.
	// Set on the Blueprint (not a native default) so the cooker follows the reference.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Crane")
	TSoftClassPtr<AMissionFuelCrane> CraneClass;

	// Deliveries stop when the crane is destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery|Crane")
	bool bStopWhenCraneDestroyed = true;

private:
	UPROPERTY()
	TWeakObjectPtr<AMissionFuelCrane> M_Crane;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_Truck;

	UPROPERTY()
	TWeakObjectPtr<UFuelDeliveryNodeComponent> M_SpawnNode;

	UPROPERTY()
	TWeakObjectPtr<UFuelDeliveryNodeComponent> M_ParkNode;

	UPROPERTY()
	TWeakObjectPtr<UFuelDeliveryNodeComponent> M_CraneNode;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> M_PreloadedTruckDeathAssets;

	EFuelDeliveryState M_State = EFuelDeliveryState::WaitingForTruck;
	bool bM_HasSpawnedCrane = false;
	FTimerHandle M_UpdateTimer;
	// World time at which the waiting state spawns the next truck.
	double M_NextTruckSeconds = 0.0;
	// Move orders given in the current leg (a few unanswered ones loosen the arrival check).
	int32 M_OrdersThisLeg = 0;
	bool bM_HasOrder = false;
	double M_UnloadStartSeconds = 0.0;
	// World time the current drive started (MaxLegSeconds).
	double M_LegStartSeconds = 0.0;
	// Spawn attempts that found no async spawner yet (map-placed depots begin play before the player controller).
	int32 M_SpawnerWaitCount = 0;
	// Crane's unloaded-piece count when the unload started (an unload that moved nothing grants nothing).
	int32 M_CraneCountAtUnloadStart = 0;
	// Stuck watch: progress per window while driving.
	double M_StuckWindowStartSeconds = -1.0;
	FVector M_StuckWindowStartLocation = FVector::ZeroVector;
	double M_LastProgressSeconds = 0.0;
	int32 M_CompletedDeliveries = 0;
	int32 M_TotalTrainingPointsGranted = 0;
	FDelegateHandle M_CraneEmptiedHandle;

	bool GetIsValidNodes() const;
	bool BeginPlay_FindNodes();
	void BeginPlay_SpawnCrane();
	void StartUpdates();

	void Update();
	bool GetIsDepotLost() const;
	void OnDepotLost();
	bool GetIsTruckAlive() const;
	void OnTruckLost();

	void SetState(EFuelDeliveryState NewState);
	void ScheduleNextTruck(float DelaySeconds);
	void SpawnTruck();
	void OnTruckSpawnedCallback(AActor* SpawnedTruck);

	void UpdateDriving();
	void IssueOrderForState();
	FVector GetApproachLocation() const;
	FVector GetParkOrderLocation() const;
	bool GetHasReachedGoal() const;
	void OnGoalReached();
	void IssueMoveOrder(const FVector& Goal, const FRotator& FinalRotation, bool bForceFinalRotation);
	void UpdateStuckWatch();
	void RecycleTruck(const FString& Reason);

	void StartUnloading();
	void UpdateUnloading();
	void OnCraneEmptiedTruck(AActor* Truck);
	void GrantTrainingPoints();
	void DriveHome();
	// The building actor is being removed with a truck still out: a timer on the truck drives it home and removes it.
	void HandTruckHomeAfterDepotRemoved();
	/**
	 * @brief One step of an orphaned truck's way home; runs without the depot, so it only uses what it is given.
	 * @param WeakTruck The truck.
	 * @param Home The spawn node location.
	 * @param ArrivalTolerance 2D distance at which the truck is removed.
	 * @param GiveUpSeconds World time after which the truck is left as an ordinary unit.
	 * @param TimerHandle The repeating timer that runs this step; cleared when done.
	 */
	static void DriveOrphanedTruckHome(const TWeakObjectPtr<AActor>& WeakTruck, const FVector& Home,
	                                   float ArrivalTolerance, double GiveUpSeconds, FTimerHandle& TimerHandle);
	void DespawnTruckAtHome();
};
