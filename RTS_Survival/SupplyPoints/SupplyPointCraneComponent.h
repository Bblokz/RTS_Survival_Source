// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageLevel.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/SupplyPoints/SupplyPointTypes.h"
#include "SupplyPointCraneComponent.generated.h"

class ASupplyPoint;
class ATankMaster;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** What the crane needs to know about one resource kind: its stockpile, where a load waits and what a load looks like. */
USTRUCT(BlueprintType)
struct FSupplyCraneResourceSetup
{
	GENERATED_BODY()

	// Static mesh component of the stockpile; its mesh is swapped by fill level.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane")
	FName StockComponentName = NAME_None;

	// Stockpile meshes by fill percentage, ordered from the highest level to the lowest.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane")
	TArray<FStorageMeshLevel> StockLevels;

	// Socket on the base mesh where a ready load waits for the hook (the bottom centre of the load).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane")
	FName PickSocketName = NAME_None;

	// The unit load the hook carries and the truck drives home with; pivot at its bottom centre.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane")
	TObjectPtr<UStaticMesh> LoadMesh = nullptr;

	// Scene component on the truck Blueprint that marks this kind's place in the cargo bed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane")
	FName TruckCargoSlotName = NAME_None;

	// Fill percentage the stockpile drops to when a load is lifted off it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane", meta = (ClampMin = "0", ClampMax = "100"))
	int32 StockLevelAfterLift = 50;
};

/** Names of the crane's moving parts on the supply point Blueprint and the limits of their travel. */
USTRUCT(BlueprintType)
struct FSupplyCraneSettings
{
	GENERATED_BODY()

	// Static mesh component with the Pick_ sockets (the merged base).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Parts")
	FName BaseComponentName = TEXT("Base");

	// Slews: turns about its own Z, its +X is the jib.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Parts")
	FName UpperWorksComponentName = TEXT("UpperWorks");

	// Child of the upper works; slides along its parent's +X.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Parts")
	FName TrolleyComponentName = TEXT("Trolley");

	// Child of the trolley; a rope of RopeMeshLength hanging down from its pivot, scaled in Z to the hook.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Parts")
	FName RopeComponentName = TEXT("Rope");

	// Child of the trolley (not of the rope); moves down along Z and swivels.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Parts")
	FName HookBlockComponentName = TEXT("HookBlock");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float RopeMeshLength = 100.f;

	// Distance from the hook block's pivot down to the bottom of a hanging load.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float HookToLoadBottom = 230.f;

	// Jib yaw relative to the supply point when the crane is idle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float RestSlewYaw = -200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float TrolleyMinRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float TrolleyMaxRadius = 750.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float RestTrolleyRadius = 385.f;

	// How far the hook hangs below the trolley when idle, and while it travels (with or without a load).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float RestHookDrop = 55.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float CarryHookDrop = 95.f;

	// A single lift never takes longer than this, so a one-kind load does not crawl through a long loading time.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Timing", meta = (ClampMin = "0.5"))
	float MaxSecondsPerLift = 6.f;

	// Seconds the crane takes back to its idle pose (after the last lift, or when loading is cut short).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Timing", meta = (ClampMin = "0.1"))
	float ReturnToRestSeconds = 1.5f;

	// Yaw of a load in the truck bed relative to the cargo slot (loads lie across the bed).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane|Travel")
	float LoadYawInBed = 90.f;
};

/** Where every moving part of the crane is: jib yaw, trolley distance, hook depth and hook swivel. */
USTRUCT()
struct FSupplyCranePose
{
	GENERATED_BODY()

	// Yaw of the jib relative to the supply point.
	float SlewYaw = 0.f;
	float TrolleyRadius = 0.f;
	// Distance of the hook block's pivot below the trolley.
	float HookDrop = 0.f;
	// World yaw of the hook block (and of a load hanging from it).
	float HookWorldYaw = 0.f;
};

enum class ESupplyCraneKeyEvent : uint8
{
	None,
	// The hook reached the waiting load: it now hangs from the hook and the stockpile steps down.
	PickLoad,
	// The load stands in the truck bed: it leaves the hook.
	ReleaseLoad
};

/** One moment of the crane cycle: the pose the crane has at that time and what happens when it gets there. */
struct FSupplyCraneKey
{
	float Time = 0.f;
	FSupplyCranePose Pose;
	ESupplyCraneKeyEvent Event = ESupplyCraneKeyEvent::None;
	ERTSResourceType ResourceType = ERTSResourceType::Resource_None;
};

/** How full one stockpile is and where it is filling to while supplies are prepared. */
struct FSupplyStockFill
{
	float Percent = 100.f;
	float FillStartPercent = 100.f;
};

/**
 * @brief Add to a supply point Blueprint to animate it from code: the crane lifts each resource kind from its bay
 * into the parked truck over the loading time, stockpiles fill while supplies are prepared and step down per lift,
 * and the loads end up in the truck's cargo slots. Parts are found by the component names in CraneSettings, so the
 * Blueprint needs no graph logic.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API USupplyPointCraneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USupplyPointCraneComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// Seconds into the running crane cycle; negative when the crane is idle.
	float GetCraneCycleSeconds() const { return bM_IsCycleRunning ? M_CycleSeconds : -1.f; }
	int32 GetLoadsPlacedInTruck() const { return M_LoadsPlacedInTruck; }
	int32 GetShownStockLevel(ERTSResourceType ResourceType) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane")
	FSupplyCraneSettings CraneSettings;

	// Radixite, metal and vehicle parts; a kind without an entry is loaded without visuals.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crane")
	TMap<ERTSResourceType, FSupplyCraneResourceSetup> ResourceSetups;

private:
	void BeginPlay_FindParts();
	void BeginPlay_CreateLoadComponents();
	void BeginPlay_BindSupplyPoint();

	bool GetIsValidSupplyPoint() const;
	bool GetHasCraneParts() const;

	// ---- Supply point events ----
	void OnSuppliesPreparing(float Duration);
	void OnSuppliesReady();
	void OnTruckLoadingStarted(ATankMaster* SupplyTruck, float Duration, const FSupplyTruckCargo& Cargo);
	void OnTruckLoadingFinished(bool bCompleted);

	// ---- Crane cycle ----
	/**
	 * @brief Lays out the whole loading as timed poses: one lift per loaded kind, then back to rest.
	 * @param SupplyTruck The parked truck; the bed targets come from its cargo slots as it really stands.
	 * @param Duration Loading time of the supply point.
	 * @param Cargo Amounts per kind; kinds with zero get no lift.
	 */
	void BuildCraneCycle(const ATankMaster* SupplyTruck, float Duration, const FSupplyTruckCargo& Cargo);
	void AddLiftKeys(ERTSResourceType ResourceType, const FSupplyCraneResourceSetup& Setup,
	                 const ATankMaster* SupplyTruck, float LiftStart, float LiftSeconds);
	void AddKey(float Time, const FSupplyCranePose& Pose, ESupplyCraneKeyEvent Event = ESupplyCraneKeyEvent::None,
	            ERTSResourceType ResourceType = ERTSResourceType::Resource_None);
	void TickCraneCycle(float DeltaTime);
	void FireKeyEvent(const FSupplyCraneKey& Key);
	void StartReturnToRest();
	/**
	 * @brief Works out the pose that puts the bottom of a hanging load on a world location.
	 * @param LoadBottomLocation Where the load's bottom centre must be.
	 * @param HookWorldYaw Yaw the load must have there.
	 * @return The pose, with the trolley clamped to its travel.
	 */
	FSupplyCranePose MakePoseOverLocation(const FVector& LoadBottomLocation, float HookWorldYaw) const;
	FSupplyCranePose MakeRestPose() const;
	static FSupplyCranePose LerpPose(const FSupplyCranePose& From, const FSupplyCranePose& To, float Alpha);
	void ApplyPose(const FSupplyCranePose& Pose);
	FTransform GetBedSlotTransform(const ATankMaster* SupplyTruck, const FSupplyCraneResourceSetup& Setup) const;
	FTransform GetPickTransform(const FSupplyCraneResourceSetup& Setup) const;

	// ---- Loads and stockpiles ----
	void SetWaitingLoadsVisible(bool bVisible);
	void PickLoad(ERTSResourceType ResourceType);
	void ReleaseLoadIntoTruck(ERTSResourceType ResourceType);
	void TickStockFill(float DeltaTime);
	void SetStockPercent(ERTSResourceType ResourceType, float Percent);
	void ApplyStockMesh(ERTSResourceType ResourceType);
	void UpdateTickEnabled();

	UPROPERTY()
	TWeakObjectPtr<ASupplyPoint> M_SupplyPoint;

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_Base;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> M_UpperWorks;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> M_Trolley;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> M_Rope;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> M_HookBlock;

	// The load hanging from the hook; shows the mesh of the kind being lifted.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_CarriedLoad;

	// One load per kind standing in its bay while supplies are ready.
	UPROPERTY()
	TMap<ERTSResourceType, TObjectPtr<UStaticMeshComponent>> M_WaitingLoads;

	UPROPERTY()
	TMap<ERTSResourceType, TWeakObjectPtr<UStaticMeshComponent>> M_StockComponents;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TruckBeingLoaded;

	TMap<ERTSResourceType, FSupplyStockFill> M_StockFills;

	TArray<FSupplyCraneKey> M_CycleKeys;

	FSupplyCranePose M_CurrentPose;

	float M_CycleSeconds = 0.f;
	int32 M_NextKeyIndex = 0;
	bool bM_IsCycleRunning = false;

	// Supplies being prepared: the stockpiles fill over this time.
	float M_FillDuration = 0.f;
	float M_FillSeconds = 0.f;
	bool bM_IsFilling = false;

	int32 M_LoadsPlacedInTruck = 0;
};
