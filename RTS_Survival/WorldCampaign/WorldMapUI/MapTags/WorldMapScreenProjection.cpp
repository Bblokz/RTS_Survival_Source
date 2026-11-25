// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/WorldMapScreenProjection.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Layout/Geometry.h"

bool WorldMapScreenProjection::ProjectWorldToWidgetLocal(const APlayerController* PlayerController,
                                                         UObject* WorldContextObject,
                                                         const FGeometry& WidgetGeometry,
                                                         const FVector& WorldLocation, FVector2D& OutLocalPosition)
{
	if (not IsValid(PlayerController))
	{
		return false;
	}

	// Pixels relative to the whole game viewport, the space ScreenToWidgetLocal expects.
	FVector2D ViewportPixel;
	if (not PlayerController->ProjectWorldLocationToScreen(WorldLocation, ViewportPixel, false))
	{
		return false;
	}

	USlateBlueprintLibrary::ScreenToWidgetLocal(WorldContextObject, WidgetGeometry, ViewportPixel, OutLocalPosition);
	return true;
}
