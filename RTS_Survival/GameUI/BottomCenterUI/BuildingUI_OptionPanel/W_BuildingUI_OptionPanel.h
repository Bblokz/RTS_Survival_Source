// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/BottomCenterUI/ChildPanels/W_BottomCenter_ChildPanel.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "W_BuildingUI_OptionPanel.generated.h"

class UW_BxpOptionHoverDescription;
class UVerticalBox;
class UW_OptionBuildingExpansion;

/**
 * @brief Container for Building Expansion options split in Tech/Economic/Defense sections.
 * @note InitArray: call in blueprint to set the flat list of all option widgets (existing).
 * @note InitSectionArrays: call in blueprint to assign which option widgets live in each section.
 */
UCLASS()
class RTS_SURVIVAL_API UW_BuildingUI_OptionPanel : public UW_BottomCenter_ChildPanel
{
	GENERATED_BODY()

public:
	void CloseOptionUI();
	void PlayOpenAnimation();
	

	/** @return Flat list for legacy access; reports if uninitialized. */
	TArray<UW_OptionBuildingExpansion*> GetBxpOptions() const;

	/**
	 * @brief Initializes the per-section slots. Call once from BP after creating children.
	 * @param TechOptions All option widgets that physically live under TechOptionsBox.
	 * @param EconomicOptions All option widgets that physically live under EconomicOptionsBox.
	 * @param DefenseOptions All option widgets that physically live under DefenseOptionsBox.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitSectionArrays(
		TArray<UW_OptionBuildingExpansion*> TechOptions,
		TArray<UW_OptionBuildingExpansion*> EconomicOptions,
		TArray<UW_OptionBuildingExpansion*> DefenseOptions);

	/** @return Slots for a section (may be empty). */
	TArray<UW_OptionBuildingExpansion*> GetSectionSlots(const EBxpOptionSection Section) const;

	/** Collapses or shows the section’s vertical box. */
	void SetSectionCollapsed(const EBxpOptionSection Section, const bool bCollapsed);
	void HideHoverDescription() const;

	UW_BxpOptionHoverDescription* GetHoverDescription() const { return HoverDescription; }

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitArray(TArray<UW_OptionBuildingExpansion*> BxpOptions);

	// Controls the visibility of the entire tech options section of bxp options.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* TechVerticalBox;

	// Contains all the UW_OptionBuildingExpansion widgets that are in the tech section.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* TechOptionsBox;

	// Controls the visibility of the entire Economic options section of bxp options.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* EconomicVerticalBox;

	// Contains all the UW_OptionBuildingExpansion widgets that are in the economics section.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* EconomicOptionsBox;

	// Controls the visibility of the entire Defense options section of bxp options.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* DefenseVerticalBox;

	// Contains all the UW_OptionBuildingExpansion widgets that are in the Defense section.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* DefenseOptionsBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_BxpOptionHoverDescription* HoverDescription;

	UPROPERTY(BlueprintReadWrite)
	UWidgetAnimation* Anim_PanelOpen = nullptr;
	

private:
	// Flat legacy cache
	UPROPERTY()
	TArray<TObjectPtr<UW_OptionBuildingExpansion>> M_BxpOptions;

	// Per-section slot caches
	UPROPERTY()
	TArray<TObjectPtr<UW_OptionBuildingExpansion>> M_TechOptionSlots;

	UPROPERTY()
	TArray<TObjectPtr<UW_OptionBuildingExpansion>> M_EconomicOptionSlots;

	UPROPERTY()
	TArray<TObjectPtr<UW_OptionBuildingExpansion>> M_DefenseOptionSlots;



	bool GetIsValidSectionBoxes() const;
};
