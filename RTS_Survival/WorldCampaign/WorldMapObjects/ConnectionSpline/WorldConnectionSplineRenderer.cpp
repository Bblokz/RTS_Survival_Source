// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldConnectionSplineRenderer.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "WorldConnectionRibbon.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Boundary/WorldSplineBoundary.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"

namespace WorldConnectionSplineConstants
{
	// Two endpoints closer than this (in cm) are treated as the same node for adjacency skipping.
	constexpr float SharedEndpointToleranceSquared = 25.f * 25.f;
	// Below this amplitude a curve is effectively a straight line, so we skip the mirrored direction.
	constexpr float StraightAmplitudeThreshold = 1.f;
	constexpr float MinSolvableSegmentLength = 1.f;
}

AWorldConnectionSplineRenderer::AWorldConnectionSplineRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWorldConnectionSplineRenderer::InitializeConnectionSplines(
	AWorldPlayerController* WorldPlayerController,
	AGeneratorWorldCampaign* WorldGenerator)
{
	M_WorldPlayerController = WorldPlayerController;
	M_WorldGenerator = WorldGenerator;

	BuildAndDrawAllConnections();
}

void AWorldConnectionSplineRenderer::RebuildConnectionSplines()
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	BuildAndDrawAllConnections();
}

void AWorldConnectionSplineRenderer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRibbons();
	M_WorldPlayerController.Reset();
	M_WorldGenerator.Reset();

	Super::EndPlay(EndPlayReason);
}

bool AWorldConnectionSplineRenderer::GetIsValidWorldGenerator() const
{
	if (not IsValid(M_WorldGenerator.Get()))
	{
		RTSFunctionLibrary::ReportError(TEXT("WorldConnectionSplineRenderer: world generator is invalid."));
		return false;
	}

	return true;
}

void AWorldConnectionSplineRenderer::BuildAndDrawAllConnections()
{
	ClearRibbons();

	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	TArray<FRibbonEdge> Edges;
	if (not GatherEdges(Edges) || Edges.Num() == 0)
	{
		return;
	}

	if (not bM_UniformWidth)
	{
		ClampEdgeWidthsAtNodes(Edges);
	}

	TArray<FVector2D> BoundaryPolygon;
	const bool bHasBoundary = BuildBoundaryPolygon(BoundaryPolygon);

	FVector2D BoundaryCentroid = FVector2D::ZeroVector;
	if (bHasBoundary)
	{
		for (const FVector2D& PolygonPoint : BoundaryPolygon)
		{
			BoundaryCentroid += PolygonPoint;
		}
		BoundaryCentroid /= static_cast<float>(BoundaryPolygon.Num());
	}

	// Greedy, deterministic solve: each edge is made to clear every previously finalized edge, so the
	// finalized set is always mutually non-overlapping.
	TArray<FSolvedEdge> FinalizedEdges;
	FinalizedEdges.Reserve(Edges.Num());

	for (const FRibbonEdge& Edge : Edges)
	{
		TArray<FVector> WorldPath;
		FSolvedEdge SolvedEdge;
		SolveEdge(Edge, BoundaryPolygon, BoundaryCentroid, bHasBoundary, FinalizedEdges, WorldPath, SolvedEdge);

		FRibbonEdge FinalEdge = Edge;
		FinalEdge.Width = SolvedEdge.HalfWidth * 2.f;
		SpawnRibbon(FinalEdge, WorldPath);

		FinalizedEdges.Add(MoveTemp(SolvedEdge));
	}
}

bool AWorldConnectionSplineRenderer::GatherEdges(TArray<FRibbonEdge>& OutEdges) const
{
	OutEdges.Reset();

	if (not GetIsValidWorldGenerator())
	{
		return false;
	}

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AConnection>& ConnectionPtr : PlacementState.CachedConnections)
	{
		AConnection* Connection = ConnectionPtr.Get();
		if (not IsValid(Connection))
		{
			continue;
		}

		const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
		if (ConnectedAnchors.Num() < 2)
		{
			continue;
		}

		AAnchorPoint* AnchorA = ConnectedAnchors[0].Get();
		AAnchorPoint* AnchorB = ConnectedAnchors[1].Get();
		if (not IsValid(AnchorA) || not IsValid(AnchorB))
		{
			continue;
		}

		const bool bIsThreeWay = Connection->GetIsThreeWayConnection();

		FRibbonEdge BaseEdge;
		BaseEdge.OwningConnection = Connection;
		BaseEdge.StartAnchor = AnchorA;
		BaseEdge.EndAnchor = AnchorB;
		BaseEdge.StartLocation = AnchorA->GetActorLocation();
		BaseEdge.EndLocation = AnchorB->GetActorLocation();
		// Keep the base leg of a T-junction straight (unless opted in) so the third leg stays connected.
		BaseEdge.bAllowCurve = not bIsThreeWay || bM_CurveThreeWayEdges;
		BaseEdge.Width = M_RibbonWidth;
		OutEdges.Add(BaseEdge);

		if (bIsThreeWay && ConnectedAnchors.Num() >= 3)
		{
			AAnchorPoint* AnchorC = ConnectedAnchors[2].Get();
			if (IsValid(AnchorC))
			{
				FRibbonEdge JunctionEdge;
				JunctionEdge.OwningConnection = Connection;
				JunctionEdge.StartAnchor = nullptr; // Junction point is not an anchor node.
				JunctionEdge.EndAnchor = AnchorC;
				JunctionEdge.StartLocation = Connection->GetJunctionLocation();
				JunctionEdge.EndLocation = AnchorC->GetActorLocation();
				JunctionEdge.bAllowCurve = bM_CurveThreeWayEdges;
				JunctionEdge.Width = M_RibbonWidth;
				OutEdges.Add(JunctionEdge);
			}
		}
	}

	return OutEdges.Num() > 0;
}

bool AWorldConnectionSplineRenderer::BuildBoundaryPolygon(TArray<FVector2D>& OutPolygon) const
{
	OutPolygon.Reset();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	AWorldSplineBoundary* Boundary = nullptr;
	int32 BoundaryCount = 0;
	for (TActorIterator<AWorldSplineBoundary> BoundaryIterator(World); BoundaryIterator; ++BoundaryIterator)
	{
		AWorldSplineBoundary* Candidate = *BoundaryIterator;
		if (not IsValid(Candidate))
		{
			continue;
		}

		++BoundaryCount;
		if (BoundaryCount == 1)
		{
			Boundary = Candidate;
		}
	}

	if (BoundaryCount != 1 || not IsValid(Boundary))
	{
		// Boundary containment is optional; without a single boundary we simply skip those checks.
		return false;
	}

	Boundary->EnsureClosedLoop();
	Boundary->GetSampledPolygon2D(M_BoundarySampleSpacing, OutPolygon);
	return OutPolygon.Num() >= 3;
}

void AWorldConnectionSplineRenderer::ClampEdgeWidthsAtNodes(TArray<FRibbonEdge>& InOutEdges) const
{
	TMap<TWeakObjectPtr<AAnchorPoint>, TArray<int32>> IncidentEdgesByAnchor;
	for (int32 EdgeIndex = 0; EdgeIndex < InOutEdges.Num(); ++EdgeIndex)
	{
		const FRibbonEdge& Edge = InOutEdges[EdgeIndex];
		if (Edge.StartAnchor.IsValid())
		{
			IncidentEdgesByAnchor.FindOrAdd(Edge.StartAnchor).Add(EdgeIndex);
		}
		if (Edge.EndAnchor.IsValid())
		{
			IncidentEdgesByAnchor.FindOrAdd(Edge.EndAnchor).Add(EdgeIndex);
		}
	}

	const float ClampRadius = FMath::Max(M_RibbonWidth, KINDA_SMALL_NUMBER);

	for (const TPair<TWeakObjectPtr<AAnchorPoint>, TArray<int32>>& AnchorPair : IncidentEdgesByAnchor)
	{
		const AAnchorPoint* Anchor = AnchorPair.Key.Get();
		const TArray<int32>& Indices = AnchorPair.Value;
		if (not IsValid(Anchor) || Indices.Num() < 2)
		{
			continue;
		}

		const FVector AnchorLocation = Anchor->GetActorLocation();
		const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);

		// Outgoing direction angle of each incident edge, measured from this node.
		struct FIncidentAngle
		{
			int32 EdgeIndex = INDEX_NONE;
			float Angle = 0.f;
		};
		TArray<FIncidentAngle> IncidentAngles;
		IncidentAngles.Reserve(Indices.Num());

		for (const int32 EdgeIndex : Indices)
		{
			const FRibbonEdge& Edge = InOutEdges[EdgeIndex];
			const bool bAnchorIsStart = (Edge.StartAnchor == AnchorPair.Key);
			const FVector FarLocation = bAnchorIsStart ? Edge.EndLocation : Edge.StartLocation;
			const FVector2D Direction = FVector2D(FarLocation.X, FarLocation.Y) - AnchorLocation2D;
			if (Direction.IsNearlyZero())
			{
				continue;
			}

			FIncidentAngle IncidentAngle;
			IncidentAngle.EdgeIndex = EdgeIndex;
			IncidentAngle.Angle = FMath::Atan2(Direction.Y, Direction.X);
			IncidentAngles.Add(IncidentAngle);
		}

		if (IncidentAngles.Num() < 2)
		{
			continue;
		}

		IncidentAngles.Sort([](const FIncidentAngle& Left, const FIncidentAngle& Right)
		{
			return Left.Angle < Right.Angle;
		});

		const int32 AngleCount = IncidentAngles.Num();
		for (int32 AngleIndex = 0; AngleIndex < AngleCount; ++AngleIndex)
		{
			const FIncidentAngle& CurrentAngle = IncidentAngles[AngleIndex];
			const FIncidentAngle& NextAngle = IncidentAngles[(AngleIndex + 1) % AngleCount];

			float Gap = NextAngle.Angle - CurrentAngle.Angle;
			if (AngleIndex == AngleCount - 1)
			{
				Gap += 2.f * PI;
			}
			Gap = FMath::Abs(Gap);

			const float AllowedWidth = 2.f * FMath::Sin(Gap * 0.5f) * ClampRadius * M_NodeWidthClampFactor;
			InOutEdges[CurrentAngle.EdgeIndex].Width = FMath::Min(InOutEdges[CurrentAngle.EdgeIndex].Width, AllowedWidth);
			InOutEdges[NextAngle.EdgeIndex].Width = FMath::Min(InOutEdges[NextAngle.EdgeIndex].Width, AllowedWidth);
		}
	}
}

void AWorldConnectionSplineRenderer::SolveEdge(
	const FRibbonEdge& Edge,
	const TArray<FVector2D>& BoundaryPolygon,
	const FVector2D& BoundaryCentroid,
	const bool bHasBoundary,
	const TArray<FSolvedEdge>& FinalizedEdges,
	TArray<FVector>& OutPath,
	FSolvedEdge& OutSolved) const
{
	const FVector2D Start2D(Edge.StartLocation.X, Edge.StartLocation.Y);
	const FVector2D End2D(Edge.EndLocation.X, Edge.EndLocation.Y);
	const float SegmentLength = FVector2D::Distance(Start2D, End2D);
	const float BaseHalfWidth = Edge.Width * 0.5f;

	// Preferred bow direction points toward the boundary interior to reduce boundary violations.
	float PreferredSign = 1.f;
	if (bHasBoundary && SegmentLength > WorldConnectionSplineConstants::MinSolvableSegmentLength)
	{
		const FVector2D Mid = (Start2D + End2D) * 0.5f;
		const FVector2D SegmentDirection = (End2D - Start2D) / SegmentLength;
		const FVector2D Perp(-SegmentDirection.Y, SegmentDirection.X);
		const float TowardCentroid = FVector2D::DotProduct(Perp, BoundaryCentroid - Mid);
		PreferredSign = (TowardCentroid < 0.f) ? -1.f : 1.f;
	}

	const float SignsToTry[2] = {PreferredSign, -PreferredSign};

	const float InitialAmplitude = Edge.bAllowCurve
		? FMath::Min(M_Curviness * SegmentLength, M_MaxCurveAmplitude)
		: 0.f;

	TArray<FVector2D> ChosenPolyline;
	float ChosenHalfWidth = BaseHalfWidth;
	bool bAccepted = false;

	// 1) Try curved candidates, relaxing amplitude toward the (guaranteed-clear) straight baseline.
	for (int32 Iteration = 0; Iteration < M_MaxRelaxationIterations && not bAccepted; ++Iteration)
	{
		const float Amplitude = InitialAmplitude * FMath::Pow(0.5f, static_cast<float>(Iteration));

		for (const float Sign : SignsToTry)
		{
			TArray<FVector2D> Candidate;
			SampleCurve2D(Start2D, End2D, Amplitude, Sign, M_CurveSampleCount, Candidate);

			const bool bViolatesBoundary = bHasBoundary
				&& PolylineViolatesBoundary(Candidate, BaseHalfWidth, BoundaryPolygon, M_BoundaryClearance);
			const bool bViolatesEdges = PolylineViolatesEdges(Candidate, BaseHalfWidth, Edge.StartAnchor,
				Edge.EndAnchor, Start2D, End2D, FinalizedEdges, M_MinSeparation);

			if (not bViolatesBoundary && not bViolatesEdges)
			{
				ChosenPolyline = MoveTemp(Candidate);
				bAccepted = true;
				break;
			}

			if (Amplitude < WorldConnectionSplineConstants::StraightAmplitudeThreshold)
			{
				// Mirrored sign is identical for a straight line; no point testing it.
				break;
			}
		}
	}

	// 2) Straight baseline at full width.
	if (not bAccepted)
	{
		TArray<FVector2D> Straight;
		SampleCurve2D(Start2D, End2D, 0.f, 1.f, M_CurveSampleCount, Straight);

		const bool bViolatesBoundary = bHasBoundary
			&& PolylineViolatesBoundary(Straight, BaseHalfWidth, BoundaryPolygon, M_BoundaryClearance);
		const bool bViolatesEdges = PolylineViolatesEdges(Straight, BaseHalfWidth, Edge.StartAnchor,
			Edge.EndAnchor, Start2D, End2D, FinalizedEdges, M_MinSeparation);

		if (not bViolatesBoundary && not bViolatesEdges)
		{
			ChosenPolyline = MoveTemp(Straight);
			bAccepted = true;
		}
	}

	// 3) Straight baseline with progressively reduced width (skipped when uniform width is requested).
	if (not bAccepted && not bM_UniformWidth)
	{
		TArray<FVector2D> Straight;
		SampleCurve2D(Start2D, End2D, 0.f, 1.f, M_CurveSampleCount, Straight);

		const float WidthFractions[3] = {0.75f, 0.5f, M_MinWidthReductionFraction};
		for (const float WidthFraction : WidthFractions)
		{
			const float ReducedHalfWidth = BaseHalfWidth * WidthFraction;
			const bool bViolatesBoundary = bHasBoundary
				&& PolylineViolatesBoundary(Straight, ReducedHalfWidth, BoundaryPolygon, M_BoundaryClearance);
			const bool bViolatesEdges = PolylineViolatesEdges(Straight, ReducedHalfWidth, Edge.StartAnchor,
				Edge.EndAnchor, Start2D, End2D, FinalizedEdges, M_MinSeparation);

			if (not bViolatesBoundary && not bViolatesEdges)
			{
				ChosenPolyline = Straight;
				ChosenHalfWidth = ReducedHalfWidth;
				bAccepted = true;
				break;
			}
		}
	}

	// 4) Last resort: accept the straight baseline. Keep full width when uniform width is requested;
	//    otherwise fall back to the minimum width to stay clear of neighbours.
	if (not bAccepted)
	{
		SampleCurve2D(Start2D, End2D, 0.f, 1.f, M_CurveSampleCount, ChosenPolyline);
		ChosenHalfWidth = bM_UniformWidth ? BaseHalfWidth : BaseHalfWidth * M_MinWidthReductionFraction;
	}

	// Reconstruct the world-space path, interpolating Z along the segment and applying the art Z offset.
	OutPath.Reset();
	OutPath.Reserve(ChosenPolyline.Num());
	const int32 PointCount = ChosenPolyline.Num();
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const float Alpha = (PointCount > 1) ? static_cast<float>(PointIndex) / static_cast<float>(PointCount - 1) : 0.f;
		const float PointZ = FMath::Lerp(Edge.StartLocation.Z, Edge.EndLocation.Z, Alpha) + M_RibbonZOffset;
		OutPath.Add(FVector(ChosenPolyline[PointIndex].X, ChosenPolyline[PointIndex].Y, PointZ));
	}

	OutSolved.Polyline2D = MoveTemp(ChosenPolyline);
	OutSolved.StartAnchor = Edge.StartAnchor;
	OutSolved.EndAnchor = Edge.EndAnchor;
	OutSolved.StartLocation2D = Start2D;
	OutSolved.EndLocation2D = End2D;
	OutSolved.HalfWidth = ChosenHalfWidth;
}

void AWorldConnectionSplineRenderer::SpawnRibbon(const FRibbonEdge& Edge, const TArray<FVector>& WorldPath)
{
	if (WorldPath.Num() < 2)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	UClass* RibbonClass = M_RibbonClass ? M_RibbonClass.Get() : AWorldConnectionRibbon::StaticClass();
	if (not IsValid(RibbonClass))
	{
		RibbonClass = AWorldConnectionRibbon::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWorldConnectionRibbon* Ribbon = World->SpawnActor<AWorldConnectionRibbon>(RibbonClass, FTransform::Identity, SpawnParameters);
	if (not IsValid(Ribbon))
	{
		return;
	}

	// Attach to the owning connection so the ribbon inherits its fog-of-war reveal corridor and lifecycle.
	if (AConnection* OwningConnection = Edge.OwningConnection.Get())
	{
		Ribbon->AttachToActor(OwningConnection, FAttachmentTransformRules::KeepWorldTransform);
	}

	Ribbon->BuildRibbon(WorldPath, BuildStyleForEdge(Edge));
	M_Ribbons.Add(Ribbon);
}

void AWorldConnectionSplineRenderer::ClearRibbons()
{
	for (const TObjectPtr<AWorldConnectionRibbon>& Ribbon : M_Ribbons)
	{
		if (IsValid(Ribbon))
		{
			Ribbon->Destroy();
		}
	}

	M_Ribbons.Reset();
}

FWorldConnectionRibbonStyle AWorldConnectionSplineRenderer::BuildStyleForEdge(const FRibbonEdge& Edge) const
{
	FWorldConnectionRibbonStyle Style;
	Style.Mesh = M_RibbonMesh;
	Style.Material = M_RibbonMaterial;
	Style.ForwardAxis = M_ForwardAxis;
	Style.Width = FMath::Max(Edge.Width, KINDA_SMALL_NUMBER);
	Style.MeshWidthAtUnitScale = M_MeshWidthAtUnitScale;
	Style.bCastShadow = bM_CastShadow;
	Style.bAllowDecals = bM_AllowDecals;
	return Style;
}

// ----------------------------------------------------------------------------------------------------
// Pure 2D geometry helpers
// ----------------------------------------------------------------------------------------------------

void AWorldConnectionSplineRenderer::SampleCurve2D(const FVector2D& Start, const FVector2D& End, const float Amplitude,
	const float DirectionSign, const int32 SampleCount, TArray<FVector2D>& OutPoints)
{
	OutPoints.Reset();

	const float SegmentLength = FVector2D::Distance(Start, End);
	if (FMath::Abs(Amplitude) < WorldConnectionSplineConstants::StraightAmplitudeThreshold
		|| SegmentLength <= WorldConnectionSplineConstants::MinSolvableSegmentLength)
	{
		OutPoints.Add(Start);
		OutPoints.Add(End);
		return;
	}

	const FVector2D Mid = (Start + End) * 0.5f;
	const FVector2D SegmentDirection = (End - Start) / SegmentLength;
	const FVector2D Perp(-SegmentDirection.Y, SegmentDirection.X);
	const FVector2D Control = Mid + Perp * (Amplitude * DirectionSign);

	const int32 Segments = FMath::Max(SampleCount, 2);
	OutPoints.Reserve(Segments + 1);
	for (int32 SampleIndex = 0; SampleIndex <= Segments; ++SampleIndex)
	{
		const float T = static_cast<float>(SampleIndex) / static_cast<float>(Segments);
		const float OneMinusT = 1.f - T;
		// Quadratic Bezier through Start, Control, End.
		const FVector2D Point = (OneMinusT * OneMinusT) * Start
			+ (2.f * OneMinusT * T) * Control
			+ (T * T) * End;
		OutPoints.Add(Point);
	}
}

bool AWorldConnectionSplineRenderer::EdgesAreAdjacent(const FSolvedEdge& SolvedEdge,
	const TWeakObjectPtr<AAnchorPoint>& StartAnchor, const TWeakObjectPtr<AAnchorPoint>& EndAnchor,
	const FVector2D& Start, const FVector2D& End)
{
	// Shared anchor node.
	if (StartAnchor.IsValid())
	{
		if (StartAnchor == SolvedEdge.StartAnchor || StartAnchor == SolvedEdge.EndAnchor)
		{
			return true;
		}
	}
	if (EndAnchor.IsValid())
	{
		if (EndAnchor == SolvedEdge.StartAnchor || EndAnchor == SolvedEdge.EndAnchor)
		{
			return true;
		}
	}

	// Shared endpoint location (covers three-way junction points that are not anchors).
	const float ToleranceSquared = WorldConnectionSplineConstants::SharedEndpointToleranceSquared;
	const FVector2D Endpoints[2] = {Start, End};
	const FVector2D SolvedEndpoints[2] = {SolvedEdge.StartLocation2D, SolvedEdge.EndLocation2D};
	for (const FVector2D& Endpoint : Endpoints)
	{
		for (const FVector2D& SolvedEndpoint : SolvedEndpoints)
		{
			if (FVector2D::DistSquared(Endpoint, SolvedEndpoint) <= ToleranceSquared)
			{
				return true;
			}
		}
	}

	return false;
}

bool AWorldConnectionSplineRenderer::PolylineViolatesEdges(const TArray<FVector2D>& Polyline, const float HalfWidth,
	const TWeakObjectPtr<AAnchorPoint>& StartAnchor, const TWeakObjectPtr<AAnchorPoint>& EndAnchor,
	const FVector2D& Start, const FVector2D& End, const TArray<FSolvedEdge>& FinalizedEdges, const float MinSeparation)
{
	if (Polyline.Num() < 2)
	{
		return false;
	}

	for (const FSolvedEdge& SolvedEdge : FinalizedEdges)
	{
		if (SolvedEdge.Polyline2D.Num() < 2)
		{
			continue;
		}

		// Adjacent edges meet at a shared node; overlap there is expected and handled by node width clamping.
		if (EdgesAreAdjacent(SolvedEdge, StartAnchor, EndAnchor, Start, End))
		{
			continue;
		}

		const float RequiredDistance = HalfWidth + SolvedEdge.HalfWidth + MinSeparation;
		const float RequiredDistanceSquared = RequiredDistance * RequiredDistance;

		for (int32 CandidateIndex = 0; CandidateIndex < Polyline.Num() - 1; ++CandidateIndex)
		{
			const FVector2D CandidateStart = Polyline[CandidateIndex];
			const FVector2D CandidateEnd = Polyline[CandidateIndex + 1];

			for (int32 SolvedIndex = 0; SolvedIndex < SolvedEdge.Polyline2D.Num() - 1; ++SolvedIndex)
			{
				const FVector2D SolvedStart = SolvedEdge.Polyline2D[SolvedIndex];
				const FVector2D SolvedEnd = SolvedEdge.Polyline2D[SolvedIndex + 1];

				if (SegmentSegmentDistanceSquared2D(CandidateStart, CandidateEnd, SolvedStart, SolvedEnd)
					< RequiredDistanceSquared)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool AWorldConnectionSplineRenderer::PolylineViolatesBoundary(const TArray<FVector2D>& Polyline, const float HalfWidth,
	const TArray<FVector2D>& BoundaryPolygon, const float Clearance)
{
	if (BoundaryPolygon.Num() < 3)
	{
		return false;
	}

	const float RequiredDistance = HalfWidth + Clearance;
	const float RequiredDistanceSquared = RequiredDistance * RequiredDistance;
	const int32 PolygonCount = BoundaryPolygon.Num();

	for (const FVector2D& Point : Polyline)
	{
		if (not IsPointInsidePolygon2D(Point, BoundaryPolygon))
		{
			return true;
		}

		for (int32 PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
		{
			const FVector2D EdgeStart = BoundaryPolygon[PolygonIndex];
			const FVector2D EdgeEnd = BoundaryPolygon[(PolygonIndex + 1) % PolygonCount];
			if (PointSegmentDistanceSquared2D(Point, EdgeStart, EdgeEnd) < RequiredDistanceSquared)
			{
				return true;
			}
		}
	}

	return false;
}

float AWorldConnectionSplineRenderer::SegmentSegmentDistanceSquared2D(const FVector2D& A0, const FVector2D& A1,
	const FVector2D& B0, const FVector2D& B1)
{
	const FVector2D D1 = A1 - A0;
	const FVector2D D2 = B1 - B0;
	const FVector2D R = A0 - B0;
	const float A = FVector2D::DotProduct(D1, D1);
	const float E = FVector2D::DotProduct(D2, D2);
	const float F = FVector2D::DotProduct(D2, R);

	float S = 0.f;
	float T = 0.f;

	if (A <= KINDA_SMALL_NUMBER && E <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::DistSquared(A0, B0);
	}

	if (A <= KINDA_SMALL_NUMBER)
	{
		S = 0.f;
		T = FMath::Clamp(F / E, 0.f, 1.f);
	}
	else
	{
		const float C = FVector2D::DotProduct(D1, R);
		if (E <= KINDA_SMALL_NUMBER)
		{
			T = 0.f;
			S = FMath::Clamp(-C / A, 0.f, 1.f);
		}
		else
		{
			const float B = FVector2D::DotProduct(D1, D2);
			const float Denominator = A * E - B * B;
			if (Denominator > KINDA_SMALL_NUMBER)
			{
				S = FMath::Clamp((B * F - C * E) / Denominator, 0.f, 1.f);
			}
			else
			{
				S = 0.f;
			}

			T = (B * S + F) / E;
			if (T < 0.f)
			{
				T = 0.f;
				S = FMath::Clamp(-C / A, 0.f, 1.f);
			}
			else if (T > 1.f)
			{
				T = 1.f;
				S = FMath::Clamp((B - C) / A, 0.f, 1.f);
			}
		}
	}

	const FVector2D ClosestOnA = A0 + D1 * S;
	const FVector2D ClosestOnB = B0 + D2 * T;
	return FVector2D::DistSquared(ClosestOnA, ClosestOnB);
}

float AWorldConnectionSplineRenderer::PointSegmentDistanceSquared2D(const FVector2D& Point, const FVector2D& SegStart,
	const FVector2D& SegEnd)
{
	const FVector2D Segment = SegEnd - SegStart;
	const float LengthSquared = FVector2D::DotProduct(Segment, Segment);
	if (LengthSquared <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::DistSquared(Point, SegStart);
	}

	const float Projection = FVector2D::DotProduct(Point - SegStart, Segment) / LengthSquared;
	const float ClampedProjection = FMath::Clamp(Projection, 0.f, 1.f);
	const FVector2D ClosestPoint = SegStart + Segment * ClampedProjection;
	return FVector2D::DistSquared(Point, ClosestPoint);
}

bool AWorldConnectionSplineRenderer::IsPointInsidePolygon2D(const FVector2D& Point, const TArray<FVector2D>& Polygon)
{
	const int32 PolygonCount = Polygon.Num();
	if (PolygonCount < 3)
	{
		return false;
	}

	bool bInside = false;
	for (int32 CurrentIndex = 0, PreviousIndex = PolygonCount - 1; CurrentIndex < PolygonCount; PreviousIndex = CurrentIndex++)
	{
		const FVector2D& CurrentVertex = Polygon[CurrentIndex];
		const FVector2D& PreviousVertex = Polygon[PreviousIndex];

		const bool bStraddlesY = (CurrentVertex.Y > Point.Y) != (PreviousVertex.Y > Point.Y);
		if (bStraddlesY)
		{
			const float IntersectX = (PreviousVertex.X - CurrentVertex.X)
				* (Point.Y - CurrentVertex.Y)
				/ (PreviousVertex.Y - CurrentVertex.Y)
				+ CurrentVertex.X;
			if (Point.X < IntersectX)
			{
				bInside = not bInside;
			}
		}
	}

	return bInside;
}
