// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "CrushableEnvActor.generated.h"

// Forward declarations
class UStaticMeshComponent;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UInstancedStaticMeshComponent;

/**
 * @brief Container mapping instanced geometry components and their state.
 *
 * Holds geometry components associated with an instanced static mesh and a flag indicating if geometry mode is active.
 */
USTRUCT()
struct FInstancedGeoMapping
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<UGeometryCollectionComponent*> GeoComponents;

	UPROPERTY()
	bool bIsUsingGeometry = false;
};

/**
 * @brief Manages destructible environment actors with dynamic instance-to-geometry switching.
 *
 * Handles conversion of mesh instances to physics-enabled geometry based on proximity, supports hierarchical instance layout,
 * and maintains mappings between static meshes and geometry collections for enhanced destruction effects.
 * Designed for real-time strategy survival gameplay.
 */
UCLASS()
class RTS_SURVIVAL_API ACrushableEnvActor : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes the crushable environment actor.
	 *
	 * @param ObjectInitializer Used to initialize properties.
	 */
	ACrushableEnvActor(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Updates the actor every frame.
	 *
	 * @param DeltaTime Elapsed time since the last frame.
	 */
	virtual void Tick(float DeltaTime) override;

protected:
	/**
	 * @brief Performs initialization logic when the actor is spawned or the game starts.
	 */
	virtual void BeginPlay() override;

	/**
	 * @brief Generates and registers geometry components for each instance in the specified static mesh component.
	 *
	 * Pre: InstStaticMesh must be valid and have an assigned static mesh with an existing geometry mapping.
	 * Post: A geometry component is created for every instance and stored in the internal mapping.
	 *
	 * @param InstStaticMesh The instanced static mesh component to process.
	 * @param ZOffsetGeoComponents Vertical offset applied to the geometry components.
	 */
	void CreateGeometryComponentsForInstances(UInstancedStaticMeshComponent* InstStaticMesh, const float ZOffsetGeoComponents);

	// Maps static mesh assets to their corresponding geometry collection assets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	TMap<UStaticMesh*, UGeometryCollection*> StaticToGeometryMapping;

	// Distance threshold for switching from instanced to geometry representation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	float RangeSwitchToGeometry = 1000.f;

	/**
	 * @brief Sets up a hierarchical instance layout within a defined bounding box.
	 *
	 * Pre: Both InstStaticMesh and PopulateMesh must be valid.
	 * Post: The instanced static mesh is populated with instances arranged in a box layout and corresponding geometry components are created.
	 *
	 * @param InstStaticMesh The component to populate with instances.
	 * @param BoxBounds Dimensions defining the layout area.
	 * @param DistanceBetweenInstances Spacing between each instance.
	 * @param ZOffsetForGeoComponents Vertical offset applied to geometry components to prevent collisions.
	 */
	UFUNCTION(BlueprintCallable)
	void SetupHierarchicalInstanceBox(UInstancedStaticMeshComponent* InstStaticMesh,
	                                  FVector2D BoxBounds, const float DistanceBetweenInstances, const float ZOffsetForGeoComponents = 0.f);

private:
	// Associates each instanced static mesh component with its generated geometry components and mode state.
	UPROPERTY()
	TMap<UInstancedStaticMeshComponent*, FInstancedGeoMapping> InstanceToGeometryMap;
	
};
