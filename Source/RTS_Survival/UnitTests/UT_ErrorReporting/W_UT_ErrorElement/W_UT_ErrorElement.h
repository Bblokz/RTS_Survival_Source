// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "W_UT_ErrorElement.generated.h"

class URichTextBlock;
class UButton;
class UScrollBox;
class UScrollBoxSlot;
class UW_UT_ErrorReport;

USTRUCT(BlueprintType)
struct FUT_ErrorElementPlacementRules
{
	GENERATED_BODY()

	/** Padding applied to this element inside the parent ScrollBox slot. */
	UPROPERTY(EditAnywhere, Category = "Layout")
	FMargin Padding = FMargin(4.f, 2.f);

	/** Horizontal alignment within the ScrollBox slot. */
	UPROPERTY(EditAnywhere, Category = "Layout")
	TEnumAsByte<EHorizontalAlignment> HAlign = HAlign_Fill;

	/** Vertical alignment within the ScrollBox slot. */
	UPROPERTY(EditAnywhere, Category = "Layout")
	TEnumAsByte<EVerticalAlignment> VAlign = VAlign_Center;

	/** If true, the report list will auto-scroll to the end after adding this element. */
	UPROPERTY(EditAnywhere, Category = "Behavior")
	bool bAutoScrollOnAdd = true;
};

/**
 * @brief Single error line item used inside the unit-test error report list.
 * Holds the formatted rich text and a "Move To" button to pan the player camera.
 * @note OnMoveToClicked: bound to MoveToButton to request a camera move to the stored world location.
 */
UCLASS()
class RTS_SURVIVAL_API UW_UT_ErrorElement : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Initialize the element after it was added to the ScrollBox. Sets text and caches the location. */
	/// @brief Initialize the element content and cached world location.
	/// @param InMessage Message to display in the rich text block.
	/// @param InWorldLocation World-space location associated with this error.
	void InitErrorElement(const FString& InMessage, const FVector& InWorldLocation);

	/** Called by the creator to set the owning report widget. */
	void SetOwningReport(UW_UT_ErrorReport* InReport);

	/** Apply the placement rules to our parent ScrollBox slot. Caller should invoke after the widget is added. */
	void ApplyPlacementRulesToParentSlot() const;

	/** Blueprint binds the button click to this; it relays the move request to the owning report. */
	UFUNCTION(BlueprintCallable, Category = "ErrorElement")
	void OnMoveToClicked();

	/** Returns whether the owning report is valid; logs standardized error when not. */
	bool GetIsValidOwningReport() const;

protected:
	virtual void NativeOnInitialized() override;

private:
	/** Rich text displaying the error message. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> ErrorRichText = nullptr;

	/** Button that, when clicked, asks to move the camera to the cached location. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MoveToButton = nullptr;

	// Layout/behavior rules configured on this widget asset's defaults in BP.
	UPROPERTY(EditDefaultsOnly, Category = "Error Element|Layout")
	FUT_ErrorElementPlacementRules M_PlacementRules;

	// Owning report reference (not owned). Needed to propagate "move to" requests.
	UPROPERTY()
	TWeakObjectPtr<UW_UT_ErrorReport> M_OwningReport;

	// Cached location associated with this error item.
	UPROPERTY()
	FVector M_ErrorLocation = FVector::ZeroVector;
};
