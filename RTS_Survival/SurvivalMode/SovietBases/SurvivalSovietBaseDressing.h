// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseLayout.h"
#include "SurvivalSovietBaseDressing.generated.h"

class ASurvivalSovietBasePad;
class FSurvivalGroundProjector;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** @brief The loaded kit assets a building's footprint is dressed with (round 5). */
struct FSurvivalSovietFootprintAssets
{
	// A slab is nine-sliced (round 6): a flat centre box (100 x 100, top at z = 0, 80 deep) stretched over the slab, and
	// four edge strips (100 long, 6 wide, a 5 cm chamfer on their outer side, local -Y) stretched only along their length,
	// so the bevel keeps its size whatever the slab's size. Aprons and forecourts use the centre box.
	UStaticMesh* SlabMesh = nullptr;
	UStaticMesh* SlabEdgeMesh = nullptr;
	// Concrete dust and gravel lobes spilling off one or two sides of a slab.
	UMaterialInterface* SpillSurface = nullptr;
	UMaterialInterface* SlabMaterial = nullptr;
	// Ground decal surfaces (instances of M_SovietGroundDecal) per class.
	UMaterialInterface* FoundationSurface = nullptr;
	UMaterialInterface* TrampledSurface = nullptr;
	UMaterialInterface* GravelWalkSurface = nullptr;
	UMaterialInterface* GravelOilySurface = nullptr;
	UMaterialInterface* RutTrackSurface = nullptr;
	TArray<UStaticMesh*> PlankMeshes;
	TArray<UStaticMesh*> SandbagMeshes;
	TArray<UStaticMesh*> FactoryClutter;
	TArray<UStaticMesh*> BarracksClutter;
	TArray<UStaticMesh*> TentClutter;
	TArray<UStaticMesh*> ShelterClutter;
	TArray<UStaticMesh*> ForecourtClutter;
	float SlabMargin = 90.f;
	float SlabRise = 10.f;
	float SlabSink = 25.f;
	float ClutterCullDistance = 12000.f;
};

/** @brief One building's footprint as the layout planned it. */
struct FSurvivalSovietFootprintRequest
{
	ESurvivalSovietFoundationStyle Style = ESurvivalSovietFoundationStyle::None;
	FVector2D Center = FVector2D::ZeroVector;
	double YawDegrees = 0.0;
	FVector2D FootprintHalf = FVector2D::ZeroVector;
	// World XY unit vector toward the nearest road.
	FVector2D DoorDirection = FVector2D::ZeroVector;
	// From the footprint edge to the road's verge (plus a small overlap), cm.
	double ApronLength = 0.0;
	int32 Seed = 0;
};

/** @brief What a footprint laid, for the manifest: seating, edges and counts. */
struct FSurvivalSovietFootprintRecord
{
	FString Style;
	FVector2D Center = FVector2D::ZeroVector;
	double YawDegrees = 0.0;
	FVector2D FootprintHalf = FVector2D::ZeroVector;
	// The door wall's outward normal (the side facing the road).
	FVector2D DoorNormal = FVector2D::ZeroVector;
	bool bSlab = false;
	int32 SlabIndex = INDEX_NONE;
	FVector2D SlabHalf = FVector2D::ZeroVector;
	// Slab top over the ground at its corners and centre: lowest and highest.
	double SlabTopOverGroundMin = 0.0;
	double SlabTopOverGroundMax = 0.0;
	// Building actors raised onto the slab, and by how much (cm) the first one was raised.
	int32 LiftedActors = 0;
	double BuildingLift = 0.0;
	int32 Clutter = 0;
	// Smallest distance from a clutter item's box to the slab (or footprint) edge; negative would mean an item crosses it.
	double ClutterMinEdgeClearance = 0.0;
	// Largest gap between a clutter item's bottom and the ground under its box corners (slopes).
	double ClutterMaxGroundGap = 0.0;
	int32 Planks = 0;
	double ApronLength = 0.0;
	FVector2D ApronEnd = FVector2D::ZeroVector;
	int32 Instances = 0;
};

/**
 * @brief The footprint dressing of one Soviet base (round 5): one instanced mesh group per mesh for the whole base (slabs,
 * aprons, planks, sandbags, clutter), no collision, no navigation effect. Slabs are one scaled chamfered box each; slabs of
 * the same yaw that overlap or nearly touch merge into one, at one height. Factory-class buildings are lifted onto their
 * slab; everything else stands on the ground. Clutter stands in one or two work corners outside every slab.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ASurvivalSovietBaseDressing : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalSovietBaseDressing();

	/**
	 * @brief Dresses one building's footprint: its ground decal (on the pad), slab or walk, apron, planks and clutter.
	 * @param Pad The building's pad; its decal is reshaped and resurfaced for the class.
	 * @param Structures The base's spawned structures: the building standing inside a new slab is lifted onto it.
	 */
	FSurvivalSovietFootprintRecord AddFootprint(const FSurvivalSovietFootprintRequest& Request, const FSurvivalSovietFootprintAssets& Assets,
	                                            ASurvivalSovietBasePad& Pad, TArrayView<const TWeakObjectPtr<AActor>> Structures);

	/** @brief The record of a footprint without laying anything (class, frame, door wall): before/after views on one DLL. */
	static FSurvivalSovietFootprintRecord MakeRecordOnly(const FSurvivalSovietFootprintRequest& Request);

	/** @return The footprint wall (outward normal) facing the door direction. */
	static FVector2D GetDoorWallNormal(double YawDegrees, const FVector2D& DoorDirection);

	/** @brief Lifts a structure spawned after its slab onto it. */
	void LiftIfOnSlab(AActor& Actor);

	int32 GetInstanceCount() const;
	int32 GetComponentCount() const { return M_Instancers.Num(); }

	/** @brief Hides or shows every base's dressing meshes (frame cost measurements). */
	static void SetAllHidden(const UWorld* World, bool bHidden);

private:
	struct FSlab
	{
		FVector2D Center = FVector2D::ZeroVector;
		double YawDegrees = 0.0;
		FVector2D Half = FVector2D::ZeroVector;
		double TopZ = 0.0;
		double BottomZ = 0.0;
		int32 Instance = INDEX_NONE;
		// Edge strips: +Y, -Y, +X, -X of the slab's frame.
		int32 EdgeInstances[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
		// Actors standing on the slab and their height above the ground before they were lifted.
		TArray<TPair<TWeakObjectPtr<AActor>, double>> Lifted;
	};

	enum class EInstanceKind : uint8
	{
		// Receives decals (the pad's concrete paints its top), casts no shadow.
		Slab,
		// Flat wood: no shadow, culled with the clutter.
		Plank,
		// Casts shadows, culled at the clutter distance.
		Clutter
	};

	UInstancedStaticMeshComponent* GetInstancer(UStaticMesh* Mesh, EInstanceKind Kind, UMaterialInterface* Material, float CullDistance);

	/**
	 * @brief Adds a slab or merges it into an overlapping one of the same yaw.
	 * @return The slab's index, or INDEX_NONE when the ground under it is too uneven for one flat slab.
	 */
	int32 AddSlab(const FVector2D& Center, double YawDegrees, const FVector2D& Half, const FSurvivalSovietFootprintAssets& Assets,
	              const FSurvivalGroundProjector& Ground, double& OutMinGround, double& OutMaxGround);
	void UpdateSlabInstance(const FSlab& Slab, const FSurvivalSovietFootprintAssets& Assets);
	void LiftOnto(FSlab& Slab, AActor& Actor, const FSurvivalGroundProjector& Ground);
	void RaiseLifted(FSlab& Slab) const;

	/**
	 * @brief A flat box from Start along Direction (an apron or forecourt), its top sloping from the start height to just
	 * over the ground at its end.
	 * @param TopStartZ Absolute top height at the start (an apron leaving a slab); unset: TopStartOverGround over the ground.
	 */
	bool LayStrip(const FVector2D& Start, const FVector2D& Direction, double Length, double Width, double TopStartOverGround,
	              double TopEndOverGround, TOptional<double> TopStartZ, const FSurvivalSovietFootprintAssets& Assets,
	              const FSurvivalGroundProjector& Ground, FVector2D& OutEnd);
	/**
	 * @brief Separate boards (3-5 side by side with gaps, per section) from the door toward the road; when the road is closer
	 * than one section, two threshold boards across the door.
	 * @return Boards laid.
	 */
	int32 LayBoards(const FVector2D& DoorEdge, const FVector2D& Direction, double Length, int32 MaxSections, double SectionLength,
	                const FSurvivalSovietFootprintAssets& Assets, FRandomStream& Random, const FSurvivalGroundProjector& Ground);
	/** @brief One or two spill lobes (concrete dust, gravel) off the sides of a slab, as decals on this actor. */
	void AddSpills(const FSlab& Slab, const FVector2D& Door, const FSurvivalSovietFootprintAssets& Assets, FRandomStream& Random);
	int32 LayPlanks(const FVector2D& Start, const FVector2D& Direction, double Length, int32 MaxPlanks, double PlankLength, double PlankWidth,
	                const FSurvivalSovietFootprintAssets& Assets, FRandomStream& Random, const FSurvivalGroundProjector& Ground);
	/**
	 * @brief Clutter in one or two tight work corners outside the keep-out rectangle (footprint or slab), on the ground.
	 * @param PreferredWalls Wall indices (0 +X, 1 -X, 2 +Y, 3 -Y in the footprint frame) the corners may use, best first.
	 */
	void LayClutterClusters(const FSurvivalSovietFootprintRequest& Request, const FVector2D& KeepHalf, const TArray<UStaticMesh*>& Meshes,
	                        TArrayView<const int32> PreferredWalls, int32 Clusters, const FSurvivalSovietFootprintAssets& Assets,
	                        FRandomStream& Random, const FSurvivalGroundProjector& Ground, FSurvivalSovietFootprintRecord& OutRecord);
	/** @brief Places a mesh with its box bottom on the ground under its centre. @return Its bottom's largest gap to the ground under its corners. */
	double PlaceOnGround(UStaticMesh* Mesh, EInstanceKind Kind, const FVector2D& WorldXY, double YawDegrees, const FVector& Scale,
	                     double ExtraRise, const FSurvivalSovietFootprintAssets& Assets, const FSurvivalGroundProjector& Ground);

	UPROPERTY()
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> M_Instancers;

	TArray<FSlab> M_Slabs;
};
