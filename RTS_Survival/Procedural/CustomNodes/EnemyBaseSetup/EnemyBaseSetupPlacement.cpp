// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "EnemyBaseSetupPlacement.h"

namespace EnemyBaseSetupPlacementConstants
{
	constexpr double MinChainStep = 50.0;
	constexpr int32 MaxChainItems = 512;
	constexpr int32 DecoratorRetryAttempts = 6;
	constexpr double MinArcSpanRadians = 0.05;

	// Sandbag walls are adjacent by design: they only avoid other solid categories, not each other.
	constexpr uint8 SandbagAvoidMask =
		EnemyOccupancyMask(EEnemyPlacementCategory::Bunker)
		| EnemyOccupancyMask(EEnemyPlacementCategory::Hedgehog)
		| EnemyOccupancyMask(EEnemyPlacementCategory::BarbedWire);

	// Barbed wire chains end to end: it avoids solids but not other wire.
	constexpr uint8 BarbedWireAvoidMask =
		EnemyOccupancyMask(EEnemyPlacementCategory::Bunker)
		| EnemyOccupancyMask(EEnemyPlacementCategory::Sandbag)
		| EnemyOccupancyMask(EEnemyPlacementCategory::Hedgehog);

	constexpr uint8 DecoratorAvoidMask =
		EnemySolidCategoryMask() | EnemyOccupancyMask(EEnemyPlacementCategory::Decorator);
}

namespace
{
	/** @brief Rounds a fractional count to an int, using the fractional part as a seeded chance. */
	int32 EBS_RandomRoundCount(FRandomStream& Random, const double Value)
	{
		const int32 Base = FMath::FloorToInt32(FMath::Max(0.0, Value));
		const double Fraction = FMath::Max(0.0, Value) - Base;
		return Base + (Random.FRand() < Fraction ? 1 : 0);
	}

	FVector2D EBS_RotateVector2D(const FVector2D& Vector, const double YawRadians)
	{
		double SinYaw = 0.0;
		double CosYaw = 0.0;
		FMath::SinCos(&SinYaw, &CosYaw, YawRadians);
		return FVector2D(
			CosYaw * Vector.X - SinYaw * Vector.Y,
			SinYaw * Vector.X + CosYaw * Vector.Y);
	}
}

// ---------------------------------------------------------------------------
// FEnemyDefenseFrame
// ---------------------------------------------------------------------------

FVector2D FEnemyDefenseFrame::Forward() const
{
	double SinYaw = 0.0;
	double CosYaw = 0.0;
	FMath::SinCos(&SinYaw, &CosYaw, FacingYaw);
	return FVector2D(CosYaw, SinYaw);
}

FVector2D FEnemyDefenseFrame::Right() const
{
	double SinYaw = 0.0;
	double CosYaw = 0.0;
	FMath::SinCos(&SinYaw, &CosYaw, FacingYaw);
	return FVector2D(-SinYaw, CosYaw);
}

// ---------------------------------------------------------------------------
// FEnemyBaseSetupBuilder — construction & primitives
// ---------------------------------------------------------------------------

FEnemyBaseSetupBuilder::FEnemyBaseSetupBuilder(
	FRandomStream& InRandom,
	FEnemyOccupancyGrid& InOccupancy,
	FEnemyBaseSetupResult& InResult)
	: M_Random(InRandom)
	, M_Occupancy(InOccupancy)
	, M_Result(InResult)
{
}

void FEnemyBaseSetupBuilder::SetBlockedPredicate(TFunction<bool(const FVector2D&)> InBlocked)
{
	M_IsBlocked = MoveTemp(InBlocked);
}

int32 FEnemyBaseSetupBuilder::BeginCluster()
{
	return M_Result.NumClusters++;
}

int32 FEnemyBaseSetupBuilder::PickWeightedEntry(const TArray<FEnemyResolvedEntry>& Entries) const
{
	if (Entries.IsEmpty())
	{
		return INDEX_NONE;
	}

	double TotalWeight = 0.0;
	for (const FEnemyResolvedEntry& Entry : Entries)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}
	if (TotalWeight <= 0.0)
	{
		return 0;
	}

	double Roll = M_Random.FRand() * TotalWeight;
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		Roll -= FMath::Max(0.0f, Entries[EntryIndex].Weight);
		if (Roll <= 0.0)
		{
			return EntryIndex;
		}
	}
	return Entries.Num() - 1;
}

double FEnemyBaseSetupBuilder::PickUniformScale(const FEnemyResolvedEntry& Entry) const
{
	if (not Entry.bOverrideScale)
	{
		return 1.0;
	}
	const double MinScale = FMath::Max(0.01f, Entry.MinScale);
	const double MaxScale = FMath::Max(MinScale, static_cast<double>(Entry.MaxScale));
	return M_Random.FRandRange(MinScale, MaxScale);
}

FEnemyFootprint FEnemyBaseSetupBuilder::MakeFootprint(
	const FEnemyResolvedEntry& Entry,
	const FVector2D& Position,
	const double YawRadians,
	const double Scale) const
{
	FEnemyFootprint Footprint;
	Footprint.Center = Position + EBS_RotateVector2D(Entry.PivotToFootprintCenter * Scale, YawRadians);
	Footprint.HalfExtents = Entry.FootprintHalfExtents * Scale;
	Footprint.YawRadians = YawRadians;
	return Footprint;
}

bool FEnemyBaseSetupBuilder::TryReserveItem(
	const EEnemyPlacementCategory Category,
	const TArray<FEnemyResolvedEntry>& Entries,
	const int32 EntryIndex,
	const FVector2D& Position,
	const double YawRadians,
	const double ExtraSpacing,
	const uint8 AvoidMask,
	const int32 Cluster)
{
	if (not Entries.IsValidIndex(EntryIndex))
	{
		return false;
	}

	const FEnemyResolvedEntry& Entry = Entries[EntryIndex];
	const double Scale = PickUniformScale(Entry);
	const FEnemyFootprint Footprint = MakeFootprint(Entry, Position, YawRadians, Scale);

	if (M_IsBlocked && M_IsBlocked(Footprint.Center))
	{
		return false;
	}
	if (AvoidMask != 0 && M_Occupancy.OverlapsAny(Footprint.Inflated(ExtraSpacing), AvoidMask))
	{
		return false;
	}

	M_Occupancy.Add(Footprint, Category);

	FEnemyPlacedActor& Placed = M_Result.Actors.Emplace_GetRef();
	Placed.Category = Category;
	Placed.AssetIndex = Entry.OutputAssetIndex != INDEX_NONE ? Entry.OutputAssetIndex : EntryIndex;
	Placed.ClusterIndex = Cluster;
	Placed.Position = Position;
	Placed.YawRadians = YawRadians;
	Placed.UniformScale = Scale;
	Placed.Footprint = Footprint;
	return true;
}

void FEnemyBaseSetupBuilder::PlaceChainAlong(
	const EEnemyPlacementCategory Category,
	const TArray<FEnemyResolvedEntry>& Entries,
	const FVector2D& Center,
	const FVector2D& Direction,
	const double HalfLength,
	const double Step,
	const double ItemYaw,
	const double ExtraSpacing,
	const uint8 AvoidMask,
	const int32 Cluster)
{
	using namespace EnemyBaseSetupPlacementConstants;

	if (Entries.IsEmpty())
	{
		return;
	}

	const double SafeStep = FMath::Max(MinChainStep, Step);
	const double Span = FMath::Max(0.0, 2.0 * HalfLength);
	const int32 NumItems = FMath::Min(MaxChainItems, FMath::Max(1, FMath::FloorToInt32(Span / SafeStep) + 1));
	const double ActualSpan = (NumItems - 1) * SafeStep;
	const double StartOffset = -ActualSpan * 0.5;

	for (int32 ItemIndex = 0; ItemIndex < NumItems; ++ItemIndex)
	{
		const FVector2D Position = Center + Direction * (StartOffset + ItemIndex * SafeStep);
		const int32 EntryIndex = PickWeightedEntry(Entries);
		TryReserveItem(Category, Entries, EntryIndex, Position, ItemYaw, ExtraSpacing, AvoidMask, Cluster);
	}
}

// ---------------------------------------------------------------------------
// Structural placements
// ---------------------------------------------------------------------------

void FEnemyBaseSetupBuilder::PlaceBunkerRow(
	const TArray<FEnemyResolvedEntry>& Bunkers,
	const FEnemyDefenseFrame& Frame,
	const double LineLength,
	const double Spacing,
	const double ExtraSpacing,
	const int32 Cluster)
{
	PlaceChainAlong(
		EEnemyPlacementCategory::Bunker,
		Bunkers,
		Frame.Origin,
		Frame.Right(),
		LineLength * 0.5,
		Spacing,
		Frame.FacingYaw,
		ExtraSpacing,
		EnemySolidCategoryMask(),
		Cluster);
}

bool FEnemyBaseSetupBuilder::PlaceSingleBuilding(
	const TArray<FEnemyResolvedEntry>& Buildings,
	const FEnemyDefenseFrame& Frame,
	const double ExtraSpacing,
	const int32 Cluster,
	FEnemyFootprint& OutBuildingFootprint)
{
	const int32 EntryIndex = PickWeightedEntry(Buildings);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	const bool bReserved = TryReserveItem(
		EEnemyPlacementCategory::Bunker, Buildings, EntryIndex, Frame.Origin, Frame.FacingYaw,
		ExtraSpacing, EnemySolidCategoryMask(), Cluster);
	if (not bReserved)
	{
		return false;
	}

	OutBuildingFootprint = M_Result.Actors.Last().Footprint;
	return true;
}

void FEnemyBaseSetupBuilder::PlaceObstacleBelt(
	const TArray<FEnemyResolvedEntry>& Obstacles,
	const FEnemyObstacleBeltSettings& Settings,
	const FEnemyDefenseFrame& Frame,
	const double FrontDistance,
	const double HalfWidth,
	const int32 Cluster)
{
	if (not Settings.bEnabled || Obstacles.IsEmpty())
	{
		return;
	}

	const double ForwardOffset = FrontDistance + Settings.DistanceFromLine;
	const double BeltHalfWidth = HalfWidth + Settings.SideOverhang;

	switch (Settings.Formation)
	{
	case EEnemyObstacleFormation::Arc:
		PlaceArcObstacleRow(Obstacles, Settings, Frame, ForwardOffset, BeltHalfWidth, Cluster);
		break;
	case EEnemyObstacleFormation::DoubleRow:
	{
		PlaceStraightObstacleRow(Obstacles, Settings, Frame, ForwardOffset, BeltHalfWidth, Cluster);
		// Second row: pushed toward the threat and staggered by half the spacing.
		const FVector2D StaggerCenter = Frame.Origin
			+ Frame.Forward() * (ForwardOffset + Settings.RowSpacing)
			+ Frame.Right() * (Settings.SpacingBetweenObstacles * 0.5);
		PlaceChainAlong(
			EEnemyPlacementCategory::Hedgehog, Obstacles, StaggerCenter, Frame.Right(),
			BeltHalfWidth, Settings.SpacingBetweenObstacles, Frame.FacingYaw, 0.0,
			EnemySolidCategoryMask(), Cluster);
		break;
	}
	case EEnemyObstacleFormation::StraightLine:
	default:
		PlaceStraightObstacleRow(Obstacles, Settings, Frame, ForwardOffset, BeltHalfWidth, Cluster);
		break;
	}
}

void FEnemyBaseSetupBuilder::PlaceStraightObstacleRow(
	const TArray<FEnemyResolvedEntry>& Obstacles,
	const FEnemyObstacleBeltSettings& Settings,
	const FEnemyDefenseFrame& Frame,
	const double ForwardOffset,
	const double HalfWidth,
	const int32 Cluster)
{
	const FVector2D Center = Frame.Origin + Frame.Forward() * ForwardOffset;
	PlaceChainAlong(
		EEnemyPlacementCategory::Hedgehog, Obstacles, Center, Frame.Right(),
		HalfWidth, Settings.SpacingBetweenObstacles, Frame.FacingYaw, 0.0,
		EnemySolidCategoryMask(), Cluster);
}

void FEnemyBaseSetupBuilder::PlaceArcObstacleRow(
	const TArray<FEnemyResolvedEntry>& Obstacles,
	const FEnemyObstacleBeltSettings& Settings,
	const FEnemyDefenseFrame& Frame,
	const double ForwardOffset,
	const double HalfWidth,
	const int32 Cluster)
{
	using namespace EnemyBaseSetupPlacementConstants;

	const double SpanRadians = FMath::Max<double>(
		MinArcSpanRadians, FMath::DegreesToRadians(static_cast<double>(Settings.ArcSpanDegrees)));
	const double DerivedRadius = HalfWidth / FMath::Max<double>(KINDA_SMALL_NUMBER, FMath::Sin(SpanRadians * 0.5));
	const double Radius = Settings.ArcRadiusOverride > 0.0f
		? FMath::Max<double>(Settings.ArcRadiusOverride, 1.0)
		: DerivedRadius;

	const FVector2D Forward = Frame.Forward();
	const FVector2D Right = Frame.Right();
	// Apex (nearest the threat) sits at the belt distance; the arc centre is one radius behind it.
	const FVector2D ArcCenter = Frame.Origin + Forward * ForwardOffset - Forward * Radius;

	const double ArcLength = Radius * SpanRadians;
	const int32 NumItems = FMath::Min(MaxChainItems,
		FMath::Max(1, FMath::FloorToInt32(ArcLength / FMath::Max<double>(MinChainStep, Settings.SpacingBetweenObstacles)) + 1));

	for (int32 ItemIndex = 0; ItemIndex < NumItems; ++ItemIndex)
	{
		const double Alpha = NumItems > 1 ? static_cast<double>(ItemIndex) / (NumItems - 1) : 0.5;
		const double Angle = FMath::Lerp(-SpanRadians * 0.5, SpanRadians * 0.5, Alpha);
		const FVector2D RadialDir = Forward * FMath::Cos(Angle) + Right * FMath::Sin(Angle);
		const FVector2D Position = ArcCenter + RadialDir * Radius;
		const double ItemYaw = FMath::Atan2(RadialDir.Y, RadialDir.X);

		const int32 EntryIndex = PickWeightedEntry(Obstacles);
		TryReserveItem(EEnemyPlacementCategory::Hedgehog, Obstacles, EntryIndex, Position, ItemYaw,
			0.0, EnemySolidCategoryMask(), Cluster);
	}
}

void FEnemyBaseSetupBuilder::PlaceBarbedWire(
	const TArray<FEnemyResolvedEntry>& Wire,
	const FEnemyBarbedWireSettings& Settings,
	const FEnemyDefenseFrame& Frame,
	const double FrontDistance,
	const double HalfWidth,
	const int32 Cluster)
{
	using namespace EnemyBaseSetupPlacementConstants;

	if (not Settings.bEnabled || Wire.IsEmpty())
	{
		return;
	}

	const FVector2D Forward = Frame.Forward();
	const FVector2D Right = Frame.Right();
	// Wire segments run along the right axis: their local +X points along the run.
	const double SegmentYaw = FMath::Atan2(Right.Y, Right.X);
	const double RunHalfWidth = HalfWidth + Settings.SideOverhang;
	const int32 Rows = FMath::Max(1, Settings.Rows);

	for (int32 RowIndex = 0; RowIndex < Rows; ++RowIndex)
	{
		const double ForwardOffset =
			FrontDistance + Settings.DistanceFromLine + RowIndex * Settings.RowSpacing;
		const FVector2D Center = Frame.Origin + Forward * ForwardOffset;
		PlaceChainAlong(
			EEnemyPlacementCategory::BarbedWire, Wire, Center, Right, RunHalfWidth,
			Settings.SegmentLength, SegmentYaw, 0.0, BarbedWireAvoidMask, Cluster);
	}
}

// ---------------------------------------------------------------------------
// Sandbag walls
// ---------------------------------------------------------------------------

void FEnemyBaseSetupBuilder::PlaceSandbagSide(
	const TArray<FEnemyResolvedEntry>& Sandbags,
	const FEnemySandbagSettings& Settings,
	const FEnemyFootprint& Building,
	const FVector2D& OutwardDir,
	const double AlongExtent,
	const double OutwardExtent,
	const int32 Cluster)
{
	using namespace EnemyBaseSetupPlacementConstants;

	// The wall runs perpendicular to the outward direction.
	const FVector2D AlongDir(-OutwardDir.Y, OutwardDir.X);
	const FVector2D Center =
		Building.Center + OutwardDir * (OutwardExtent + Settings.DistanceFromBuilding);
	const double SegmentYaw = FMath::Atan2(AlongDir.Y, AlongDir.X);

	PlaceChainAlong(
		EEnemyPlacementCategory::Sandbag, Sandbags, Center, AlongDir, AlongExtent,
		Settings.SegmentLength, SegmentYaw, 0.0, SandbagAvoidMask, Cluster);
}

void FEnemyBaseSetupBuilder::PlaceSandbagWall(
	const TArray<FEnemyResolvedEntry>& Sandbags,
	const FEnemySandbagSettings& Settings,
	const FEnemyFootprint& Building,
	const int32 Cluster)
{
	if (not Settings.bEnabled || Sandbags.IsEmpty())
	{
		return;
	}

	double SinYaw = 0.0;
	double CosYaw = 0.0;
	FMath::SinCos(&SinYaw, &CosYaw, Building.YawRadians);
	const FVector2D Forward(CosYaw, SinYaw);
	const FVector2D Right(-SinYaw, CosYaw);

	const double HalfDepth = Building.HalfExtents.X;
	const double HalfWidth = Building.HalfExtents.Y;
	const double FlankHalfLength = HalfDepth + Settings.FlankExtension;

	// Flanks (left & right) are always present.
	PlaceSandbagSide(Sandbags, Settings, Building, Right, FlankHalfLength, HalfWidth, Cluster);
	PlaceSandbagSide(Sandbags, Settings, Building, -Right, FlankHalfLength, HalfWidth, Cluster);

	if (Settings.Coverage == EEnemySandbagCoverage::FrontAndFlanks
		|| Settings.Coverage == EEnemySandbagCoverage::Surround)
	{
		PlaceSandbagSide(Sandbags, Settings, Building, Forward, HalfWidth, HalfDepth, Cluster);
	}
	if (Settings.Coverage == EEnemySandbagCoverage::Surround)
	{
		PlaceSandbagSide(Sandbags, Settings, Building, -Forward, HalfWidth, HalfDepth, Cluster);
	}
}

// ---------------------------------------------------------------------------
// Scatter (decorators / foliage / decals)
// ---------------------------------------------------------------------------

FBox2D FEnemyBaseSetupBuilder::ComputeClusterBounds(const int32 Cluster) const
{
	FBox2D Bounds(ForceInit);
	for (const FEnemyPlacedActor& Actor : M_Result.Actors)
	{
		if (Actor.ClusterIndex == Cluster)
		{
			Bounds += Actor.Footprint.GetAABB();
		}
	}
	return Bounds;
}

FVector2D FEnemyBaseSetupBuilder::SampleRingPosition(
	const FBox2D& Bounds,
	const double MinDistance,
	const double MaxDistance) const
{
	const FVector2D Center = Bounds.GetCenter();
	const FVector2D Extent = Bounds.GetExtent();

	const double Angle = M_Random.FRandRange(0.0, 2.0 * PI);
	const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
	const double SupportRadius =
		FMath::Abs(Direction.X) * Extent.X + FMath::Abs(Direction.Y) * Extent.Y;
	const double Distance = M_Random.FRandRange(FMath::Min(MinDistance, MaxDistance), FMath::Max(MinDistance, MaxDistance));

	return Center + Direction * (SupportRadius + Distance);
}

void FEnemyBaseSetupBuilder::ScatterDecorators(
	const TArray<FEnemyResolvedEntry>& Decorators,
	const FEnemyDecoratorProfile& Profile,
	const int32 Cluster)
{
	using namespace EnemyBaseSetupPlacementConstants;

	if (Decorators.IsEmpty() || M_Random.FRand() > Profile.ChancePerSetup)
	{
		return;
	}

	const FBox2D Bounds = ComputeClusterBounds(Cluster);
	if (not Bounds.bIsValid)
	{
		return;
	}

	const int32 Count = EBS_RandomRoundCount(M_Random, Profile.CountPerSetup);
	const int32 Attempts = Profile.bAvoidOverlap ? DecoratorRetryAttempts : 1;

	for (int32 DecoratorIndex = 0; DecoratorIndex < Count; ++DecoratorIndex)
	{
		for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
		{
			const FVector2D Position = SampleRingPosition(
				Bounds, Profile.MinDistanceFromBounds, Profile.MaxDistanceFromBounds);
			const double Yaw = M_Random.FRandRange(0.0, 2.0 * PI);
			const int32 EntryIndex = PickWeightedEntry(Decorators);
			if (TryReserveItem(EEnemyPlacementCategory::Decorator, Decorators, EntryIndex, Position,
				Yaw, 0.0, DecoratorAvoidMask, Cluster))
			{
				break;
			}
		}
	}
}

void FEnemyBaseSetupBuilder::ScatterFoliage(const FEnemyFoliageSettings& Settings, const int32 Cluster)
{
	if (Settings.Foliage.IsEmpty())
	{
		return;
	}

	const FBox2D Bounds = ComputeClusterBounds(Cluster);
	if (not Bounds.bIsValid)
	{
		return;
	}

	double TotalWeight = 0.0;
	for (const FEnemyFoliageEntry& Entry : Settings.Foliage)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}

	const int32 Count = EBS_RandomRoundCount(M_Random, Settings.CountPerSetup);
	for (int32 FoliageIndex = 0; FoliageIndex < Count; ++FoliageIndex)
	{
		const FVector2D Position = SampleRingPosition(
			Bounds, Settings.MinDistanceFromBounds, Settings.MaxDistanceFromBounds);
		if (M_IsBlocked && M_IsBlocked(Position))
		{
			continue;
		}

		int32 EntryIndex = 0;
		double Roll = M_Random.FRand() * FMath::Max<double>(KINDA_SMALL_NUMBER, TotalWeight);
		for (int32 CandidateIndex = 0; CandidateIndex < Settings.Foliage.Num(); ++CandidateIndex)
		{
			Roll -= FMath::Max(0.0f, Settings.Foliage[CandidateIndex].Weight);
			if (Roll <= 0.0)
			{
				EntryIndex = CandidateIndex;
				break;
			}
		}

		const FEnemyFoliageEntry& Entry = Settings.Foliage[EntryIndex];
		FEnemyFoliageSpawn& Spawn = M_Result.Foliage.Emplace_GetRef();
		Spawn.MeshEntryIndex = EntryIndex;
		Spawn.ClusterIndex = Cluster;
		Spawn.Position = Position;
		Spawn.YawRadians = M_Random.FRandRange(0.0, 2.0 * PI);
		Spawn.UniformScale = M_Random.FRandRange(
			FMath::Max(0.01f, Entry.MinScale), FMath::Max(Entry.MinScale, Entry.MaxScale));
		Spawn.bAlignToGround = true;
	}
}

void FEnemyBaseSetupBuilder::ScatterDecals(const FEnemyDecalSettings& Settings, const int32 Cluster)
{
	if (Settings.Decals.IsEmpty())
	{
		return;
	}

	const FBox2D Bounds = ComputeClusterBounds(Cluster);
	if (not Bounds.bIsValid)
	{
		return;
	}

	double TotalWeight = 0.0;
	for (const FEnemyDecalEntry& Entry : Settings.Decals)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}

	const int32 Count = EBS_RandomRoundCount(M_Random, Settings.CountPerSetup);
	for (int32 DecalIndex = 0; DecalIndex < Count; ++DecalIndex)
	{
		const FVector2D Position = SampleRingPosition(
			Bounds, Settings.MinDistanceFromBounds, Settings.MaxDistanceFromBounds);
		if (M_IsBlocked && M_IsBlocked(Position))
		{
			continue;
		}

		int32 EntryIndex = 0;
		double Roll = M_Random.FRand() * FMath::Max<double>(KINDA_SMALL_NUMBER, TotalWeight);
		for (int32 CandidateIndex = 0; CandidateIndex < Settings.Decals.Num(); ++CandidateIndex)
		{
			Roll -= FMath::Max(0.0f, Settings.Decals[CandidateIndex].Weight);
			if (Roll <= 0.0)
			{
				EntryIndex = CandidateIndex;
				break;
			}
		}

		const FEnemyDecalEntry& Entry = Settings.Decals[EntryIndex];
		FEnemyDecalSpawn& Spawn = M_Result.Decals.Emplace_GetRef();
		Spawn.EntryIndex = EntryIndex;
		Spawn.ClusterIndex = Cluster;
		Spawn.Position = Position;
		Spawn.YawRadians = M_Random.FRandRange(0.0, 2.0 * PI);
		Spawn.Size = M_Random.FRandRange(
			FMath::Max(1.0f, Entry.MinScale), FMath::Max(Entry.MinScale, Entry.MaxScale));
	}
}
