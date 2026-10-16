// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "StaticPreviewMesh.generated.h"

class UBoxComponent;


/**
 * @brief A class to place a static mesh on which the preview is placed.
 * This is used when the construction preview location is determined.
 */
UCLASS()
class RTS_SURVIVAL_API AStaticPreviewMesh : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AStaticPreviewMesh(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Sets the static mesh of the preview.
	 * @param NewPreviewMesh The mesh that will be displayed.
	 * @param PreviewMaterial The material that will be displayed on all of the material slots.
	 */
	void SetPreviewMesh(UStaticMesh* NewPreviewMesh, UMaterialInstance* PreviewMaterial);

	/**
	 * @brief Reserves a world aligned area for building placement, so a planned footprint stays blocked while its
	 * construction waits; the area keeps its world rotation whatever the yaw of the preview.
	 * @param FootprintArea The world XY area to reserve.
	 * @param CenterHeight The world height around which the area is reserved.
	 */
	void BlockPlacementFootprint(const FBox2D& FootprintArea, const float CenterHeight);

protected:
	virtual void BeginPlay() override;

private:
	/** The static mesh on which the preview is placed. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_PreviewMesh;

	// Only made for previews that reserve a footprint; the grid overlay of other previews treats it as occupied.
	UPROPERTY()
	TObjectPtr<UBoxComponent> M_FootprintBlocker;

	// Name for the preview mesh component.
	static FName PreviewMeshComponentName;
	

};
