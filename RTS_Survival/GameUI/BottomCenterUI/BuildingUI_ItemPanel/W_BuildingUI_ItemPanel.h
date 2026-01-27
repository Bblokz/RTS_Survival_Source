// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetSwitcher.h"
#include "RTS_Survival/GameUI/BottomCenterUI/BuildingUISettings/BuildingUISettings.h"
#include "RTS_Survival/GameUI/BottomCenterUI/ChildPanels/W_BottomCenter_ChildPanel.h"
#include "RTS_Survival/Player/GameUI_Utils/AGameUIController.h"
#include "W_BuildingUI_ItemPanel.generated.h"

class UW_ItemBuildingExpansion;
enum class EActionUINomadicButton : uint8;
class UW_BottomCenterUI;
class ACPPController;
class UButton;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_BuildingUI_ItemPanel : public UW_BottomCenter_ChildPanel
{
	GENERATED_BODY()

public:
	/**
	 * @post The CancelBuilding button is shown instead of the construct building button.
	 */
	void ShowCancelBuilding() const;

	/**
	 * @post The construct building button is shown.
	 */
	void ShowConstructBuilding() const;

	/**
	 * The convert to vehicle button is shown.
	 */
	void ShowConvertToVehicle() const;

	/**
	 * @post The CancelBuilding button is shown instead of the construct building button.
	 */
	void ShowCancelVehicleConversion() const;

	void DetermineNomadicButton(const EActionUINomadicButton NomadicButtonState,
	                            ENomadicSubtype NomadicSubtype);


	TArray<UW_ItemBuildingExpansion*> GetBxpItems() const;

protected:
	// Controls the UI for converting to building, truck or cancelling those actions.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWidgetSwitcher* WS_CreateCancelBuilding;

	UFUNCTION(BlueprintCallable)
	void InitArray(TArray<UW_ItemBuildingExpansion*> Items);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_SetNomadicButtonStyleForType(const ENomadicSubtype NomadicSubtype);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CreateBuildingButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CancelBuildingButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* ConvertToVehicleButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CancelVehicleConversionButton;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FBuildingUISettings BuildingUISettings;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickedCancelBuildingButton();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickedCreateBuildingButton();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickedConvertToVehicleButton();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickedCancelVehicleConversionButton();

private:
	bool GetIsValidCreateBuildingButton() const;
	bool GetIsValidCancelBuildingButton() const;
	bool GetIsValidConvertToVehicleButton() const;
	bool GetIsValidCancelVehicleConversionButton() const;

	UPROPERTY()
	TArray<UW_ItemBuildingExpansion*> M_BxpItems;
};
