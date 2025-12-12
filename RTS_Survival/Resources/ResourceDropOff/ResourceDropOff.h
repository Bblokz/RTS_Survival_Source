// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Resources/HarvesterCargoSlot/HarvesterCargoSlot.h"

#include "ResourceDropOff.generated.h"


class UPlayerResourceManager;
class IResourceStorageOwner;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UResourceDropOff : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UResourceDropOff();

	/**
	 * Attempts to drop off as much of the provided amount of the provided resource type as possible.
	 * @param ResourceType What resource is being dropped off
	 * @param Amount The amount to maximally drop off.
	 * @return The amount left in the harvester after dropping off as much as possible.
	 */
	int32 DropOffResources(ERTSResourceType ResourceType, int32 Amount);

	/** @brief Provide the type and amount of the resource to add, this will add the amount to the
	 * currently stored resources.
	 * @post Updates visual of resources stored.
	 * @note Note Not used for harvesting but from resources added from elsewhere.
	 */
	void AddResourcesNotHarvested(ERTSResourceType ResourceType, int32 Amount);

	/**
	 * @brief Adds the capacity for the provided resource type and optionally also adds the amount.
	 * @param ResourceType The type of resource to add capacity for.
	 * @param Amount The amount of extra capacity / resource amount stored.
	 * @param bAlsoAddAmount if true add the capacity and the amount. if false only upgrade the capacity.
	 * @note bAlsoAddAmount is set to true for the card system; as cards provide both resources to start with as well as
	 * their capacity.
	 * @post The visuals of the resources stored are updated.
	 */
	void UpgradeDropOffCapacity(const ERTSResourceType ResourceType, const int32 Amount,
	                            const bool bAlsoAddAmount = false);

	/**
	 * @brief Remove the provided amount of the provided resource type from the drop off.
	 * @post Updates visuals of resources stored.
	 * @param ResourceType The type of resource to remove.
	 * @param Amount How much to remove.
	 */
	void RemoveResources(ERTSResourceType ResourceType, int32 Amount);

	/** @return The drop off location. NOT THREAD SAFE! */
	FVector GetDropOffLocationNotThreadSafe() const;

	/** @return Whether the drop off has capcity left for the specified amount. */
	bool GetHasCapacityForDropOff(ERTSResourceType ResourceType, int32 Amount) const;

	/** @return Get the amount of capacity that is left for the provided resource.
	 * @param ResourceType The type of resource to get the capacity for.*/
	int32 GetDropOffCapacity(ERTSResourceType ResourceType) const;

	/** @return Get the amount of resource that is stored in the drop off. */
	int32 GetDropOffAmountStored(ERTSResourceType ResourceType) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Resources")
	TMap<ERTSResourceType, FHarvesterCargoSlot> GetResourceDropOffCapacity() const;

	/** @return The array of resources types that are in the capacity of this drop off.
	 * @note does NOT check if the capacity is full or not. 
	 */
	TArray<ERTSResourceType> GetResourcesSupported() const;
	inline bool GetIsDropOffActive() const { return bIsDropOffActive; }

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline void SetIsDropOffActive(const bool IsActive) { bIsDropOffActive = IsActive; }

protected:
	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitResourceDropOff(
		TMap<ERTSResourceType, int32> MaxDropOffPerResource,
		UMeshComponent* OwnerMeshComponent,
		FName DropOffSocketName, const float DropOffTextZOffset = 100);

private:
	bool bIsDropOffActive;
	TMap<ERTSResourceType, FHarvesterCargoSlot> M_ResourceDropOffCapacity;

	void RegisterWithGameResourceManager();
	void DeRegisterFromGameResourceManager();
	void DebugDroppingOff(const ERTSResourceType ResourceType, const int32 Amount);

	UPROPERTY()
	UMeshComponent* M_OwnerMeshComponent;

	FName M_DropOffSocketName;

	TWeakInterfacePtr<IResourceStorageOwner> M_ResourceStorageOwner;

	UPROPERTY()
	TObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;

	int8 GetOwningPlayer() const;

	float M_DropOffTextZOffset = 100;

	/**
	 * @brief Provide the resource type for which the DropOff needs to update its visuals.
	 * @param ResourceTypeChanged The type of resource for which the capacity or amount or both was changed
	 */
	void UpdateDropOffVisuals(const ERTSResourceType ResourceTypeChanged);

	void CreateTextOfDropOff(const ERTSResourceType ResourceType, const int32 Amount) const;
};
