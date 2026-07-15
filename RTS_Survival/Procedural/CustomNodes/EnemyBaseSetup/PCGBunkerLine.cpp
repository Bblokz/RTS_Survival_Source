// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGBunkerLine.h"

#include "EnemyBaseSetupPlacement.h"
#include "PCGEnemyBaseSetupShared.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PCGBunkerLine"

namespace BunkerLineConstants
{
	const FName PointsPinLabel = TEXT("LineCenters");
	constexpr double OccupancyCellSize = 1000.0;
}

namespace BunkerLineInternal
{
	using namespace EnemyBaseSetupShared;

	struct FBunkerLineResolved
	{
		TArray<FEnemyResolvedEntry> Bunkers;
		TArray<FEnemyResolvedEntry> Sandbags;
		TArray<FEnemyResolvedEntry> Obstacles;
		TArray<FEnemyResolvedEntry> Wire;
		TArray<TArray<FEnemyResolvedEntry>> DecoratorProfiles;

		// Largest bunker half extents, used to size the belt / wire / parapet without per-bunker data.
		double MaxBunkerHalfDepth = 200.0;
		double MaxBunkerHalfWidth = 200.0;
	};

	void MeasureMaxBunkerExtents(FBunkerLineResolved& Resolved)
	{
		for (const FEnemyResolvedEntry& Entry : Resolved.Bunkers)
		{
			Resolved.MaxBunkerHalfDepth = FMath::Max(Resolved.MaxBunkerHalfDepth, Entry.FootprintHalfExtents.X);
			Resolved.MaxBunkerHalfWidth = FMath::Max(Resolved.MaxBunkerHalfWidth, Entry.FootprintHalfExtents.Y);
		}
	}

	void ResolveBunkerLineAssets(
		FPCGContext* Context,
		const UPCGBunkerLineSettings& Settings,
		UWorld& World,
		FEnemyBaseSetupAssets& OutAssets,
		FBunkerLineResolved& OutResolved)
	{
		ResolveBlueprintList(Context, World, Settings.Bunkers,
			OutAssets.Category(EEnemyPlacementCategory::Bunker), OutResolved.Bunkers);
		ResolveBlueprintList(Context, World, Settings.Sandbags.SandbagUnits,
			OutAssets.Category(EEnemyPlacementCategory::Sandbag), OutResolved.Sandbags);
		ResolveBlueprintList(Context, World, Settings.ObstacleBelt.Obstacles,
			OutAssets.Category(EEnemyPlacementCategory::Hedgehog), OutResolved.Obstacles);
		ResolveBlueprintList(Context, World, Settings.BarbedWire.WireSegments,
			OutAssets.Category(EEnemyPlacementCategory::BarbedWire), OutResolved.Wire);

		for (const FEnemyDecoratorProfile& Profile : Settings.DecoratorProfiles)
		{
			TArray<FEnemyResolvedEntry>& ProfileEntries = OutResolved.DecoratorProfiles.Emplace_GetRef();
			ResolveBlueprintList(Context, World, Profile.Decorators,
				OutAssets.Category(EEnemyPlacementCategory::Decorator), ProfileEntries);
		}

		ResolveFoliage(Settings.Foliage.Foliage, OutAssets.FoliageMeshPaths);
		ResolveDecals(Settings.Decals.Decals, OutAssets.DecalMaterials);
		MeasureMaxBunkerExtents(OutResolved);
	}

	void BuildOneBunkerLine(
		const UPCGBunkerLineSettings& Settings,
		const FBunkerLineResolved& Resolved,
		FEnemyBaseSetupBuilder& Builder,
		const FEnemyDefenseFrame& Frame)
	{
		const int32 Cluster = Builder.BeginCluster();

		Builder.PlaceBunkerRow(
			Resolved.Bunkers, Frame, Settings.LineLength, Settings.BunkerSpacing,
			Settings.BunkerSpacingExtra, Cluster);

		const double HalfWidth = Settings.LineLength * 0.5 + Resolved.MaxBunkerHalfWidth;

		if (Settings.Sandbags.bEnabled)
		{
			// Synthesise one footprint enclosing the whole row so the parapet wraps the line.
			FEnemyFootprint LineFootprint;
			LineFootprint.Center = Frame.Origin;
			LineFootprint.YawRadians = Frame.FacingYaw;
			LineFootprint.HalfExtents = FVector2D(Resolved.MaxBunkerHalfDepth, HalfWidth);
			Builder.PlaceSandbagWall(Resolved.Sandbags, Settings.Sandbags, LineFootprint, Cluster);
		}

		Builder.PlaceObstacleBelt(
			Resolved.Obstacles, Settings.ObstacleBelt, Frame, Resolved.MaxBunkerHalfDepth, HalfWidth, Cluster);
		Builder.PlaceBarbedWire(
			Resolved.Wire, Settings.BarbedWire, Frame, Resolved.MaxBunkerHalfDepth, HalfWidth, Cluster);

		for (int32 ProfileIndex = 0; ProfileIndex < Resolved.DecoratorProfiles.Num(); ++ProfileIndex)
		{
			Builder.ScatterDecorators(
				Resolved.DecoratorProfiles[ProfileIndex], Settings.DecoratorProfiles[ProfileIndex], Cluster);
		}
		Builder.ScatterFoliage(Settings.Foliage, Cluster);
		Builder.ScatterDecals(Settings.Decals, Cluster);
	}

	void GenerateBunkerLines(
		const UPCGBunkerLineSettings& Settings,
		const FBunkerLineResolved& Resolved,
		const TArray<FEnemyInputPoint>& InputPoints,
		const TArray<FVector2D>& AimTargets,
		TFunction<bool(const FVector2D&)> ExclusionPredicate,
		FEnemyBaseSetupResult& OutResult)
	{
		FEnemyOccupancyGrid Occupancy(BunkerLineConstants::OccupancyCellSize);
		FRandomStream Random(Settings.RandomSeed);
		FEnemyBaseSetupBuilder Builder(Random, Occupancy, OutResult);
		if (ExclusionPredicate)
		{
			Builder.SetBlockedPredicate(MoveTemp(ExclusionPredicate));
		}

		for (const FEnemyInputPoint& InputPoint : InputPoints)
		{
			FEnemyDefenseFrame Frame;
			Frame.Origin = InputPoint.Position;
			Frame.FacingYaw = ComputeFacingYaw(
				Settings.FacingMode, InputPoint.Position, InputPoint.YawRadians,
				Settings.FixedYawDegrees, AimTargets);
			BuildOneBunkerLine(Settings, Resolved, Builder, Frame);
		}
	}

	void EmitBunkerLineOutputs(
		FPCGContext* Context,
		UWorld& World,
		const FEnemyBaseSetupResult& Result,
		const FEnemyBaseSetupAssets& Assets,
		const int32 RandomSeed,
		const TArray<AActor*>& SpawnedActors)
	{
		EmitCategoryOutput(Context, World, Result, Assets,
			EEnemyPlacementCategory::Bunker, Pins::Bunkers, RandomSeed, SpawnedActors);
		EmitCategoryOutput(Context, World, Result, Assets,
			EEnemyPlacementCategory::Sandbag, Pins::Sandbags, RandomSeed, SpawnedActors);
		EmitCategoryOutput(Context, World, Result, Assets,
			EEnemyPlacementCategory::Hedgehog, Pins::Obstacles, RandomSeed, SpawnedActors);
		EmitCategoryOutput(Context, World, Result, Assets,
			EEnemyPlacementCategory::BarbedWire, Pins::BarbedWire, RandomSeed, SpawnedActors);
		EmitCategoryOutput(Context, World, Result, Assets,
			EEnemyPlacementCategory::Decorator, Pins::Decorators, RandomSeed, SpawnedActors);
		EmitTotalBoundsOutput(Context, World, Result, Pins::TotalBounds, RandomSeed, SpawnedActors);
		EmitOccupiedBoundsOutput(Context, World, Result, Pins::OccupiedBounds, RandomSeed, SpawnedActors);
		EmitFoliageOutput(Context, World, Result, Assets, Pins::Foliage, RandomSeed, SpawnedActors);
	}
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FName UPCGBunkerLineSettings::GetDefaultNodeName() const
{
	return FName(TEXT("BunkerLine"));
}

FText UPCGBunkerLineSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Bunker Line");
}

FText UPCGBunkerLineSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Lays a row of bunkers at each input point facing a chosen direction, with an anti-tank "
		"obstacle belt (straight, arc or double row) and barbed wire in front and an optional sandbag "
		"parapet wrapping the row. Facing comes from the point rotation, a fixed yaw, or toward / away "
		"from the AimTarget. Bunkers, Sandbags, Obstacles, BarbedWire and Decorators output "
		"points-with-bounds; TotalBounds encloses each line. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGBunkerLineSettings::CreateElement() const
{
	return MakeShared<FPCGBunkerLineElement>();
}

TArray<FPCGPinProperties> UPCGBunkerLineSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// One bunker line is centered on each of these points.
	Pins.Emplace_GetRef(BunkerLineConstants::PointsPinLabel, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(EnemyBaseSetupShared::Pins::AimTarget, EPCGDataType::Spatial);
	Pins.Emplace(EnemyBaseSetupShared::Pins::Exclusion, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGBunkerLineSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(EnemyBaseSetupShared::Pins::TotalBounds, EPCGDataType::Point);
	Pins.Emplace(EnemyBaseSetupShared::Pins::Bunkers, EPCGDataType::Point);
	Pins.Emplace(EnemyBaseSetupShared::Pins::Sandbags, EPCGDataType::Point);
	Pins.Emplace(EnemyBaseSetupShared::Pins::Obstacles, EPCGDataType::Point);
	Pins.Emplace(EnemyBaseSetupShared::Pins::BarbedWire, EPCGDataType::Point);
	Pins.Emplace(EnemyBaseSetupShared::Pins::Decorators, EPCGDataType::Point);
	Pins.Emplace(EnemyBaseSetupShared::Pins::Foliage, EPCGDataType::Point);
	Pins.Emplace(EnemyBaseSetupShared::Pins::OccupiedBounds, EPCGDataType::Point);
	return Pins;
}

// ---------------------------------------------------------------------------
// Element
// ---------------------------------------------------------------------------

bool FPCGBunkerLineElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	using namespace BunkerLineInternal;

	const UPCGBunkerLineSettings* Settings = Context->GetInputSettings<UPCGBunkerLineSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	const TArray<EnemyBaseSetupShared::FEnemyInputPoint> InputPoints =
		EnemyBaseSetupShared::CollectInputPoints(Context, BunkerLineConstants::PointsPinLabel);
	const TArray<FVector2D> AimTargets =
		EnemyBaseSetupShared::CollectAimTargets(Context, EnemyBaseSetupShared::Pins::AimTarget);
	TFunction<bool(const FVector2D&)> ExclusionPredicate =
		EnemyBaseSetupShared::BuildExclusionPredicate(Context, EnemyBaseSetupShared::Pins::Exclusion);

	FEnemyBaseSetupAssets Assets;
	FBunkerLineResolved Resolved;
	ResolveBunkerLineAssets(Context, *Settings, World, Assets, Resolved);

	FEnemyBaseSetupResult Result;
	GenerateBunkerLines(*Settings, Resolved, InputPoints, AimTargets, MoveTemp(ExclusionPredicate), Result);

	TArray<AActor*> SpawnedActors;
	EnemyBaseSetupShared::SpawnSetupActors(World, Result, Assets, SpawnedActors);
	EnemyBaseSetupShared::SpawnDecalsActor(World, Result, Assets, SpawnedActors);
	EnemyBaseSetupShared::RegisterManagedActors(Context, SpawnedActors);

	EmitBunkerLineOutputs(Context, World, Result, Assets, Settings->RandomSeed, SpawnedActors);
	return true;
}

#undef LOCTEXT_NAMESPACE
