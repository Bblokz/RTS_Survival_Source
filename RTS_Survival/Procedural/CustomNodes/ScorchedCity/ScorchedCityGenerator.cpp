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
}

FScorchedCityGenerator::FScorchedCityGenerator(const FScorchedCityGenParams& InParams)
	: M_Params(InParams)
	, M_Random(InParams.RandomSeed)
	, M_Occupancy(FMath::Clamp(InParams.GridBlockSize * 0.5, 800.0, 3000.0))
{
}

void FScorchedCityGenerator::Generate(FScorchedCityGenResult& OutResult)
{
	BuildZones();
	BuildGridRoads();
	BuildCurvedRoads();
	AddIntersectionFootprints();
	BuildLots();
	PlaceBuildings(OutResult);
	PlacePowerLines(OutResult);
	BuildScatter(OutResult, OutResult);
	ExportRoads(OutResult);

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

			const double Heading = FMath::Atan2(Direction.Y, Direction.X)
				+ M_Random.FRandRange(-0.2, 0.2);
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

	struct FBranchRequest
	{
		FVector2D Position;
		double Heading;
	};
	TArray<FBranchRequest> Branches;

	for (int32 Step = 0; Step < MaxStepsPerWalker; ++Step)
	{
		Heading += M_Random.FRandRange(-1.0, 1.0) * MaxTurn;
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
			// Try to end in a junction with an existing road instead of a dead stop.
			const int32 SnapNode = FindNearestNode(Next, M_Params.GridBlockSize * 0.45);
			if (SnapNode != INDEX_NONE && not M_Nodes[SnapNode].Edges.IsEmpty())
			{
				Points.Add(M_Nodes[SnapNode].Position);
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

	if (Points.Num() >= 2)
	{
		const int32 EndNode = FindOrAddNode(Points.Last(), M_Params.RoadWidth * 1.5, false);
		Points.Last() = M_Nodes[EndNode].Position;
		CommitEdge(StartNodeIndex, EndNode, MoveTemp(Points), true, false);
	}

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
	for (const FScorchedRoadNode& Node : M_Nodes)
	{
		if (Node.Edges.Num() < 3)
		{
			continue;
		}

		FScorchedFootprint Footprint;
		Footprint.Center = Node.Position;
		Footprint.HalfExtents = FVector2D(M_Params.IntersectionSize * 0.5, M_Params.IntersectionSize * 0.5);
		Footprint.YawRadians = 0.0;
		M_Occupancy.Add(Footprint, EScorchedOccupancy::Intersection);
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
	const double LotWidth = M_Params.LotWidth * (Edge.bMajor ? MajorRoadLotWidthMultiplier : 1.0);

	double ArcDistance = LotWidth * 0.5 + M_Params.IntersectionSize * 0.5;
	const double EdgeLength = Edge.Length();

	while (ArcDistance + LotWidth * 0.5 < EdgeLength)
	{
		FVector2D RoadPoint;
		FVector2D RoadDirection;
		if (not SamplePolyline(Edge.Points, ArcDistance, RoadPoint, RoadDirection))
		{
			break;
		}

		TryCreateLot(EdgeIndex, RoadPoint, RoadDirection, 1.0);
		TryCreateLot(EdgeIndex, RoadPoint, RoadDirection, -1.0);

		ArcDistance += LotWidth * (1.0 + LotGapFraction);
	}
}

void FScorchedCityGenerator::TryCreateLot(
	const int32 EdgeIndex,
	const FVector2D& RoadPoint,
	const FVector2D& RoadDirection,
	const double Side)
{
	using namespace ScorchedCityGenConstants;

	const FScorchedRoadEdge& Edge = M_Edges[EdgeIndex];
	const double LotWidth = M_Params.LotWidth * (Edge.bMajor ? MajorRoadLotWidthMultiplier : 1.0);
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
	const uint8 BlockMask = ScorchedOccupancyMaskAll();

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
			if (M_Occupancy.OverlapsAny(Footprint.Inflated(Spacing), BlockMask))
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
			break;
		}
	}
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

	for (const FScorchedRoadEdge& Edge : M_Edges)
	{
		if (Edge.Length() >= M_Params.PowerLines.PowerLineSpacing)
		{
			PlacePowerLinesAlongEdge(Edge, OutResult);
		}
	}
}

void FScorchedCityGenerator::PlacePowerLinesAlongEdge(
	const FScorchedRoadEdge& Edge,
	FScorchedCityGenResult& OutResult)
{
	const FScorchedPowerLineSettings& Settings = M_Params.PowerLines;
	const double Side = (M_Random.FRand() < 0.5) ? 1.0 : -1.0;
	const double EdgeOffset = M_Params.RoadWidth * 0.5 + Settings.OffsetFromRoadEdge;
	const double UsableLength = Edge.Length() - M_Params.IntersectionSize * 0.5;

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

	double ArcDistance = M_Params.IntersectionSize;
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
	// 4-way meshes go on nodes with 4+ connections; roads are trimmed back around them.
	TSet<int32> MeshNodes;
	for (int32 NodeIndex = 0; NodeIndex < M_Nodes.Num(); ++NodeIndex)
	{
		if (M_Nodes[NodeIndex].Edges.Num() < 4)
		{
			continue;
		}
		MeshNodes.Add(NodeIndex);

		FScorchedIntersectionSpawn Intersection;
		Intersection.Position = M_Nodes[NodeIndex].Position;
		if (not M_Nodes[NodeIndex].bGridNode && not M_Nodes[NodeIndex].Edges.IsEmpty())
		{
			const FScorchedRoadEdge& FirstEdge = M_Edges[M_Nodes[NodeIndex].Edges[0]];
			if (FirstEdge.Points.Num() >= 2)
			{
				const FVector2D Direction = FirstEdge.Points[1] - FirstEdge.Points[0];
				Intersection.YawRadians = FMath::Atan2(Direction.Y, Direction.X);
			}
		}
		OutResult.Intersections.Add(Intersection);
	}

	const double TrimDistance = M_Params.IntersectionSize * 0.5;
	for (const FScorchedRoadEdge& Edge : M_Edges)
	{
		TArray<FVector2D> Points = Edge.Points;
		if (MeshNodes.Contains(Edge.NodeA))
		{
			TrimPolylineFront(Points, TrimDistance);
		}
		if (MeshNodes.Contains(Edge.NodeB))
		{
			Algo::Reverse(Points);
			TrimPolylineFront(Points, TrimDistance);
			Algo::Reverse(Points);
		}

		if (Points.Num() < 2)
		{
			continue;
		}

		FScorchedRoadSplineResult Spline;
		Spline.Points = MoveTemp(Points);
		Spline.bCurved = Edge.bCurved;
		OutResult.RoadSplines.Add(MoveTemp(Spline));
	}
}
