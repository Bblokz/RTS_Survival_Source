// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Resources/HarvesterPosition/HarvestLocation.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageOwner.h"
#include "ResourceComponent.generated.h"


class UHarvester;
class UNavigationSystemV1;
enum class ERTSResourceType : uint8;

UENUM(BlueprintType)
enum class EGatherType : uint8
{
	Gather_None,
	Gaher_Instant,
	Gather_Harvest
};

// A wrapper for the amount to harvest on a resource and why type of resource it is.
// Expects the owner of this component to implement the IResourceStorageOwner Interface.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UResourceComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class RTS_SURVIVAL_API ACPPResourceMaster;

public:
	// Sets default values for this component's properties
	UResourceComponent();

	/**
	 * @brief Sets the provided location as occupied by a harvester if bIsOccupied is true, this means that
	 * when the harvester will look for a valid harvesting location this location will block new harvester as it is in use.
	 *
	 * @brief When bIsOccupied is false the location is freed up for other harvesters to use.
	 * @param HarvestingPosition The location the harvester is planning to harvest from.
	 * @param bIsOccupied Whether the harvester is still using the location.
	 */
	void RegisterOccupiedLocation(const FHarvestLocation& HarvestingPosition, const bool bIsOccupied);

	UFUNCTION(BlueprintCallable, notblueprintable)
	inline bool StillContainsResources() const { return TotalAmount > 0; }

	int32 GetTotalAmount() const { return TotalAmount; }

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline ERTSResourceType GetResourceType() const { return ResourceType; }

	/** @return The amount of resources harvested. */
	int32 HarvestResource(const int32 MaxAmountHarvesterCapacity);

	/**
	 * @brief Gets all currently vacant harvest directions and computes harvest locations for them based on
	 * resource postion + radius * direction where radius + ResourceInnerRadius + HarvesterRadius.
	 * @param HarvesterLocation The location of the harvester.
	 * @param RequestingHarvester The harvester that is requesting the location.
	 * @param HarvesterRadius The radius of the harvester.
	 * @param OutHarvestLocation The location to harvest from.
	 *
	 * @return True if a vacant position was found that could be Projected to the navmesh. False otherwise.
	 */
	bool GetHarvestLocationClosestTo(const FVector& HarvesterLocation,
	                                 UHarvester* RequestingHarvester,
	                                 const float HarvesterRadius,
	                                 FHarvestLocation&
	                                 OutHarvestLocation);


	/** @return If any harvester sockets are available; return the first one's location.
	 * Otherwise project the resource location to the navmesh and return the projected vector.
	 */
	FVector GetResourceLocationNotThreadSafe();

	bool IsResourceFullyOccupiedByHarvesters() const;

	inline int32 GetMaxHarvesters() const { return M_MaxHarvesters; }
	
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	int32 GetCurrentHarvesters() const;

protected:
	/**
	 * @brief Setup the mesh and sockets used for harvesters to navigate to.
	 * @param MaxHarvesters The maximum amount of harvesters allowed on this resource.
	 * @param ResourceInnerRadius The inner radius of the resource, used to generate harvest locations. 
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Constructor")
	void InitResourceComponent(
		const int32 MaxHarvesters, const float ResourceInnerRadius = 200);

	// The type of resource this object represents, needs to be set before begin play
	// so the registration with the gamestate can be done correctly.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	ERTSResourceType ResourceType;

	// Called when the game starts
	virtual void BeginPlay() override;

	// How much one harvesting operation can maximally yield from this resource.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 AmountPerHarvest;

	// How many resources there are left.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 TotalAmount;

	// Defines how the resource is havested.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	EGatherType GatherType;

private:
	// Saves this resource in UGameResourceManager.
	void RegisterWithGameResourceManager();



	UPROPERTY()
	TScriptInterface<IResourceStorageOwner> M_ResourceOwner = nullptr;

	/**
	 * @param ONLY CALL AT BEGINPLAY
	 * @param FallBackLocation The location to use if the owner is not valid.  
	 * @return The location of the owner projected to the navigation mesh.
	 */
	FVector GetOwnerLocation(const FVector& FallBackLocation) const;

	// How much of the resource was originally stored.
	int32 M_OriginalTotalAmount;

	/**
	 * @brief Determines the closest socket and its location and returns the array of sockets
	 * ordered from closest to furthest from the provided harvester location.
	 * @param OutHarvesterPosition A TMap of socket name -> projected location.
	 * @return The array of sockets sorted by distance to the harvester position.
	 */
	bool GetHarvesterPositionProjected(
		FHarvestLocation& OutHarvesterPosition) const;


	void DebugPercentage(const int32 PercentageLeft);

	// Maximum of harvester allowed on the resource.
	int32 M_MaxHarvesters;

	float M_ResourceInnerRadius = 200.f;

	UPROPERTY()
	FVector M_CachedOwnerLocation;

	UPROPERTY()
	UNavigationSystemV1* M_NavigationSystem = nullptr;

	// Keeps track of which directions are occupied by harvesters.
	TMap<EHarvestLocationDirection, bool> M_IsHarvestLocationDirectionTaken;
	// Keeps track of the direction vectors for each direction that is allowed on this resource for
	// generating harvesting positions.
	TMap<EHarvestLocationDirection, FVector> M_DirectionVectors;
	void InitializeHarvesterPositionsAsVacant(const int32 MaxHarvesters);
	void InitializeDirectionVectors();

	/** @return All the directions possible on this resource that are marked as vacant. */
	TArray<EHarvestLocationDirection> GetVacantDirections() const;

	TArray<FHarvestLocation> GenerateHarvestLocations(const float HarvesterRadius,
	                                                  const TArray<EHarvestLocationDirection>& PositionDirections,
	                                                  const FVector& HarvesterLocation);

	void DebugGeneratedLocations(const TArray<FHarvestLocation>& GeneratedLocations);
};
