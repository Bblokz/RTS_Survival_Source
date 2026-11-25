// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UObject;
struct FGeometry;

/** World-to-widget projection shared by the world map widgets that sit on top of map actors. */
namespace WorldMapScreenProjection
{
	// Height of the campaign top bar in UI units at the 1920x1080 reference: the strip (54) plus its 1 unit bottom
	// edge. The bar keeps those numbers private in W_WorldCampaignBar.cpp (WorldCampaignBarLayout::StripHeight), so
	// this copy must be changed with them. Map widgets keep tags and plates out of this band.
	constexpr float CampaignTopBarHeight = 55.f;

	/**
	 * @brief Projects a world point into a widget's local space. Goes through viewport pixels and the viewport's own
	 * geometry, so DPI scale and wherever the HUD placed the widget are both accounted for.
	 * @param PlayerController Controller whose camera views the map.
	 * @param WorldContextObject Any object in the game world (the widget itself).
	 * @param WidgetGeometry Geometry of the widget the result is local to (its NativeTick geometry).
	 * @param WorldLocation Point to project.
	 * @param OutLocalPosition Receives the point in the widget's local units.
	 * @return false when the point is behind the camera or there is no controller.
	 */
	RTS_SURVIVAL_API bool ProjectWorldToWidgetLocal(const APlayerController* PlayerController,
	                                                UObject* WorldContextObject, const FGeometry& WidgetGeometry,
	                                                const FVector& WorldLocation, FVector2D& OutLocalPosition);
}
