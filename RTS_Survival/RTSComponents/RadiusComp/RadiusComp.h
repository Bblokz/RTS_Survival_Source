// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RadiusComp.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URadiusComp : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URadiusComp();

	inline float GetRadius() const { return M_Radius; }

	/**
	 * @brief Shows the radius mesh, scaling it appropriately.
	 */
	UFUNCTION(BlueprintCallable)
	void ShowRadius();

	/**
	 * @brief Hides the radius mesh.
	 */
	UFUNCTION(BlueprintCallable)
	void HideRadius();

	/**
	 * @brief Initializes the radius component with the specified parameters.
	 * @param RadiusMesh The mesh this component will scale.
	 * @param Radius The total radius to show.
	 * @param UnitsPerScale The amount of units per scale of scaling the mesh.
	 * @param ZScale Scale always set for z on the mesh.
	 * @param RenderHeight To lift the mesh if nessasary.
	 */
	UFUNCTION(BlueprintCallable, Category="RadiusComp")
	void InitRadiusComp(UStaticMesh* RadiusMesh, const float Radius, const float UnitsPerScale, const float ZScale,
	                    const float RenderHeight);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void UpdateRadius(float Radius);


	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetEnabled(bool bIsEnabled);

	inline bool GetIsEnabled() const { return bM_IsEnabled; }

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Material")
	void SetNewMaterial(UMaterialInterface* NewMaterial);

	/**
	 * @brief Apply a scalar parameter for full circle materials that need the radius in cm.
	 * @param ParameterName Material parameter to set; must exist on the material.
	 * @param RadiusCm Radius in centimeters used by the material.
	 */
	void SetMaterialScalarParameter(FName ParameterName, float RadiusCm);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	/** The mesh component that displays the radius mesh. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RadiusMeshComponent;

private:
	void CreateRadiusMeshComponent();
	void UpdateMeshComponentTransform();

	/** The static mesh used to display the radius. */
	UPROPERTY()
	UStaticMesh* M_RadiusMesh;

	/** The desired radius to display. */
	float M_Radius;

	/** The units per scale factor. */
	float M_UnitsPerScale;


	float M_ZScale;

	float M_RenderHeight;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> M_Material;

	bool bM_IsEnabled;
};
