// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyFieldConstructionComponent.h"

#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyNavigationAIComponent/EnemyNavigationAIComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionAbilityComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EnemyFieldConstructionStrategyData
{
	TArray<EFieldConstructionType> GetVsTanksHierarchy()
	{
		return {
			EFieldConstructionType::RussianLightATMine,
			EFieldConstructionType::RussianMediumATMine,
			EFieldConstructionType::RussianHeavyATMine,
			EFieldConstructionType::RussianHedgeHog,
			EFieldConstructionType::RussianBarrier
		};
	}

	TArray<EFieldConstructionType> GetVsInfantryHierarchy()
	{
		return {
			EFieldConstructionType::RussianAntiInfantryStickMine,
			EFieldConstructionType::RussianWire,
			EFieldConstructionType::RussianHedgeHog,
			EFieldConstructionType::RussianBarrier
		};
	}

	TArray<EFieldConstructionType> GetVsMixedHierarchy()
	{
		return {
			EFieldConstructionType::RussianAntiInfantryStickMine,
			EFieldConstructionType::RussianLightATMine,
			EFieldConstructionType::RussianMediumATMine,
			EFieldConstructionType::RussianHeavyATMine,
			EFieldConstructionType::RussianWire,
			EFieldConstructionType::RussianHedgeHog,
			EFieldConstructionType::RussianBarrier
		};
	}

	TArray<EFieldConstructionType> GetMinesOnlyHierarchy()
	{
		return {
			EFieldConstructionType::RussianAntiInfantryStickMine,
			EFieldConstructionType::RussianLightATMine,
			EFieldConstructionType::RussianMediumATMine,
			EFieldConstructionType::RussianHeavyATMine
		};
	}

	TArray<EFieldConstructionType> GetHedgehogsOnlyHierarchy()
	{
		return {
			EFieldConstructionType::RussianHedgeHog
		};
	}
}

namespace EnemyFieldConstructionConstants
{
	constexpr int32 GroupSplitThresholdSmall = 10;
	constexpr int32 GroupSplitThresholdMedium = 20;
	constexpr int32 MaxConstructionAssignmentAttempts = 8;
	constexpr float DefaultFieldConstructionIntervalSeconds = 6.f;
	constexpr float MinFieldConstructionIntervalSeconds = 0.1f;
	constexpr float AdditionalLocationsProjectionScale = 1.f;
	constexpr float AdditionalLocationsAreaExtentScale = 0.5f;
	constexpr float AdditionalLocationsAreaMinExtent = 500.f;
	constexpr float AdditionalLocationsAreaExtentZ = 100.f;
}

UEnemyFieldConstructionComponent::UEnemyFieldConstructionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	M_FieldConstructionOrderInterval = EnemyFieldConstructionConstants::DefaultFieldConstructionIntervalSeconds;
}

void UEnemyFieldConstructionComponent::InitFieldConstructionComponent(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
}

void UEnemyFieldConstructionComponent::CreateFieldConstructionOrder(
	const TArray<ASquadController*>& SquadControllers,
	const TArray<FVector>& ConstructionLocations,
	const FFindAddtionalLocationsStrategy& AdditionalLocationsStrategy,
	const EFieldConstructionStrategy Strategy)
{
	if (SquadControllers.Num() == 0 || ConstructionLocations.Num() == 0)
	{
		return;
	}
	FEnemyFieldConstructionOrder NewOrder;
	NewOrder.OrderId = GetNextFieldConstructionOrderId();
	NewOrder.ConstructionLocations = ConstructionLocations;
	NewOrder.AdditionalLocationsStrategy = AdditionalLocationsStrategy;
	BuildOrderSquadAbilityData(NewOrder, SquadControllers);
	if (NewOrder.SquadConstructionData.Num() == 0)
	{
		return;
	}

	DetermineFieldConstructionStrategy(NewOrder, Strategy);
	if (NewOrder.StrategyHierarchy.Num() == 0)
	{
		return;
	}

	BuildLocationStrategyForOrder(NewOrder);
	if (NewOrder.LocationStrategy.LocationPairs.Num() == 0)
	{
		return;
	}

	M_FieldConstructionOrders.Add(NewOrder);
	StartFieldConstructionTimerIfNeeded();
}

void UEnemyFieldConstructionComponent::SetFieldConstructionOrderInterval(const float NewIntervalSeconds)
{
	const float ClampedInterval = FMath::Max(
		NewIntervalSeconds,
		EnemyFieldConstructionConstants::MinFieldConstructionIntervalSeconds);

	M_FieldConstructionOrderInterval = ClampedInterval;

	if (UWorld* World = GetWorld())
	{
		if (World->GetTimerManager().IsTimerActive(M_FieldConstructionTimerHandle))
		{
			World->GetTimerManager().ClearTimer(M_FieldConstructionTimerHandle);
			World->GetTimerManager().SetTimer(
				M_FieldConstructionTimerHandle,
				this,
				&UEnemyFieldConstructionComponent::ProcessFieldConstructionOrders,
				M_FieldConstructionOrderInterval,
				true);
		}
	}
}

void UEnemyFieldConstructionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyFieldConstructionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FieldConstructionTimerHandle);
	}
}

bool UEnemyFieldConstructionComponent::EnsureEnemyControllerIsValid()
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("EnemyFieldConstructionComponent has no valid enemy controller! "
		"\n see UEnemyFieldConstructionComponent::EnsureEnemyControllerIsValid"));
	M_EnemyController = Cast<AEnemyController>(GetOwner());
	return M_EnemyController.IsValid();
}

void UEnemyFieldConstructionComponent::StartFieldConstructionTimerIfNeeded()
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(M_FieldConstructionTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_FieldConstructionTimerHandle,
		this,
		&UEnemyFieldConstructionComponent::ProcessFieldConstructionOrders,
		M_FieldConstructionOrderInterval,
		true);
}

void UEnemyFieldConstructionComponent::StopFieldConstructionTimerIfIdle()
{
	if (M_FieldConstructionOrders.Num() > 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FieldConstructionTimerHandle);
	}
}

void UEnemyFieldConstructionComponent::ProcessFieldConstructionOrders()
{
	if (M_FieldConstructionOrders.Num() == 0)
	{
		StopFieldConstructionTimerIfIdle();
		return;
	}

	FEnemyFieldConstructionOrder& Order = M_FieldConstructionOrders[0];
	RemoveInvalidSquadEntries(Order);
	if (Order.SquadConstructionData.Num() == 0)
	{
		M_FieldConstructionOrders.RemoveAt(0);
		StopFieldConstructionTimerIfIdle();
		return;
	}

	if (Order.LocationStrategy.LocationPairs.Num() == 0)
	{
		if (not TryHandleEmptyLocationPairs(Order))
		{
			M_FieldConstructionOrders.RemoveAt(0);
			StopFieldConstructionTimerIfIdle();
		}
		return;
	}

	RemoveUnavailableLocationPairs(Order);
	if (Order.LocationStrategy.LocationPairs.Num() == 0)
	{
		if (not TryHandleEmptyLocationPairs(Order))
		{
			M_FieldConstructionOrders.RemoveAt(0);
			StopFieldConstructionTimerIfIdle();
		}
		return;
	}

	TryAssignConstructionToIdleSquads(Order);
	if (Order.LocationStrategy.LocationPairs.Num() == 0)
	{
		if (not TryHandleEmptyLocationPairs(Order))
		{
			M_FieldConstructionOrders.RemoveAt(0);
			StopFieldConstructionTimerIfIdle();
		}
	}
}

void UEnemyFieldConstructionComponent::RemoveInvalidSquadEntries(FEnemyFieldConstructionOrder& Order) const
{
	for (int32 Index = Order.SquadConstructionData.Num() - 1; Index >= 0; --Index)
	{
		FEnemyFieldConstructionSquadEntry& SquadEntry = Order.SquadConstructionData[Index];
		if (not SquadEntry.SquadController.IsValid())
		{
			Order.SquadConstructionData.RemoveAt(Index);
			continue;
		}

		for (int32 AbilityIndex = SquadEntry.FieldConstructionAbilityComponents.Num() - 1; AbilityIndex >= 0; --AbilityIndex)
		{
			if (SquadEntry.FieldConstructionAbilityComponents[AbilityIndex].IsValid())
			{
				continue;
			}

			SquadEntry.FieldConstructionAbilityComponents.RemoveAt(AbilityIndex);
			if (SquadEntry.FieldConstructionTypes.IsValidIndex(AbilityIndex))
			{
				SquadEntry.FieldConstructionTypes.RemoveAt(AbilityIndex);
			}
		}

		if (SquadEntry.FieldConstructionAbilityComponents.Num() == 0 || SquadEntry.FieldConstructionTypes.Num() == 0)
		{
			Order.SquadConstructionData.RemoveAt(Index);
		}
	}
}

void UEnemyFieldConstructionComponent::RemoveUnavailableLocationPairs(FEnemyFieldConstructionOrder& Order) const
{
	const TArray<EFieldConstructionType> AvailableTypes = GetAvailableFieldConstructionTypes(Order);
	if (AvailableTypes.Num() == 0)
	{
		Order.LocationStrategy.LocationPairs.Reset();
		return;
	}

	const TSet<EFieldConstructionType> AvailableTypeSet(AvailableTypes);
	Order.LocationStrategy.LocationPairs.RemoveAll([&AvailableTypeSet](const FFieldConstructionLocationPair& Pair)
	{
		return not AvailableTypeSet.Contains(Pair.ConstructionType);
	});
}

bool UEnemyFieldConstructionComponent::TryHandleEmptyLocationPairs(FEnemyFieldConstructionOrder& Order)
{
	if (Order.LocationStrategy.LocationPairs.Num() > 0)
	{
		return true;
	}

	if (Order.bIsAwaitingAdditionalLocationsSearch)
	{
		return true;
	}

	if (Order.AdditionalLocationsStrategy.Strategy == EAdditionalLocationsStrategy::None)
	{
		return false;
	}

	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	AEnemyController* EnemyController = M_EnemyController.Get();
	if (not IsValid(EnemyController))
	{
		return false;
	}

	UEnemyNavigationAIComponent* EnemyNavigationAIComponent = EnemyController->GetEnemyNavigationAIComponent();
	if (not IsValid(EnemyNavigationAIComponent))
	{
		return false;
	}

	FVector AverageSquadLocation = FVector::ZeroVector;
	if (not TryGetAverageSquadLocation(Order, AverageSquadLocation))
	{
		return false;
	}

	const EAdditionalLocationsStrategy Strategy = Order.AdditionalLocationsStrategy.Strategy;
	if (Strategy == EAdditionalLocationsStrategy::ContinueOnClosestRoad)
	{
		return TryQueueAdditionalLocationsOnClosestRoad(Order, AverageSquadLocation, EnemyNavigationAIComponent);
	}

	if (Strategy == EAdditionalLocationsStrategy::ContinueOnAreaBetweenCurrentAndEnemyLocation)
	{
		return TryQueueAdditionalLocationsInAreaBetweenSquadAndEnemy(
			Order,
			AverageSquadLocation,
			EnemyNavigationAIComponent);
	}

	return false;
}

bool UEnemyFieldConstructionComponent::TryQueueAdditionalLocationsOnClosestRoad(
	FEnemyFieldConstructionOrder& Order,
	const FVector& AverageSquadLocation,
	UEnemyNavigationAIComponent* EnemyNavigationAIComponent)
{
	if (not IsValid(EnemyNavigationAIComponent))
	{
		return false;
	}

	const int32 OrderId = Order.OrderId;
	Order.bIsAwaitingAdditionalLocationsSearch = true;

	const FFindAddtionalLocationsStrategy& AdditionalLocationsStrategy = Order.AdditionalLocationsStrategy;
	TWeakObjectPtr<UEnemyFieldConstructionComponent> WeakThis(this);

	EnemyNavigationAIComponent->FindDefaultNavCostPointsAlongClosestRoadSplineAsync(
		AverageSquadLocation,
		AdditionalLocationsStrategy.PointDensityScaler,
		EnemyFieldConstructionConstants::AdditionalLocationsProjectionScale,
		[WeakThis, OrderId](const TArray<FVector>& Points)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->HandleAdditionalLocationsFound(OrderId, Points);
		});

	return true;
}

bool UEnemyFieldConstructionComponent::TryQueueAdditionalLocationsInAreaBetweenSquadAndEnemy(
	FEnemyFieldConstructionOrder& Order,
	const FVector& AverageSquadLocation,
	UEnemyNavigationAIComponent* EnemyNavigationAIComponent)
{
	if (not IsValid(EnemyNavigationAIComponent))
	{
		return false;
	}

	const FFindAddtionalLocationsStrategy& AdditionalLocationsStrategy = Order.AdditionalLocationsStrategy;
	if (AdditionalLocationsStrategy.AmountOfLocationsToFind <= 0)
	{
		return false;
	}

	const int32 OrderId = Order.OrderId;
	Order.bIsAwaitingAdditionalLocationsSearch = true;

	const FVector EnemyLocation = AdditionalLocationsStrategy.EnemyLocation;
	const FVector DistanceDelta = EnemyLocation - AverageSquadLocation;
	const FVector AbsDelta = DistanceDelta.GetAbs();
	const float BoxExtentX = FMath::Max(
		AbsDelta.X * EnemyFieldConstructionConstants::AdditionalLocationsAreaExtentScale,
		EnemyFieldConstructionConstants::AdditionalLocationsAreaMinExtent);
	const float BoxExtentY = FMath::Max(
		AbsDelta.Y * EnemyFieldConstructionConstants::AdditionalLocationsAreaExtentScale,
		EnemyFieldConstructionConstants::AdditionalLocationsAreaMinExtent);
	const FVector BoxExtent(
		BoxExtentX,
		BoxExtentY,
		EnemyFieldConstructionConstants::AdditionalLocationsAreaExtentZ);

	TWeakObjectPtr<UEnemyFieldConstructionComponent> WeakThis(this);
	EnemyNavigationAIComponent->FindDefaultNavCostPointsInAreaBetweenTwoPointsAsync(
		AverageSquadLocation,
		EnemyLocation,
		BoxExtent,
		AdditionalLocationsStrategy.PointDensityScaler,
		EnemyFieldConstructionConstants::AdditionalLocationsProjectionScale,
		AdditionalLocationsStrategy.AmountOfLocationsToFind,
		[WeakThis, OrderId](const TArray<FVector>& Points)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->HandleAdditionalLocationsFound(OrderId, Points);
		});

	return true;
}

void UEnemyFieldConstructionComponent::BuildOrderSquadAbilityData(
	FEnemyFieldConstructionOrder& Order,
	const TArray<ASquadController*>& SquadControllers) const
{
	for (ASquadController* SquadController : SquadControllers)
	{
		if (not IsValid(SquadController))
		{
			continue;
		}

		TInlineComponentArray<UFieldConstructionAbilityComponent*> FoundAbilityComponents;
		SquadController->GetComponents(FoundAbilityComponents);

		FEnemyFieldConstructionSquadEntry SquadEntry;
		SquadEntry.SquadController = SquadController;

		for (UFieldConstructionAbilityComponent* AbilityComponent : FoundAbilityComponents)
		{
			if (not IsValid(AbilityComponent))
			{
				continue;
			}

			const EFieldConstructionType ConstructionType = AbilityComponent->GetConstructionType();
			if (not GetIsRussianFieldConstructionType(ConstructionType))
			{
				continue;
			}

			SquadEntry.FieldConstructionAbilityComponents.Add(AbilityComponent);
			SquadEntry.FieldConstructionTypes.Add(ConstructionType);
		}

		if (SquadEntry.FieldConstructionAbilityComponents.Num() == 0)
		{
			continue;
		}

		Order.SquadConstructionData.Add(SquadEntry);
	}
}

void UEnemyFieldConstructionComponent::DetermineFieldConstructionStrategy(
	FEnemyFieldConstructionOrder& Order,
	const EFieldConstructionStrategy RequestedStrategy) const
{
	const TArray<EFieldConstructionType> AvailableTypes = GetAvailableFieldConstructionTypes(Order);
	if (AvailableTypes.Num() == 0)
	{
		return;
	}

	EFieldConstructionStrategy SelectedStrategy = RequestedStrategy;
	if (RequestedStrategy == EFieldConstructionStrategy::None)
	{
		TArray<EFieldConstructionStrategy> ValidStrategies;
		constexpr EFieldConstructionStrategy StrategyOptions[] = {
			EFieldConstructionStrategy::VsTanks,
			EFieldConstructionStrategy::VsInfantry,
			EFieldConstructionStrategy::VsMixed,
			EFieldConstructionStrategy::MinesOnly,
			EFieldConstructionStrategy::HedgehogsOnly
		};

		for (const EFieldConstructionStrategy StrategyOption : StrategyOptions)
		{
			TArray<EFieldConstructionType> CandidateHierarchy = GetStrategyHierarchy(StrategyOption);
			CandidateHierarchy.RemoveAll([&AvailableTypes](const EFieldConstructionType CandidateType)
			{
				return not AvailableTypes.Contains(CandidateType);
			});

			if (CandidateHierarchy.Num() > 0)
			{
				ValidStrategies.Add(StrategyOption);
			}
		}

		if (ValidStrategies.Num() == 0)
		{
			return;
		}

		const int32 RandomIndex = FMath::RandRange(0, ValidStrategies.Num() - 1);
		SelectedStrategy = ValidStrategies[RandomIndex];
	}

	Order.Strategy = SelectedStrategy;
	Order.StrategyHierarchy = GetStrategyHierarchy(SelectedStrategy);
	Order.StrategyHierarchy.RemoveAll([&AvailableTypes](const EFieldConstructionType CandidateType)
	{
		return not AvailableTypes.Contains(CandidateType);
	});
}

void UEnemyFieldConstructionComponent::BuildLocationStrategyForOrder(FEnemyFieldConstructionOrder& Order) const
{
	const int32 LocationCount = Order.ConstructionLocations.Num();
	if (LocationCount == 0 || Order.StrategyHierarchy.Num() == 0)
	{
		return;
	}

	int32 GroupCount = 2;
	if (LocationCount < EnemyFieldConstructionConstants::GroupSplitThresholdSmall)
	{
		GroupCount = 2;
	}
	else if (LocationCount < EnemyFieldConstructionConstants::GroupSplitThresholdMedium)
	{
		GroupCount = 3;
	}
	else
	{
		GroupCount = 4;
	}

	const TArray<TArray<FVector>> LocationGroups = SplitLocationsIntoGroups(Order.ConstructionLocations, GroupCount);
	const TArray<TArray<EFieldConstructionType>> TypeGroups = SplitConstructionTypesIntoGroups(
		Order.StrategyHierarchy,
		GroupCount);

	TArray<FFieldConstructionLocationPair> LocationPairs;
	for (int32 GroupIndex = 0; GroupIndex < LocationGroups.Num(); ++GroupIndex)
	{
		const bool bRandomizeTypes = (GroupIndex == LocationGroups.Num() - 1)
			|| (LocationGroups.Num() == 2 && GroupIndex == 1);

		const TArray<FVector>& GroupLocations = LocationGroups[GroupIndex];
		const TArray<EFieldConstructionType>& GroupTypes = TypeGroups.IsValidIndex(GroupIndex)
			? TypeGroups[GroupIndex]
			: Order.StrategyHierarchy;

		AppendLocationPairs(LocationPairs, GroupLocations, GroupTypes, bRandomizeTypes);
	}

	Order.LocationStrategy.LocationPairs = LocationPairs;
}

TArray<int32> UEnemyFieldConstructionComponent::GetLocationOrderByDijkstra(const TArray<FVector>& Locations) const
{
	const int32 LocationCount = Locations.Num();
	TArray<int32> OrderedIndices;
	OrderedIndices.Reserve(LocationCount);

	if (LocationCount == 0)
	{
		return OrderedIndices;
	}

	TArray<float> Distances;
	Distances.Init(FLT_MAX, LocationCount);

	TArray<bool> bVisited;
	bVisited.Init(false, LocationCount);

	Distances[0] = 0.f;

	for (int32 Iteration = 0; Iteration < LocationCount; ++Iteration)
	{
		const int32 BestIndex = GetNextUnvisitedLocationIndex(Distances, bVisited);

		if (BestIndex == INDEX_NONE)
		{
			break;
		}

		UpdateDijkstraDistances(BestIndex, Locations, Distances, bVisited);
	}

	TArray<int32> SortedIndices;
	SortedIndices.Reserve(LocationCount);
	for (int32 Index = 0; Index < LocationCount; ++Index)
	{
		SortedIndices.Add(Index);
	}

	SortedIndices.Sort([&Distances](const int32 FirstIndex, const int32 SecondIndex)
	{
		return Distances[FirstIndex] < Distances[SecondIndex];
	});

	return SortedIndices;
}

int32 UEnemyFieldConstructionComponent::GetNextUnvisitedLocationIndex(
	const TArray<float>& Distances,
	const TArray<bool>& bVisited) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistance = FLT_MAX;
	for (int32 Index = 0; Index < Distances.Num(); ++Index)
	{
		if (bVisited[Index])
		{
			continue;
		}
		if (Distances[Index] < BestDistance)
		{
			BestDistance = Distances[Index];
			BestIndex = Index;
		}
	}
	return BestIndex;
}

void UEnemyFieldConstructionComponent::UpdateDijkstraDistances(
	const int32 SourceIndex,
	const TArray<FVector>& Locations,
	TArray<float>& Distances,
	TArray<bool>& bVisited) const
{
	const int32 LocationCount = Locations.Num();
	bVisited[SourceIndex] = true;

	for (int32 NeighborIndex = 0; NeighborIndex < LocationCount; ++NeighborIndex)
	{
		if (bVisited[NeighborIndex])
		{
			continue;
		}

		const float EdgeWeight = FVector::DistSquared(Locations[SourceIndex], Locations[NeighborIndex]);
		const float NewDistance = Distances[SourceIndex] + EdgeWeight;
		if (NewDistance < Distances[NeighborIndex])
		{
			Distances[NeighborIndex] = NewDistance;
		}
	}
}

TArray<TArray<FVector>> UEnemyFieldConstructionComponent::SplitLocationsIntoGroups(
	const TArray<FVector>& Locations,
	const int32 GroupCount) const
{
	TArray<TArray<FVector>> Groups;
	if (Locations.Num() == 0 || GroupCount <= 0)
	{
		return Groups;
	}

	Groups.SetNum(GroupCount);

	const TArray<int32> OrderedIndices = GetLocationOrderByDijkstra(Locations);
	const int32 TotalLocations = OrderedIndices.Num();
	const int32 BaseGroupSize = TotalLocations / GroupCount;
	int32 RemainingLocations = TotalLocations % GroupCount;

	int32 CurrentIndex = 0;
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		const int32 GroupSize = BaseGroupSize + (RemainingLocations > 0 ? 1 : 0);
		if (RemainingLocations > 0)
		{
			--RemainingLocations;
		}

		TArray<FVector> GroupLocations;
		GroupLocations.Reserve(GroupSize);
		for (int32 GroupOffset = 0; GroupOffset < GroupSize; ++GroupOffset)
		{
			if (not OrderedIndices.IsValidIndex(CurrentIndex))
			{
				break;
			}
			GroupLocations.Add(Locations[OrderedIndices[CurrentIndex]]);
			++CurrentIndex;
		}
		Groups[GroupIndex] = GroupLocations;
	}

	return Groups;
}

TArray<TArray<EFieldConstructionType>> UEnemyFieldConstructionComponent::SplitConstructionTypesIntoGroups(
	const TArray<EFieldConstructionType>& ConstructionTypes,
	const int32 GroupCount) const
{
	TArray<TArray<EFieldConstructionType>> Groups;
	if (ConstructionTypes.Num() == 0 || GroupCount <= 0)
	{
		return Groups;
	}

	Groups.SetNum(GroupCount);

	const int32 TotalTypes = ConstructionTypes.Num();
	const int32 BaseGroupSize = TotalTypes / GroupCount;
	int32 RemainingTypes = TotalTypes % GroupCount;

	int32 CurrentIndex = 0;
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		const int32 GroupSize = BaseGroupSize + (RemainingTypes > 0 ? 1 : 0);
		if (RemainingTypes > 0)
		{
			--RemainingTypes;
		}

		TArray<EFieldConstructionType> GroupTypes;
		GroupTypes.Reserve(GroupSize);
		for (int32 GroupOffset = 0; GroupOffset < GroupSize; ++GroupOffset)
		{
			if (not ConstructionTypes.IsValidIndex(CurrentIndex))
			{
				break;
			}
			GroupTypes.Add(ConstructionTypes[CurrentIndex]);
			++CurrentIndex;
		}
		Groups[GroupIndex] = GroupTypes;
	}

	return Groups;
}

void UEnemyFieldConstructionComponent::AppendLocationPairs(
	TArray<FFieldConstructionLocationPair>& OutPairs,
	const TArray<FVector>& Locations,
	const TArray<EFieldConstructionType>& ConstructionTypes,
	const bool bRandomizeTypes) const
{
	if (Locations.Num() == 0 || ConstructionTypes.Num() == 0)
	{
		return;
	}

	const int32 ConstructionTypeCount = ConstructionTypes.Num();
	for (int32 LocationIndex = 0; LocationIndex < Locations.Num(); ++LocationIndex)
	{
		EFieldConstructionType SelectedType = ConstructionTypes[0];
		if (bRandomizeTypes)
		{
			const int32 RandomIndex = FMath::RandRange(0, ConstructionTypeCount - 1);
			SelectedType = ConstructionTypes[RandomIndex];
		}
		else if (ConstructionTypes.IsValidIndex(LocationIndex))
		{
			SelectedType = ConstructionTypes[LocationIndex];
		}
		else
		{
			SelectedType = ConstructionTypes.Last();
		}

		FFieldConstructionLocationPair Pair;
		Pair.ConstructionLocation = Locations[LocationIndex];
		Pair.ConstructionType = SelectedType;
		OutPairs.Add(Pair);
	}
}

FEnemyFieldConstructionOrder* UEnemyFieldConstructionComponent::GetFieldConstructionOrderById(const int32 OrderId)
{
	for (FEnemyFieldConstructionOrder& Order : M_FieldConstructionOrders)
	{
		if (Order.OrderId == OrderId)
		{
			return &Order;
		}
	}

	return nullptr;
}

bool UEnemyFieldConstructionComponent::TryGetAverageSquadLocation(
	const FEnemyFieldConstructionOrder& Order,
	FVector& OutAverageLocation) const
{
	if (Order.SquadConstructionData.Num() == 0)
	{
		return false;
	}

	FVector LocationSum = FVector::ZeroVector;
	int32 ValidSquadCount = 0;
	for (const FEnemyFieldConstructionSquadEntry& SquadEntry : Order.SquadConstructionData)
	{
		const ASquadController* SquadController = SquadEntry.SquadController.Get();
		if (not IsValid(SquadController))
		{
			continue;
		}

		LocationSum += SquadController->GetActorLocation();
		++ValidSquadCount;
	}

	if (ValidSquadCount == 0)
	{
		return false;
	}

	OutAverageLocation = LocationSum / static_cast<float>(ValidSquadCount);
	return true;
}

int32 UEnemyFieldConstructionComponent::GetNextFieldConstructionOrderId()
{
	const int32 NextOrderId = M_NextFieldConstructionOrderId;
	if (M_NextFieldConstructionOrderId == TNumericLimits<int32>::Max())
	{
		M_NextFieldConstructionOrderId = 0;
	}
	else
	{
		++M_NextFieldConstructionOrderId;
	}

	return NextOrderId;
}

void UEnemyFieldConstructionComponent::HandleAdditionalLocationsFound(
	const int32 OrderId,
	const TArray<FVector>& Locations)
{
	FEnemyFieldConstructionOrder* Order = GetFieldConstructionOrderById(OrderId);
	if (Order == nullptr)
	{
		return;
	}

	Order->bIsAwaitingAdditionalLocationsSearch = false;
	if (Locations.Num() == 0)
	{
		return;
	}

	Order->ConstructionLocations = Locations;
	BuildLocationStrategyForOrder(*Order);
}

TArray<EFieldConstructionType> UEnemyFieldConstructionComponent::GetAvailableFieldConstructionTypes(
	const FEnemyFieldConstructionOrder& Order) const
{
	TArray<EFieldConstructionType> AvailableTypes;
	for (const FEnemyFieldConstructionSquadEntry& SquadEntry : Order.SquadConstructionData)
	{
		for (const EFieldConstructionType ConstructionType : SquadEntry.FieldConstructionTypes)
		{
			AvailableTypes.AddUnique(ConstructionType);
		}
	}
	return AvailableTypes;
}

bool UEnemyFieldConstructionComponent::TryAssignConstructionToIdleSquads(FEnemyFieldConstructionOrder& Order)
{
	if (Order.LocationStrategy.LocationPairs.Num() == 0)
	{
		return false;
	}

	bool bHasAssigned = false;
	for (int32 SquadIndex = Order.SquadConstructionData.Num() - 1; SquadIndex >= 0; --SquadIndex)
	{
		FEnemyFieldConstructionSquadEntry& SquadEntry = Order.SquadConstructionData[SquadIndex];
		ASquadController* SquadController = SquadEntry.SquadController.Get();
		if (not IsValid(SquadController))
		{
			continue;
		}

		if (not SquadController->GetIsUnitIdle())
		{
			continue;
		}

		bool bWasAssigned = false;
		for (int32 AttemptIndex = 0; AttemptIndex < EnemyFieldConstructionConstants::MaxConstructionAssignmentAttempts;
		     ++AttemptIndex)
		{
			if (Order.LocationStrategy.LocationPairs.Num() == 0)
			{
				break;
			}

			const int32 PairIndex = AttemptIndex % Order.LocationStrategy.LocationPairs.Num();
			const FFieldConstructionLocationPair& Pair = Order.LocationStrategy.LocationPairs[PairIndex];
			if (not SquadEntry.FieldConstructionTypes.Contains(Pair.ConstructionType))
			{
				continue;
			}

			const ECommandQueueError CommandError = SquadController->FieldConstruction(
				Pair.ConstructionType,
				true,
				Pair.ConstructionLocation,
				FRotator::ZeroRotator,
				nullptr);

			if (CommandError == ECommandQueueError::NoError)
			{
				Order.LocationStrategy.LocationPairs.RemoveAt(PairIndex);
				bWasAssigned = true;
				bHasAssigned = true;
				break;
			}
		}

		if (not bWasAssigned)
		{
			Order.SquadConstructionData.RemoveAt(SquadIndex);
		}
	}

	return bHasAssigned;
}

bool UEnemyFieldConstructionComponent::GetIsRussianFieldConstructionType(
	const EFieldConstructionType ConstructionType) const
{
	switch (ConstructionType)
	{
	case EFieldConstructionType::RussianHedgeHog:
	case EFieldConstructionType::RussianBarrier:
	case EFieldConstructionType::RussianWire:
	case EFieldConstructionType::RussianAntiInfantryStickMine:
	case EFieldConstructionType::RussianLightATMine:
	case EFieldConstructionType::RussianMediumATMine:
	case EFieldConstructionType::RussianHeavyATMine:
		return true;
	default:
		return false;
	}
}

TArray<EFieldConstructionType> UEnemyFieldConstructionComponent::GetStrategyHierarchy(
	const EFieldConstructionStrategy Strategy) const
{
	TArray<EFieldConstructionType> Hierarchy;

	switch (Strategy)
	{
	case EFieldConstructionStrategy::VsTanks:
		Hierarchy = EnemyFieldConstructionStrategyData::GetVsTanksHierarchy();
		break;
	case EFieldConstructionStrategy::VsInfantry:
		Hierarchy = EnemyFieldConstructionStrategyData::GetVsInfantryHierarchy();
		break;
	case EFieldConstructionStrategy::VsMixed:
		Hierarchy = EnemyFieldConstructionStrategyData::GetVsMixedHierarchy();
		break;
	case EFieldConstructionStrategy::MinesOnly:
		Hierarchy = EnemyFieldConstructionStrategyData::GetMinesOnlyHierarchy();
		break;
	case EFieldConstructionStrategy::HedgehogsOnly:
		Hierarchy = EnemyFieldConstructionStrategyData::GetHedgehogsOnlyHierarchy();
		break;
	default:
		break;
	}

	return Hierarchy;
}
