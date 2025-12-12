// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/EnvironmentActor/EnvironmentActor.h"
#include "ObstacleEnvActor.generated.h"

struct FRTSRoadInstanceSetup;

UCLASS()
class RTS_SURVIVAL_API AObstacleEnvActor : public AEnvironmentActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AObstacleEnvActor(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Set up the collision on each of the components, note that obstacle actors cannot be destroyed and so
	// if set to blocking weapons the weapons will not be able to pass through the obstacle at any time.
	UFUNCTION(BlueprintCallable)
	void SetupCollision(TArray<UMeshComponent*> Components, const bool bBlockWeapons = false);

	/**
 * @brief Blueprint callable function to set up a hierarchical instance box.
 *
 * This function wraps FRTSInstanceHelpers::SetupHierarchicalInstanceBox.
 *
 * @param InstStaticMesh The instanced static mesh component to modify.
 * @param BoxBounds The 2D bounds for the instance box.
 * @param DistanceBetweenInstances The spacing between each instance.
 * @param RequestingActor The actor making the request.
 */
	UFUNCTION(BlueprintCallable, Category = "RTS Instance Helpers")
	static void SetupHierarchicalInstanceBox_BP(UInstancedStaticMeshComponent* InstStaticMesh,
												  FVector2D BoxBounds,
												  float DistanceBetweenInstances,
												  AActor* RequestingActor);

	/**
	 * @brief Blueprint callable function to place instances along a virtual road.
	 *
	 * This function wraps FRTSInstanceHelpers::SetupHierarchicalInstanceAlongRoad.
	 *
	 * @param RoadSetup Defines the parameters for the road (e.g. distances, sidedness).
	 * @param InstStaticMesh The instance component to place the instances on.
	 * @param RequestingActor The actor making the request.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS Instance Helpers")
	static void SetupHierarchicalInstanceAlongRoad_BP(FRTSRoadInstanceSetup RoadSetup,
													  UInstancedStaticMeshComponent* InstStaticMesh,
													  AActor* RequestingActor);


};
