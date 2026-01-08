// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyNavigationAIComponent.generated.h"

class AEnemyController;
class ARecastNavMesh;
class ARoadSplineActor;
class USplineComponent;

UENUM(BlueprintType)
enum class EOnProjectionFailedStrategy : uint8
{
	None,
	LookAtXY,
	LookAtDoubleExtent
};

/**
 * @brief Supports enemy controller navigation checks by projecting locations and sampling navigable areas.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyNavigationAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyNavigationAIComponent();

	void InitNavigationAIComponent(AEnemyController* EnemyController);

	/**
	 * @brief Projects a location onto navigable space to keep AI decisions grounded on reachable terrain.
	 * @param OriginalLocation Input location to project onto the navmesh.
	 * @param ProjectionScale Scales the RTS projection extent used by the nav system.
	 * @param OutProjectedLocation Resulting projected point if successful.
	 * @param ProjectionFailedStrategy Controls fallback sampling when the initial projection fails.
	 * @return True when a valid projected point is found.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	bool GetNavigablePoint(
		const FVector& OriginalLocation,
		const float ProjectionScale,
		FVector& OutProjectedLocation,
		const EOnProjectionFailedStrategy ProjectionFailedStrategy = EOnProjectionFailedStrategy::None) const;

	/**
	 * @brief Projects a location that remains on default-cost navmesh to avoid expensive traversal areas.
	 * @param OriginalLocation Input location to project onto the navmesh.
	 * @param ProjectionScale Scales the RTS projection extent used by the nav system.
	 * @param OutProjectedLocation Resulting projected point if successful.
	 * @param ProjectionFailedStrategy Controls fallback sampling when the initial projection fails.
	 * @return True when a projected point on default nav area is found.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	bool GetNavPointDefaultCosts(
		const FVector& OriginalLocation,
		const float ProjectionScale,
		FVector& OutProjectedLocation,
		const EOnProjectionFailedStrategy ProjectionFailedStrategy = EOnProjectionFailedStrategy::None) const;

	/**
	 * @brief Samples a navigable region between points to supply navigation targets without blocking gameplay.
	 * @param StartPoint First point used to define the search region.
	 * @param EndPoint Second point used to define the search region.
	 * @param BoxExtent Defines the half-size of the sample area around the projected center point.
	 * @param SampleDensityScalar Controls spacing between sampled points.
	 * @param ProjectionScale Scales the RTS projection extent used by the nav system.
	 * @param MaxPointsToReturn Maximum number of points to return (clamped internally).
	 * @param OnPointsFound Callback invoked with the projected points on the game thread.
	 */
	void FindDefaultNavCostPointsInAreaBetweenTwoPointsAsync(
		const FVector& StartPoint,
		const FVector& EndPoint,
		const FVector& BoxExtent,
		const float SampleDensityScalar,
		const float ProjectionScale,
		const int32 MaxPointsToReturn,
		TFunction<void(const TArray<FVector>&)> OnPointsFound) const;

	/**
	 * @brief Samples navigable points along the closest road spline to guide enemy movement paths.
	 * @param StartSearchPoint World location used to choose the closest road spline.
	 * @param SampleDensityScalar Controls spacing between sampled spline points.
	 * @param ProjectionScale Scales the RTS projection extent used by the nav system.
	 * @param OnPointsFound Callback invoked with the projected points on the game thread.
	 */
	void FindDefaultNavCostPointsAlongClosestRoadSplineAsync(
		const FVector& StartSearchPoint,
		const float SampleDensityScalar,
		const float ProjectionScale,
		TFunction<void(const TArray<FVector>&)> OnPointsFound) const;

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	UPROPERTY()
	TWeakObjectPtr<ARecastNavMesh> M_RecastNavMesh = nullptr;

	UPROPERTY()
	TArray<TWeakObjectPtr<ARoadSplineActor>> M_RoadSplineActors;

	bool EnsureEnemyControllerIsValid() const;
	bool GetIsValidRecastNavMesh() const;

	void CacheRecastNavMesh();
	void CacheRoadSplineActors();

	bool TryProjectPointToNavigation(
		const FVector& OriginalLocation,
		const float ProjectionScale,
		FNavLocation& OutNavLocation) const;

	bool TryProjectPointWithStrategy(
		const FVector& OriginalLocation,
		const float ProjectionScale,
		const EOnProjectionFailedStrategy ProjectionFailedStrategy,
		FNavLocation& OutNavLocation) const;

	bool TryProjectDefaultCostPoint(
		const FVector& OriginalLocation,
		const float ProjectionScale,
		const FString& DebugContext,
		FNavLocation& OutNavLocation) const;

	bool TryProjectDefaultCostPointWithStrategy(
		const FVector& OriginalLocation,
		const float ProjectionScale,
		const EOnProjectionFailedStrategy ProjectionFailedStrategy,
		FNavLocation& OutNavLocation) const;

	ARoadSplineActor* GetClosestRoadSplineActor(const FVector& SearchPoint) const;

	void QueueDefaultCostBatchProjectionAsync(
		const TArray<FVector>& SamplePoints,
		const FVector& ProjectionExtent,
		TFunction<void(const TArray<FVector>&)> OnPointsFound,
		const FString& DebugContext) const;

	void DebugProjectionAttempt(const FVector& Location, const FVector& Extent, const FString& Context) const;
	void DebugProjectionResult(const FVector& Location, const bool bSuccess, const FString& Context) const;
	void DebugOffsetAttempt(const FVector& OriginalLocation, const FVector& OffsetLocation, const FString& Context) const;
	void DebugDefaultAreaResult(const FVector& Location, const bool bIsDefaultArea, const FString& Context) const;
	void DebugBatchResult(const TArray<FVector>& Points, const FString& Context) const;
};
