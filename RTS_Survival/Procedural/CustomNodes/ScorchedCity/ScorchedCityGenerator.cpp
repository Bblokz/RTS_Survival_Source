// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "ScorchedCityGenerator.h"

#include "Algo/Reverse.h"

namespace ScorchedCityGenConstants
{
	// Zone flag bits stored per macro cell.
	constexpr uint8 ZoneFlagGrid = 1 << 0;
	constexpr uint8 ZoneFlagDense = 1 << 1;

	// Salts for independent deterministic noise channels.
	constexpr int32 ZoneNoiseSaltLayout = 101;
	constexpr int32 ZoneNoiseSaltDensity = 202;

	// Curved road walker limits; walkers also stop on collisions and city bounds.
	constexpr int32 MaxStepsPerWalker = 160;
	constexpr int32 MaxWalkerDepth = 3;
	constexpr double MinCurveTurnDegrees = 4.0;
	constexpr double MaxCurveTurnDegrees = 38.0;
	constexpr double SegmentLengthJitter = 0.35;
	constexpr double GridConnectionHeadingJitter = 0.0;

	// Placement retry counts; kept small because every attempt costs an occupancy query.
	constexpr int32 BuildingPicksPerLot = 4;
	constexpr int32 JitterAttemptsPerPick = 3;

	// Density gate multipliers for dense/sparse macro zones.
	constexpr double DenseZoneLotChanceMultiplier = 1.25;
	constexpr double SparseZoneLotChanceMultiplier = 0.55;

	// Scatter count multipliers for dense/sparse macro zones.
	constexpr double DenseZoneScatterMultiplier = 1.2;
	constexpr double SparseZoneScatterMultiplier = 0.85;

	constexpr double LotFootprintShrink = 0.95;
	constexpr double LotGapFraction = 0.05;
	constexpr double MajorRoadLotWidthMultiplier = 1.4;
	constexpr double MajorRoadLotDepthMultiplier = 1.3;

	// Scatter overlap tests use a square of this fraction of the mesh radius.
	constexpr double ScatterFootprintFraction = 0.75;

	constexpr int32 DecalAttemptsPerSpawn = 18;
	constexpr double DecalRoadLengthUnit = 1000.0;
	constexpr double DecalPowerPoleJitter = 80.0;

	// Curved roads must be long enough (in segment lengths) to read as a natural curve,
	// unless they end connected to another road.
	constexpr double MinCurvedRoadSegmentFactor = 2.2;
	// A walker may only snap-connect into a road when the turn required is gentle.
	constexpr double MaxSnapConnectTurnDegrees = 70.0;
	// Clear space (in segment lengths) required ahead of a junction before seeding a walker.
	constexpr double CurvedSeedClearanceSegmentFactor = 1.8;

	// Corner fillets: fraction of a block used as the curve radius, and arc tessellation.
	constexpr double CornerFilletBlockFraction = 0.35;
	constexpr double CornerFilletMaxEdgeFraction = 0.4;
	constexpr int32 CornerFilletArcPoints = 7;

	// Short streets between large 4-ways still get one narrower lot when the usable span is
	// at least this fraction of the normal lot width; below it the face genuinely has no room.
	constexpr double MinAdaptiveLotWidthFraction = 0.55;
	// At or above this OverallDensity, every street with lots gets at least one building.
	constexpr double MinDensityForEdgeFill = 0.35;

	// Auxiliary blueprints: nearby positions tried per placement before giving up.
	constexpr int32 AuxiliaryPlacementAttempts = 5;

	// Road blocks: items overlap-free along the arc, with a little air between them.
	constexpr double RoadBlockItemSpacingFactor = 1.15;
	constexpr int32 MaxRoadBlockItems = 32;
	constexpr int32 MinRoadBlockItems = 2;

	// Orphan road endings are paired when closer than this many grid blocks.
	constexpr double OrphanConnectMaxBlockFactor = 1.75;
	// Both endings must roughly face each other for the connection to look natural.
	constexpr double OrphanConnectMinFacingDot = 0.15;
	constexpr int32 OrphanCurveMaxPoints = 16;
	// How far past an ending we probe to decide whether it sits at the city rim.
	constexpr double OrphanRimProbeRoadWidths = 3.0;
}

FScorchedCityGenerator::FScorchedCityGenerator(const FScorchedCityGenParams& InParams)
	: M_Params(InParams)
	, M_Random(InParams.RandomSeed)
	, M_Occupancy(FMath::Clamp(InParams.GridBlockSize * 0.5, 800.0, 3000.0))
	, M_DecalOccupancy(FMath::Clamp(InParams.GridBlockSize * 0.25, 400.0, 1500.0))
{
}

void FScorchedCityGenerator::Generate(FScorchedCityGenResult& OutResult)
{
	BuildZones();
	BuildGridRoads();
	BuildCurvedRoads();
	SmoothCornerNodes();
	ConnectOrphanRoadEnds(OutResult);
	AddIntersectionFootprints();
	BuildLots();
	PlaceBuildings(OutResult);
	PlacePowerLines(OutResult);
	PlaceRoadLanterns(OutResult);
	PlaceRoadBlocks(OutResult);
	PlaceAuxiliaryBlueprints(OutResult);
	BuildScatter(OutResult, OutResult);
	ExportRoads(OutResult);
	BuildDecals(OutResult);

	OutResult.Lots = M_Lots;
	OutResult.OccupiedFootprints = M_Occupancy.GetEntries();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool FScorchedCityGenerator::IsInsideCity(const FVector2D& Position, const double Margin) const
{
	const bool bInsideRectangle =
		FMath::Abs(Position.X) <= M_Params.CityLengthX * 0.5 - Margin
		&& FMath::Abs(Position.Y) <= M_Params.CityLengthY * 0.5 - Margin;
	if (not bInsideRectangle)
	{
		return false;
	}

	// The input shape (e.g. a closed spline loop) trims the rectangle; margin is rect-only,
	// which keeps the shape test cheap and is good enough for filling the area.
	return not M_Params.IsInsideArea || M_Params.IsInsideArea(Position);
}

bool FScorchedCityGenerator::IsFootprintInsideCity(const FScorchedFootprint& Footprint, const double Margin) const
{
	FVector2D Corners[4];
	Footprint.GetCorners(Corners);

	for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
	{
		if (not IsInsideCity(Corners[CornerIndex], Margin))
		{
			return false;
		}
	}
	return true;
}

bool FScorchedCityGenerator::IsExcludedAt(const FVector2D& Position) const
{
	return M_Params.IsExcluded && M_Params.IsExcluded(Position);
}

double FScorchedCityGenerator::CellNoise(const int32 Salt, const FIntPoint& Cell) const
{
	const uint32 Hash = HashCombine(
		HashCombine(GetTypeHash(M_Params.RandomSeed + Salt), GetTypeHash(Cell.X)),
		GetTypeHash(Cell.Y));
	return FRandomStream(static_cast<int32>(Hash)).FRand();
}

bool FScorchedCityGenerator::SamplePolyline(
	const TArray<FVector2D>& Points,
	const double Distance,
	FVector2D& OutPosition,
	FVector2D& OutDirection)
{
	double Remaining = FMath::Max(0.0, Distance);
	for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
	{
		const FVector2D SegmentStart = Points[PointIndex - 1];
		const FVector2D SegmentEnd = Points[PointIndex];
		const double SegmentLength = FVector2D::Distance(SegmentStart, SegmentEnd);

		if (Remaining <= SegmentLength && SegmentLength > UE_KINDA_SMALL_NUMBER)
		{
			OutDirection = (SegmentEnd - SegmentStart) / SegmentLength;
			OutPosition = SegmentStart + OutDirection * Remaining;
			return true;
		}
		Remaining -= SegmentLength;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Zoning
// ---------------------------------------------------------------------------

void FScorchedCityGenerator::BuildZones()
{
	using namespace ScorchedCityGenConstants;

	const double BlockSize = M_Params.GridBlockSize;
	const double HalfX = M_Params.CityLengthX * 0.5;
	const double HalfY = M_Params.CityLengthY * 0.5;

	M_ZoneCellMin = FIntPoint(FMath::FloorToInt32(-HalfX / BlockSize), FMath::FloorToInt32(-HalfY / BlockSize));
	M_ZoneCellMax = FIntPoint(FMath::FloorToInt32(HalfX / BlockSize), FMath::FloorToInt32(HalfY / BlockSize));

	const double TotalLayout = M_Params.GridLayoutAmount + M_Params.CurvedRoadAmount;
	const double GridProbability = (TotalLayout <= UE_KINDA_SMALL_NUMBER)
		? 1.0
		: M_Params.GridLayoutAmount / TotalLayout;
	const double DenseProbability = FMath::Clamp(0.15 + 0.6 * M_Params.OverallDensity, 0.0, 1.0);

	for (int32 CellX = M_ZoneCellMin.X; CellX <= M_ZoneCellMax.X; ++CellX)
	{
		for (int32 CellY = M_ZoneCellMin.Y; CellY <= M_ZoneCellMax.Y; ++CellY)
		{
			const FIntPoint Cell(CellX, CellY);
			uint8 Flags = 0;
			if (CellNoise(ZoneNoiseSaltLayout, Cell) < GridProbability)
			{
				Flags |= ZoneFlagGrid;
			}
			if (CellNoise(ZoneNoiseSaltDensity, Cell) < DenseProbability)
			{
				Flags |= ZoneFlagDense;
			}
			M_ZoneFlags.Add(Cell, Flags);
		}
	}
}

bool FScorchedCityGenerator::IsGridCell(const FIntPoint& Cell) const
{
	const uint8* Flags = M_ZoneFlags.Find(Cell);
	return Flags != nullptr && (*Flags & ScorchedCityGenConstants::ZoneFlagGrid) != 0;
}

bool FScorchedCityGenerator::IsGridZoneAt(const FVector2D& Position) const
{
	const double BlockSize = M_Params.GridBlockSize;
	return IsGridCell(FIntPoint(
		FMath::FloorToInt32(Position.X / BlockSize),
		FMath::FloorToInt32(Position.Y / BlockSize)));
}

bool FScorchedCityGenerator::IsDenseAt(const FVector2D& Position) const
{
	const double BlockSize = M_Params.GridBlockSize;
	const uint8* Flags = M_ZoneFlags.Find(FIntPoint(
		FMath::FloorToInt32(Position.X / BlockSize),
		FMath::FloorToInt32(Position.Y / BlockSize)));
	return Flags != nullptr && (*Flags & ScorchedCityGenConstants::ZoneFlagDense) != 0;
}

// ---------------------------------------------------------------------------
// Road network core
// ---------------------------------------------------------------------------

int32 FScorchedCityGenerator::FindOrAddNode(const FVector2D& Position, const double SnapRadius, const bool bGridNode)
{
	for (int32 NodeIndex = 0; NodeIndex < M_Nodes.Num(); ++NodeIndex)
	{
		if (FVector2D::DistSquared(M_Nodes[NodeIndex].Position, Position) <= SnapRadius * SnapRadius)
		{
			return NodeIndex;
		}
	}

	FScorchedRoadNode Node;
	Node.Position = Position;
	Node.bGridNode = bGridNode;
	return M_Nodes.Add(Node);
}

int32 FScorchedCityGenerator::FindNearestNode(const FVector2D& Position, const double SearchRadius) const
{
	int32 NearestNode = INDEX_NONE;
	double NearestDistanceSquared = SearchRadius * SearchRadius;

	for (int32 NodeIndex = 0; NodeIndex < M_Nodes.Num(); ++NodeIndex)
	{
		const double DistanceSquared = FVector2D::DistSquared(M_Nodes[NodeIndex].Position, Position);
		if (DistanceSquared <= NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestNode = NodeIndex;
		}
	}
	return NearestNode;
}

int32 FScorchedCityGenerator::CommitEdge(
	const int32 NodeAIndex,
	const int32 NodeBIndex,
	TArray<FVector2D>&& Points,
	const bool bCurved,
	const bool bMajor)
{
	FScorchedRoadEdge Edge;
	Edge.NodeA = NodeAIndex;
	Edge.NodeB = NodeBIndex;
	Edge.Points = MoveTemp(Points);
	Edge.bCurved = bCurved;
	Edge.bMajor = bMajor;

	const int32 EdgeIndex = M_Edges.Add(MoveTemp(Edge));
	M_Nodes[NodeAIndex].Edges.Add(EdgeIndex);
	M_Nodes[NodeBIndex].Edges.Add(EdgeIndex);

	const TArray<FVector2D>& EdgePoints = M_Edges[EdgeIndex].Points;
	for (int32 PointIndex = 1; PointIndex < EdgePoints.Num(); ++PointIndex)
	{
		RasterizeSegment(EdgePoints[PointIndex - 1], EdgePoints[PointIndex]);
	}
	return EdgeIndex;
}

void FScorchedCityGenerator::RasterizeSegment(const FVector2D& From, const FVector2D& To)
{
	const double SegmentLength = FVector2D::Distance(From, To);
	if (SegmentLength <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector2D Direction = (To - From) / SegmentLength;
	const double Yaw = FMath::Atan2(Direction.Y, Direction.X);
	const int32 NumChunks = FMath::Max(1, FMath::CeilToInt32(SegmentLength / M_Params.RoadMeshLength));
	const double ChunkLength = SegmentLength / NumChunks;

	for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)
	{
		FScorchedFootprint Chunk;
		Chunk.Center = From + Direction * ChunkLength * (ChunkIndex + 0.5);
		Chunk.HalfExtents = FVector2D(ChunkLength * 0.5, M_Params.RoadWidth * 0.5);
		Chunk.YawRadians = Yaw;
		M_Occupancy.Add(Chunk, EScorchedOccupancy::Road);
	}
}

bool FScorchedCityGenerator::IsSegmentBlockedByRoads(const FVector2D& From, const FVector2D& To) const
{
	const double SegmentLength = FVector2D::Distance(From, To);
	if (SegmentLength <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector2D Direction = (To - From) / SegmentLength;
	// Pull the start forward so the walker's own previous segment does not block it.
	const FVector2D EffectiveFrom = From + Direction * FMath::Min(M_Params.RoadWidth, SegmentLength * 0.4);
	const double EffectiveLength = FVector2D::Distance(EffectiveFrom, To);

	FScorchedFootprint Candidate;
	Candidate.Center = (EffectiveFrom + To) * 0.5;
	Candidate.HalfExtents = FVector2D(EffectiveLength * 0.5, M_Params.RoadWidth * 0.5);
	Candidate.YawRadians = FMath::Atan2(Direction.Y, Direction.X);

	const double MinRoadSpacing = M_Params.RoadSetback + M_Params.RoadWidth * 0.5;
	const uint8 RoadMask = ScorchedOccupancyMask(EScorchedOccupancy::Road)
		| ScorchedOccupancyMask(EScorchedOccupancy::Intersection);
	return M_Occupancy.OverlapsAny(Candidate.Inflated(MinRoadSpacing), RoadMask);
}

bool FScorchedCityGenerator::IsConnectionSegmentBlockedByRoads(const FVector2D& From, const FVector2D& To) const
{
	const double SegmentLength = FVector2D::Distance(From, To);
	const double EndpointTrim = IntersectionMaxHalfExtent() + M_Params.RoadWidth * 0.25;
	if (SegmentLength <= EndpointTrim * 2.0)
	{
		return false;
	}

	const FVector2D Direction = (To - From) / SegmentLength;
	const FVector2D EffectiveFrom = From + Direction * EndpointTrim;
	const FVector2D EffectiveTo = To - Direction * EndpointTrim;
	const double EffectiveLength = FVector2D::Distance(EffectiveFrom, EffectiveTo);

	FScorchedFootprint Candidate;
	Candidate.Center = (EffectiveFrom + EffectiveTo) * 0.5;
	Candidate.HalfExtents = FVector2D(EffectiveLength * 0.5, M_Params.RoadWidth * 0.45);
	Candidate.YawRadians = FMath::Atan2(Direction.Y, Direction.X);

	const uint8 RoadMask = ScorchedOccupancyMask(EScorchedOccupancy::Road)
		| ScorchedOccupancyMask(EScorchedOccupancy::Intersection);
	return M_Occupancy.OverlapsAny(Candidate, RoadMask);
}

// ---------------------------------------------------------------------------
// Grid roads
// ---------------------------------------------------------------------------

void FScorchedCityGenerator::BuildGridRoads()
{
	if (M_Params.GridLayoutAmount <= 0.0)
	{
		return;
	}

	const double BlockSize = M_Params.GridBlockSize;
	TMap<FIntPoint, int32> CrossingToNode;

	// Create a node at every lattice crossing that touches at least one grid-zone cell.
	for (int32 LatticeX = M_ZoneCellMin.X; LatticeX <= M_ZoneCellMax.X + 1; ++LatticeX)
	{
		for (int32 LatticeY = M_ZoneCellMin.Y; LatticeY <= M_ZoneCellMax.Y + 1; ++LatticeY)
		{
			const bool bTouchesGrid =
				IsGridCell(FIntPoint(LatticeX - 1, LatticeY - 1)) || IsGridCell(FIntPoint(LatticeX, LatticeY - 1))
				|| IsGridCell(FIntPoint(LatticeX - 1, LatticeY)) || IsGridCell(FIntPoint(LatticeX, LatticeY));

			const FVector2D Position(LatticeX * BlockSize, LatticeY * BlockSize);
			if (bTouchesGrid && IsInsideCity(Position, M_Params.RoadWidth))
			{
				FScorchedRoadNode Node;
				Node.Position = Position;
				Node.bGridNode = true;
				CrossingToNode.Add(FIntPoint(LatticeX, LatticeY), M_Nodes.Add(Node));
			}
		}
	}

	// Connect neighboring crossings when the street between them borders a grid-zone cell.
	const int32 MajorInterval = FMath::Max(1, M_Params.MajorRoadInterval);
	for (int32 LatticeX = M_ZoneCellMin.X; LatticeX <= M_ZoneCellMax.X + 1; ++LatticeX)
	{
		for (int32 LatticeY = M_ZoneCellMin.Y; LatticeY <= M_ZoneCellMax.Y + 1; ++LatticeY)
		{
			const int32* NodeHere = CrossingToNode.Find(FIntPoint(LatticeX, LatticeY));
			if (NodeHere == nullptr)
			{
				continue;
			}

			const int32* NodeRight = CrossingToNode.Find(FIntPoint(LatticeX + 1, LatticeY));
			const bool bStreetAlongX =
				IsGridCell(FIntPoint(LatticeX, LatticeY - 1)) || IsGridCell(FIntPoint(LatticeX, LatticeY));
			if (NodeRight != nullptr && bStreetAlongX)
			{
				TArray<FVector2D> Points = {M_Nodes[*NodeHere].Position, M_Nodes[*NodeRight].Position};
				CommitEdge(*NodeHere, *NodeRight, MoveTemp(Points), false, (LatticeY % MajorInterval) == 0);
			}

			const int32* NodeUp = CrossingToNode.Find(FIntPoint(LatticeX, LatticeY + 1));
			const bool bStreetAlongY =
				IsGridCell(FIntPoint(LatticeX - 1, LatticeY)) || IsGridCell(FIntPoint(LatticeX, LatticeY));
			if (NodeUp != nullptr && bStreetAlongY)
			{
				TArray<FVector2D> Points = {M_Nodes[*NodeHere].Position, M_Nodes[*NodeUp].Position};
				CommitEdge(*NodeHere, *NodeUp, MoveTemp(Points), false, (LatticeX % MajorInterval) == 0);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Curved roads
// ---------------------------------------------------------------------------

void FScorchedCityGenerator::BuildCurvedRoads()
{
	if (M_Params.CurvedRoadAmount <= 0.0)
	{
		return;
	}

	const double CityArea = M_Params.CityLengthX * M_Params.CityLengthY;
	const double BlockArea = FMath::Square(M_Params.GridBlockSize);
	int32 WalkerBudget = FMath::Clamp(FMath::RoundToInt32(CityArea / BlockArea * 0.5), 4, 48);

	// Seed walkers from grid nodes that border curved zones, heading into the curved zone.
	const int32 NumSeedNodes = M_Nodes.Num();
	const double HalfBlock = M_Params.GridBlockSize * 0.5;
	const FVector2D CardinalDirections[4] = {{1.0, 0.0}, {-1.0, 0.0}, {0.0, 1.0}, {0.0, -1.0}};

	for (int32 NodeIndex = 0; NodeIndex < NumSeedNodes && WalkerBudget > 0; ++NodeIndex)
	{
		if (not M_Nodes[NodeIndex].bGridNode)
		{
			continue;
		}

		for (const FVector2D& Direction : CardinalDirections)
		{
			const FVector2D Probe = M_Nodes[NodeIndex].Position + Direction * HalfBlock * 1.2;
			if (IsGridZoneAt(Probe) || not IsInsideCity(Probe, M_Params.RoadWidth))
			{
				continue;
			}

			// Only seed when there is space for a natural curve: the stretch right after the
			// junction must be free of other roads.
			const FVector2D ClearanceStart = M_Nodes[NodeIndex].Position
				+ Direction * (IntersectionMaxHalfExtent() + M_Params.RoadWidth);
			const FVector2D ClearanceEnd = ClearanceStart
				+ Direction * (M_Params.CurvedRoadSegmentLength * ScorchedCityGenConstants::CurvedSeedClearanceSegmentFactor);
			if (not IsInsideCity(ClearanceEnd, M_Params.RoadWidth)
				|| IsSegmentBlockedByRoads(ClearanceStart, ClearanceEnd))
			{
				continue;
			}

			const double Heading = FMath::Atan2(Direction.Y, Direction.X)
				+ M_Random.FRandRange(
					-ScorchedCityGenConstants::GridConnectionHeadingJitter,
					ScorchedCityGenConstants::GridConnectionHeadingJitter);
			--WalkerBudget;
			GrowCurvedRoad(NodeIndex, Heading, 0, WalkerBudget);
			if (WalkerBudget <= 0)
			{
				break;
			}
		}
	}

	// Purely curved cities have no grid nodes to seed from: fan out from the area center.
	if (M_Nodes.IsEmpty())
	{
		// The bounds center can fall outside a concave input shape; fall back to the first
		// macro cell center that lies inside the area.
		FVector2D SeedPosition = FVector2D::ZeroVector;
		if (not IsInsideCity(SeedPosition, M_Params.RoadWidth))
		{
			const double BlockSize = M_Params.GridBlockSize;
			for (int32 CellX = M_ZoneCellMin.X; CellX <= M_ZoneCellMax.X; ++CellX)
			{
				for (int32 CellY = M_ZoneCellMin.Y; CellY <= M_ZoneCellMax.Y; ++CellY)
				{
					const FVector2D CellCenter((CellX + 0.5) * BlockSize, (CellY + 0.5) * BlockSize);
					if (IsInsideCity(CellCenter, M_Params.RoadWidth))
					{
						SeedPosition = CellCenter;
						CellX = M_ZoneCellMax.X + 1;
						break;
					}
				}
			}
		}

		const int32 CenterNode = FindOrAddNode(SeedPosition, 1.0, false);
		const int32 NumRadial = FMath::Clamp(FMath::RoundToInt32(FMath::Sqrt(CityArea) / M_Params.GridBlockSize), 3, 8);
		for (int32 RadialIndex = 0; RadialIndex < NumRadial; ++RadialIndex)
		{
			const double Heading = (2.0 * PI * RadialIndex) / NumRadial + M_Random.FRandRange(-0.3, 0.3);
			--WalkerBudget;
			GrowCurvedRoad(CenterNode, Heading, 0, WalkerBudget);
		}
	}
}

void FScorchedCityGenerator::GrowCurvedRoad(
	const int32 StartNodeIndex,
	const double StartHeading,
	const int32 Depth,
	int32& WalkerBudget)
{
	using namespace ScorchedCityGenConstants;

	const double MaxTurn = FMath::DegreesToRadians(
		FMath::Lerp(MinCurveTurnDegrees, MaxCurveTurnDegrees, FMath::Clamp(M_Params.CurvedRoadCurvature, 0.0, 1.0)));
	const double MinSelfDistanceSquared = FMath::Square(M_Params.CurvedRoadSegmentLength * 0.8);

	TArray<FVector2D> Points = {M_Nodes[StartNodeIndex].Position};
	FVector2D Position = Points[0];
	double Heading = StartHeading;
	const bool bStartsOnGridNode = M_Nodes[StartNodeIndex].bGridNode;

	struct FBranchRequest
	{
		FVector2D Position;
		double Heading;
	};
	TArray<FBranchRequest> Branches;

	for (int32 Step = 0; Step < MaxStepsPerWalker; ++Step)
	{
		if (not (bStartsOnGridNode && Step == 0))
		{
			Heading += M_Random.FRandRange(-1.0, 1.0) * MaxTurn;
		}
		const double SegmentLength = M_Params.CurvedRoadSegmentLength
			* (1.0 + M_Params.CurvedRoadVariation * M_Random.FRandRange(-SegmentLengthJitter, SegmentLengthJitter));
		FVector2D Next = Position + FVector2D(FMath::Cos(Heading), FMath::Sin(Heading)) * SegmentLength;

		// Stop at the city boundary; clamp the final segment onto the inset rectangle.
		if (not IsInsideCity(Next, M_Params.RoadWidth))
		{
			const double InsetX = M_Params.CityLengthX * 0.5 - M_Params.RoadWidth;
			const double InsetY = M_Params.CityLengthY * 0.5 - M_Params.RoadWidth;
			Next.X = FMath::Clamp(Next.X, -InsetX, InsetX);
			Next.Y = FMath::Clamp(Next.Y, -InsetY, InsetY);
			if (FVector2D::DistSquared(Position, Next) > FMath::Square(M_Params.RoadWidth))
			{
				Points.Add(Next);
			}
			break;
		}

		// Self-avoidance: never come back near the walker's own earlier points.
		bool bSelfBlocked = false;
		for (int32 PointIndex = 0; PointIndex < Points.Num() - 2 && not bSelfBlocked; ++PointIndex)
		{
			bSelfBlocked = FVector2D::DistSquared(Points[PointIndex], Next) < MinSelfDistanceSquared;
		}
		// The first segment legitimately leaves the start junction, so roads may not block it.
		const bool bRoadBlocked = Step > 0 && IsSegmentBlockedByRoads(Position, Next);
		if (bSelfBlocked || bRoadBlocked || IsGridZoneAt(Next))
		{
			// Try to end in a junction with an existing road instead of a dead stop, but only
			// when reaching it reads as a natural curve (no harsh turn into the junction).
			const int32 SnapNode = FindNearestNode(Next, M_Params.GridBlockSize * 0.45);
			if (SnapNode != INDEX_NONE
				&& SnapNode != StartNodeIndex
				&& not M_Nodes[SnapNode].Edges.IsEmpty())
			{
				const FVector2D ToSnapNode = (M_Nodes[SnapNode].Position - Position).GetSafeNormal();
				const FVector2D HeadingDirection(FMath::Cos(Heading), FMath::Sin(Heading));
				const double MaxTurnCos = FMath::Cos(FMath::DegreesToRadians(MaxSnapConnectTurnDegrees));
				if (FVector2D::DotProduct(ToSnapNode, HeadingDirection) >= MaxTurnCos
					&& not IsConnectionSegmentBlockedByRoads(Position, M_Nodes[SnapNode].Position))
				{
					Points.Add(M_Nodes[SnapNode].Position);
				}
			}
			break;
		}

		Points.Add(Next);
		Position = Next;

		const bool bCanBranch = WalkerBudget > 0 && Depth < MaxWalkerDepth && Points.Num() >= 3;
		if (bCanBranch && M_Random.FRand() < M_Params.CurvedRoadBranchChance)
		{
			const double BranchSide = (M_Random.FRand() < 0.5) ? 1.0 : -1.0;
			const double BranchHeading = Heading + BranchSide
				* FMath::DegreesToRadians(90.0 + M_Params.CurvedRoadVariation * M_Random.FRandRange(-25.0, 25.0));
			Branches.Add(FBranchRequest{Position, BranchHeading});
		}
	}

	if (Points.Num() < 2)
	{
		return;
	}

	const int32 EndNode = FindOrAddNode(Points.Last(), M_Params.RoadWidth * 1.5, false);
	const bool bEndsConnected = EndNode != StartNodeIndex && M_Nodes[EndNode].Edges.Num() > 0;

	// Discard stubs: a curved road must either join another road or be long enough to read
	// as a deliberate curve. Looping back onto the own start node is never natural.
	double PolylineLength = 0.0;
	for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
	{
		PolylineLength += FVector2D::Distance(Points[PointIndex - 1], Points[PointIndex]);
	}
	const double MinCommitLength = M_Params.CurvedRoadSegmentLength * MinCurvedRoadSegmentFactor;
	if (EndNode == StartNodeIndex || (not bEndsConnected && PolylineLength < MinCommitLength))
	{
		// Nothing was committed; branch requests would hang off a road that does not exist.
		return;
	}

	Points.Last() = M_Nodes[EndNode].Position;
	CommitEdge(StartNodeIndex, EndNode, MoveTemp(Points), true, false);

	for (const FBranchRequest& Branch : Branches)
	{
		if (WalkerBudget <= 0)
		{
			break;
		}
		--WalkerBudget;
		const int32 BranchNode = FindOrAddNode(Branch.Position, M_Params.RoadWidth, false);
		GrowCurvedRoad(BranchNode, Branch.Heading, Depth + 1, WalkerBudget);
	}
}

void FScorchedCityGenerator::AddIntersectionFootprints()
{
	for (int32 NodeIndex = 0; NodeIndex < M_Nodes.Num(); ++NodeIndex)
	{
		const FScorchedRoadNode& Node = M_Nodes[NodeIndex];
		if (Node.Edges.Num() < 3)
		{
			continue;
		}

		// Match the spawned intersection mesh exactly: same non-square extents, same yaw.
		FScorchedFootprint Footprint;
		Footprint.Center = Node.Position;
		Footprint.HalfExtents = FVector2D(M_Params.IntersectionSizeX * 0.5, M_Params.IntersectionSizeY * 0.5);
		Footprint.YawRadians = ComputeIntersectionYawAtNode(NodeIndex);
		M_Occupancy.Add(Footprint, EScorchedOccupancy::Intersection);
	}
}

FVector2D FScorchedCityGenerator::EdgeDirectionAwayFromNode(
	const FScorchedRoadEdge& Edge,
	const int32 NodeIndex) const
{
	if (Edge.Points.Num() < 2)
	{
		return FVector2D(1.0, 0.0);
	}

	if (Edge.NodeA == NodeIndex)
	{
		return (Edge.Points[1] - Edge.Points[0]).GetSafeNormal();
	}
	return (Edge.Points[Edge.Points.Num() - 2] - Edge.Points.Last()).GetSafeNormal();
}

double FScorchedCityGenerator::ComputeIntersectionYawAtNode(const int32 NodeIndex) const
{
	const FScorchedRoadNode& Node = M_Nodes[NodeIndex];
	if (Node.bGridNode || Node.Edges.IsEmpty())
	{
		// Grid intersections are axis-aligned in city space.
		return 0.0;
	}

	const FVector2D Direction = EdgeDirectionAwayFromNode(M_Edges[Node.Edges[0]], NodeIndex);
	return FMath::Atan2(Direction.Y, Direction.X);
}

double FScorchedCityGenerator::ComputeEdgeEndClearance(const FScorchedRoadEdge& Edge, const int32 NodeIndex) const
{
	if (not M_Nodes.IsValidIndex(NodeIndex) || M_Nodes[NodeIndex].Edges.Num() < 3)
	{
		// No intersection piece at this end.
		return 0.0;
	}

	FScorchedFootprint MeshFootprint;
	MeshFootprint.HalfExtents = FVector2D(M_Params.IntersectionSizeX * 0.5, M_Params.IntersectionSizeY * 0.5);
	MeshFootprint.YawRadians = ComputeIntersectionYawAtNode(NodeIndex);
	return MeshFootprint.SupportRadius(EdgeDirectionAwayFromNode(Edge, NodeIndex));
}

void FScorchedCityGenerator::RetargetEdgeEndpoint(
	const int32 EdgeIndex,
	const int32 OldNodeIndex,
	const int32 NewNodeIndex,
	const FVector2D& NewEndpointPosition)
{
	FScorchedRoadEdge& Edge = M_Edges[EdgeIndex];
	if (Edge.NodeA == OldNodeIndex)
	{
		Edge.NodeA = NewNodeIndex;
		Edge.Points[0] = NewEndpointPosition;
	}
	else
	{
		Edge.NodeB = NewNodeIndex;
		Edge.Points.Last() = NewEndpointPosition;
	}
	M_Nodes[NewNodeIndex].Edges.Add(EdgeIndex);
}

namespace
{
	/** @brief Quarter-circle fillet from CornerStart to CornerEnd around ArcCenter. */
	TArray<FVector2D> BuildCornerFilletArc(
		const FVector2D& ArcCenter,
		const FVector2D& CornerStart,
		const FVector2D& CornerEnd,
		const int32 NumPoints)
	{
		TArray<FVector2D> ArcPoints;
		ArcPoints.Reserve(NumPoints);

		const FVector2D StartOffset = CornerStart - ArcCenter;
		const FVector2D EndOffset = CornerEnd - ArcCenter;
		const double StartAngle = FMath::Atan2(StartOffset.Y, StartOffset.X);
		double DeltaAngle = FMath::Atan2(EndOffset.Y, EndOffset.X) - StartAngle;
		// Take the short way around (a corner fillet is always a quarter turn or less).
		while (DeltaAngle > PI) { DeltaAngle -= 2.0 * PI; }
		while (DeltaAngle < -PI) { DeltaAngle += 2.0 * PI; }
		const double Radius = StartOffset.Size();

		for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			const double Alpha = static_cast<double>(PointIndex) / (NumPoints - 1);
			const double Angle = StartAngle + DeltaAngle * Alpha;
			ArcPoints.Add(ArcCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}

		// Exact endpoints so the fillet meets the shortened streets without gaps.
		ArcPoints[0] = CornerStart;
		ArcPoints.Last() = CornerEnd;
		return ArcPoints;
	}
}

void FScorchedCityGenerator::SmoothCornerNodes()
{
	using namespace ScorchedCityGenConstants;

	// New nodes appended by the fillets are never corners themselves; cache the count.
	const int32 NumOriginalNodes = M_Nodes.Num();
	for (int32 NodeIndex = 0; NodeIndex < NumOriginalNodes; ++NodeIndex)
	{
		if (M_Nodes[NodeIndex].Edges.Num() != 2)
		{
			continue;
		}

		const int32 EdgeAIndex = M_Nodes[NodeIndex].Edges[0];
		const int32 EdgeBIndex = M_Nodes[NodeIndex].Edges[1];
		const FScorchedRoadEdge& EdgeA = M_Edges[EdgeAIndex];
		const FScorchedRoadEdge& EdgeB = M_Edges[EdgeBIndex];

		// Only plain straight streets meeting at a right angle qualify.
		const bool bBothStraight = not EdgeA.bCurved && not EdgeB.bCurved
			&& EdgeA.Points.Num() == 2 && EdgeB.Points.Num() == 2
			&& EdgeAIndex != EdgeBIndex
			&& EdgeA.NodeA != EdgeA.NodeB && EdgeB.NodeA != EdgeB.NodeB;
		if (not bBothStraight)
		{
			continue;
		}

		const FVector2D DirectionA = EdgeDirectionAwayFromNode(EdgeA, NodeIndex);
		const FVector2D DirectionB = EdgeDirectionAwayFromNode(EdgeB, NodeIndex);
		if (FMath::Abs(FVector2D::DotProduct(DirectionA, DirectionB)) > 0.05)
		{
			continue;
		}

		const double FilletRadius = FMath::Min3(
			M_Params.GridBlockSize * CornerFilletBlockFraction,
			EdgeA.Length() * CornerFilletMaxEdgeFraction,
			EdgeB.Length() * CornerFilletMaxEdgeFraction);
		if (FilletRadius < M_Params.RoadWidth * 1.2)
		{
			continue;
		}

		const FVector2D CornerPosition = M_Nodes[NodeIndex].Position;
		const FVector2D CornerStart = CornerPosition + DirectionA * FilletRadius;
		const FVector2D CornerEnd = CornerPosition + DirectionB * FilletRadius;
		const FVector2D ArcCenter = CornerPosition + DirectionA * FilletRadius + DirectionB * FilletRadius;
		const bool bMajorFillet = EdgeA.bMajor && EdgeB.bMajor;

		// Pull both streets back and bridge them with the curved fillet edge.
		FScorchedRoadNode NewNodeA;
		NewNodeA.Position = CornerStart;
		const int32 NewNodeAIndex = M_Nodes.Add(NewNodeA);
		RetargetEdgeEndpoint(EdgeAIndex, NodeIndex, NewNodeAIndex, CornerStart);

		FScorchedRoadNode NewNodeB;
		NewNodeB.Position = CornerEnd;
		const int32 NewNodeBIndex = M_Nodes.Add(NewNodeB);
		RetargetEdgeEndpoint(EdgeBIndex, NodeIndex, NewNodeBIndex, CornerEnd);

		M_Nodes[NodeIndex].Edges.Reset();

		TArray<FVector2D> ArcPoints =
			BuildCornerFilletArc(ArcCenter, CornerStart, CornerEnd, CornerFilletArcPoints);
		CommitEdge(NewNodeAIndex, NewNodeBIndex, MoveTemp(ArcPoints), true, bMajorFillet);
	}
}

bool FScorchedCityGenerator::IsPolylineBlockedByRoads(
	const TArray<FVector2D>& Points,
	const double EndpointIgnoreDistance) const
{
	double TotalLength = 0.0;
	for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
	{
		TotalLength += FVector2D::Distance(Points[PointIndex - 1], Points[PointIndex]);
	}

	const uint8 RoadMask = ScorchedOccupancyMask(EScorchedOccupancy::Road)
		| ScorchedOccupancyMask(EScorchedOccupancy::Intersection);

	double ArcDistance = 0.0;
	for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
	{
		const FVector2D SegmentStart = Points[PointIndex - 1];
		const FVector2D SegmentEnd = Points[PointIndex];
		const double SegmentLength = FVector2D::Distance(SegmentStart, SegmentEnd);
		const double SegmentMidDistance = ArcDistance + SegmentLength * 0.5;
		ArcDistance += SegmentLength;

		// Both endpoints legitimately touch existing roads; ignore those stretches.
		if (SegmentMidDistance < EndpointIgnoreDistance
			|| SegmentMidDistance > TotalLength - EndpointIgnoreDistance)
		{
			continue;
		}

		const FVector2D Direction = (SegmentEnd - SegmentStart).GetSafeNormal();
		FScorchedFootprint Candidate;
		Candidate.Center = (SegmentStart + SegmentEnd) * 0.5;
		Candidate.HalfExtents = FVector2D(SegmentLength * 0.5, M_Params.RoadWidth * 0.45);
		Candidate.YawRadians = FMath::Atan2(Direction.Y, Direction.X);
		if (M_Occupancy.OverlapsAny(Candidate, RoadMask))
		{
			return true;
		}
	}
	return false;
}

namespace
{
	/** @brief Cubic bezier between two road endings, using their outward directions as tangents. */
	TArray<FVector2D> BuildOrphanConnectionCurve(
		const FVector2D& StartPosition,
		const FVector2D& StartOutward,
		const FVector2D& EndPosition,
		const FVector2D& EndOutward,
		const double SegmentLength,
		const int32 MaxPoints)
	{
		const double Distance = FVector2D::Distance(StartPosition, EndPosition);
		const FVector2D Control1 = StartPosition + StartOutward * (Distance / 3.0);
		const FVector2D Control2 = EndPosition + EndOutward * (Distance / 3.0);
		const int32 NumPoints = FMath::Clamp(
			FMath::CeilToInt32(Distance / FMath::Max(100.0, SegmentLength * 0.5)) + 1, 4, MaxPoints);

		TArray<FVector2D> CurvePoints;
		CurvePoints.Reserve(NumPoints);
		for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			const double Alpha = static_cast<double>(PointIndex) / (NumPoints - 1);
			const double OneMinusAlpha = 1.0 - Alpha;
			CurvePoints.Add(
				StartPosition * (OneMinusAlpha * OneMinusAlpha * OneMinusAlpha)
				+ Control1 * (3.0 * OneMinusAlpha * OneMinusAlpha * Alpha)
				+ Control2 * (3.0 * OneMinusAlpha * Alpha * Alpha)
				+ EndPosition * (Alpha * Alpha * Alpha));
		}
		return CurvePoints;
	}
}

void FScorchedCityGenerator::ConnectOrphanRoadEnds(FScorchedCityGenResult& OutResult)
{
	using namespace ScorchedCityGenConstants;

	struct FOrphanEnd
	{
		int32 NodeIndex = INDEX_NONE;
		FVector2D Position = FVector2D::ZeroVector;
		// Continues the road outward, past its ending.
		FVector2D OutwardDirection = FVector2D(1.0, 0.0);
	};

	TArray<FOrphanEnd> Orphans;
	for (int32 NodeIndex = 0; NodeIndex < M_Nodes.Num(); ++NodeIndex)
	{
		if (M_Nodes[NodeIndex].Edges.Num() != 1)
		{
			continue;
		}

		FOrphanEnd Orphan;
		Orphan.NodeIndex = NodeIndex;
		Orphan.Position = M_Nodes[NodeIndex].Position;
		Orphan.OutwardDirection =
			-EdgeDirectionAwayFromNode(M_Edges[M_Nodes[NodeIndex].Edges[0]], NodeIndex);
		Orphans.Add(Orphan);
	}

	// Pair up endings that face each other, closest pairs first (deterministic order).
	struct FOrphanPair
	{
		int32 IndexA = INDEX_NONE;
		int32 IndexB = INDEX_NONE;
		double Distance = 0.0;
	};
	TArray<FOrphanPair> Pairs;
	const double MaxConnectDistance = M_Params.GridBlockSize * OrphanConnectMaxBlockFactor;

	for (int32 IndexA = 0; IndexA < Orphans.Num(); ++IndexA)
	{
		for (int32 IndexB = IndexA + 1; IndexB < Orphans.Num(); ++IndexB)
		{
			const double Distance = FVector2D::Distance(Orphans[IndexA].Position, Orphans[IndexB].Position);
			if (Distance > MaxConnectDistance || Distance < M_Params.RoadWidth)
			{
				continue;
			}

			const FVector2D AToB = (Orphans[IndexB].Position - Orphans[IndexA].Position) / Distance;
			const bool bFacing =
				FVector2D::DotProduct(Orphans[IndexA].OutwardDirection, AToB) > OrphanConnectMinFacingDot
				&& FVector2D::DotProduct(Orphans[IndexB].OutwardDirection, -AToB) > OrphanConnectMinFacingDot;
			if (bFacing)
			{
				Pairs.Add(FOrphanPair{IndexA, IndexB, Distance});
			}
		}
	}
	Pairs.StableSort([](const FOrphanPair& A, const FOrphanPair& B) { return A.Distance < B.Distance; });

	TSet<int32> ConnectedOrphans;
	for (const FOrphanPair& Pair : Pairs)
	{
		if (ConnectedOrphans.Contains(Pair.IndexA) || ConnectedOrphans.Contains(Pair.IndexB))
		{
			continue;
		}

		const FOrphanEnd& OrphanA = Orphans[Pair.IndexA];
		const FOrphanEnd& OrphanB = Orphans[Pair.IndexB];
		TArray<FVector2D> CurvePoints = BuildOrphanConnectionCurve(
			OrphanA.Position, OrphanA.OutwardDirection,
			OrphanB.Position, OrphanB.OutwardDirection,
			M_Params.CurvedRoadSegmentLength, OrphanCurveMaxPoints);

		bool bCurveInsideCity = true;
		for (const FVector2D& CurvePoint : CurvePoints)
		{
			if (not IsInsideCity(CurvePoint, M_Params.RoadWidth * 0.5))
			{
				bCurveInsideCity = false;
				break;
			}
		}

		if (not bCurveInsideCity
			|| IsPolylineBlockedByRoads(CurvePoints, M_Params.RoadWidth * 1.5))
		{
			continue;
		}

		CommitEdge(OrphanA.NodeIndex, OrphanB.NodeIndex, MoveTemp(CurvePoints), true, false);
		ConnectedOrphans.Add(Pair.IndexA);
		ConnectedOrphans.Add(Pair.IndexB);
	}

	// Whatever is left at the rim gets exported so other PCG logic can connect to it.
	for (int32 OrphanIndex = 0; OrphanIndex < Orphans.Num(); ++OrphanIndex)
	{
		if (ConnectedOrphans.Contains(OrphanIndex))
		{
			continue;
		}

		const FOrphanEnd& Orphan = Orphans[OrphanIndex];
		const FVector2D RimProbe = Orphan.Position
			+ Orphan.OutwardDirection * (M_Params.RoadWidth * OrphanRimProbeRoadWidths);
		if (IsInsideCity(RimProbe, 0.0))
		{
			continue;
		}

		FScorchedOrphanRoadEnd OrphanEnd;
		OrphanEnd.Position = Orphan.Position;
		OrphanEnd.YawRadians = FMath::Atan2(Orphan.OutwardDirection.Y, Orphan.OutwardDirection.X);
		OutResult.OuterOrphanRoads.Add(OrphanEnd);
	}
}

// ---------------------------------------------------------------------------
// Lots
// ---------------------------------------------------------------------------

void FScorchedCityGenerator::BuildLots()
{
	for (int32 EdgeIndex = 0; EdgeIndex < M_Edges.Num(); ++EdgeIndex)
	{
		BuildLotsAlongEdge(EdgeIndex);
	}
}

void FScorchedCityGenerator::BuildLotsAlongEdge(const int32 EdgeIndex)
{
	using namespace ScorchedCityGenConstants;

	const FScorchedRoadEdge& Edge = M_Edges[EdgeIndex];
	const double FullLotWidth = M_Params.LotWidth * (Edge.bMajor ? MajorRoadLotWidthMultiplier : 1.0);
	const double EdgeLength = Edge.Length();

	// Directional per-end clearance, capped so a large 4-way asset can never consume the
	// whole street: mid-block lots must survive or no buildings would spawn at all.
	const double MaxEndClearance = EdgeLength * 0.35;
	const double StartClearance =
		FMath::Min(ComputeEdgeEndClearance(Edge, Edge.NodeA), MaxEndClearance);
	const double EndClearance =
		FMath::Min(ComputeEdgeEndClearance(Edge, Edge.NodeB), MaxEndClearance);
	const double UsableSpan = EdgeLength - StartClearance - EndClearance;

	// Short street (e.g. between two large 4-ways): a full lot does not fit, but the block
	// face should not stay empty — create one narrower centered lot when there is room.
	if (UsableSpan < FullLotWidth)
	{
		if (UsableSpan >= FullLotWidth * MinAdaptiveLotWidthFraction)
		{
			FVector2D RoadPoint;
			FVector2D RoadDirection;
			if (SamplePolyline(Edge.Points, StartClearance + UsableSpan * 0.5, RoadPoint, RoadDirection))
			{
				TryCreateLot(EdgeIndex, RoadPoint, RoadDirection, 1.0, UsableSpan);
				TryCreateLot(EdgeIndex, RoadPoint, RoadDirection, -1.0, UsableSpan);
			}
		}
		return;
	}

	double ArcDistance = StartClearance + FullLotWidth * 0.5;
	while (ArcDistance + FullLotWidth * 0.5 < EdgeLength - EndClearance)
	{
		FVector2D RoadPoint;
		FVector2D RoadDirection;
		if (not SamplePolyline(Edge.Points, ArcDistance, RoadPoint, RoadDirection))
		{
			break;
		}

		TryCreateLot(EdgeIndex, RoadPoint, RoadDirection, 1.0, FullLotWidth);
		TryCreateLot(EdgeIndex, RoadPoint, RoadDirection, -1.0, FullLotWidth);

		ArcDistance += FullLotWidth * (1.0 + LotGapFraction);
	}
}

void FScorchedCityGenerator::TryCreateLot(
	const int32 EdgeIndex,
	const FVector2D& RoadPoint,
	const FVector2D& RoadDirection,
	const double Side,
	const double LotWidth)
{
	using namespace ScorchedCityGenConstants;

	const FScorchedRoadEdge& Edge = M_Edges[EdgeIndex];
	const double LotDepth = M_Params.LotDepth * (Edge.bMajor ? MajorRoadLotDepthMultiplier : 1.0);

	// Normal pointing away from the road on the requested side.
	const FVector2D AwayFromRoad = FVector2D(-RoadDirection.Y, RoadDirection.X) * Side;
	const double CenterOffset = M_Params.RoadWidth * 0.5 + M_Params.RoadSetback
		+ M_Params.MinRoadBuildingDistance + LotDepth * 0.5;

	FScorchedLot Lot;
	Lot.Center = RoadPoint + AwayFromRoad * CenterOffset;
	Lot.HalfExtents = FVector2D(LotWidth * 0.5, LotDepth * 0.5) * LotFootprintShrink;
	Lot.YawRadians = FMath::Atan2(RoadDirection.Y, RoadDirection.X);
	Lot.FacingYawRadians = FMath::Atan2(-AwayFromRoad.Y, -AwayFromRoad.X);
	Lot.EdgeIndex = EdgeIndex;
	Lot.bOnGridRoad = not Edge.bCurved;
	Lot.bMajorRoad = Edge.bMajor;
	Lot.bDense = IsDenseAt(Lot.Center);

	const FVector2D NodeAPosition = M_Nodes[Edge.NodeA].Position;
	const FVector2D NodeBPosition = M_Nodes[Edge.NodeB].Position;
	const double CornerDistanceSquared = FMath::Square(LotWidth * 1.2);
	Lot.bCorner = FVector2D::DistSquared(Lot.Center, NodeAPosition) < CornerDistanceSquared
		|| FVector2D::DistSquared(Lot.Center, NodeBPosition) < CornerDistanceSquared;

	FScorchedFootprint LotFootprint;
	LotFootprint.Center = Lot.Center;
	LotFootprint.HalfExtents = Lot.HalfExtents;
	LotFootprint.YawRadians = Lot.YawRadians;

	if (not IsFootprintInsideCity(LotFootprint, 0.0))
	{
		return;
	}

	// Reject lots that clip other roads (e.g. the far side of a block or a curved road).
	const uint8 RoadMask = ScorchedOccupancyMask(EScorchedOccupancy::Road)
		| ScorchedOccupancyMask(EScorchedOccupancy::Intersection);
	if (M_Occupancy.OverlapsAny(LotFootprint.Inflated(-M_Params.RoadWidth * 0.25), RoadMask))
	{
		return;
	}

	M_Lots.Add(Lot);
}

// ---------------------------------------------------------------------------
// Buildings
// ---------------------------------------------------------------------------

double FScorchedCityGenerator::ComputeBuildingWeightForLot(
	const FScorchedResolvedBuilding& Building,
	const FScorchedLot& Lot) const
{
	double ZoneMultiplier = 1.0;
	switch (Building.ZonePreference)
	{
		case EScorchedZonePreference::GridRoads: ZoneMultiplier = Lot.bOnGridRoad ? 2.0 : 0.4; break;
		case EScorchedZonePreference::CurvedRoads: ZoneMultiplier = Lot.bOnGridRoad ? 0.4 : 2.0; break;
		case EScorchedZonePreference::Corners: ZoneMultiplier = Lot.bCorner ? 3.0 : 0.5; break;
		case EScorchedZonePreference::DenseZones: ZoneMultiplier = Lot.bDense ? 2.0 : 0.5; break;
		case EScorchedZonePreference::SparseZones: ZoneMultiplier = Lot.bDense ? 0.5 : 2.0; break;
		default: break;
	}

	// Believable composition: big buildings gravitate to major roads and corners.
	double SizeMultiplier = 1.0;
	if (Building.SizeCategory == EScorchedBuildingSize::Large)
	{
		SizeMultiplier = (Lot.bMajorRoad || Lot.bCorner) ? 1.8 : 0.35;
	}
	else if (Building.SizeCategory == EScorchedBuildingSize::Small)
	{
		SizeMultiplier = Lot.bMajorRoad ? 0.6 : 1.4;
	}

	// Loose fit pre-filter; the exact rotated fit is tested during placement.
	const double MaxHalfExtent = Building.FootprintHalfExtents.GetMax() + Building.MinSpacing;
	if (MaxHalfExtent > Lot.HalfExtents.GetMax())
	{
		return 0.0;
	}

	return static_cast<double>(Building.Weight) * ZoneMultiplier * SizeMultiplier;
}

int32 FScorchedCityGenerator::PickWeightedBuildingForLot(const FScorchedLot& Lot)
{
	double TotalWeight = 0.0;
	TArray<double, TInlineAllocator<32>> Weights;
	Weights.Reserve(M_Params.Buildings.Num());

	for (const FScorchedResolvedBuilding& Building : M_Params.Buildings)
	{
		const double Weight = ComputeBuildingWeightForLot(Building, Lot);
		Weights.Add(Weight);
		TotalWeight += Weight;
	}

	if (TotalWeight <= 0.0)
	{
		return INDEX_NONE;
	}

	double Pick = M_Random.FRand() * TotalWeight;
	for (int32 BuildingIndex = 0; BuildingIndex < Weights.Num(); ++BuildingIndex)
	{
		Pick -= Weights[BuildingIndex];
		if (Pick <= 0.0)
		{
			return BuildingIndex;
		}
	}
	return Weights.Num() - 1;
}

bool FScorchedCityGenerator::TryPlaceBuildingOnLot(
	const FScorchedLot& Lot,
	const FScorchedResolvedBuilding& Building,
	FScorchedBuildingSpawn& OutSpawn)
{
	using namespace ScorchedCityGenConstants;

	// Build the yaw-offset candidates for the allowed rotation mode.
	TArray<double, TInlineAllocator<8>> YawOffsets;
	switch (Building.RotationMode)
	{
		case EScorchedBuildingRotationMode::FaceRoad90Steps:
			for (int32 StepIndex = 0; StepIndex < 4; ++StepIndex) { YawOffsets.Add(StepIndex * PI * 0.5); }
			break;
		case EScorchedBuildingRotationMode::FaceRoad45Steps:
			for (int32 StepIndex = 0; StepIndex < 8; ++StepIndex) { YawOffsets.Add(StepIndex * PI * 0.25); }
			break;
		case EScorchedBuildingRotationMode::RandomYaw:
			for (int32 RandomIndex = 0; RandomIndex < 3; ++RandomIndex) { YawOffsets.Add(M_Random.FRand() * 2.0 * PI); }
			break;
		default:
			YawOffsets.Add(0.0);
			break;
	}
	// Deterministic shuffle so variety does not always prefer the first offset.
	for (int32 ShuffleIndex = YawOffsets.Num() - 1; ShuffleIndex > 0; --ShuffleIndex)
	{
		YawOffsets.Swap(ShuffleIndex, M_Random.RandRange(0, ShuffleIndex));
	}

	const FVector2D FacingDirection(FMath::Cos(Lot.FacingYawRadians), FMath::Sin(Lot.FacingYawRadians));
	const FVector2D LateralDirection(-FacingDirection.Y, FacingDirection.X);
	const uint8 RoadMask = ScorchedOccupancyMask(EScorchedOccupancy::Road)
		| ScorchedOccupancyMask(EScorchedOccupancy::Intersection);
	const uint8 SpacingMask = ScorchedOccupancyMask(EScorchedOccupancy::Building)
		| ScorchedOccupancyMask(EScorchedOccupancy::PowerPole);

	for (const double YawOffset : YawOffsets)
	{
		const double BuildingYaw = Lot.FacingYawRadians + YawOffset;

		FScorchedFootprint Footprint;
		Footprint.HalfExtents = Building.FootprintHalfExtents;
		Footprint.YawRadians = BuildingYaw;

		// Exact rotated fit inside the lot.
		const double FrontExtent = Footprint.SupportRadius(FacingDirection);
		const double SideExtent = Footprint.SupportRadius(LateralDirection);
		if (FrontExtent > Lot.HalfExtents.Y || SideExtent > Lot.HalfExtents.X)
		{
			continue;
		}

		const double Spacing = M_Params.BuildingSpacingExtra
			+ M_Random.FRandRange(Building.MinSpacing, Building.MaxSpacing);
		const double MaxLateralJitter = (Lot.HalfExtents.X - SideExtent) * 0.8;

		for (int32 Attempt = 0; Attempt < JitterAttemptsPerPick; ++Attempt)
		{
			// Push the building to the road-facing front of the lot, jitter sideways.
			const double LateralJitter = (Attempt == 0)
				? 0.0
				: M_Random.FRandRange(-MaxLateralJitter, MaxLateralJitter);
			Footprint.Center = Lot.Center
				+ FacingDirection * (Lot.HalfExtents.Y - FrontExtent)
				+ LateralDirection * LateralJitter;

			if (not IsFootprintInsideCity(Footprint, 0.0) || IsExcludedAt(Footprint.Center))
			{
				continue;
			}
			if (M_Occupancy.OverlapsAny(Footprint, RoadMask))
			{
				continue;
			}
			if (M_Occupancy.OverlapsAny(Footprint.Inflated(Spacing), SpacingMask))
			{
				continue;
			}

			// Actor pivot = footprint center minus the rotated pivot-to-center offset.
			double SinYaw = 0.0;
			double CosYaw = 0.0;
			FMath::SinCos(&SinYaw, &CosYaw, BuildingYaw);
			const FVector2D& PivotOffset = Building.PivotToFootprintCenter;
			const FVector2D RotatedOffset(
				PivotOffset.X * CosYaw - PivotOffset.Y * SinYaw,
				PivotOffset.X * SinYaw + PivotOffset.Y * CosYaw);

			OutSpawn.ActorPosition = Footprint.Center - RotatedOffset;
			OutSpawn.YawRadians = BuildingYaw;
			OutSpawn.Footprint = Footprint;
			return true;
		}
	}
	return false;
}

void FScorchedCityGenerator::PlaceBuildings(FScorchedCityGenResult& OutResult)
{
	using namespace ScorchedCityGenConstants;

	if (M_Params.Buildings.IsEmpty())
	{
		return;
	}

	// Fill important lots first so large buildings can claim major roads and corners.
	TArray<int32> LotOrder;
	LotOrder.Reserve(M_Lots.Num());
	for (int32 LotIndex = 0; LotIndex < M_Lots.Num(); ++LotIndex)
	{
		LotOrder.Add(LotIndex);
	}
	LotOrder.StableSort([this](const int32 A, const int32 B)
	{
		const FScorchedLot& LotA = M_Lots[A];
		const FScorchedLot& LotB = M_Lots[B];
		if (LotA.bMajorRoad != LotB.bMajorRoad)
		{
			return LotA.bMajorRoad;
		}
		return LotA.bCorner && not LotB.bCorner;
	});

	TSet<int32> EdgesWithBuilding;
	for (const int32 LotIndex : LotOrder)
	{
		FScorchedLot& Lot = M_Lots[LotIndex];

		// Density gate: dense zones fill most lots, sparse zones leave gaps.
		const double UseChance = FMath::Clamp(
			M_Params.OverallDensity
			* (Lot.bDense ? DenseZoneLotChanceMultiplier : SparseZoneLotChanceMultiplier),
			0.0, 1.0);
		if (M_Random.FRand() > UseChance)
		{
			continue;
		}

		if (TryFillLotWithAnyBuilding(Lot, OutResult))
		{
			EdgesWithBuilding.Add(Lot.EdgeIndex);
		}
	}

	// Guarantee pass: the density gate can randomly skip every lot of a block, leaving whole
	// squares empty. At moderate densities, force at least one building per street that has
	// lots, so identical blocks never differ between "filled" and "completely empty".
	if (M_Params.OverallDensity < MinDensityForEdgeFill)
	{
		return;
	}
	for (const int32 LotIndex : LotOrder)
	{
		FScorchedLot& Lot = M_Lots[LotIndex];
		if (Lot.bUsed || EdgesWithBuilding.Contains(Lot.EdgeIndex))
		{
			continue;
		}

		if (TryFillLotWithAnyBuilding(Lot, OutResult))
		{
			EdgesWithBuilding.Add(Lot.EdgeIndex);
		}
	}
}

bool FScorchedCityGenerator::TryFillLotWithAnyBuilding(FScorchedLot& Lot, FScorchedCityGenResult& OutResult)
{
	using namespace ScorchedCityGenConstants;

	for (int32 PickIndex = 0; PickIndex < BuildingPicksPerLot; ++PickIndex)
	{
		const int32 BuildingIndex = PickWeightedBuildingForLot(Lot);
		if (BuildingIndex == INDEX_NONE)
		{
			break;
		}

		FScorchedBuildingSpawn Spawn;
		if (not TryPlaceBuildingOnLot(Lot, M_Params.Buildings[BuildingIndex], Spawn))
		{
			continue;
		}

		Spawn.AssetIndex = BuildingIndex;
		M_Occupancy.Add(Spawn.Footprint, EScorchedOccupancy::Building);
		OutResult.Buildings.Add(Spawn);
		Lot.bUsed = true;
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Road-side objects
// ---------------------------------------------------------------------------

int32 FScorchedCityGenerator::PickWeightedRoadSideEntry(const TArray<FScorchedResolvedRoadSideEntry>& Entries)
{
	double TotalWeight = 0.0;
	for (const FScorchedResolvedRoadSideEntry& Entry : Entries)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}
	if (TotalWeight <= 0.0)
	{
		return INDEX_NONE;
	}

	double Pick = M_Random.FRand() * TotalWeight;
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		Pick -= FMath::Max(0.0f, Entries[EntryIndex].Weight);
		if (Pick <= 0.0)
		{
			return EntryIndex;
		}
	}
	return Entries.Num() - 1;
}

void FScorchedCityGenerator::PlaceRoadLanterns(FScorchedCityGenResult& OutResult)
{
	if (M_Params.RoadLanterns.IsEmpty())
	{
		return;
	}

	// Lanterns avoid everything already placed; blocked spots are simply skipped.
	const uint8 BlockMask = ScorchedOccupancyMaskAll() & ~ScorchedOccupancyMask(EScorchedOccupancy::Decal);

	for (const FScorchedRoadEdge& Edge : M_Edges)
	{
		const double EdgeLength = Edge.Length();
		double Side = (M_Random.FRand() < 0.5) ? 1.0 : -1.0;

		for (double ArcDistance = M_Params.RoadLanternSpacing * 0.5;
			ArcDistance < EdgeLength;
			ArcDistance += M_Params.RoadLanternSpacing, Side = -Side)
		{
			FVector2D RoadPoint;
			FVector2D RoadDirection;
			if (not SamplePolyline(Edge.Points, ArcDistance, RoadPoint, RoadDirection))
			{
				break;
			}

			const int32 EntryIndex = PickWeightedRoadSideEntry(M_Params.RoadLanterns);
			if (EntryIndex == INDEX_NONE)
			{
				return;
			}
			const FScorchedResolvedRoadSideEntry& Entry = M_Params.RoadLanterns[EntryIndex];

			// Facing the road: local +X points from the object toward the road centerline.
			const FVector2D AwayFromRoad = FVector2D(-RoadDirection.Y, RoadDirection.X) * Side;
			const double FacingYaw = FMath::Atan2(-AwayFromRoad.Y, -AwayFromRoad.X);
			const FVector2D PivotPosition = RoadPoint + AwayFromRoad
				* (M_Params.RoadWidth * 0.5 + M_Params.RoadLanternOffsetFromRoadEdge + Entry.FootprintHalfExtents.X);

			double SinYaw = 0.0;
			double CosYaw = 0.0;
			FMath::SinCos(&SinYaw, &CosYaw, FacingYaw);
			const FVector2D& PivotOffset = Entry.PivotToFootprintCenter;

			FScorchedFootprint Footprint;
			Footprint.Center = PivotPosition + FVector2D(
				PivotOffset.X * CosYaw - PivotOffset.Y * SinYaw,
				PivotOffset.X * SinYaw + PivotOffset.Y * CosYaw);
			Footprint.HalfExtents = Entry.FootprintHalfExtents;
			Footprint.YawRadians = FacingYaw;

			if (not IsFootprintInsideCity(Footprint, 0.0) || IsExcludedAt(PivotPosition)
				|| M_Occupancy.OverlapsAny(Footprint, BlockMask))
			{
				continue;
			}

			M_Occupancy.Add(Footprint, EScorchedOccupancy::Auxiliary);

			FScorchedAuxiliarySpawn Spawn;
			Spawn.AssetIndex = EntryIndex;
			Spawn.Position = PivotPosition;
			Spawn.YawRadians = FacingYaw;
			OutResult.RoadLanterns.Add(Spawn);
		}
	}
}

void FScorchedCityGenerator::PlaceRoadBlocks(FScorchedCityGenResult& OutResult)
{
	if (M_Params.RoadBlockTypes.IsEmpty() || M_Params.RoadBlockChance <= 0.0)
	{
		return;
	}

	for (const FScorchedRoadEdge& Edge : M_Edges)
	{
		const double EdgeLength = Edge.Length();
		for (double ArcDistance = M_Params.RoadBlockInterval * 0.5;
			ArcDistance < EdgeLength;
			ArcDistance += M_Params.RoadBlockInterval)
		{
			// Seeded chance: either this candidate spot gets a block, or we move to the next.
			if (M_Random.FRand() > M_Params.RoadBlockChance)
			{
				continue;
			}

			FVector2D RoadPoint;
			FVector2D RoadDirection;
			if (not SamplePolyline(Edge.Points, ArcDistance, RoadPoint, RoadDirection))
			{
				break;
			}

			// Never across an intersection piece.
			FScorchedFootprint CenterProbe;
			CenterProbe.Center = RoadPoint;
			CenterProbe.HalfExtents = FVector2D(M_Params.RoadWidth * 0.5, M_Params.RoadWidth * 0.5);
			if (M_Occupancy.OverlapsAny(CenterProbe, ScorchedOccupancyMask(EScorchedOccupancy::Intersection)))
			{
				continue;
			}

			BuildRoadBlockAt(RoadPoint, RoadDirection, OutResult);
		}
	}
}

void FScorchedCityGenerator::BuildRoadBlockAt(
	const FVector2D& RoadPoint,
	const FVector2D& RoadDirection,
	FScorchedCityGenResult& OutResult)
{
	using namespace ScorchedCityGenConstants;

	// One road block always uses a single item type.
	const int32 TypeIndex = PickWeightedRoadSideEntry(M_Params.RoadBlockTypes);
	if (TypeIndex == INDEX_NONE)
	{
		return;
	}
	const FScorchedResolvedRoadSideEntry& Type = M_Params.RoadBlockTypes[TypeIndex];

	// Radius from the yaw span so the arc's lateral reach always covers the road width:
	// 2 * R * sin(Span/2) == RoadWidth. A 180 degree span becomes a U across the road.
	const double SpanRadians = FMath::DegreesToRadians(M_Random.FRandRange(
		M_Params.RoadBlockMinYawSpanDegrees,
		FMath::Max(M_Params.RoadBlockMinYawSpanDegrees, M_Params.RoadBlockMaxYawSpanDegrees)));
	const double HalfSpanSin = FMath::Max(0.05, FMath::Sin(FMath::Min(SpanRadians, PI) * 0.5));
	const double Radius = M_Params.RoadWidth / (2.0 * HalfSpanSin);

	// The arc opens along the road; flip decides which oncoming direction it faces.
	const double RoadYaw = FMath::Atan2(RoadDirection.Y, RoadDirection.X);
	const double BaseAngle = RoadYaw + ((M_Random.FRand() < 0.5) ? 0.0 : PI);

	// Item count from the type's real bounds so neighbors never overlap along the arc.
	const double ItemSize = FMath::Max(20.0, Type.FootprintHalfExtents.GetMax() * 2.0);
	const double ArcLength = Radius * SpanRadians;
	const int32 ItemCount = FMath::Clamp(
		FMath::FloorToInt32(ArcLength / (ItemSize * RoadBlockItemSpacingFactor)) + 1,
		MinRoadBlockItems, MaxRoadBlockItems);

	for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex)
	{
		const double Alpha = (ItemCount == 1) ? 0.5 : static_cast<double>(ItemIndex) / (ItemCount - 1);
		const double ItemAngle = BaseAngle - SpanRadians * 0.5 + SpanRadians * Alpha;
		const FVector2D RadialDirection(FMath::Cos(ItemAngle), FMath::Sin(ItemAngle));
		const FVector2D PivotPosition = RoadPoint + RadialDirection * Radius;
		// Each item faces outward, toward whatever approaches the block.
		const double ItemYaw = ItemAngle;

		double SinYaw = 0.0;
		double CosYaw = 0.0;
		FMath::SinCos(&SinYaw, &CosYaw, ItemYaw);
		const FVector2D& PivotOffset = Type.PivotToFootprintCenter;

		FScorchedFootprint Footprint;
		Footprint.Center = PivotPosition + FVector2D(
			PivotOffset.X * CosYaw - PivotOffset.Y * SinYaw,
			PivotOffset.X * SinYaw + PivotOffset.Y * CosYaw);
		Footprint.HalfExtents = Type.FootprintHalfExtents;
		Footprint.YawRadians = ItemYaw;

		// Road blocks intentionally sit on roads; only solid obstacles reject an item,
		// which leaves believable gaps instead of dropping the whole block.
		const uint8 BlockMask = ScorchedOccupancyMask(EScorchedOccupancy::Building)
			| ScorchedOccupancyMask(EScorchedOccupancy::PowerPole)
			| ScorchedOccupancyMask(EScorchedOccupancy::Auxiliary);
		if (not IsFootprintInsideCity(Footprint, 0.0) || IsExcludedAt(PivotPosition)
			|| M_Occupancy.OverlapsAny(Footprint, BlockMask))
		{
			continue;
		}

		M_Occupancy.Add(Footprint, EScorchedOccupancy::Auxiliary);

		FScorchedAuxiliarySpawn Spawn;
		Spawn.AssetIndex = TypeIndex;
		Spawn.Position = PivotPosition;
		Spawn.YawRadians = ItemYaw;
		OutResult.RoadBlockItems.Add(Spawn);
	}
}

// ---------------------------------------------------------------------------
// Auxiliary blueprints
// ---------------------------------------------------------------------------

void FScorchedCityGenerator::PlaceAuxiliaryBlueprints(FScorchedCityGenResult& OutResult)
{
	for (int32 AuxIndex = 0; AuxIndex < M_Params.AuxiliaryBlueprints.Num(); ++AuxIndex)
	{
		const FScorchedResolvedAuxiliary& Aux = M_Params.AuxiliaryBlueprints[AuxIndex];

		// Around each placed building: sample a ring measured from the footprint edge.
		if (Aux.CountPerBuilding > 0.0f)
		{
			for (const FScorchedBuildingSpawn& Building : OutResult.Buildings)
			{
				const int32 Count = ComputeDecalCount(Aux.CountPerBuilding, 1.0);
				for (int32 SampleIndex = 0; SampleIndex < Count; ++SampleIndex)
				{
					const double Angle = M_Random.FRand() * 2.0 * PI;
					const FVector2D RingDirection(FMath::Cos(Angle), FMath::Sin(Angle));
					const double EdgeDistance = Building.Footprint.SupportRadius(RingDirection)
						+ M_Random.FRandRange(Aux.MinDistanceFromBuilding, Aux.MaxDistanceFromBuilding);
					TryPlaceAuxiliaryAt(
						AuxIndex, Building.Footprint.Center + RingDirection * EdgeDistance, OutResult);
				}
			}
		}

		// Around the squares: sample points inside the generated lots along every block face.
		if (Aux.CountPerLot > 0.0f)
		{
			for (const FScorchedLot& Lot : M_Lots)
			{
				const int32 Count = ComputeDecalCount(Aux.CountPerLot, 1.0);
				for (int32 SampleIndex = 0; SampleIndex < Count; ++SampleIndex)
				{
					double SinYaw = 0.0;
					double CosYaw = 0.0;
					FMath::SinCos(&SinYaw, &CosYaw, Lot.YawRadians);
					const FVector2D LocalOffset(
						M_Random.FRandRange(-Lot.HalfExtents.X, Lot.HalfExtents.X),
						M_Random.FRandRange(-Lot.HalfExtents.Y, Lot.HalfExtents.Y));
					const FVector2D SamplePosition = Lot.Center + FVector2D(
						LocalOffset.X * CosYaw - LocalOffset.Y * SinYaw,
						LocalOffset.X * SinYaw + LocalOffset.Y * CosYaw);
					TryPlaceAuxiliaryAt(AuxIndex, SamplePosition, OutResult);
				}
			}
		}
	}
}

bool FScorchedCityGenerator::TryPlaceAuxiliaryAt(
	const int32 AuxIndex,
	const FVector2D& BasePosition,
	FScorchedCityGenResult& OutResult)
{
	using namespace ScorchedCityGenConstants;

	const FScorchedResolvedAuxiliary& Aux = M_Params.AuxiliaryBlueprints[AuxIndex];

	// Roads are always avoided; buildings/poles only when the entry asks for it.
	uint8 BlockMask = ScorchedOccupancyMask(EScorchedOccupancy::Road)
		| ScorchedOccupancyMask(EScorchedOccupancy::Intersection)
		| ScorchedOccupancyMask(EScorchedOccupancy::Auxiliary);
	if (Aux.bCheckBuildingCollision)
	{
		BlockMask |= ScorchedOccupancyMask(EScorchedOccupancy::Building);
	}
	if (Aux.bCheckPoleCollision)
	{
		BlockMask |= ScorchedOccupancyMask(EScorchedOccupancy::PowerPole);
	}

	const double Scale = Aux.bOverrideScale
		? M_Random.FRandRange(Aux.MinScale, FMath::Max(Aux.MinScale, Aux.MaxScale))
		: 1.0;
	const double Yaw = M_Random.FRand() * 2.0 * PI;
	const double JitterRadius = Aux.FootprintHalfExtents.GetMax() * Scale + M_Params.RoadWidth * 0.25;

	double SinYaw = 0.0;
	double CosYaw = 0.0;
	FMath::SinCos(&SinYaw, &CosYaw, Yaw);
	const FVector2D ScaledPivotOffset = Aux.PivotToFootprintCenter * Scale;
	const FVector2D RotatedPivotOffset(
		ScaledPivotOffset.X * CosYaw - ScaledPivotOffset.Y * SinYaw,
		ScaledPivotOffset.X * SinYaw + ScaledPivotOffset.Y * CosYaw);

	for (int32 Attempt = 0; Attempt < AuxiliaryPlacementAttempts; ++Attempt)
	{
		FVector2D PivotPosition = BasePosition;
		if (Attempt > 0)
		{
			// Collision failed at the sampled spot: try a few positions around it.
			const double JitterAngle = M_Random.FRand() * 2.0 * PI;
			PivotPosition += FVector2D(FMath::Cos(JitterAngle), FMath::Sin(JitterAngle))
				* (JitterRadius * M_Random.FRandRange(0.5, 1.5));
		}

		FScorchedFootprint Footprint;
		Footprint.Center = PivotPosition + RotatedPivotOffset;
		Footprint.HalfExtents = Aux.FootprintHalfExtents * Scale;
		Footprint.YawRadians = Yaw;

		if (not IsFootprintInsideCity(Footprint, 0.0) || IsExcludedAt(PivotPosition))
		{
			continue;
		}
		if (M_Occupancy.OverlapsAny(Footprint, BlockMask))
		{
			continue;
		}

		M_Occupancy.Add(Footprint, EScorchedOccupancy::Auxiliary);

		FScorchedAuxiliarySpawn Spawn;
		Spawn.AssetIndex = AuxIndex;
		Spawn.Position = PivotPosition;
		Spawn.YawRadians = Yaw;
		Spawn.UniformScale = Scale;
		OutResult.Auxiliaries.Add(Spawn);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Power lines
// ---------------------------------------------------------------------------

void FScorchedCityGenerator::PlacePowerLines(FScorchedCityGenResult& OutResult)
{
	if (not M_Params.PowerLines.bEnablePowerLines)
	{
		return;
	}

	// Per-edge usability is decided inside PlacePowerLinesAlongEdge with capped clearances,
	// so large intersection assets can never silence power lines city-wide.
	for (const FScorchedRoadEdge& Edge : M_Edges)
	{
		PlacePowerLinesAlongEdge(Edge, OutResult);
	}
}

void FScorchedCityGenerator::PlacePowerLinesAlongEdge(
	const FScorchedRoadEdge& Edge,
	FScorchedCityGenResult& OutResult)
{
	const FScorchedPowerLineSettings& Settings = M_Params.PowerLines;
	const double Side = (M_Random.FRand() < 0.5) ? 1.0 : -1.0;
	const double EdgeOffset = M_Params.RoadWidth * 0.5 + Settings.OffsetFromRoadEdge;

	// Walk the whole edge: every pole spot is occupancy-tested (intersections included), so
	// large arc clearances are redundant — they shrank the usable span below one spacing on
	// normal streets, which meant the two-pole pair never fit and only single poles spawned.
	const double EdgeLength = Edge.Length();
	const double StartClearance = Settings.PoleClearance;
	const double UsableLength = EdgeLength - Settings.PoleClearance;
	if (UsableLength - StartClearance < Settings.PowerLineSpacing * 0.5)
	{
		return;
	}

	uint8 BlockMask = ScorchedOccupancyMask(EScorchedOccupancy::Road)
		| ScorchedOccupancyMask(EScorchedOccupancy::Intersection)
		| ScorchedOccupancyMask(EScorchedOccupancy::PowerPole);
	if (Settings.bAvoidBuildingOverlap)
	{
		BlockMask |= ScorchedOccupancyMask(EScorchedOccupancy::Building);
	}

	// Returns the pole footprint at an arc distance, or false when the spot is unusable.
	auto ComputePole = [&](const double ArcDistance, FVector2D& OutPosition, double& OutRoadYaw) -> bool
	{
		FVector2D RoadPoint;
		FVector2D RoadDirection;
		if (not SamplePolyline(Edge.Points, ArcDistance, RoadPoint, RoadDirection))
		{
			return false;
		}

		const FVector2D Normal = FVector2D(-RoadDirection.Y, RoadDirection.X) * Side;
		OutPosition = RoadPoint + Normal * EdgeOffset;
		OutRoadYaw = FMath::Atan2(RoadDirection.Y, RoadDirection.X);

		FScorchedFootprint Clearance;
		Clearance.Center = OutPosition;
		Clearance.HalfExtents = FVector2D(Settings.PoleClearance, Settings.PoleClearance);
		Clearance.YawRadians = OutRoadYaw;
		return IsInsideCity(OutPosition, 0.0) && not M_Occupancy.OverlapsAny(Clearance, BlockMask);
	};

	auto InsertPoleFootprint = [&](const FVector2D& Position, const double Yaw)
	{
		FScorchedFootprint Pole;
		Pole.Center = Position;
		Pole.HalfExtents = FVector2D(Settings.PoleClearance, Settings.PoleClearance);
		Pole.YawRadians = Yaw;
		M_Occupancy.Add(Pole, EScorchedOccupancy::PowerPole);
	};

	double ArcDistance = StartClearance;
	while (ArcDistance < UsableLength)
	{
		FVector2D PolePosition;
		double RoadYaw = 0.0;
		if (not ComputePole(ArcDistance, PolePosition, RoadYaw))
		{
			ArcDistance += Settings.PowerLineSpacing;
			continue;
		}

		FVector2D NextPolePosition;
		double NextRoadYaw = 0.0;
		const double NextArcDistance = ArcDistance + Settings.PowerLineSpacing;
		const bool bNextValid = NextArcDistance < UsableLength
			&& ComputePole(NextArcDistance, NextPolePosition, NextRoadYaw);

		if (bNextValid && M_Random.FRand() >= Settings.BrokenSectionChance)
		{
			// Two-pole section spanning toward the next pole position.
			const FVector2D Span = NextPolePosition - PolePosition;
			FScorchedPoleSpawn Spawn;
			Spawn.bTwoPole = true;
			Spawn.Position = PolePosition;
			Spawn.SecondPosition = NextPolePosition;
			Spawn.YawRadians = FMath::Atan2(Span.Y, Span.X);
			OutResult.Poles.Add(Spawn);

			InsertPoleFootprint(PolePosition, RoadYaw);
			InsertPoleFootprint(NextPolePosition, NextRoadYaw);
			// Leave one spacing of slack so chained assets never double up a pole.
			ArcDistance = NextArcDistance + Settings.PowerLineSpacing;
		}
		else
		{
			// Broken section, endpoint, or isolated pole.
			FScorchedPoleSpawn Spawn;
			Spawn.bTwoPole = false;
			Spawn.Position = PolePosition;
			Spawn.YawRadians = Settings.bFollowRoadDirection ? RoadYaw : M_Random.FRand() * 2.0 * PI;
			OutResult.Poles.Add(Spawn);

			InsertPoleFootprint(PolePosition, RoadYaw);
			ArcDistance += Settings.PowerLineSpacing;
		}
	}
}

// ---------------------------------------------------------------------------
// Scatter
// ---------------------------------------------------------------------------

void FScorchedCityGenerator::BuildScatter(
	const FScorchedCityGenResult& ResultSoFar,
	FScorchedCityGenResult& OutResult)
{
	const int32 NumBuildings = ResultSoFar.Buildings.Num();
	for (int32 BuildingIndex = 0; BuildingIndex < NumBuildings; ++BuildingIndex)
	{
		for (int32 ProfileIndex = 0; ProfileIndex < M_Params.ScatterProfiles.Num(); ++ProfileIndex)
		{
			BuildScatterForBuilding(ResultSoFar.Buildings[BuildingIndex], ProfileIndex, OutResult);
		}
	}
}

void FScorchedCityGenerator::BuildScatterForBuilding(
	const FScorchedBuildingSpawn& Building,
	const int32 ProfileIndex,
	FScorchedCityGenResult& OutResult)
{
	using namespace ScorchedCityGenConstants;

	const FScorchedScatterProfile& Profile = M_Params.ScatterProfiles[ProfileIndex];
	const TArray<double>& MeshRadii = M_Params.ScatterMeshRadii[ProfileIndex];
	if (Profile.Meshes.IsEmpty() || M_Random.FRand() > Profile.ChancePerBuilding)
	{
		return;
	}

	const double ZoneMultiplier = IsDenseAt(Building.Footprint.Center)
		? DenseZoneScatterMultiplier
		: SparseZoneScatterMultiplier;
	const int32 Count = FMath::RoundToInt32(
		Profile.MeshesPerBuilding * ZoneMultiplier * M_Random.FRandRange(0.7, 1.3));

	// Optional clustering: group samples around a few ring positions instead of a uniform ring.
	const bool bClustered = M_Random.FRand() < Profile.ClusterAmount;
	TArray<FVector2D, TInlineAllocator<4>> ClusterCenters;
	if (bClustered)
	{
		const int32 NumClusters = FMath::Clamp(1 + Count / 5, 1, 4);
		for (int32 ClusterIndex = 0; ClusterIndex < NumClusters; ++ClusterIndex)
		{
			const double Angle = M_Random.FRand() * 2.0 * PI;
			const FVector2D RingDirection(FMath::Cos(Angle), FMath::Sin(Angle));
			const double EdgeDistance = Building.Footprint.SupportRadius(RingDirection)
				+ M_Random.FRandRange(Profile.MinDistanceFromBuildingBounds, Profile.MaxDistanceFromBuildingBounds);
			ClusterCenters.Add(Building.Footprint.Center + RingDirection * EdgeDistance);
		}
	}

	uint8 BlockMask = 0;
	if (Profile.bAvoidRoads)
	{
		BlockMask |= ScorchedOccupancyMask(EScorchedOccupancy::Road)
			| ScorchedOccupancyMask(EScorchedOccupancy::Intersection);
	}
	if (Profile.bAvoidBuildings)
	{
		BlockMask |= ScorchedOccupancyMask(EScorchedOccupancy::Building);
	}

	double TotalMeshWeight = 0.0;
	for (const FScorchedScatterMeshEntry& Entry : Profile.Meshes)
	{
		TotalMeshWeight += FMath::Max(0.0f, Entry.Weight);
	}
	if (TotalMeshWeight <= 0.0)
	{
		return;
	}

	for (int32 SampleIndex = 0; SampleIndex < Count; ++SampleIndex)
	{
		FVector2D SamplePosition;
		if (bClustered)
		{
			const FVector2D& ClusterCenter = ClusterCenters[SampleIndex % ClusterCenters.Num()];
			const double Angle = M_Random.FRand() * 2.0 * PI;
			SamplePosition = ClusterCenter
				+ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (M_Random.FRand() * Profile.ClusterRadius);
		}
		else
		{
			const double Angle = M_Random.FRand() * 2.0 * PI;
			const FVector2D RingDirection(FMath::Cos(Angle), FMath::Sin(Angle));
			const double EdgeDistance = Building.Footprint.SupportRadius(RingDirection)
				+ M_Random.FRandRange(Profile.MinDistanceFromBuildingBounds, Profile.MaxDistanceFromBuildingBounds);
			SamplePosition = Building.Footprint.Center + RingDirection * EdgeDistance;
		}

		// Weighted mesh pick.
		double MeshPick = M_Random.FRand() * TotalMeshWeight;
		int32 MeshIndex = 0;
		for (int32 EntryIndex = 0; EntryIndex < Profile.Meshes.Num(); ++EntryIndex)
		{
			MeshPick -= FMath::Max(0.0f, Profile.Meshes[EntryIndex].Weight);
			if (MeshPick <= 0.0)
			{
				MeshIndex = EntryIndex;
				break;
			}
		}

		const double Scale = M_Random.FRandRange(Profile.MinScale, Profile.MaxScale);

		if (not IsInsideCity(SamplePosition, 0.0) || IsExcludedAt(SamplePosition))
		{
			continue;
		}

		if (BlockMask != 0)
		{
			FScorchedFootprint Candidate;
			Candidate.Center = SamplePosition;
			const double CandidateHalf = FMath::Max(10.0, MeshRadii[MeshIndex] * Scale * ScatterFootprintFraction);
			Candidate.HalfExtents = FVector2D(CandidateHalf, CandidateHalf);
			if (M_Occupancy.OverlapsAny(Candidate, BlockMask))
			{
				continue;
			}
		}

		FScorchedScatterCandidate Candidate;
		Candidate.ProfileIndex = ProfileIndex;
		Candidate.MeshIndex = MeshIndex;
		Candidate.Position = SamplePosition;
		Candidate.YawRadians = Profile.bRandomYaw ? M_Random.FRand() * 2.0 * PI : Building.YawRadians;
		Candidate.UniformScale = Scale;
		Candidate.bAlignToGround = Profile.bAlignToGround;
		OutResult.Scatter.Add(Candidate);
	}
}

// ---------------------------------------------------------------------------
// Decals
// ---------------------------------------------------------------------------

namespace
{
	FVector2D RotateDecalOffset(const FVector2D& Offset, const double YawRadians)
	{
		double SinYaw = 0.0;
		double CosYaw = 0.0;
		FMath::SinCos(&SinYaw, &CosYaw, YawRadians);
		return FVector2D(
			Offset.X * CosYaw - Offset.Y * SinYaw,
			Offset.X * SinYaw + Offset.Y * CosYaw);
	}

	FVector2D SampleDecalPointInFootprint(
		const FScorchedFootprint& Footprint,
		const double Size,
		FRandomStream& Random)
	{
		const double Margin = Size * 0.5;
		const double RangeX = FMath::Max(0.0, Footprint.HalfExtents.X - Margin);
		const double RangeY = FMath::Max(0.0, Footprint.HalfExtents.Y - Margin);
		const FVector2D LocalOffset(Random.FRandRange(-RangeX, RangeX), Random.FRandRange(-RangeY, RangeY));
		return Footprint.Center + RotateDecalOffset(LocalOffset, Footprint.YawRadians);
	}
}

int32 FScorchedCityGenerator::ComputeDecalCount(const double Density, const double UnitCount)
{
	const double CountFloat = FMath::Max(0.0, Density) * FMath::Max(0.0, UnitCount);
	const int32 BaseCount = FMath::FloorToInt32(CountFloat);
	return BaseCount + (M_Random.FRand() < CountFloat - BaseCount ? 1 : 0);
}

int32 FScorchedCityGenerator::PickWeightedDecalEntry(const FScorchedDecalPlacementSettings& Settings)
{
	double TotalWeight = 0.0;
	for (const FScorchedDecalEntry& Entry : Settings.Decals)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}

	if (TotalWeight <= 0.0)
	{
		return INDEX_NONE;
	}

	double Pick = M_Random.FRand() * TotalWeight;
	for (int32 EntryIndex = 0; EntryIndex < Settings.Decals.Num(); ++EntryIndex)
	{
		Pick -= FMath::Max(0.0f, Settings.Decals[EntryIndex].Weight);
		if (Pick <= 0.0)
		{
			return EntryIndex;
		}
	}
	return Settings.Decals.Num() - 1;
}

bool FScorchedCityGenerator::TryReserveDecal(
	const FScorchedDecalPlacementSettings& Settings,
	const EScorchedDecalSet Set,
	const int32 EntryIndex,
	const FVector2D& Position,
	const double YawRadians,
	const double Size,
	FScorchedCityGenResult& OutResult)
{
	if (not Settings.Decals.IsValidIndex(EntryIndex) || not IsInsideCity(Position, 0.0))
	{
		return false;
	}

	FScorchedFootprint Footprint;
	Footprint.Center = Position;
	Footprint.HalfExtents = FVector2D(Size * 0.5, Size * 0.5);
	Footprint.YawRadians = YawRadians;

	const double Separation = Size * 0.5 * FMath::Clamp(static_cast<double>(Settings.Scatter), 0.0, 1.0);
	if (M_DecalOccupancy.OverlapsAny(Footprint.Inflated(Separation),
		ScorchedOccupancyMask(EScorchedOccupancy::Decal)))
	{
		return false;
	}

	M_DecalOccupancy.Add(Footprint, EScorchedOccupancy::Decal);

	FScorchedDecalSpawn Spawn;
	Spawn.Set = Set;
	Spawn.EntryIndex = EntryIndex;
	Spawn.Position = Position;
	Spawn.YawRadians = YawRadians;
	Spawn.Size = Size;
	OutResult.Decals.Add(Spawn);
	return true;
}

void FScorchedCityGenerator::BuildDecals(FScorchedCityGenResult& OutResult)
{
	BuildBuildingFootprintDecals(OutResult);
	BuildRoadDecals(OutResult);
	BuildLotDecals(OutResult);
	BuildPowerLineDecals(OutResult);
}

void FScorchedCityGenerator::BuildDecalsInFootprint(
	const FScorchedDecalPlacementSettings& Settings,
	const EScorchedDecalSet Set,
	const FScorchedFootprint& Area,
	const double UnitCount,
	const double BaseYawRadians,
	FScorchedCityGenResult& OutResult)
{
	if (Settings.Density <= 0.0f || Settings.Decals.IsEmpty())
	{
		return;
	}

	const int32 Count = ComputeDecalCount(Settings.Density, UnitCount);
	for (int32 DecalIndex = 0; DecalIndex < Count; ++DecalIndex)
	{
		for (int32 Attempt = 0; Attempt < ScorchedCityGenConstants::DecalAttemptsPerSpawn; ++Attempt)
		{
			const int32 EntryIndex = PickWeightedDecalEntry(Settings);
			if (EntryIndex == INDEX_NONE)
			{
				break;
			}

			const FScorchedDecalEntry& Entry = Settings.Decals[EntryIndex];
			const double MaxScale = FMath::Max(Entry.MinScale, Entry.MaxScale);
			const double Size = M_Random.FRandRange(Entry.MinScale, MaxScale);
			const FVector2D Position = SampleDecalPointInFootprint(Area, Size, M_Random);
			const double Yaw = BaseYawRadians + M_Random.FRandRange(-PI, PI);
			if (TryReserveDecal(Settings, Set, EntryIndex, Position, Yaw, Size, OutResult))
			{
				break;
			}
		}
	}
}

void FScorchedCityGenerator::BuildDecalsAtPoint(
	const FScorchedDecalPlacementSettings& Settings,
	const EScorchedDecalSet Set,
	const FVector2D& Position,
	const double BaseYawRadians,
	FScorchedCityGenResult& OutResult)
{
	if (Settings.Density <= 0.0f || Settings.Decals.IsEmpty())
	{
		return;
	}

	const int32 Count = ComputeDecalCount(Settings.Density, 1.0);
	for (int32 DecalIndex = 0; DecalIndex < Count; ++DecalIndex)
	{
		for (int32 Attempt = 0; Attempt < ScorchedCityGenConstants::DecalAttemptsPerSpawn; ++Attempt)
		{
			const int32 EntryIndex = PickWeightedDecalEntry(Settings);
			if (EntryIndex == INDEX_NONE)
			{
				break;
			}

			const FScorchedDecalEntry& Entry = Settings.Decals[EntryIndex];
			const double MaxScale = FMath::Max(Entry.MinScale, Entry.MaxScale);
			const double Size = M_Random.FRandRange(Entry.MinScale, MaxScale);
			const double Jitter = M_Random.FRand() * ScorchedCityGenConstants::DecalPowerPoleJitter;
			const double JitterYaw = M_Random.FRand() * 2.0 * PI;
			const FVector2D JitterOffset(FMath::Cos(JitterYaw) * Jitter, FMath::Sin(JitterYaw) * Jitter);
			const double Yaw = BaseYawRadians + M_Random.FRandRange(-PI, PI);
			if (TryReserveDecal(Settings, Set, EntryIndex, Position + JitterOffset, Yaw, Size, OutResult))
			{
				break;
			}
		}
	}
}

void FScorchedCityGenerator::BuildRoadSplineDecals(
	const FScorchedDecalPlacementSettings& Settings,
	const FScorchedRoadSplineResult& Road,
	FScorchedCityGenResult& OutResult)
{
	if (Settings.Density <= 0.0f || Settings.Decals.IsEmpty())
	{
		return;
	}

	double RoadLength = 0.0;
	for (int32 PointIndex = 1; PointIndex < Road.Points.Num(); ++PointIndex)
	{
		RoadLength += FVector2D::Distance(Road.Points[PointIndex - 1], Road.Points[PointIndex]);
	}

	const double UnitCount = RoadLength / ScorchedCityGenConstants::DecalRoadLengthUnit;
	const int32 Count = ComputeDecalCount(Settings.Density, UnitCount);
	for (int32 DecalIndex = 0; DecalIndex < Count; ++DecalIndex)
	{
		for (int32 Attempt = 0; Attempt < ScorchedCityGenConstants::DecalAttemptsPerSpawn; ++Attempt)
		{
			const int32 EntryIndex = PickWeightedDecalEntry(Settings);
			if (EntryIndex == INDEX_NONE)
			{
				break;
			}

			FVector2D RoadPoint;
			FVector2D RoadDirection;
			if (not SamplePolyline(Road.Points, M_Random.FRandRange(0.0, RoadLength), RoadPoint, RoadDirection))
			{
				break;
			}

			const FScorchedDecalEntry& Entry = Settings.Decals[EntryIndex];
			const double MaxScale = FMath::Max(Entry.MinScale, Entry.MaxScale);
			const double Size = M_Random.FRandRange(Entry.MinScale, MaxScale);
			const double LateralRange = FMath::Max(0.0, M_Params.RoadWidth * 0.5 - Size * 0.5);
			const FVector2D Normal(-RoadDirection.Y, RoadDirection.X);
			const FVector2D DecalPosition = RoadPoint + Normal * M_Random.FRandRange(-LateralRange, LateralRange);
			const double RoadYaw = FMath::Atan2(RoadDirection.Y, RoadDirection.X);
			const double Yaw = RoadYaw + M_Random.FRandRange(-0.35, 0.35);
			if (TryReserveDecal(Settings, EScorchedDecalSet::Road, EntryIndex, DecalPosition, Yaw, Size, OutResult))
			{
				break;
			}
		}
	}
}

void FScorchedCityGenerator::BuildLotDecals(FScorchedCityGenResult& OutResult)
{
	const FScorchedDecalPlacementSettings& Settings = M_Params.LotDecals;
	for (const FScorchedLot& Lot : M_Lots)
	{
		FScorchedFootprint Area;
		Area.Center = Lot.Center;
		Area.HalfExtents = Lot.HalfExtents;
		Area.YawRadians = Lot.YawRadians;
		BuildDecalsInFootprint(Settings, EScorchedDecalSet::Lot, Area, 1.0, Lot.YawRadians, OutResult);
	}
}

void FScorchedCityGenerator::BuildBuildingFootprintDecals(FScorchedCityGenResult& OutResult)
{
	const FScorchedDecalPlacementSettings& Settings = M_Params.BuildingFootprintDecals;
	for (const FScorchedBuildingSpawn& Building : OutResult.Buildings)
	{
		BuildDecalsInFootprint(
			Settings,
			EScorchedDecalSet::BuildingFootprint,
			Building.Footprint,
			1.0,
			Building.YawRadians,
			OutResult);
	}
}

void FScorchedCityGenerator::BuildRoadDecals(FScorchedCityGenResult& OutResult)
{
	const FScorchedDecalPlacementSettings& Settings = M_Params.RoadDecals;
	for (const FScorchedRoadSplineResult& Road : OutResult.RoadSplines)
	{
		BuildRoadSplineDecals(Settings, Road, OutResult);
	}

	for (const FScorchedIntersectionSpawn& Intersection : OutResult.Intersections)
	{
		FScorchedFootprint Area;
		Area.Center = Intersection.Position;
		Area.HalfExtents = FVector2D(M_Params.IntersectionSizeX * 0.5, M_Params.IntersectionSizeY * 0.5);
		Area.YawRadians = Intersection.YawRadians;
		BuildDecalsInFootprint(Settings, EScorchedDecalSet::Road, Area, 1.0, Intersection.YawRadians, OutResult);
	}
}

void FScorchedCityGenerator::BuildPowerLineDecals(FScorchedCityGenResult& OutResult)
{
	for (const FScorchedPoleSpawn& Pole : OutResult.Poles)
	{
		if (Pole.bTwoPole)
		{
			BuildDecalsAtPoint(
				M_Params.PowerLineDecals,
				EScorchedDecalSet::PowerLine,
				Pole.Position,
				Pole.YawRadians,
				OutResult);
			BuildDecalsAtPoint(
				M_Params.PowerLineDecals,
				EScorchedDecalSet::PowerLine,
				Pole.SecondPosition,
				Pole.YawRadians,
				OutResult);
		}
		else
		{
			BuildDecalsAtPoint(
				M_Params.DestroyedPowerLineDecals,
				EScorchedDecalSet::DestroyedPowerLine,
				Pole.Position,
				Pole.YawRadians,
				OutResult);
		}
	}
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

namespace
{
	/** @brief Cuts Distance of arc length off the front of a polyline (for intersection meshes). */
	void TrimPolylineFront(TArray<FVector2D>& Points, double Distance)
	{
		while (Points.Num() >= 2 && Distance > 0.0)
		{
			const double SegmentLength = FVector2D::Distance(Points[0], Points[1]);
			if (SegmentLength > Distance)
			{
				Points[0] += (Points[1] - Points[0]) * (Distance / SegmentLength);
				return;
			}
			Distance -= SegmentLength;
			Points.RemoveAt(0);
		}
	}
}

void FScorchedCityGenerator::ExportRoads(FScorchedCityGenResult& OutResult) const
{
	// Junction meshes go on nodes with 3+ connections; roads are trimmed back around them.
	TSet<int32> MeshNodes;
	for (int32 NodeIndex = 0; NodeIndex < M_Nodes.Num(); ++NodeIndex)
	{
		if (M_Nodes[NodeIndex].Edges.Num() < 3)
		{
			continue;
		}
		MeshNodes.Add(NodeIndex);

		FScorchedIntersectionSpawn Intersection;
		Intersection.Position = M_Nodes[NodeIndex].Position;
		Intersection.YawRadians = ComputeIntersectionYawAtNode(NodeIndex);
		OutResult.Intersections.Add(Intersection);
	}

	// Trim each road end back by the intersection footprint's support along the exit
	// direction, so splines end flush with a non-square 4-way instead of running under it.
	// This trim is deliberately uncapped: the spline must never render below the mesh.
	for (const FScorchedRoadEdge& Edge : M_Edges)
	{
		TArray<FVector2D> Points = Edge.Points;
		if (Points.Num() >= 2 && MeshNodes.Contains(Edge.NodeA))
		{
			TrimPolylineFront(Points, ComputeEdgeEndClearance(Edge, Edge.NodeA));
		}
		if (Points.Num() >= 2 && MeshNodes.Contains(Edge.NodeB))
		{
			Algo::Reverse(Points);
			TrimPolylineFront(Points, ComputeEdgeEndClearance(Edge, Edge.NodeB));
			Algo::Reverse(Points);
		}

		if (Points.Num() < 2)
		{
			continue;
		}

		// Drop slivers left between two large adjacent intersections.
		double RemainingLength = 0.0;
		for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
		{
			RemainingLength += FVector2D::Distance(Points[PointIndex - 1], Points[PointIndex]);
		}
		if (RemainingLength < M_Params.RoadWidth * 0.5)
		{
			continue;
		}

		FScorchedRoadSplineResult Spline;
		Spline.Points = MoveTemp(Points);
		Spline.bCurved = Edge.bCurved;
		OutResult.RoadSplines.Add(MoveTemp(Spline));
	}
}
