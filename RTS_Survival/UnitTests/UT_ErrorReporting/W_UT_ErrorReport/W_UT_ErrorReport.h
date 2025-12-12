// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_UT_ErrorReport.generated.h"

class UScrollBox;
class UW_UT_ErrorElement;
class AUT_ErrorReportManager;

/**
 * @brief Root widget that lists unit-test error items in a ScrollBox and proxies "MoveTo" to the manager.
 * Added to the viewport by AUT_ErrorReportManager at BeginPlay and kept interactive for mouse input.
 */
UCLASS()
class RTS_SURVIVAL_API UW_UT_ErrorReport : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Manager sets itself here right after creating the widget. */
	void SetErrorReportManager(AUT_ErrorReportManager* InManager);

	/** Add a new error entry to the scrollbox and optionally auto-scroll to the end. */
	/// @brief Adds a UI entry for a newly reported error.
	/// @param Message Error message text.
	/// @param WorldLocation Associated world location (used by the element's move button).
	void AddError(const FString& Message, const FVector& WorldLocation);

	/** Move the camera to a world-space location (relays to manager). */
	void MoveTo(const FVector& WorldLocation) const;

	/** Returns whether the manager reference is valid; logs standardized error when not. */
	bool GetIsValidErrorReportManager() const;

protected:
	virtual void NativeOnInitialized() override;

private:
	// ScrollBox that hosts all error elements (bind in BP as "ErrorScrolBox").
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ErrorScrolBox = nullptr;

	// Element class (non-soft). Derived from UW_UT_ErrorElement, configured on this widget's defaults.
	UPROPERTY(EditDefaultsOnly, Category = "Error Report")
	TSubclassOf<UW_UT_ErrorElement> M_ErrorElementClass;

	// Back-reference to the manager (not owned).
	UPROPERTY()
	TWeakObjectPtr<AUT_ErrorReportManager> M_ErrorReportManager;

	// Apply per-element placement rules and perform optional auto-scroll.
	void FinalizeNewElement(UW_UT_ErrorElement* NewElement);
};
