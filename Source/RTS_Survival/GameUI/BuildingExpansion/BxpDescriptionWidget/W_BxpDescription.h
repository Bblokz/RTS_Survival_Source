// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "W_BxpDescription.generated.h"

class UW_CostDisplay;
class UMainGameUI;
/**
 * Displays text and cost of the bxp clicked.
 * Make sure to hide other bottom panel widgets in the main menu, do not use SetBxpDescriptionVisiblity but
 * use UMainGameUI::SetBottomUIPanel to regulate which widget will be displayed.
 * @note this widget is at the same place as the TrainingDescription And ActionUI.
 */
UCLASS()
class RTS_SURVIVAL_API UW_BxpDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetBxpDescriptionVisibility(
		const bool bVisible,
		const EBuildingExpansionType BxpType = EBuildingExpansionType::BXT_Invalid,
		const bool bIsBuilt = false);

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnUpdateBxpDescription(const EBuildingExpansionType BxpType, const bool bIsBuilt);

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	UW_CostDisplay* CostDisplay;



};
