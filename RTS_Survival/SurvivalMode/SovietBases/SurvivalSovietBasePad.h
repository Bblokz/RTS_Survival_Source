// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalSovietBasePad.generated.h"

class UDecalComponent;

/** @brief Whether a ground decal is drawn as a yard (rounded rectangle) or as a road piece (soft sides, ruts, end fades). */
enum class ESurvivalSovietGroundDecalMode : uint8
{
	Pad,
	Road
};

/**
 * @brief The shape a ground decal paints, in world cm. M_SovietGroundDecal reads it from its dynamic instance, so edges, ruts
 * and fades keep their size whatever the decal's size.
 */
struct FSurvivalSovietGroundDecalShape
{
	FVector2D Center = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	// Along the yaw, then across it.
	FVector2D HalfSize = FVector2D(500.0, 500.0);
	ESurvivalSovietGroundDecalMode Mode = ESurvivalSovietGroundDecalMode::Pad;
	double CornerRadius = 120.0;
	// Width of the soft edge, and how far world noise pushes the edge in and out.
	double EdgeWidth = 240.0;
	double EdgeNoise = 140.0;
	// Road only: fade out at the start (-X) or the end (+X) instead of a hard cut.
	bool bFadeStart = false;
	bool bFadeEnd = false;
	double FadeLength = 900.0;
	// How strongly the edge takes the verge tint: a junction patch over its streets keeps almost none.
	double VergeStrength = 1.0;
	// Higher draws over lower: pads, then roads, then junctions and aprons.
	int32 SortOrder = -10;
	// A long road is painted in pieces that each follow the ground; a piece covers BoxHalfAlong either side of BoxCenter
	// along the yaw, while the shape (ruts, fades) stays the whole segment's. Zero: the box covers the whole shape.
	FVector2D BoxCenter = FVector2D::ZeroVector;
	double BoxHalfAlong = 0.0;
};


/**
 * @brief The ground under a building of a Soviet base and the base's roads: a decal of concrete, gravel or packed earth that
 * follows the terrain. Nothing to fight or collide with; it stays when its building falls, like a real foundation.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ASurvivalSovietBasePad : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalSovietBasePad();

	/**
	 * @brief Sizes the pad as a yard of HalfSize; the actor's own scale goes back to one.
	 * @param HalfSize Half size along the actor's X and Y, cm.
	 * @param Material The surface; a null material leaves the pad invisible.
	 */
	void InitPad(const FVector2D& HalfSize, UMaterialInterface* Material);

	/**
	 * @brief Paints Shape with Material: places the actor on Shape's centre and yaw (keeping its Z) and fills the dynamic
	 * instance the ground decal material reads its frame from.
	 * @param Shape World-space shape; the decal box covers it plus its edge noise.
	 * @param Material An instance of M_SovietGroundDecal; another material still paints, without the shaped edge.
	 */
	void InitShape(const FSurvivalSovietGroundDecalShape& Shape, UMaterialInterface* Material);

	/**
	 * @brief Paints a second decal in road mode from the building's door edge to the road: a rutted track (vehicle and
	 * fuel shelters). Call after the pad's own shape is set.
	 * @param Door World unit vector out of the door wall.
	 * @param DoorReach Distance from the pad centre to the door wall, cm.
	 * @param Length From the door wall to the road's verge, cm.
	 */
	void PaintApron(const FVector2D& Door, double DoorReach, double Length, double Width, UMaterialInterface* Material);

private:
	UPROPERTY()
	TObjectPtr<UDecalComponent> M_ApronDecal = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDecalComponent> M_Decal;
};
