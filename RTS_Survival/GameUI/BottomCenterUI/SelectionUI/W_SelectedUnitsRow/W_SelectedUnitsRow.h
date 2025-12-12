// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_SelectedUnit/SelectedUnitWidgetState/SelectedUnitsWidgetState.h"
#include "W_SelectedUnitsRow.generated.h"

class UW_SelectedUnit;
class UW_EnumerateUnitSelection;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_SelectedUnitsRow : public UUserWidget
{
	GENERATED_BODY()

public:
	// Set selected unit widgets array before main game UI init here
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void InitArraysBeforeMainGameUIInit();

	float GetAmountSelectedUnitsWidgets() const { return SelectedUnitsWidgets.Num(); }


		void SetupRowForSelectedUnits(TArray<FSelectedUnitsWidgetState> WidgetStatesRow);
    
    	/** Hide all tiles beyond VisibleCount. */
    	void HideUnusedTilesFromIndex(const int32 VisibleCount);
    
    	/** Number of tile slots in this row. */
    	int32 GetCapacity() const { return SelectedUnitsWidgets.Num(); }
    
    	/** @return Button 1/2 (bound in BP). */
    	UW_EnumerateUnitSelection* GetEnumerateButton1() const { return EnumerateButton1; }
    	UW_EnumerateUnitSelection* GetEnumerateButton2() const { return EnumerateButton2; }
    
    	/** Panel sets itself on every tile for click propagation. */
    	void InitTilesOwner(class UW_SelectionPanel* OwnerPanel);

protected:
	// An enumeration button widget that defines which units are displayed in the rows of the selection panel.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_EnumerateUnitSelection* EnumerateButton1;

	// the second enumeration button widget that defines which units are displayed in the rows of the selection panel.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_EnumerateUnitSelection* EnumerateButton2;

	UPROPERTY(BlueprintReadWrite)
	TArray<UW_SelectedUnit*> SelectedUnitsWidgets;

	UW_SelectedUnit* GetIsValidSelectedUnitWidget_AtIndex(const int32 Index);

	private:
    	void ApplyStateToTile(UW_SelectedUnit* Tile, const FSelectedUnitsWidgetState& State) const;
};
