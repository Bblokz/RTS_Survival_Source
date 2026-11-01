// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalSovietBasePad.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class FSurvivalGroundProjector;

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

/** @brief What a building's pad lays beyond its decal (round 4): a sunk slab, an apron to the road, sparse clutter. */
struct FSurvivalSovietPadDressing
{
	// 0 none, 1 concrete slab (permanent buildings), 2 timber boards (tents).
	uint8 Style = 0;
	FVector2D FootprintHalf = FVector2D::ZeroVector;
	// World XY unit vector from the building toward its nearest road.
	FVector2D DoorDirection = FVector2D::ZeroVector;
	double ApronLength = 0.0;
	int32 Seed = 0;
	float SlabMargin = 45.f;
	float SlabRise = 16.f;
	TArray<UStaticMesh*> TileMeshes;
	TArray<UStaticMesh*> ClutterMeshes;
	TArray<UStaticMesh*> BoardMeshes;
	UMaterialInterface* ApronMaterial = nullptr;
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
	 * @brief Lays the building's foundation dressing in world space (instanced meshes without collision, one apron decal).
	 * Call after InitPad: the actor's yaw is the footprint's.
	 * @return How many mesh instances were added (cost note).
	 */
	int32 InitDressing(const FSurvivalSovietPadDressing& Dressing);

private:
	UInstancedStaticMeshComponent* GetOrAddInstancer(UStaticMesh* Mesh);
	int32 LayFoundationTiles(const FSurvivalSovietPadDressing& Dressing, FRandomStream& Random, const FSurvivalGroundProjector& Ground);
	int32 LayClutter(const FSurvivalSovietPadDressing& Dressing, FRandomStream& Random, const FSurvivalGroundProjector& Ground);
	int32 LayTentBoards(const FSurvivalSovietPadDressing& Dressing, FRandomStream& Random, const FSurvivalGroundProjector& Ground);
	void LayApron(const FSurvivalSovietPadDressing& Dressing);
	/** @return The footprint's reach along a world direction (support radius of the rotated rectangle). */
	double GetFootprintReach(const FVector2D& FootprintHalf, const FVector2D& Direction) const;
	bool PlaceOnGround(UStaticMesh* Mesh, const FVector2D& WorldXY, double YawDegrees, const FVector& Scale, double Rise,
	                   const FSurvivalGroundProjector& Ground);

	UPROPERTY()
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> M_Instancers;

	UPROPERTY()
	TObjectPtr<UDecalComponent> M_ApronDecal = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDecalComponent> M_Decal;
};
