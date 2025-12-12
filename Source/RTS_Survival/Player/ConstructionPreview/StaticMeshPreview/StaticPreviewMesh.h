// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "StaticPreviewMesh.generated.h"


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

protected:
	virtual void BeginPlay() override;

private:
	/** The static mesh on which the preview is placed. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_PreviewMesh;

	// Name for the preview mesh component.
	static FName PreviewMeshComponentName;
	

};
