// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSMinimapIconHelpers.h"

#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"

namespace FRTSMinimapIconHelpers
{
	float GetMiniMapIconSizePixels(const AFowManager& FowManager,
	                               const AActor* OwnerActor,
	                               const EAllUnitType UnitType,
	                               const uint8 UnitSubtype)
	{
		if (UnitType == EAllUnitType::UNType_BuildingExpansion)
		{
			return FowManager.GetMiniMapMediumIconSizePixels();
		}

		if (UnitType == EAllUnitType::UNType_Nomadic)
		{
			return FowManager.GetMiniMapLargeIconSizePixels();
		}

		return FowManager.GetMiniMapSmallIconSizePixels();
	}

	ERTSMinimapIconColor GetMiniMapIconColor(const URTSComponent& RTSComponent,
	                                         const EAllUnitType UnitType,
	                                         const uint8 UnitSubtype)
	{
		(void)UnitSubtype;

		if (RTSComponent.GetOwningPlayer() == 1)
		{
			if (UnitType == EAllUnitType::UNType_Nomadic || UnitType == EAllUnitType::UNType_BuildingExpansion)
			{
				return ERTSMinimapIconColor::PlayerBuilding;
			}

			return ERTSMinimapIconColor::DefaultPlayerUnit;
		}

		return ERTSMinimapIconColor::DefaultEnemy;
	}
}
