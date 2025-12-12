// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/DecalComponent.h"
#include "SelectionComponent.generated.h"

// Declare multicast delegates for selection and deselection.
DECLARE_MULTICAST_DELEGATE(FOnUnitSelected);
DECLARE_MULTICAST_DELEGATE(FOnUnitDeselected);
DECLARE_MULTICAST_DELEGATE(FOnUnitHovered);
DECLARE_MULTICAST_DELEGATE(FOnUnitUnHovered);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPrimarySameTypeChanged, bool /*bIsInPrimarySameType*/);

class UBoxComponent;
/**
 * @brief Container for SelectionArea and selection decals and associated parameters.
 * Allows for deselected decal to be null.
 * @note SET IN BP
 * @note InitSelectionComponent
 */ 
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RTS_SURVIVAL_API USelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USelectionComponent(const FObjectInitializer& ObjectInitializer);

	// Delegate called when a unit is selected.
	FOnUnitSelected OnUnitSelected;
	
	// Delegate called when a unit is deselected.
	FOnUnitDeselected OnUnitDeselected;

	// Delegate called when a unit is hovered.
	FOnUnitHovered OnUnitHovered;

	// Delegate called when a unit is unhovered.
	FOnUnitUnHovered OnUnitUnHovered;

	/** Fired whenever this unit is (or is no longer) part of the PrimarySelectedSameType group. */
    FOnPrimarySameTypeChanged OnPrimarySameTypeChanged;

	// Sets whether this unit shares the same unit type with the primary selected unit type.
	void SetIsInPrimarySameType(const bool bIsInPrimarySameType);

	bool GetIsInPrimarySameType() const;

	void OnUnitHoverChange(const bool bHovered) const;

	inline UBoxComponent* GetSelectionArea() const { return M_SelectionArea;}

	UFUNCTION(BlueprintCallable)
	inline bool GetIsSelected() const {return bM_IsSelected;}

	UFUNCTION(BlueprintCallable)
	inline bool GetCanBeSelected() const {return bM_CanBeSelected;}

	UFUNCTION(BlueprintCallable)
	void SetCanBeSelected(bool bnewCanBeSelected) {bM_CanBeSelected = bnewCanBeSelected;}

	/**
	 * @brief Update this seletion component's decal settings.
	 * @param bNewUseDeselectedDecal Whether to use the deselected decal.
	 */
	UFUNCTION()
	void SetDeselectedDecalSetting(const bool bNewUseDeselectedDecal);

	void UpdateSelectionArea(const FVector& NewSize, const FVector& NewPosition) const;

	/**
	 * @brief Sets selection flag and makes the SelectionDecal visible.
	 */ 
	void SetUnitSelected();

	/**
	 * @brief Sets selection flag and makes the DeselectionDecal visible.
	 */
	UFUNCTION(BlueprintCallable)
	void SetUnitDeselected();

	// Hides both the selected and deselected decals.
	UFUNCTION(BlueprintCallable)
	void HideDecals();

	/**
	 * @brief Updates the selection materials during runtime.
	 * @param NewSelectedMaterial The new material used when the unit is selected.
	 * @param NewDeselectedMaterial The new material used when the unit is deselected.
	 * @post the cached materials are updated and the decal uses the new material.
	 */
	void UpdateSelectionMaterials(UMaterialInterface* NewSelectedMaterial, UMaterialInterface* NewDeselectedMaterial);

	TPair<UMaterialInterface*, UMaterialInterface*> GetMaterials() const;
	
	void SetDecalRelativeLocation(const FVector NewLocation);
	void UpdateDecalScale(const FVector NewScale);

	UDecalComponent* GetSelectedDecal() const {return M_SelectedDecalRef;}
	

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	/**
	 * 
	 * @param NewSelectionArea The area in which the unit can be selected.
	 * @param NewSelectedDecal The selection decal of the unit; needs to be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "ReferenceCasts")
	void InitSelectionComponent(
		UBoxComponent* NewSelectionArea,
		UDecalComponent* NewSelectedDecal);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Material")
	UMaterialInterface* SelectedMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Material")
	UMaterialInterface* DeselectedMaterial;

	
private:
	
	bool bM_IsSelected;
	bool bM_CanBeSelected;

	// Whether this unit shares the same unit type with the primary selected unit type.
	UPROPERTY()
	bool bM_IsPrimarySelectedSameType = false;
	
	/**
	 * @brief Makes the SelectedDecal visible and hides the DeselectedDecal.
	 */
	void SetDecalSelected() const;
	
	/**
	 * @brief Makes the DeselectedDecal visible and hides the DeselectedDecal.
	 */
	void SetDecalDeselected() const;

	bool bM_UseDeselectDecal = false;

	// Set with init function; area in which the unit can be selected.
	UPROPERTY()
	UBoxComponent* M_SelectionArea;
	
	// Set with init function; the decal that is visible when the unit is selected.
	UPROPERTY()
	UDecalComponent* M_SelectedDecalRef;
	
		
};
