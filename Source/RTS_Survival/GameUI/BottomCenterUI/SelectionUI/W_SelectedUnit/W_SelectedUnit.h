// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SelectedUnitWidgetState/SelectedUnitsWidgetState.h"
#include "W_SelectedUnit.generated.h"

class USlateBrushAsset;
class UButtonWidgetStyle;
class UImage;
class UButton;
class UW_SelectionPanel;

/**
 * @brief A tile representing one unit in the two-row selection panel.
 * @note SetupSelectedUnitWidget: called by the row to populate visuals/state.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SelectedUnit : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupSelectedUnitWidget(const FSelectedUnitsWidgetState& WidgetState);

	/** @brief Panel sets this at construction time. */
	void InitSelectionPanelOwner(UW_SelectionPanel* InOwner);

	/** @brief The most recent state assigned via SetupSelectedUnitWidget (cached for click handling). */
	const FSelectedUnitsWidgetState& GetCurrentState() const { return M_CurrentWidgetState; }

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* SelectedUnitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* SelectedUnitImage;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_SetupSelectedUnitWidget(const FSelectedUnitsWidgetState& WidgetState);
	
	/** @brief Bound from BP to the button's OnClicked. */
	UFUNCTION(BlueprintCallable)
	void OnClickedSelectedUnit();

	/** @brief Called from BP when the visual is updated; keeps our cached copy in sync. */
	UFUNCTION(BlueprintCallable)
	void CacheSelectedUnitState(const FSelectedUnitsWidgetState& NewState);

	// This widget represents the primary selected unit.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FButtonStyle M_PrimaryUnitBtnStyle;
	// This widget is not the primary selected unit but has the same type.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FButtonStyle M_PrimaryTypeBtnStyle;
	// This widget is a selected unit of not the same type as the primary selected unit.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FButtonStyle M_NotPrimary;

	UFUNCTION(BlueprintCallable)
	void SetImage(USlateBrushAsset* ImageBrushAsset);

private:
	/** Not owned here; panel lives in the widget tree. */
	UPROPERTY()
	TWeakObjectPtr<UW_SelectionPanel> M_SelectionPanelOwner;

	UPROPERTY()
	FSelectedUnitsWidgetState M_CurrentWidgetState;

};
