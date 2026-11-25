// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AFowManager;
class FSlateWindowElementList;
class UWorld;
struct FDirectionFindingLook;
struct FGeometry;

/**
 * @brief Paints the direction-finding wedges and their overlap for UW_MiniMap (minimap and tactical map) on maps
 * with an ADirectionFindingManager; does nothing elsewhere.
 */
class FMiniMapDirectionFindingPainter
{
public:
	/** @return True when the world has a direction-finding manager whose hunt has started. */
	static bool GetHasWedges(const UWorld* World);

	/**
	 * @brief Draws the wedges in world space converted through the minimap image's geometry.
	 * @param World The game world (the manager is found through UDirectionFindingSubsystem).
	 * @param FowManager Gives the minimap's world square.
	 * @param MiniMapGeometry Paint-space geometry of the map image (the same space as AllottedGeometry).
	 * @param AllottedGeometry The widget's paint geometry.
	 * @param bIsTacticalMap Thicker edges on the tactical map.
	 * @param OutDrawElements Paint output.
	 * @param LayerId First layer to use.
	 * @return The next free layer.
	 */
	static int32 Paint(const UWorld* World, const AFowManager& FowManager, const FGeometry& MiniMapGeometry,
	                   const FGeometry& AllottedGeometry, const bool bIsTacticalMap,
	                   FSlateWindowElementList& OutDrawElements, const int32 LayerId);

private:
	static void PaintFill(const TArray<FVector2f>& LocalPolygon, const FLinearColor& Color,
	                      const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
	                      const int32 LayerId);

	static void PaintEdge(const TArray<FVector2f>& LocalPolygon, const FLinearColor& Color, const float Thickness,
	                      const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
	                      const int32 LayerId);

	/**
	 * @brief World XY polygon to the widget's local space.
	 * @param WorldPolygon Polygon in world XY (cm).
	 * @param MapOrigin World XY of the minimap's centre.
	 * @param MapSize World width of the minimap square.
	 * @param MiniMapGeometry Paint-space geometry of the map image (the same space as AllottedGeometry).
	 * @param AllottedGeometry The widget's paint geometry.
	 */
	static TArray<FVector2f> ToLocal(const TArray<FVector2D>& WorldPolygon, const FVector2D& MapOrigin,
	                                 const double MapSize, const FGeometry& MiniMapGeometry,
	                                 const FGeometry& AllottedGeometry);
};
