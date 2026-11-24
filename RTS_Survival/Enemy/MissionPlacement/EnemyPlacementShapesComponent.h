// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"

#include "EnemyPlacementShapesComponent.generated.h"

/** World-space shapes of one layer of a placed enemy mission actor; rebuilt whenever the actor is moved or edited. */
struct RTS_SURVIVAL_API FEnemyPlacementShapes
{
	TArray<FDebugRenderSceneProxy::FDebugLine> Lines;
	TArray<FDebugRenderSceneProxy::FDashedLine> DashedLines;
	TArray<FDebugRenderSceneProxy::FArrowLine> ArrowLines;
	TArray<FDebugRenderSceneProxy::FSphere> Spheres;
	TArray<FDebugRenderSceneProxy::FWireCylinder> Cylinders;
	TArray<FDebugRenderSceneProxy::FDebugBox> Boxes;

	// Flat circle on the ground made of line segments.
	void AddCircle(const FVector& Center, const float Radius, const FColor& Color, const float Thickness);

	/**
	 * @brief Flat arc on the ground made of line segments, centred on a direction.
	 * @param Center Middle of the circle the arc lies on.
	 * @param Radius Radius of that circle.
	 * @param CentreDirection Direction the middle of the arc points to (only X and Y are used).
	 * @param ArcDegrees Full width of the arc.
	 * @param Color Line colour.
	 * @param Thickness Line thickness.
	 */
	void AddArc(const FVector& Center, const float Radius, const FVector& CentreDirection, const float ArcDegrees,
	            const FColor& Color, const float Thickness);

	// Flat box on the ground, turned to Yaw.
	void AddFlatBox(const FVector& Center, const FVector& HalfExtent, const float Yaw, const FColor& Color,
	                const bool bSolid);

	FBox GetBounds() const;
};

UENUM()
enum class EEnemyPlacementShapesLayer : uint8
{
	// Always drawn in level viewports: what the actor is and where it acts.
	Summary,
	// Drawn only while the actor is selected (or with RTS.EnemyPlacement.Gizmos 2): facing, radii, links.
	Detail
};

/**
 * @brief Draws one layer of a placed enemy mission actor (wave spawner, convoy route, strongpoint) in the level
 * editor viewports only. Two of these per actor keep a busy map readable: the summary always shows, the detail only
 * for the selected actor. RTS.EnemyPlacement.Gizmos 0 hides all, 1 = default, 2 shows every detail.
 */
UCLASS(ClassGroup = (Custom), HideCategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming))
class RTS_SURVIVAL_API UEnemyPlacementShapesComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UEnemyPlacementShapesComponent();

	void SetLayer(const EEnemyPlacementShapesLayer NewLayer) { M_Layer = NewLayer; }
	void SetShapes(FEnemyPlacementShapes&& Shapes);

protected:
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
	FEnemyPlacementShapes M_Shapes;
	EEnemyPlacementShapesLayer M_Layer = EEnemyPlacementShapesLayer::Summary;
};
