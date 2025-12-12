// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ResourceSceneSetup/ResourceSceneSetup.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageOwner.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"


#include "Resource.generated.h"

struct FCollapseFX;
struct FCollapseForce;
struct FCollapseDuration;
class UGeometryCollection;
class UResourceComponent;
// This struct defines all prameters needed to setup a resource with attachments.
USTRUCT(BlueprintType)
struct FResourceAttachmentsSetup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UStaticMeshComponent* MainResourceMesh = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<UStaticMesh*> AttachableResourceMeshes = {};

	// Min generated amount of attachedmeshes
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MinAmountAttachments = 0;

	// Max generated amount of attachedmeshes
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxAmountAttachments = 0;

	// Multiplies with the scale of each attachment.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ScaleMultiplier = 1.f;
};


// ForwardDeclaration
class RTS_SURVIVAL_API ACPPController;


UCLASS()
class RTS_SURVIVAL_API ACPPResourceMaster : public AActorObjectsMaster, public IResourceStorageOwner
{
	GENERATED_BODY()

public:
	ACPPResourceMaster(const FObjectInitializer& ObjectInitializer);

	inline UResourceComponent* GetResourceComponent() { return ResourceComponent; }

	virtual void OnResourceStorageChanged(int32 PercentageResourcesFilled, const ERTSResourceType ResourceType) override;
	virtual void OnResourceStorageEmpty() override;

protected:

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupResourceCollisions(TArray<UMeshComponent*> ResourceMeshComponents, TArray<UGeometryCollectionComponent*> ResourceGeometryComponents) const;
	
	UFUNCTION(BlueprintImplementableEvent, Category="ResourceCapacity")
	void Bp_OnResourceStorageChanged(int32 PercentageResourcesFilled);

	
	UFUNCTION(BlueprintImplementableEvent, Category="ResourceCapacity")
	void BP_OnResourceStorageEmpty();
	
	virtual void PostInitializeComponents() override;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	UResourceComponent* ResourceComponent;

	/**
	 * Generates attachments as provided by the setup. Note that the sockets are found automatically.
	 * The "Core" socket is ignored as well as "Harvest" sockets are ignored.
	 * 
	 * @param AttachmentsSetup Defines how to setup the mesh attachments.
	 * @return The amount of attachments generated.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	int32 GenerateResourceAttachments(FResourceAttachmentsSetup AttachmentsSetup);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void AddToManuallyAddedMeshes(UStaticMeshComponent* MeshComponent);

	/** @brief Add random degrees of rotation to the component; relatively to the current scene component rotation.
	 * @return The chosen rotation. 
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	float SetCompRandomRotation(
		const float MinRotXY,
		const float MaxRotXY,
		const float MinRotZ,
		const float MaxRotZ,
		USceneComponent* Component);

	/** @brief Randomly, uniformly scale the component.
	 * @return The chosen scale.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	float SetCompRandomScale(
		const float MinScale,
		const float MaxScale,
		USceneComponent* Component);

	
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	float SetComponentScaleOnAxis(
		const float MinScale,
		const float MaxScale,
		const bool bScaleX,
		const bool bScaleY,
		const bool bScaleZ,
		USceneComponent* Component);

	/** @brief Add Radom relatlive offset on X and Y to the relative location of the component.
	 * @return The chosen offset.*/
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	float SetCompRandomOffset(
		const float MinOffsetXY,
		const float MaxOffsetXY,
		USceneComponent* Component);

	/**
	 * Picks random materials for each decal based on weighted chances.
	 * @return indices chosen from the provided array
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	TArray<int32> SetRandomDecalsMaterials(
		const TArray<FWeightedDecalMaterial>& DecalMaterials,
		TArray<UDecalComponent*> DecalComponents
	);

	/**
	 * Picks random meshes for each component based on weighted chances.
	 * @return indices chosen from the provided array
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	TArray<int32> SetRandomStaticMesh(
		const TArray<FWeightedStaticMesh>& Meshes,
		TArray<UStaticMeshComponent*> Components
	);


	/**
	 * @brief Calculates a resource amount based on how much larger or smaller the actual scale is
	 *        relative to a "base scale," then rounds the result to the nearest multiple.
	 * 
	 * Example:
	 *   - BaseScale = 2.0
	 *   - BaseWorth = 200 (the resource worth at scale = 2.0)
	 *   - CurrentScale = 2.5  => 25% bigger => raw worth = 250
	 *   - NearestMultiple = 5 => final = 250 (already multiple of 5)
	 *   - If it were 247 => final = 245 (round down), 248 => 250 (round up)
	 *
	 * @param BaseScale The reference scale at which BaseWorth is defined.
	 * @param BaseWorth The resource worth at BaseScale.
	 * @param CurrentScale The actual uniform scale of this resource.
	 * @param NearestMultiple Round the final amount to this multiple (e.g. 5, 10, 2, or 1).
	 * @return The final resource amount, scaled and rounded.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	int32 CalculateResourceWorthFromScale(
		float BaseScale,
		int32 BaseWorth,
		float CurrentScale,
		int32 NearestMultiple
	);


	// If set to true the constructor will no longer rebuild attachments.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bKeepCurrentAttachments = false;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	void CollapseMesh(
		UGeometryCollectionComponent* GeoCollapseComp,
		TSoftObjectPtr<UGeometryCollection> GeoCollection,
		UMeshComponent* MeshToCollapse,
		FCollapseDuration CollapseDuration,
		FCollapseForce CollapseForce,
		FCollapseFX CollapseFX
	);


private:
	TArray<TObjectPtr<UStaticMeshComponent>> AttachedMeshes;
	TArray<TObjectPtr<UStaticMeshComponent>> ManuallyAttachedMeshes;

	void ClearAttachments();

	int32 M_AmountOfAttachedMeshes;

void OnResourceStorageChangedNoMeshes(int32 PercentageResourcesFilled);

};
