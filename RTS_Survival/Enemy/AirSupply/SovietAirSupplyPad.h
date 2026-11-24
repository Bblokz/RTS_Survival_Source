// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovietAirSupplyPadTypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "SovietAirSupplyPad.generated.h"

class AAircraftMaster;
class UArrowComponent;
class UInstancedStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSovietAirSupplyDelivered, ASovietAirSupplyPad*, Pad,
                                               int32, TrainingPointsGranted, int32, BuildingPointsGranted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSovietAirSupplyPadEvent, ASovietAirSupplyPad*, Pad);

/** Where one cargo plane starts, lands and leaves; all on the line of the pad's landing arrow. */
struct FSovietAirSupplyFlightPlan
{
	FVector Direction = FVector::ForwardVector;
	FVector SpawnLocation = FVector::ZeroVector;
	FVector LandingLocation = FVector::ZeroVector;
	FVector RetreatLocation = FVector::ZeroVector;
};

/**
 * @brief Self-contained Soviet air supply point to bring an enemy area to life; needs no mission scripting.
 * In a loop: a cargo plane (a regular, shootable aircraft owned by Settings.EnemyPlayer) flies in along the landing
 * arrow, lands vertically on the pad, unloads fuel barrels and munition crates, two unarmed workers carry every item
 * into its storage, the AI player gets the delivery's training and building points, and the plane lifts off
 * vertically and flies away. SecondsBetweenPlanes after it left (or was shot down) the next plane comes.
 * A plane shot down before all cargo is stored gives nothing; its cargo vanishes from the pad.
 * @note The Blueprint sets the meshes (pad, radio mast, storages, workers, cargo) and places the scene points:
 * LandingPoint (its X arrow = flight direction), FuelStoragePoint, MunitionStoragePoint, WorkerPostA/B.
 * @note With no enemy controller for EnemyPlayer the planes still fly, but deliveries give no points (logged).
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASovietAirSupplyPad : public AActor
{
	GENERATED_BODY()

public:
	ASovietAirSupplyPad();

	/** @brief Starts (first plane after FirstPlaneDelaySeconds) or stops the loop; a plane already up finishes. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Air Supply")
	void SetDeliveriesEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Supply")
	ESovietAirSupplyState GetSupplyState() const { return M_State; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Supply")
	AAircraftMaster* GetCurrentPlane() const { return M_Plane.Get(); }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Supply")
	int32 GetCompletedDeliveries() const { return M_CompletedDeliveries; }

	// Planes shot down (or removed by a safety timeout) before their cargo was stored.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Supply")
	int32 GetLostDeliveries() const { return M_LostDeliveries; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Supply")
	int32 GetTotalTrainingPointsGranted() const { return M_TotalTrainingPointsGranted; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Air Supply")
	int32 GetTotalBuildingPointsGranted() const { return M_TotalBuildingPointsGranted; }

	// All cargo of a plane is in storage and the points were given.
	UPROPERTY(BlueprintAssignable, Category = "Air Supply")
	FOnSovietAirSupplyDelivered OnDeliveryCompleted;

	// A plane was shot down (or timed out) before its cargo was stored: no points.
	UPROPERTY(BlueprintAssignable, Category = "Air Supply")
	FOnSovietAirSupplyPadEvent OnDeliveryLost;

	// A cargo plane took off towards the pad.
	UPROPERTY(BlueprintAssignable, Category = "Air Supply")
	FOnSovietAirSupplyPadEvent OnPlaneLaunched;

#if RTS_WITH_MAP_TESTS
	int32 MapTest_GetStoredItems() const { return M_StoredItemCount; }
	int32 MapTest_GetItemsOnPile() const;
	bool MapTest_GetIsAnyWorkerCarrying() const;
	FVector MapTest_GetWorkerLocation(int32 WorkerIndex) const;
	// Edit before FinishSpawning (BeginPlay starts the countdown).
	FSovietAirSupplySettings& MapTest_GetEditableSettings() { return Settings; }
	FVector MapTest_GetLandingLocation() const;
	FVector MapTest_GetStorageLocation(ESovietAirSupplyCargo Type) const { return GetStorageLocation(Type); }
#endif

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// How often the plane comes, what a delivery gives and which plane flies; edit per placed pad.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Supply", meta = (ShowOnlyInnerProperties))
	FSovietAirSupplySettings Settings;

	// Worker animations and speeds, cargo carry offsets; set on the Blueprint.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Supply|Visuals")
	FSovietAirSupplyWorkerVisuals WorkerVisuals;

	// ----- Components (meshes are assigned on the Blueprint) -----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PadMesh = nullptr;

	// Decor: the pad's radio mast and the two storage sheds (add more meshes on the Blueprint freely).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RadioMastMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FuelStorageMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MunitionStorageMesh = nullptr;

	// Where the plane touches down; its X arrow is the direction the plane flies in and out.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LandingPoint = nullptr;

	// The workers put fuel barrels down here (the fuel storage's door).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> FuelStoragePoint = nullptr;

	// The workers put munition crates down here (the munition storage's door).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MunitionStoragePoint = nullptr;

	// Where each worker stands while no plane is on the pad (faces the point's X arrow).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> WorkerPostA = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> WorkerPostB = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> WorkerMeshA = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> WorkerMeshB = nullptr;

	// The barrel or crate in a worker's arms (mesh set at pick-up from the matching pile).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CarriedCargoA = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CarriedCargoB = nullptr;

	// Fuel barrels unloaded from the plane (set its static mesh on the Blueprint).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> FuelCargoPile = nullptr;

	// Munition crates unloaded from the plane (set its static mesh on the Blueprint).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> MunitionCargoPile = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UArrowComponent> LandingArrow = nullptr;
#endif

private:
	enum class EWorkerState : uint8
	{
		AtPost,
		WalkingToCargo,
		WaitingForCargo,
		PickingUp,
		CarryingToStorage,
		PuttingDown,
		WalkingToPost
	};

	struct FCargoItem
	{
		ESovietAirSupplyCargo Type = ESovietAirSupplyCargo::Fuel;
		int32 InstanceIndex = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		bool bAppeared = false;
		bool bClaimed = false;
	};

	struct FWorker
	{
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		TWeakObjectPtr<UStaticMeshComponent> Carried;
		TWeakObjectPtr<USceneComponent> Post;
		EWorkerState State = EWorkerState::AtPost;
		FVector Target = FVector::ZeroVector;
		float StateStartSeconds = 0.f;
		float StateEndSeconds = 0.f;
		// The item has gone into (pick up) or out of (put down) the arms during this handling.
		bool bHandedOver = false;
		int32 ItemIndex = INDEX_NONE;
		float LaneSign = 1.f;
		TWeakObjectPtr<UAnimSequenceBase> PlayingAnim;
	};

	ESovietAirSupplyState M_State = ESovietAirSupplyState::Disabled;
	bool bM_DeliveriesEnabled = false;

	// Raised for every plane: callbacks of an older plane (a wreck still falling) are ignored.
	int32 M_RunId = 0;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> M_Plane;

	UPROPERTY()
	TSubclassOf<AAircraftMaster> M_LoadedAircraftClass;

	TSharedPtr<FStreamableHandle> M_AircraftClassLoadHandle;
	FDelegateHandle M_PlaneDiesHandle;
	FDelegateHandle M_PlaneLandedHandle;

	FTimerHandle M_NextPlaneTimerHandle;
	FTimerHandle M_SafetyTimerHandle;
	FTimerHandle M_TakeOffTimerHandle;
	FTimerHandle M_UnloadItemTimerHandle;

	TArray<FCargoItem> M_CargoItems;
	int32 M_AppearedItemCount = 0;
	int32 M_StoredItemCount = 0;
	TArray<FWorker> M_Workers;

	int32 M_CompletedDeliveries = 0;
	int32 M_LostDeliveries = 0;
	int32 M_TotalTrainingPointsGranted = 0;
	int32 M_TotalBuildingPointsGranted = 0;

	void Constructor_CreateDecorMeshes();
	void Constructor_CreateLayoutPoints();
	void Constructor_CreateWorkers();
	void Constructor_CreateCargoPiles();

	// ----- Loop -----
	void SetState(ESovietAirSupplyState NewState);
	void ScheduleNextPlane(float DelaySeconds);
	void ClearLoopTimers();
	void LoadAircraftClass();
	void OnAircraftClassLoaded();
	void LaunchPlane();
	FVector GetFlightDirection() const;
	FSovietAirSupplyFlightPlan BuildFlightPlan() const;
	/** @return The plane owned by Settings.EnemyPlayer at the plan's start, or null (error reported). */
	AAircraftMaster* SpawnPlane(const FSovietAirSupplyFlightPlan& Plan) const;
	void OrderPlaneToPad(AAircraftMaster& Plane, const FSovietAirSupplyFlightPlan& Plan) const;
	/**
	 * @brief One safety timer per run (approach, then unload); the run id keeps a late timer off the next plane.
	 * @param Seconds Time until OnTimedOut.
	 * @param OnTimedOut Called with the run id that was current when the timer started.
	 */
	void StartSafetyTimer(float Seconds, void (ASovietAirSupplyPad::*OnTimedOut)(int32));
	void BindPlane(AAircraftMaster* Plane);
	void UnbindPlane();

	void OnPlaneLanded(AAircraftMaster* Plane, int32 RunId);
	void OnPlaneDies(int32 RunId);
	UFUNCTION()
	void OnPlaneDestroyed(AActor* DestroyedActor);
	void OnApproachTimedOut(int32 RunId);
	void OnUnloadTimedOut(int32 RunId);
	void LoseDelivery(const TCHAR* Reason);
	void CompleteDelivery();
	void ReleasePlaneForTakeOff();
	void GrantPoints(int32& OutTraining, int32& OutBuilding) const;

	// ----- Cargo -----
	void BuildCargoPile(const AAircraftMaster& Plane);
	void UnloadNextItem();
	void ClearCargo();
	UInstancedStaticMeshComponent* GetPile(ESovietAirSupplyCargo Type) const;
	FVector GetStorageLocation(ESovietAirSupplyCargo Type) const;
	bool GetIsValidLandingPoint() const;
	bool GetIsValidStoragePoint(ESovietAirSupplyCargo Type) const;
	bool GetIsValidPile(ESovietAirSupplyCargo Type) const;
	int32 ClaimNextItem();

	// ----- Workers -----
	void InitWorkers();
	void TickWorker(FWorker& Worker, float DeltaSeconds);
	void TickWorkerPickingUp(FWorker& Worker, float NowSeconds);
	void TickWorkerPuttingDown(FWorker& Worker, float NowSeconds);
	float GetHandOverSeconds(const FWorker& Worker) const;
	bool MoveWorker(FWorker& Worker, float Speed, float DeltaSeconds) const;
	void SetWorkerState(FWorker& Worker, EWorkerState NewState);
	void SendWorkerToNextItem(FWorker& Worker);
	void PlayWorkerAnim(FWorker& Worker, UAnimSequenceBase* Anim, bool bLooping) const;
	void PlaceWorkerAtPost(FWorker& Worker) const;
	void AttachCarriedItem(FWorker& Worker) const;
	void DropCarriedItem(FWorker& Worker) const;
	void SendAllWorkersHome();
	FVector GetLaneTarget(const FWorker& Worker, const FVector& From, const FVector& To) const;
	float GetGroundZ(const FVector& Location) const;
	bool GetAreAllWorkersAtPost() const;
};
