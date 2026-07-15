// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGFortifiedBuilding.h"

#include "EnemyBaseSetupPlacement.h"
#include "PCGEnemyBaseSetupShared.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PCGFortifiedBuilding"

namespace FortifiedBuildingConstants
{
	const FName PointsPinLabel = TEXT("Points");
	constexpr double OccupancyCellSize = 800.0;
}

namespace FortifiedBuildingInternal
{
	using namespace EnemyBaseSetupShared;

	/** @brief Placement-side resolved entries for one Fortified Building execution. */
	struct FFortifiedResolved
	{
		TArray<FEnemyResolvedEntry> Buildings;
		TArray<FEnemyResolvedEntry> Sandbags;
		TArray<FEnemyResolvedEntry> Obstacles;
		TArray<FEnemyResolvedEntry> Wire;
		TArray<TArray<FEnemyResolvedEntry>> DecoratorProfiles;
	};

	void ResolveFortifiedAssets(
		FPCGContext* Context,
		const UPCGFortifiedBuildingSettings& Settings,
		UWorld& World,
		FEnemyBaseSetupAssets& OutAssets,
		FFortifiedResolved& OutResolved)
	{
		ResolveBlueprintList(Context, World, Settings.Buildings,
			OutAssets.Category(EEnemyPlacementCategory::Bunker), OutResolved.Buildings);
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
	}

	/** @brief Places one fortified building and its defensive works for a single input point. */
	void BuildOneFortifiedCluster(
		const UPCGFortifiedBuildingSettings& Settings,
		const FFortifiedResolved& Resolved,
		FEnemyBaseSetupBuilder& Builder,
		const FEnemyDefenseFrame& Frame)
	{
		const int32 Cluster = Builder.BeginCluster();

		FEnemyFootprint BuildingFootprint;
		if (not Builder.PlaceSingleBuilding(
			Resolved.Buildings, Frame, Settings.BuildingSpacingExtra, Cluster, BuildingFootprint))
		{
			return;
		}

		Builder.PlaceSandbagWall(Resolved.Sandbags, Settings.Sandbags, BuildingFootprint, Cluster);

		const double FrontDistance = BuildingFootprint.SupportRadius(Frame.Forward());
		const double HalfWidth = BuildingFootprint.SupportRadius(Frame.Right());
		Builder.PlaceObstacleBelt(Resolved.Obstacles, Settings.ObstacleBelt, Frame, FrontDistance, HalfWidth, Cluster);
		Builder.PlaceBarbedWire(Resolved.Wire, Settings.BarbedWire, Frame, FrontDistance, HalfWidth, Cluster);

		for (int32 ProfileIndex = 0; ProfileIndex < Resolved.DecoratorProfiles.Num(); ++ProfileIndex)
		{
			Builder.ScatterDecorators(
				Resolved.DecoratorProfiles[ProfileIndex], Settings.DecoratorProfiles[ProfileIndex], Cluster);
		}
		Builder.ScatterFoliage(Settings.Foliage, Cluster);
		Builder.ScatterDecals(Settings.Decals, Cluster);
	}

	void GenerateFortified(
		const UPCGFortifiedBuildingSettings& Settings,
		const FFortifiedResolved& Resolved,
		const TArray<FEnemyInputPoint>& InputPoints,
		const TArray<FVector2D>& AimTargets,
		TFunction<bool(const FVector2D&)> ExclusionPredicate,
		FEnemyBaseSetupResult& OutResult)
	{
		FEnemyOccupancyGrid Occupancy(FortifiedBuildingConstants::OccupancyCellSize);
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
			BuildOneFortifiedCluster(Settings, Resolved, Builder, Frame);
		}
	}

	void EmitFortifiedOutputs(
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
FName UPCGFortifiedBuildingSettings::GetDefaultNodeName() const
{
	return FName(TEXT("FortifiedBuilding"));
}

FText UPCGFortifiedBuildingSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Fortified Building");
}

FText UPCGFortifiedBuildingSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Places a military building at each input point with a sandbag wall (flanks / front / full "
		"ring), and optional anti-tank obstacle belt and barbed wire in front. The facing comes from "
		"the point rotation, a fixed yaw, or toward / away from the AimTarget. Buildings, Sandbags, "
		"Obstacles, BarbedWire and Decorators output points-with-bounds; TotalBounds encloses each "
		"cluster; Foliage outputs points for a Static Mesh Spawner. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGFortifiedBuildingSettings::CreateElement() const
{
	return MakeShared<FPCGFortifiedBuildingElement>();
}

TArray<FPCGPinProperties> UPCGFortifiedBuildingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// One building is placed at each of these points.
	Pins.Emplace_GetRef(FortifiedBuildingConstants::PointsPinLabel, EPCGDataType::Point).SetRequiredPin();
	// Optional target the buildings face toward / away from (its nearest point is used per building).
	Pins.Emplace(EnemyBaseSetupShared::Pins::AimTarget, EPCGDataType::Spatial);
	// Optional exclusion: nothing is placed where these sample density > 0.
	Pins.Emplace(EnemyBaseSetupShared::Pins::Exclusion, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGFortifiedBuildingSettings::OutputPinProperties() const
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

bool FPCGFortifiedBuildingElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	using namespace FortifiedBuildingInternal;

	const UPCGFortifiedBuildingSettings* Settings = Context->GetInputSettings<UPCGFortifiedBuildingSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	const TArray<EnemyBaseSetupShared::FEnemyInputPoint> InputPoints =
		EnemyBaseSetupShared::CollectInputPoints(Context, FortifiedBuildingConstants::PointsPinLabel);
	const TArray<FVector2D> AimTargets =
		EnemyBaseSetupShared::CollectAimTargets(Context, EnemyBaseSetupShared::Pins::AimTarget);
	TFunction<bool(const FVector2D&)> ExclusionPredicate =
		EnemyBaseSetupShared::BuildExclusionPredicate(Context, EnemyBaseSetupShared::Pins::Exclusion);

	FEnemyBaseSetupAssets Assets;
	FFortifiedResolved Resolved;
	ResolveFortifiedAssets(Context, *Settings, World, Assets, Resolved);

	FEnemyBaseSetupResult Result;
	GenerateFortified(*Settings, Resolved, InputPoints, AimTargets, MoveTemp(ExclusionPredicate), Result);

	TArray<AActor*> SpawnedActors;
	EnemyBaseSetupShared::SpawnSetupActors(World, Result, Assets, SpawnedActors);
	EnemyBaseSetupShared::SpawnDecalsActor(World, Result, Assets, SpawnedActors);
	EnemyBaseSetupShared::RegisterManagedActors(Context, SpawnedActors);

	EmitFortifiedOutputs(Context, World, Result, Assets, Settings->RandomSeed, SpawnedActors);
	return true;
}

#undef LOCTEXT_NAMESPACE
