// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "SovietRefineryComponent.generated.h"

class AResourceUnloadStation;
class UResourceDropOff;
class USovietRefineryDropOff;
class UStaticMeshComponent;
class UMeshComponent;
class UEnemyStrategicAIComponent;
class USceneComponent;

/** One stock fill or level bar mesh on the refinery, found by its component tag. */
USTRUCT()
struct FSovietRefineryStockVisual
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> Component;

	ERTSResourceType Resource = ERTSResourceType::Resource_None;
	int32 Order = 0;
	FVector RestScale = FVector::OneVector;
};

/** A harvester this refinery trained or adopted, and what it harvests. */
USTRUCT()
struct FSovietRefineryHarvester
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Harvester;

	ERTSResourceType Resource = ERTSResourceType::Resource_None;
};

/**
 * @brief Turns an enemy building into a Soviet refinery (Docs/EnemyAI/SovietRefinery.md, EnemyHarvestEconomy.md):
 * - creates the refinery's drop-off (its cargo slots are the stockpile) and spawns the crane unload station;
 * - converts the stock over time: radixite into the owning AI's training points, metal into its building points;
 * - trains free T-34 Breaker harvesters at its exit until it has N (FEnemyAIMissionSettings::MaxHarvestersPerRefinery)
 *   and replaces losses; harvesters hand-placed near it count towards N;
 * - scales the stock fill meshes and level bars with the stock.
 * @note Starts once the building stands (an ABuildingExpansion under construction waits). Stock is lost with the
 * building. Stock visuals are components tagged "SovRefFill_<Radixite|Metal>_<order>" and
 * "SovRefBar_<Radixite|Metal>_<order>"; the exit is a scene component tagged "SovRefHarvesterExit".
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USovietRefineryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovietRefineryComponent();

	UFUNCTION(BlueprintCallable, Category="Soviet Refinery")
	int32 GetStock(ERTSResourceType Resource) const;

	UFUNCTION(BlueprintCallable, Category="Soviet Refinery")
	int32 GetAliveHarvesterCount() const;

	UFUNCTION(BlueprintCallable, Category="Soviet Refinery")
	bool GetIsRefineryActive() const { return bM_IsActive; }

	UResourceDropOff* GetRefineryDropOff() const;
	/** @return The living harvesters this refinery trained or adopted, with what each harvests. */
	TArray<AActor*> GetHarvesters(TArray<ERTSResourceType>* OutResources = nullptr) const;
	AResourceUnloadStation* GetUnloadStation() const { return M_UnloadStation.Get(); }
	int32 GetMaxHarvesters() const;

	// Totals converted so far (tests, debugging).
	int32 GetTotalTrainingPointsGranted() const { return M_TotalTrainingPoints; }
	int32 GetTotalBuildingPointsGranted() const { return M_TotalBuildingPoints; }
	int32 GetTotalConverted(const ERTSResourceType Resource) const
	{
		return Resource == ERTSResourceType::Resource_Metal ? M_TotalConvertedMetal : M_TotalConvertedRadixite;
	}

#if RTS_WITH_MAP_TESTS
	void MapTest_SetHarvesterTrainSeconds(const float Seconds) { HarvesterTrainSeconds = Seconds; }
	void MapTest_SetMaxHarvestersOverride(const int32 Max) { M_MapTest_MaxHarvestersOverride = Max; }
	void MapTest_AddStock(ERTSResourceType Resource, int32 Amount);
	void MapTest_RunConversionNow() { ConvertStock(); }
#endif

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The crane station spawned on the building mesh (BP_SovRefCraneStation_A1 / _G2).
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Unload")
	TSubclassOf<AResourceUnloadStation> UnloadStationClass;

	// ------------------------------------------------------------------ stock and conversion (provisional numbers)

	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Stock", meta=(ClampMin="1"))
	int32 RadixiteStockCapacity = 1500;

	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Stock", meta=(ClampMin="1"))
	int32 MetalStockCapacity = 1500;

	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Conversion", meta=(ClampMin="0.5"))
	float ConversionIntervalSeconds = 10.f;

	// Stock taken out of each resource per interval.
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Conversion", meta=(ClampMin="0"))
	int32 RadixiteConvertedPerInterval = 15;

	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Conversion", meta=(ClampMin="0"))
	int32 MetalConvertedPerInterval = 15;

	// Training points per converted radixite (10 points = one BT-7); fractions carry over.
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Conversion", meta=(ClampMin="0"))
	float TrainingPointsPerRadixite = 0.02f;

	// Building points per converted metal; fractions carry over.
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Conversion", meta=(ClampMin="0"))
	float BuildingPointsPerMetal = 0.02f;

	// Fill meshes scale in Z from this (stock just above 0) to 1 (full); at 0 they hide.
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Stock", meta=(ClampMin="0.01", ClampMax="1"))
	float MinFillScale = 0.04f;

	// A1's tripper car (component tagged "SovRefTripper") shuttles between these rail ends (building frame, cm) while
	// there is stock; without a tagged component nothing ticks.
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Stock")
	FVector TripperRailStart = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Stock")
	FVector TripperRailEnd = FVector::ZeroVector;

	// Seconds for one run from end to end.
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Stock", meta=(ClampMin="1"))
	float TripperRunSeconds = 14.f;

	// ------------------------------------------------------------------ harvesters

	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Harvesters")
	ETankSubtype HarvesterSubtype = ETankSubtype::Tank_T34_Harvester;

	// Used when the owner has no enemy controller (tests, odd maps); missions set it on the controller.
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Harvesters", meta=(ClampMin="0"))
	int32 FallbackMaxHarvesters = 2;

	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Harvesters", meta=(ClampMin="0.1"))
	float HarvesterTrainSeconds = 45.f;

	// Hand-placed harvesters of the same owner within this distance join this refinery when it starts.
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Harvesters", meta=(ClampMin="0"))
	float AdoptHarvesterRadius = 6000.f;

	// Exit used when the Blueprint has no scene component tagged SovRefHarvesterExit (building frame, cm).
	UPROPERTY(EditDefaultsOnly, Category="Soviet Refinery|Harvesters")
	FTransform FallbackHarvesterExit = FTransform(FRotator::ZeroRotator, FVector(0.f, -3000.f, 0.f));

private:
	bool bM_IsActive = false;
	FTimerHandle M_WaitForStandingTimer;
	FTimerHandle M_ConversionTimer;
	FTimerHandle M_HarvesterTimer;
	FTimerHandle M_HarvesterTrainTimer;

	UPROPERTY()
	TObjectPtr<USovietRefineryDropOff> M_DropOff;

	UPROPERTY()
	TWeakObjectPtr<AResourceUnloadStation> M_UnloadStation;

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_BuildingMesh;

	UPROPERTY()
	TArray<FSovietRefineryStockVisual> M_StockVisuals;

	UPROPERTY()
	TArray<FSovietRefineryHarvester> M_Harvesters;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> M_Tripper;
	float M_TripperPhase = 0.f;

	bool bM_IsTrainingHarvester = false;
	bool bM_IsSpawningHarvester = false;
	int32 M_SpawnRequestID = 0;

	float M_TrainingPointRemainder = 0.f;
	float M_BuildingPointRemainder = 0.f;
	int32 M_TotalTrainingPoints = 0;
	int32 M_TotalBuildingPoints = 0;
	int32 M_TotalConvertedRadixite = 0;
	int32 M_TotalConvertedMetal = 0;

#if RTS_WITH_MAP_TESTS
	int32 M_MapTest_MaxHarvestersOverride = INDEX_NONE;
#endif

	void TryActivate();
	bool GetIsOwnerStanding() const;
	void ActivateRefinery();
	bool CreateDropOff();
	void SpawnUnloadStation();
	void CollectStockVisuals();
	void UpdateStockVisuals() const;

	void ConvertStock();
	UEnemyStrategicAIComponent* GetStrategicAI() const;

	void AdoptNearbyHarvesters();
	void HarvesterThink();
	void PruneHarvesters();
	void OnHarvesterTrained();
	void OnHarvesterSpawned(AActor* SpawnedActor);
	ERTSResourceType PickResourceForNewHarvester() const;
	void SendHarvesterToWork(AActor* Harvester, ERTSResourceType Resource) const;
	FTransform GetHarvesterExitTransform() const;
	bool TryGetNavigableLocation(const FVector& Desired, FVector& OutLocation) const;
	FString GetOwnerNameForLog() const;
};
