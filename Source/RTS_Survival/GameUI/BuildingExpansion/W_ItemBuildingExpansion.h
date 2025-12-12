// Copyright Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BxpItemWidgetState/BxpItemWidgetState.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "W_ItemBuildingExpansion.generated.h"

class UW_BxpDescription;
class UImage;
class UButton;
class USizeBox;
class UMainGameUI;
enum class EBuildingExpansionStatus : uint8;
enum class EBuildingExpansionType : uint8;

/**
 * @brief This widget represents a building expansion item in the UI.
 * The amount of items in the scrollbar is determined by the primary selected actor.
 * The status is updated directly from the bxp which requests
 */
UCLASS()
class RTS_SURVIVAL_API UW_ItemBuildingExpansion : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes the MainGameUI reference as well as the index that the widget occupies in the scrollbar.
	 * @note This item live in the item scrollbar, in contrast to the W_OptionBuildingExpansion which lives in the option scrollbar.
	 * @param NewMainGameU The MainGameUI reference to set.
	 * @param NewIndex The index of the widget in the scrollbar.
	 */
	void InitW_ItemBuildingExpansion(UMainGameUI* NewMainGameU, const int NewIndex);

	/**
	 * @brief Called by main game UI to update the state of this button depending on the primary actor selected.
	 * @param Type The type of the building expansion to display.
	 * @param Status The construction/building state of the expansion which alters how the buttons looks as well.
	 * @param ConstructionRules
	 * @post Calls UpdateVisibleButtonState in blueprint to update the visibility in the UI.
	 */
	void UpdateItemBuildingExpansionData(EBuildingExpansionType Type, EBuildingExpansionStatus Status, const FBxpConstructionRules& ConstructionRules);

	inline TPair<EBuildingExpansionType, EBuildingExpansionStatus> GetItemBuildingExpansionData() const
	{
		return TPair<EBuildingExpansionType, EBuildingExpansionStatus>(M_StatusTypeAndRules.M_Type,
			M_StatusTypeAndRules.M_Status);
	}

	/**
	 * Change the button to disabled or enabled state.
	 * @param bEnabled Determines if the button is enabled.
	 */
	void EnableDisableItem(const bool bEnabled);

protected:
	
	/**
	 * @brief Updates the visible state of the button in the blueprint by switching on the provided type.
	 * @param Type The type of the building expansion.
	 * @param Status The status of the building expansion.
	 * @note bp callable so when a button is re enabled we can use this function to update the visibility.
	 */
	UFUNCTION(blueprintImplementableEvent, BlueprintCallable, Category = "BuildingExpansion")
	void UpdateVisibleButtonState(EBuildingExpansionType Type, EBuildingExpansionStatus Status);

	/** Update the main game UI with the click and the type, status and index saved in this button. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "BuildingExpansion")
	void OnClickedItemBuildingExpansion() const;

	/** @brief Implemented in blueprint instance; disables the button. */
	UFUNCTION(BlueprintImplementableEvent)
	void OnItemDisabled(EBuildingExpansionStatus Status);

	/**
	 * @brief Implemented in blueprint instance; enables the button with either build, packup or deploy or cancel
	 * depending on the status.
	 * @param Status The whether the bxp is built, packed or needs to be build.
	 * @param Type The type of the building expansion.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnItemEnabled(const EBuildingExpansionStatus Status, const EBuildingExpansionType Type);

	// The style for the button.
	UPROPERTY(EditAnywhere, Category = "Style")
	USlateWidgetStyleAsset* ButtonStyleAsset;

	/**
	 * @brief Updates the look of the button used by this item using a reference to the slate set in blueprint.
	 * called at init of this widget.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void UpdateButtonWithGlobalSlateStyle() const;

	// The size box that is the parent of all elements in this widget.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	USizeBox* M_BxpItemSizeBox;

	// The button of this widget that is underneath the size box in the hierarchy.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UButton* M_BxpItemButton;

	// The image of this widget that is a child of the button.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UImage* M_BxpItemImage;

private:
	// Uproperty; makes use of GC.
	UPROPERTY()
	UMainGameUI* M_MainGameUI;
	bool EnsureMainGameUIIsValid()const;

	int M_WidgetScrollbarIndex;

	// Contains the status and type of the bxp of this item. Also contains the construction rules
	// and possibly attached socket if the bxp is packed up and needs to be placed again.
	 FBxpItemWidgetState M_StatusTypeAndRules;

	UPROPERTY()
	TObjectPtr<UW_BxpDescription> M_BxpDescription;
};
