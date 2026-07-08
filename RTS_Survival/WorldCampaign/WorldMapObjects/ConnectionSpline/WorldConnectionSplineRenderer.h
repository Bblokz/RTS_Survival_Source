// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "WorldConnectionRibbon.h"
#include "WorldConnectionSplineRenderer.generated.h"

class AAnchorPoint;
class AConnection;
class AGeneratorWorldCampaign;
class AWorldPlayerController;
class AWorldConnectionRibbon;
class UStaticMesh;
class UMaterialInterface;

/**
 * @brief Spawned by the world player controller after the campaign graph exists to draw art-directed,
 *        width-aware, non-intersecting spline ribbons between connected anchors.
 *
 * @note Pipeline is split into a deterministic 2D geometry solve and a visual build:
 *   1. Gather one or more centerline "edges" per AConnection (three-way junction legs stay straight).
 *   2. Clamp each edge's width at shared nodes so fan-outs never overlap.
 *   3. Greedily curve each edge (freely, bounded by Curviness) and relax the curve back toward its
 *      straight baseline until it clears every finalized edge (width-aware) and the boundary polygon.
 *   4. Spawn one AWorldConnectionRibbon per solved path, attached to its owning AConnection.
 *
 * The connection graph is already planar (the generator forbids crossing centerlines), so the straight
 * baseline is always a valid fallback and the relaxation loop is guaranteed to terminate.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldConnectionSplineRenderer : public AActor
{
	GENERATED_BODY()

public:
	AWorldConnectionSplineRenderer();

	/**
	 * @brief Solves and builds all connection ribbons for the current generated campaign.
	 * @param WorldPlayerController Controller that owns the world campaign flow.
	 * @param WorldGenerator Generator whose placement state holds the authoritative connection graph.
	 */
	void InitializeConnectionSplines(AWorldPlayerController* WorldPlayerController, AGeneratorWorldCampaign* WorldGenerator);

	/** @brief Clears and rebuilds every ribbon from the current generated graph (editor/debug helper). */
	UFUNCTION(CallInEditor, Category = "World Campaign|Connection Spline")
	void RebuildConnectionSplines();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// GC note: the structs below are transient stack locals used only inside a single synchronous
	// BuildAndDrawAllConnections() pass, and they reference world objects exclusively through
	// TWeakObjectPtr. TWeakObjectPtr is GC-safe without UPROPERTY: it never keeps an object alive and
	// self-nulls once the object is destroyed, so it can never dangle. Do NOT convert these to raw
	// pointers or (non-UPROPERTY) TObjectPtr, which would not be GC-tracked. Persistent state that must
	// survive the pass (the spawned ribbon actors) is held by the UPROPERTY M_Ribbons array below.

	/** @brief One centerline segment to be rendered. A normal connection makes one; a three-way makes two. */
	struct FRibbonEdge
	{
		TWeakObjectPtr<AConnection> OwningConnection;
		TWeakObjectPtr<AAnchorPoint> StartAnchor;
		TWeakObjectPtr<AAnchorPoint> EndAnchor;
		FVector StartLocation = FVector::ZeroVector;
		FVector EndLocation = FVector::ZeroVector;
		bool bAllowCurve = true;
		float Width = 0.f;
	};

	/** @brief A solved, finalized edge kept for adjacency and intersection tests against later edges. */
	struct FSolvedEdge
	{
		TArray<FVector2D> Polyline2D;
		TWeakObjectPtr<AAnchorPoint> StartAnchor;
		TWeakObjectPtr<AAnchorPoint> EndAnchor;
		FVector2D StartLocation2D = FVector2D::ZeroVector;
		FVector2D EndLocation2D = FVector2D::ZeroVector;
		float HalfWidth = 0.f;
	};

	bool GetIsValidWorldGenerator() const;

	void BuildAndDrawAllConnections();
	bool GatherEdges(TArray<FRibbonEdge>& OutEdges) const;
	bool BuildBoundaryPolygon(TArray<FVector2D>& OutPolygon) const;
	void ClampEdgeWidthsAtNodes(TArray<FRibbonEdge>& InOutEdges) const;

	/**
	 * @brief Solves a single edge into a curved-or-straight world-space path that clears prior edges/boundary.
	 * @param Edge Edge to solve.
	 * @param BoundaryPolygon Sampled boundary polygon (may be empty, in which case boundary checks are skipped).
	 * @param BoundaryCentroid Centroid used to bias the preferred curve direction inward.
	 * @param bHasBoundary Whether BoundaryPolygon is usable.
	 * @param FinalizedEdges Already-solved edges the candidate must not violate.
	 * @param OutPath Resulting world-space path points (>= 2).
	 * @param OutSolved Finalized 2D record appended by the caller for subsequent edges.
	 */
	void SolveEdge(
		const FRibbonEdge& Edge,
		const TArray<FVector2D>& BoundaryPolygon,
		const FVector2D& BoundaryCentroid,
		bool bHasBoundary,
		const TArray<FSolvedEdge>& FinalizedEdges,
		TArray<FVector>& OutPath,
		FSolvedEdge& OutSolved) const;

	void SpawnRibbon(const FRibbonEdge& Edge, const TArray<FVector>& WorldPath);
	void ClearRibbons();

	FWorldConnectionRibbonStyle BuildStyleForEdge(const FRibbonEdge& Edge) const;

	// --- Pure 2D geometry helpers (unit-tested standalone) ---
	static void SampleCurve2D(const FVector2D& Start, const FVector2D& End, float Amplitude, float DirectionSign,
	                          int32 SampleCount, TArray<FVector2D>& OutPoints);
	static bool PolylineViolatesEdges(const TArray<FVector2D>& Polyline, float HalfWidth,
	                                  const TWeakObjectPtr<AAnchorPoint>& StartAnchor,
	                                  const TWeakObjectPtr<AAnchorPoint>& EndAnchor,
	                                  const FVector2D& Start, const FVector2D& End,
	                                  const TArray<FSolvedEdge>& FinalizedEdges, float MinSeparation);
	static bool PolylineViolatesBoundary(const TArray<FVector2D>& Polyline, float HalfWidth,
	                                     const TArray<FVector2D>& BoundaryPolygon, float Clearance);
	static bool EdgesAreAdjacent(const FSolvedEdge& SolvedEdge,
	                             const TWeakObjectPtr<AAnchorPoint>& StartAnchor,
	                             const TWeakObjectPtr<AAnchorPoint>& EndAnchor,
	                             const FVector2D& Start, const FVector2D& End);
	static float SegmentSegmentDistanceSquared2D(const FVector2D& A0, const FVector2D& A1,
	                                             const FVector2D& B0, const FVector2D& B1);
	static float PointSegmentDistanceSquared2D(const FVector2D& Point, const FVector2D& SegStart, const FVector2D& SegEnd);
	static bool IsPointInsidePolygon2D(const FVector2D& Point, const TArray<FVector2D>& Polygon);

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;

	UPROPERTY()
	TArray<TObjectPtr<AWorldConnectionRibbon>> M_Ribbons;

	// --- Art direction ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AWorldConnectionRibbon> M_RibbonClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> M_RibbonMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> M_RibbonMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ESplineMeshAxis::Type> M_ForwardAxis = ESplineMeshAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float M_RibbonWidth = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float M_MeshWidthAtUnitScale = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true"))
	float M_RibbonZOffset = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true"))
	bool bM_CastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Art", meta = (AllowPrivateAccess = "true"))
	bool bM_AllowDecals = true;

	// --- Curvature ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Curvature", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float M_Curviness = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Curvature", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float M_MaxCurveAmplitude = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Curvature", meta = (AllowPrivateAccess = "true", ClampMin = "2"))
	int32 M_CurveSampleCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Curvature", meta = (AllowPrivateAccess = "true"))
	bool bM_CurveThreeWayEdges = false;

	// --- Separation / boundary / solver ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Solver", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float M_MinSeparation = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Solver", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float M_BoundaryClearance = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Solver", meta = (AllowPrivateAccess = "true", ClampMin = "10.0"))
	float M_BoundarySampleSpacing = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Solver", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 M_MaxRelaxationIterations = 6;

	/**
	 * @brief When true, all ribbons keep the same width and edges are allowed to merge where they share a node.
	 * @note Disable to re-enable per-node angular width clamping (thinner ribbons at tight fan-outs).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Solver", meta = (AllowPrivateAccess = "true"))
	bool bM_UniformWidth = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Solver", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", ClampMax = "1.0", EditCondition = "!bM_UniformWidth"))
	float M_NodeWidthClampFactor = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline|Solver", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", ClampMax = "1.0"))
	float M_MinWidthReductionFraction = 0.4f;
};
