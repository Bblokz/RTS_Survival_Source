// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_BxpDescription.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/CostWidget/W_CostDisplay.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

void UW_BxpDescription::SetBxpDescriptionVisibility(const bool bVisible, const EBuildingExpansionType BxpType,
                                                    const bool bIsBuilt)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	SetVisibility(NewVisibility);
	if (bVisible)
	{
		OnUpdateBxpDescription(BxpType, bIsBuilt);
		if (IsValid(CostDisplay))
		{
			CostDisplay->SetupCost(URTSBlueprintFunctionLibrary::BP_GetPlayerBxpData(BxpType, this).Cost.ResourceCosts);
		}
	}
}
