// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGEnemyBaseSetup.h"

#include "EnemyBaseSetupPlacement.h"
#include "PCGEnemyBaseSetupShared.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PCGEnemyBaseSetup"

namespace EnemyBaseSetupConstants
{
	const FName AreaPinLabel = TEXT("Area");
	const FName InteriorPointsPinLabel = TEXT("InteriorPoints");
	constexpr double OccupancyCellSize = 1200.0;
	constexpr int32 InteriorScatterAttempts = 8;
	constexpr double InteriorAreaMargin = 500.0;
}

namespace EnemyBaseSetupInternal
{
	using namespace EnemyBaseSetupShared;

	/** @brief One bunker line to lay along a base side: its center, outward facing and length. */
	struct FPerimeterLine
	{
		FVector2D Center = FVector2D::ZeroVector;
		double FacingYaw = 0.0;
		double Length = 0.0;
	};

	struct FMacroResolved
	{
		TArray<FEnemyResolvedEntry> PerimeterBunkers;
		TArray<FEnemyResolvedEntry> InteriorBuildings;
		TArray<FEnemyResolvedEntry> Sandbags;
		TArray<FEnemyResolvedEntry> Obstacles;
		TArray<FEnemyResolvedEntry> Wire;
		TArray<TArray<FEnemyResolvedEntry>> DecoratorProfiles;

		double MaxBunkerHalfDepth = 200.0;
		double MaxBunkerHalfWidth = 200.0;
	};

	void MeasureMaxBunkerExtents(FMacroResolved& Resolved)
	{
		for (const FEnemyResolvedEntry& Entry : Resolved.PerimeterBunkers)
		{
			Resolved.MaxBunkerHalfDepth = FMath::Max(Resolved.MaxBunkerHalfDepth, Entry.FootprintHalfExtents.X);
			Resolved.MaxBunkerHalfWidth = FMath::Max(Resolved.MaxBunkerHalfWidth, Entry.FootprintHalfExtents.Y);
		}
	}

	void ResolveMacroAssets(
		FPCGContext* Context,
		const UPCGEnemyBaseSetupSettings& Settings,
		UWorld& World,
		FEnemyBaseSetupAssets& OutAssets,
		FMacroResolved& OutResolved)
	{
		ResolveBlueprintList(Context, World, Settings.Bunkers,
			OutAssets.Category(EEnemyPlacementCategory::Bunker), OutResolved.PerimeterBunkers);
		ResolveBlueprintList(Context, World, Settings.InteriorBuildings,
			OutAssets.Category(EEnemyPlacementCategory::Bunker), OutResolved.InteriorBuildings);
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

	// --- Perimeter layout ---

	void AppendFullPerimeter(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FVector2D& Min,
		const FVector2D& Max,
		TArray<FPerimeterLine>& OutLines)
	{
		const FVector2D Center = (Min + Max) * 0.5;
		const double SideX = FMath::Max(0.0, (Max.X - Min.X) - Settings.LineMargin);
		const double SideY = FMath::Max(0.0, (Max.Y - Min.Y) - Settings.LineMargin);
		const double Inset = Settings.PerimeterInset;

		OutLines.Add({ FVector2D(Max.X - Inset, Center.Y), 0.0, SideY });
		OutLines.Add({ FVector2D(Min.X + Inset, Center.Y), PI, SideY });
		OutLines.Add({ FVector2D(Center.X, Max.Y - Inset), HALF_PI, SideX });
		OutLines.Add({ FVector2D(Center.X, Min.Y + Inset), -HALF_PI, SideX });
	}

	void AppendFrontLine(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FVector2D& Min,
		const FVector2D& Max,
		const FVector2D& AimDirection,
		TArray<FPerimeterLine>& OutLines)
	{
		const FVector2D Center = (Min + Max) * 0.5;
		const double SideX = FMath::Max(0.0, (Max.X - Min.X) - Settings.LineMargin);
		const double SideY = FMath::Max(0.0, (Max.Y - Min.Y) - Settings.LineMargin);
		const double Inset = Settings.PerimeterInset;

		// Pick the side whose outward normal best aligns with the aim direction.
		const FVector2D Normals[] = {
			FVector2D(1.0, 0.0), FVector2D(-1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(0.0, -1.0) };
		int32 BestSide = 0;
		double BestDot = -2.0;
		for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
		{
			const double Dot = FVector2D::DotProduct(Normals[SideIndex], AimDirection);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				BestSide = SideIndex;
			}
		}

		switch (BestSide)
		{
		case 1: OutLines.Add({ FVector2D(Min.X + Inset, Center.Y), PI, SideY }); break;
		case 2: OutLines.Add({ FVector2D(Center.X, Max.Y - Inset), HALF_PI, SideX }); break;
		case 3: OutLines.Add({ FVector2D(Center.X, Min.Y + Inset), -HALF_PI, SideX }); break;
		case 0:
		default: OutLines.Add({ FVector2D(Max.X - Inset, Center.Y), 0.0, SideY }); break;
		}
	}

	void AppendCorners(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FVector2D& Min,
		const FVector2D& Max,
		TArray<FPerimeterLine>& OutLines)
	{
		const double Inset = Settings.PerimeterInset;
		const double Length = Settings.CornerLineLength;
		const double Quarter = PI * 0.25;

		OutLines.Add({ FVector2D(Max.X - Inset, Max.Y - Inset), Quarter, Length });
		OutLines.Add({ FVector2D(Min.X + Inset, Max.Y - Inset), 3.0 * Quarter, Length });
		OutLines.Add({ FVector2D(Min.X + Inset, Min.Y + Inset), -3.0 * Quarter, Length });
		OutLines.Add({ FVector2D(Max.X - Inset, Min.Y + Inset), -Quarter, Length });
	}

	TArray<FPerimeterLine> ComputePerimeterLines(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FVector2D& Min,
		const FVector2D& Max,
		const FVector2D& AimDirection)
	{
		TArray<FPerimeterLine> Lines;
		switch (Settings.PerimeterLayout)
		{
		case EEnemyPerimeterLayout::FrontLine:
			AppendFrontLine(Settings, Min, Max, AimDirection, Lines);
			break;
		case EEnemyPerimeterLayout::Corners:
			AppendCorners(Settings, Min, Max, Lines);
			break;
		case EEnemyPerimeterLayout::FullPerimeter:
		default:
			AppendFullPerimeter(Settings, Min, Max, Lines);
			break;
		}
		return Lines;
	}

	// --- Cluster builders ---

	void ScatterClusterDecoration(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FMacroResolved& Resolved,
		FEnemyBaseSetupBuilder& Builder,
		const int32 Cluster)
	{
		for (int32 ProfileIndex = 0; ProfileIndex < Resolved.DecoratorProfiles.Num(); ++ProfileIndex)
		{
			Builder.ScatterDecorators(
				Resolved.DecoratorProfiles[ProfileIndex], Settings.DecoratorProfiles[ProfileIndex], Cluster);
		}
		Builder.ScatterFoliage(Settings.Foliage, Cluster);
		Builder.ScatterDecals(Settings.Decals, Cluster);
	}

	void BuildPerimeterLine(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FMacroResolved& Resolved,
		FEnemyBaseSetupBuilder& Builder,
		const FPerimeterLine& Line)
	{
		const int32 Cluster = Builder.BeginCluster();
		FEnemyDefenseFrame Frame;
		Frame.Origin = Line.Center;
		Frame.FacingYaw = Line.FacingYaw;

		Builder.PlaceBunkerRow(
			Resolved.PerimeterBunkers, Frame, Line.Length, Settings.BunkerSpacing,
			Settings.BunkerSpacingExtra, Cluster);

		const double HalfWidth = Line.Length * 0.5 + Resolved.MaxBunkerHalfWidth;
		if (Settings.Sandbags.bEnabled)
		{
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
		ScatterClusterDecoration(Settings, Resolved, Builder, Cluster);
	}

	bool BuildInteriorBuilding(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FMacroResolved& Resolved,
		FEnemyBaseSetupBuilder& Builder,
		const FEnemyDefenseFrame& Frame)
	{
		const int32 Cluster = Builder.BeginCluster();
		FEnemyFootprint BuildingFootprint;
		if (not Builder.PlaceSingleBuilding(
			Resolved.InteriorBuildings, Frame, Settings.InteriorSpacingExtra, Cluster, BuildingFootprint))
		{
			return false;
		}

		Builder.PlaceSandbagWall(Resolved.Sandbags, Settings.Sandbags, BuildingFootprint, Cluster);
		ScatterClusterDecoration(Settings, Resolved, Builder, Cluster);
		return true;
	}

	void BuildInteriorFromPoints(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FMacroResolved& Resolved,
		FEnemyBaseSetupBuilder& Builder,
		const TArray<FEnemyInputPoint>& InteriorPoints,
		const FVector2D& Center)
	{
		const TArray<FVector2D> FacingTargets = { Center };
		for (const FEnemyInputPoint& InteriorPoint : InteriorPoints)
		{
			FEnemyDefenseFrame Frame;
			Frame.Origin = InteriorPoint.Position;
			Frame.FacingYaw = ComputeFacingYaw(
				Settings.InteriorFacingMode, InteriorPoint.Position, InteriorPoint.YawRadians, 0.0, FacingTargets);
			BuildInteriorBuilding(Settings, Resolved, Builder, Frame);
		}
	}

	void ScatterInteriorBuildings(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FMacroResolved& Resolved,
		FEnemyBaseSetupBuilder& Builder,
		FRandomStream& Random,
		const TFunction<bool(const FVector2D&)>& ExclusionPredicate,
		const FVector2D& Min,
		const FVector2D& Max,
		const FVector2D& Center)
	{
		using namespace EnemyBaseSetupConstants;
		const TArray<FVector2D> FacingTargets = { Center };
		const FVector2D SampleMin(Min.X + InteriorAreaMargin, Min.Y + InteriorAreaMargin);
		const FVector2D SampleMax(Max.X - InteriorAreaMargin, Max.Y - InteriorAreaMargin);

		for (int32 BuildingIndex = 0; BuildingIndex < Settings.InteriorBuildingCount; ++BuildingIndex)
		{
			for (int32 Attempt = 0; Attempt < InteriorScatterAttempts; ++Attempt)
			{
				const FVector2D Position(
					Random.FRandRange(SampleMin.X, SampleMax.X),
					Random.FRandRange(SampleMin.Y, SampleMax.Y));
				if (ExclusionPredicate && ExclusionPredicate(Position))
				{
					continue;
				}

				FEnemyDefenseFrame Frame;
				Frame.Origin = Position;
				Frame.FacingYaw = ComputeFacingYaw(
					Settings.InteriorFacingMode, Position, 0.0, 0.0, FacingTargets);
				if (BuildInteriorBuilding(Settings, Resolved, Builder, Frame))
				{
					break;
				}
			}
		}
	}

	void GenerateEnemyBase(
		const UPCGEnemyBaseSetupSettings& Settings,
		const FMacroResolved& Resolved,
		const TArray<FPerimeterLine>& PerimeterLines,
		const TArray<FEnemyInputPoint>& InteriorPoints,
		const TFunction<bool(const FVector2D&)>& ExclusionPredicate,
		const FVector2D& Min,
		const FVector2D& Max,
		const FVector2D& Center,
		FEnemyBaseSetupResult& OutResult)
	{
		FEnemyOccupancyGrid Occupancy(EnemyBaseSetupConstants::OccupancyCellSize);
		FRandomStream Random(Settings.RandomSeed);
		FEnemyBaseSetupBuilder Builder(Random, Occupancy, OutResult);
		if (ExclusionPredicate)
		{
			Builder.SetBlockedPredicate(ExclusionPredicate);
		}

		for (const FPerimeterLine& Line : PerimeterLines)
		{
			BuildPerimeterLine(Settings, Resolved, Builder, Line);
		}

		if (not InteriorPoints.IsEmpty())
		{
			BuildInteriorFromPoints(Settings, Resolved, Builder, InteriorPoints, Center);
		}
		else
		{
			ScatterInteriorBuildings(
				Settings, Resolved, Builder, Random, ExclusionPredicate, Min, Max, Center);
		}
	}

	void EmitMacroOutputs(
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

	FVector2D ResolveAimDirection(const TArray<FVector2D>& AimTargets, const FVector2D& Center)
	{
		if (AimTargets.IsEmpty())
		{
			return FVector2D(1.0, 0.0);
		}
		const FVector2D Direction = AimTargets[0] - Center;
		return Direction.IsNearlyZero() ? FVector2D(1.0, 0.0) : Direction.GetSafeNormal();
	}
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FName UPCGEnemyBaseSetupSettings::GetDefaultNodeName() const
{
	return FName(TEXT("EnemyBaseSetup"));
}

FText UPCGEnemyBaseSetupSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Enemy Base Setup");
}

FText UPCGEnemyBaseSetupSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Fortifies a whole base area: wraps it in bunker lines (full perimeter, a front line facing "
		"the AimTarget, or corner strongpoints) with obstacle belts, barbed wire and sandbag "
		"parapets, and drops fortified interior buildings (at InteriorPoints or scattered) facing "
		"outward. Decorators, foliage and decals dress every cluster. Everything shares one occupancy "
		"grid so nothing overlaps. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGEnemyBaseSetupSettings::CreateElement() const
{
	return MakeShared<FPCGEnemyBaseSetupElement>();
}

TArray<FPCGPinProperties> UPCGEnemyBaseSetupSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// The base area; its bounds define the perimeter rectangle.
	Pins.Emplace_GetRef(EnemyBaseSetupConstants::AreaPinLabel, EPCGDataType::Spatial).SetRequiredPin();
	// Optional explicit interior building positions; when empty, interior buildings are scattered.
	Pins.Emplace(EnemyBaseSetupConstants::InteriorPointsPinLabel, EPCGDataType::Point);
	Pins.Emplace(EnemyBaseSetupShared::Pins::AimTarget, EPCGDataType::Spatial);
	Pins.Emplace(EnemyBaseSetupShared::Pins::Exclusion, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGEnemyBaseSetupSettings::OutputPinProperties() const
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

bool FPCGEnemyBaseSetupElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	using namespace EnemyBaseSetupInternal;

	const UPCGEnemyBaseSetupSettings* Settings = Context->GetInputSettings<UPCGEnemyBaseSetupSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	const TArray<FBox> AreaBounds =
		EnemyBaseSetupShared::CollectSpatialBounds(Context, EnemyBaseSetupConstants::AreaPinLabel);
	if (AreaBounds.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingArea",
			"EnemyBaseSetup: the Area pin requires valid spatial data (e.g. a closed spline or volume)."));
		return true;
	}
	FBox UnionBounds(EForceInit::ForceInit);
	for (const FBox& Bounds : AreaBounds)
	{
		UnionBounds += Bounds;
	}
	const FVector2D Min(UnionBounds.Min);
	const FVector2D Max(UnionBounds.Max);
	const FVector2D Center = (Min + Max) * 0.5;

	const TArray<FEnemyInputPoint> InteriorPoints =
		EnemyBaseSetupShared::CollectInputPoints(Context, EnemyBaseSetupConstants::InteriorPointsPinLabel);
	const TArray<FVector2D> AimTargets =
		EnemyBaseSetupShared::CollectAimTargets(Context, EnemyBaseSetupShared::Pins::AimTarget);
	const TFunction<bool(const FVector2D&)> ExclusionPredicate =
		EnemyBaseSetupShared::BuildExclusionPredicate(Context, EnemyBaseSetupShared::Pins::Exclusion);

	FEnemyBaseSetupAssets Assets;
	FMacroResolved Resolved;
	ResolveMacroAssets(Context, *Settings, World, Assets, Resolved);

	const FVector2D AimDirection = ResolveAimDirection(AimTargets, Center);
	const TArray<FPerimeterLine> PerimeterLines = ComputePerimeterLines(*Settings, Min, Max, AimDirection);

	FEnemyBaseSetupResult Result;
	GenerateEnemyBase(
		*Settings, Resolved, PerimeterLines, InteriorPoints, ExclusionPredicate, Min, Max, Center, Result);

	TArray<AActor*> SpawnedActors;
	EnemyBaseSetupShared::SpawnSetupActors(World, Result, Assets, SpawnedActors);
	EnemyBaseSetupShared::SpawnDecalsActor(World, Result, Assets, SpawnedActors);
	EnemyBaseSetupShared::RegisterManagedActors(Context, SpawnedActors);

	EmitMacroOutputs(Context, World, Result, Assets, Settings->RandomSeed, SpawnedActors);
	return true;
}

#undef LOCTEXT_NAMESPACE
