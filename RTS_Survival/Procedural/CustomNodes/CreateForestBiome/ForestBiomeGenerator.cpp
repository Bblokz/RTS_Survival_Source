// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "ForestBiomeGenerator.h"

namespace ForestBiomeGenConstants
{
	// One density tile is DensityAreaUnit x DensityAreaUnit units (1000 x 1000 = 10 m x 10 m).
	constexpr double DensityAreaUnit = 1000.0;

	// Rejection sampling budget: attempts allowed per requested placement before giving up. Keeps
	// generation bounded when the area is nearly full and no free spot remains.
	constexpr int32 AttemptsPerPlacement = 6;

	// Spatial-hash cell size; roughly the typical query reach for good locality.
	constexpr double OccupancyCellSize = 600.0;

	// Number of Monte Carlo samples used to estimate the spawnable fraction of the bounds.
	constexpr int32 InsideFractionSampleCount = 1024;

	// Lower bound on the spawnable fraction when sizing the attempt budget, so a mostly-excluded
	// biome cannot inflate the attempt count without limit.
	constexpr double MinInsideFractionForBudget = 0.05;

	constexpr double TwoPi = 2.0 * PI;
}

FForestBiomeGenerator::FForestBiomeGenerator(const FForestBiomeGenParams& InParams)
	: M_Params(InParams)
	, M_Random(InParams.RandomSeed)
	, M_Occupancy(ForestBiomeGenConstants::OccupancyCellSize)
{
}

void FForestBiomeGenerator::Generate(FForestBiomeGenResult& OutResult)
{
	ComputeInsideFraction();

	// Order matters: the biggest, most important props reserve space first and auxiliaries fill
	// whatever the trees and bushes left free.
	ScatterProps(M_Params.LargeTrees, ComputeTargetCount(M_Params.LargeTreesPer1000),
		EForestOccupancy::LargeTree, ForestSolidPropMask(), OutResult.LargeTrees);

	ScatterProps(M_Params.RegularTrees, ComputeTargetCount(M_Params.RegularTreesPer1000),
		EForestOccupancy::RegularTree, ForestSolidPropMask(), OutResult.RegularTrees);

	ScatterProps(M_Params.Bushes, ComputeTargetCount(M_Params.BushesPer1000),
		EForestOccupancy::Bush, ForestSolidPropMask(), OutResult.Bushes);

	ScatterFoliage(ComputeTargetCount(M_Params.FoliagePer1000), OutResult.Foliage);

	const double GlobalDecalSizeMultiplier = PickScale(
		true, M_Params.MinGlobalDecalSizeMultiplier, M_Params.MaxGlobalDecalSizeMultiplier);
	ScatterDecals(
		ComputeTargetCount(M_Params.DecalsPer1000), GlobalDecalSizeMultiplier, OutResult.Decals);

	ScatterAuxiliaries(OutResult);
}

void FForestBiomeGenerator::ComputeInsideFraction()
{
	// With no area shape and no exclusion the whole bounding rectangle is spawnable.
	if (not M_Params.IsInsideArea && not M_Params.IsExcluded)
	{
		M_InsideFraction = 1.0;
		return;
	}

	int32 AllowedSamples = 0;
	for (int32 Sample = 0; Sample < ForestBiomeGenConstants::InsideFractionSampleCount; ++Sample)
	{
		if (IsAllowedPoint(RandomLocalPoint()))
		{
			++AllowedSamples;
		}
	}

	M_InsideFraction =
		static_cast<double>(AllowedSamples) / ForestBiomeGenConstants::InsideFractionSampleCount;
}

int32 FForestBiomeGenerator::ComputeTargetCount(const double DensityPer1000) const
{
	if (DensityPer1000 <= 0.0 || M_InsideFraction <= 0.0)
	{
		return 0;
	}

	const double AreaUnit = ForestBiomeGenConstants::DensityAreaUnit;
	const double AreaTiles = (M_Params.BiomeLengthX * M_Params.BiomeLengthY) / (AreaUnit * AreaUnit);
	const double SpawnableTiles = AreaTiles * M_InsideFraction;
	const double Count = DensityPer1000 * SpawnableTiles * FMath::Max(0.0, M_Params.GlobalDensityMultiplier);
	return FMath::Max(0, FMath::RoundToInt32(Count));
}

int32 FForestBiomeGenerator::ComputeAttemptBudget(const int32 TargetCount) const
{
	// Most attempts land outside the spawnable area when the fraction is small; scale the budget by
	// its inverse so the target can still be reached, with a floor to keep it bounded.
	const double Fraction = FMath::Max(ForestBiomeGenConstants::MinInsideFractionForBudget, M_InsideFraction);
	return FMath::CeilToInt32(TargetCount * ForestBiomeGenConstants::AttemptsPerPlacement / Fraction);
}

void FForestBiomeGenerator::ScatterProps(
	const TArray<FForestResolvedProp>& Entries,
	const int32 TargetCount,
	const EForestOccupancy ReserveType,
	const uint8 AvoidMask,
	TArray<FForestPlacement>& OutPlacements)
{
	if (Entries.IsEmpty() || TargetCount <= 0)
	{
		return;
	}

	const int32 MaxAttempts = ComputeAttemptBudget(TargetCount);
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < MaxAttempts && Placed < TargetCount; ++Attempt)
	{
		const int32 EntryIndex = PickWeightedProp(Entries);
		if (EntryIndex == INDEX_NONE)
		{
			return;
		}

		const FForestResolvedProp& Entry = Entries[EntryIndex];
		const double Scale = PickScale(Entry.bOverrideScale, Entry.MinScale, Entry.MaxScale);
		const double Radius = Entry.Radius * Scale;

		const FVector2D Point = RandomLocalPoint();
		if (not IsAllowedPoint(Point))
		{
			continue;
		}
		if (M_Occupancy.OverlapsAny(Point, Radius, AvoidMask, M_Params.PropSpacing))
		{
			continue;
		}

		M_Occupancy.Add(Point, Radius, ReserveType);

		FForestPlacement& Placement = OutPlacements.Emplace_GetRef();
		Placement.EntryIndex = EntryIndex;
		Placement.Position = Point;
		Placement.YawRadians = M_Random.FRandRange(0.0, ForestBiomeGenConstants::TwoPi);
		Placement.UniformScale = Scale;
		++Placed;
	}
}

void FForestBiomeGenerator::ScatterFoliage(const int32 TargetCount, TArray<FForestPlacement>& OutPlacements)
{
	if (M_Params.Foliage.IsEmpty() || TargetCount <= 0)
	{
		return;
	}

	const int32 MaxAttempts = ComputeAttemptBudget(TargetCount);
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < MaxAttempts && Placed < TargetCount; ++Attempt)
	{
		const int32 EntryIndex = PickWeightedFoliage(M_Params.Foliage);
		if (EntryIndex == INDEX_NONE)
		{
			return;
		}

		const FForestResolvedFoliage& Entry = M_Params.Foliage[EntryIndex];
		const double Scale = PickScale(true, Entry.MinScale, Entry.MaxScale);
		const double Radius = Entry.Radius * Scale;

		const FVector2D Point = RandomLocalPoint();
		if (not IsAllowedPoint(Point))
		{
			continue;
		}
		// Groundcover: keep clear of tree/bush trunks, but foliage may overlap other foliage.
		if (M_Occupancy.OverlapsAny(Point, Radius, ForestSolidPropMask(), 0.0))
		{
			continue;
		}

		FForestPlacement& Placement = OutPlacements.Emplace_GetRef();
		Placement.EntryIndex = EntryIndex;
		Placement.Position = Point;
		Placement.YawRadians = M_Random.FRandRange(0.0, ForestBiomeGenConstants::TwoPi);
		Placement.UniformScale = Scale;
		++Placed;
	}
}

void FForestBiomeGenerator::ScatterDecals(
	const int32 TargetCount,
	const double GlobalSizeMultiplier,
	TArray<FForestDecalPlacement>& OutPlacements)
{
	if (M_Params.Decals.IsEmpty() || TargetCount <= 0)
	{
		return;
	}

	const int32 MaxAttempts = ComputeAttemptBudget(TargetCount);
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < MaxAttempts && Placed < TargetCount; ++Attempt)
	{
		const int32 EntryIndex = PickWeightedDecal(M_Params.Decals);
		if (EntryIndex == INDEX_NONE)
		{
			return;
		}

		const FVector2D Point = RandomLocalPoint();
		if (not IsAllowedPoint(Point))
		{
			continue;
		}

		const FForestResolvedDecal& Entry = M_Params.Decals[EntryIndex];
		FForestDecalPlacement& Placement = OutPlacements.Emplace_GetRef();
		Placement.EntryIndex = EntryIndex;
		Placement.Position = Point;
		Placement.YawRadians = M_Random.FRandRange(0.0, ForestBiomeGenConstants::TwoPi);
		Placement.DecalSize = Entry.DecalSize * GlobalSizeMultiplier;
		++Placed;
	}
}

void FForestBiomeGenerator::ScatterAuxiliaries(FForestBiomeGenResult& OutResult)
{
	if (M_Params.Auxiliaries.IsEmpty())
	{
		return;
	}

	const int32 TotalCount = ComputeTargetCount(M_Params.AuxiliariesPer1000);
	if (TotalCount <= 0)
	{
		return;
	}

	double WeightSum = 0.0;
	for (const FForestResolvedAuxiliary& Entry : M_Params.Auxiliaries)
	{
		WeightSum += FMath::Max(0.0f, Entry.Weight);
	}
	if (WeightSum <= 0.0)
	{
		return;
	}

	// Place larger auxiliaries first: their wider isolation is reserved before smaller props fill in,
	// which is what makes large props end up more isolated than medium, and medium more than small.
	TArray<int32> Order;
	Order.Reserve(M_Params.Auxiliaries.Num());
	for (int32 Index = 0; Index < M_Params.Auxiliaries.Num(); ++Index)
	{
		Order.Add(Index);
	}
	Order.Sort([this](const int32 A, const int32 B)
	{
		return static_cast<uint8>(M_Params.Auxiliaries[A].Size) > static_cast<uint8>(M_Params.Auxiliaries[B].Size);
	});

	for (const int32 Index : Order)
	{
		const double Share = FMath::Max(0.0f, M_Params.Auxiliaries[Index].Weight) / WeightSum;
		const int32 EntryCount = FMath::RoundToInt32(TotalCount * Share);
		ScatterAuxiliaryEntry(Index, EntryCount, OutResult.Auxiliaries);
	}
}

void FForestBiomeGenerator::ScatterAuxiliaryEntry(
	const int32 ResolvedIndex,
	const int32 TargetCount,
	TArray<FForestPlacement>& OutPlacements)
{
	if (TargetCount <= 0)
	{
		return;
	}

	const FForestResolvedAuxiliary& Entry = M_Params.Auxiliaries[ResolvedIndex];
	const uint8 AuxiliaryMask = ForestOccupancyMask(EForestOccupancy::Auxiliary);

	const int32 MaxAttempts = ComputeAttemptBudget(TargetCount);
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < MaxAttempts && Placed < TargetCount; ++Attempt)
	{
		const double Scale = PickScale(Entry.bOverrideScale, Entry.MinScale, Entry.MaxScale);
		const double Radius = Entry.Radius * Scale;

		const FVector2D Point = RandomLocalPoint();
		if (not IsAllowedPoint(Point))
		{
			continue;
		}
		// Never sit on a tree or bush.
		if (M_Occupancy.OverlapsAny(Point, Radius, ForestSolidPropMask(), 0.0))
		{
			continue;
		}
		// Keep the size-scaled isolation distance clear of every other auxiliary.
		if (M_Occupancy.OverlapsAny(Point, Radius, AuxiliaryMask, Entry.IsolationDistance))
		{
			continue;
		}

		M_Occupancy.Add(Point, Radius, EForestOccupancy::Auxiliary);

		FForestPlacement& Placement = OutPlacements.Emplace_GetRef();
		Placement.EntryIndex = ResolvedIndex;
		Placement.Position = Point;
		Placement.YawRadians = M_Random.FRandRange(0.0, ForestBiomeGenConstants::TwoPi);
		Placement.UniformScale = Scale;
		++Placed;
	}
}

FVector2D FForestBiomeGenerator::RandomLocalPoint()
{
	return FVector2D(
		M_Random.FRandRange(-0.5 * M_Params.BiomeLengthX, 0.5 * M_Params.BiomeLengthX),
		M_Random.FRandRange(-0.5 * M_Params.BiomeLengthY, 0.5 * M_Params.BiomeLengthY));
}

bool FForestBiomeGenerator::IsAllowedPoint(const FVector2D& Point) const
{
	if (M_Params.IsInsideArea && not M_Params.IsInsideArea(Point))
	{
		return false;
	}
	if (M_Params.IsExcluded && M_Params.IsExcluded(Point))
	{
		return false;
	}
	return true;
}

double FForestBiomeGenerator::PickScale(const bool bOverride, const float MinScale, const float MaxScale)
{
	if (not bOverride)
	{
		return 1.0;
	}

	const double Low = FMath::Min(MinScale, MaxScale);
	const double High = FMath::Max(MinScale, MaxScale);
	return M_Random.FRandRange(Low, High);
}

int32 FForestBiomeGenerator::PickWeightedProp(const TArray<FForestResolvedProp>& Entries)
{
	if (Entries.IsEmpty())
	{
		return INDEX_NONE;
	}

	double WeightSum = 0.0;
	for (const FForestResolvedProp& Entry : Entries)
	{
		WeightSum += FMath::Max(0.0f, Entry.Weight);
	}
	if (WeightSum <= 0.0)
	{
		return 0;
	}

	double Roll = M_Random.FRandRange(0.0, WeightSum);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Roll -= FMath::Max(0.0f, Entries[Index].Weight);
		if (Roll <= 0.0)
		{
			return Index;
		}
	}
	return Entries.Num() - 1;
}

int32 FForestBiomeGenerator::PickWeightedFoliage(const TArray<FForestResolvedFoliage>& Entries)
{
	if (Entries.IsEmpty())
	{
		return INDEX_NONE;
	}

	double WeightSum = 0.0;
	for (const FForestResolvedFoliage& Entry : Entries)
	{
		WeightSum += FMath::Max(0.0f, Entry.Weight);
	}
	if (WeightSum <= 0.0)
	{
		return 0;
	}

	double Roll = M_Random.FRandRange(0.0, WeightSum);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Roll -= FMath::Max(0.0f, Entries[Index].Weight);
		if (Roll <= 0.0)
		{
			return Index;
		}
	}
	return Entries.Num() - 1;
}

int32 FForestBiomeGenerator::PickWeightedDecal(const TArray<FForestResolvedDecal>& Entries)
{
	if (Entries.IsEmpty())
	{
		return INDEX_NONE;
	}

	double WeightSum = 0.0;
	for (const FForestResolvedDecal& Entry : Entries)
	{
		WeightSum += FMath::Max(0.0f, Entry.Weight);
	}
	if (WeightSum <= 0.0)
	{
		return 0;
	}

	double Roll = M_Random.FRandRange(0.0, WeightSum);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Roll -= FMath::Max(0.0f, Entries[Index].Weight);
		if (Roll <= 0.0)
		{
			return Index;
		}
	}
	return Entries.Num() - 1;
}
