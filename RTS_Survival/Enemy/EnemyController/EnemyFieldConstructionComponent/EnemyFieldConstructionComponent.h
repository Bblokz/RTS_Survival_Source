// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionTypes/FieldConstructionTypes.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFieldConstructionComponent/FindAdditionalLocationsStrategy.h"
#include "EnemyFieldConstructionComponent.generated.h"

class AEnemyController;
class ASquadController;
class UEnemyNavigationAIComponent;
class UFieldConstructionAbilityComponent;

UENUM(BlueprintType)
enum class EFieldConstructionStrategy : uint8
{
	None,
	VsTanks,
	VsInfantry,
	VsMixed,
	MinesOnly,
	HedgehogsOnly
};

USTRUCT()
struct FEnemyFieldConstructionSquadEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ASquadController> SquadController = nullptr;

	UPROPERTY()
	TArray<TWeakObjectPtr<UFieldConstructionAbilityComponent>> FieldConstructionAbilityComponents;

	UPROPERTY()
	TArray<EFieldConstructionType> FieldConstructionTypes;
};

USTRUCT()
struct FFieldConstructionLocationPair
{
	GENERATED_BODY()

	UPROPERTY()
	FVector ConstructionLocation = FVector::ZeroVector;

	UPROPERTY()
	EFieldConstructionType ConstructionType = EFieldConstructionType::RussianHedgeHog;
};

USTRUCT()
struct FFieldConstructionLocationsStrategy
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FFieldConstructionLocationPair> LocationPairs;
};

USTRUCT()
struct FEnemyFieldConstructionOrder
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FEnemyFieldConstructionSquadEntry> SquadConstructionData;

	UPROPERTY()
	TArray<FVector> ConstructionLocations;

	UPROPERTY()
	FFindAddtionalLocationsStrategy AdditionalLocationsStrategy;

	UPROPERTY()
	EFieldConstructionStrategy Strategy = EFieldConstructionStrategy::None;

	// Filtered hierarchy based on selected strategy and available construction abilities.
	UPROPERTY()
	TArray<EFieldConstructionType> StrategyHierarchy;

	UPROPERTY()
	FFieldConstructionLocationsStrategy LocationStrategy;

	UPROPERTY()
	int32 OrderId = 0;

	// When true the order is waiting on async location sampling to finish.
	UPROPERTY()
	bool bIsAwaitingAdditionalLocationsSearch = false;
};

/**
 * @brief Handles enemy field construction orders and assigns squads to build based on available abilities.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyFieldConstructionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyFieldConstructionComponent();

	void InitFieldConstructionComponent(AEnemyController* EnemyController);

	/**
	 * @brief Creates a field construction order and assigns construction types based on the chosen strategy.
	 * @param SquadControllers Squads considered for construction abilities.
	 * @param ConstructionLocations Locations to fill with field constructions.
	 * @param AdditionalLocationsStrategy Strategy to fetch more locations when the order runs out.
	 * @param Strategy Desired strategy or None to pick automatically.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void CreateFieldConstructionOrder(
		const TArray<ASquadController*>& SquadControllers,
		const TArray<FVector>& ConstructionLocations,
		const FFindAddtionalLocationsStrategy& AdditionalLocationsStrategy,
		const EFieldConstructionStrategy Strategy = EFieldConstructionStrategy::None);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetFieldConstructionOrderInterval(const float NewIntervalSeconds);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;
	bool EnsureEnemyControllerIsValid();

	// Active orders processed by a single timer.
	TArray<FEnemyFieldConstructionOrder> M_FieldConstructionOrders;

	FTimerHandle M_FieldConstructionTimerHandle;

	float M_FieldConstructionOrderInterval = 20.f;
	int32 M_NextFieldConstructionOrderId = 0;

	void StartFieldConstructionTimerIfNeeded();
	void StopFieldConstructionTimerIfIdle();

	void ProcessFieldConstructionOrders();
	void RemoveInvalidSquadEntries(FEnemyFieldConstructionOrder& Order) const;
	void RemoveUnavailableLocationPairs(FEnemyFieldConstructionOrder& Order) const;
	bool TryHandleEmptyLocationPairs(FEnemyFieldConstructionOrder& Order);
	bool TryQueueAdditionalLocationsOnClosestRoad(
		FEnemyFieldConstructionOrder& Order,
		const FVector& AverageSquadLocation,
		UEnemyNavigationAIComponent* EnemyNavigationAIComponent);
	bool TryQueueAdditionalLocationsInAreaBetweenSquadAndEnemy(
		FEnemyFieldConstructionOrder& Order,
		const FVector& AverageSquadLocation,
		UEnemyNavigationAIComponent* EnemyNavigationAIComponent);

	void BuildOrderSquadAbilityData(FEnemyFieldConstructionOrder& Order,
	                                const TArray<ASquadController*>& SquadControllers) const;
	void DetermineFieldConstructionStrategy(FEnemyFieldConstructionOrder& Order,
	                                        const EFieldConstructionStrategy RequestedStrategy) const;

	void BuildLocationStrategyForOrder(FEnemyFieldConstructionOrder& Order) const;

	TArray<int32> GetLocationOrderByDijkstra(const TArray<FVector>& Locations) const;
	int32 GetNextUnvisitedLocationIndex(const TArray<float>& Distances, const TArray<bool>& bVisited) const;
	void UpdateDijkstraDistances(int32 SourceIndex, const TArray<FVector>& Locations,
	                             TArray<float>& Distances, TArray<bool>& bVisited) const;
	TArray<TArray<FVector>> SplitLocationsIntoGroups(const TArray<FVector>& Locations, const int32 GroupCount) const;
	TArray<TArray<EFieldConstructionType>> SplitConstructionTypesIntoGroups(
		const TArray<EFieldConstructionType>& ConstructionTypes,
		const int32 GroupCount) const;

	void AppendLocationPairs(
		TArray<FFieldConstructionLocationPair>& OutPairs,
		const TArray<FVector>& Locations,
		const TArray<EFieldConstructionType>& ConstructionTypes,
		const bool bRandomizeTypes) const;

	FEnemyFieldConstructionOrder* GetFieldConstructionOrderById(const int32 OrderId);
	bool TryGetAverageSquadLocation(const FEnemyFieldConstructionOrder& Order, FVector& OutAverageLocation) const;
	int32 GetNextFieldConstructionOrderId();
	void HandleAdditionalLocationsFound(const int32 OrderId, const TArray<FVector>& Locations);

	TArray<EFieldConstructionType> GetAvailableFieldConstructionTypes(const FEnemyFieldConstructionOrder& Order) const;

	bool TryAssignConstructionToIdleSquads(FEnemyFieldConstructionOrder& Order);

	bool GetIsRussianFieldConstructionType(const EFieldConstructionType ConstructionType) const;
	TArray<EFieldConstructionType> GetStrategyHierarchy(const EFieldConstructionStrategy Strategy) const;
};
