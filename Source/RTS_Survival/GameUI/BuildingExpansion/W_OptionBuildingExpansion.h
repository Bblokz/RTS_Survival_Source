// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WidgetBxpOptionState/BxpOptionData.h"
#include "W_OptionBuildingExpansion.generated.h"


class UW_BxpOptionHoverDescription;
struct FWidgetBxpOptionState;
class UImage;
class UButton;
class USizeBox;
class UMainGameUI;
enum class EBuildingExpansionType : uint8;

USTRUCT()
struct FOptionHoverData
{
	GENERATED_BODY()
	UPROPERTY()
	bool bIsBlockedByNoSockets = false;
	UPROPERTY()
	bool bIsBlockedByNotUnique = false;
	UPROPERTY()
	EBxpConstructionType ConstructionType = EBxpConstructionType::None;
};

/**
 * @brief a widget that represents a building expansion option in the UI.
 * Has a OnHover function which will ask the main game ui to switch to bottom right panel to bxp description of type
 * not built. When the hover ends we reset the bottom right to the action ui.
 */
UCLASS()
class RTS_SURVIVAL_API UW_OptionBuildingExpansion : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes the MainGameUI reference as well as the index that the widget occupies in the scrollbar.
	 * @note This item lives in the option scrollbar, in contrast to the W_ItemBuildingExpansion which lives in the item scrollbar.
	 * @param NewMainGameU The MainGameUI reference to set.
	 * @param NewIndex The index of the widget in the scrollbar.
	 */
	void InitW_OptionBuildingExpansion(UMainGameUI* NewMainGameU, const int NewIndex);


	void SetHoverDescription(UW_BxpOptionHoverDescription* NewHoverDescription);

	/**
	 * Called by main game UI to update the state of this button depending on the primary actor selected.
	 * @param BxpOptionWidgetState The type of the building expansion to display and the construction rules for it.
	 * @param bHasFreeSocketsRemaining
	 * @param bAlreadyHasBxpItemOfType
	 * @post Calls UpdateVisibleButtonState in blueprint to update the visibility in the UI.
	 */
	void UpdateOptionBuildingExpansion(const FBxpOptionData& BxpOptionWidgetState, const bool bHasFreeSocketsRemaining,
	                                   const bool bAlreadyHasBxpItemOfType);

protected:
	/**
	 * @brief Updates the visible state of the button in the blueprint by switching on the provided type.
	 * @param Type The type of the building expansion to display.
	 */
	UFUNCTION(blueprintImplementableEvent, Category = "BuildingExpansion")
	void UpdateVisibleButtonState(EBuildingExpansionType Type);

	/** Update the main game UI with the click and the type and index saved in this button. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "BuildingExpansion")
	void OnClickedBuildingExpansionOption();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "BxpDescription")
	void OnHoverBxpOption(const bool bHover);

	// The style for the button.
	UPROPERTY(EditAnywhere, Category = "Style")
	USlateWidgetStyleAsset* ButtonStyleAsset;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void UpdateButtonWithGlobalSlateStyle();

	// The size box that is the parent of all elements in this widget.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	USizeBox* M_BxpOptionSizeBox;

	// The button of this widget that is underneath the size box in the hierarchy.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UButton* M_BxpOptionButton;

	// The image of this widget that is a child of the button.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UImage* M_BxpOptionImage;

	UPROPERTY(BlueprintReadOnly)
	UW_BxpOptionHoverDescription* HoverDescription;

private:
	UPROPERTY()
	UMainGameUI* M_MainGameUI;

	bool EnsureMainGameUIIsValid() const;

	void UpdateHoverDescription(const bool bHover);
	bool EnsureIsValidHoverDescriptionWidget();

	int M_WidgetScrollbarIndex;

	// Contains the type of bxp as well as the rules for building this bxp.
	FBxpOptionData M_BxpOptionWidgetState;

	FOptionHoverData M_OptionHoverData;

	bool bM_IsDisabled = false;
};
