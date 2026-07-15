// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGSymmetricalDefenses.h"

#include "EnemyBaseSetupPlacement.h"
#include "PCGEnemyBaseSetupShared.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PCGSymmetricalDefenses"

namespace SymmetricalDefensesConstants
{
	const FName CandidatePointsPinLabel = TEXT("CandidatePoints");
	const FName DefendCenterPinLabel = TEXT("DefendCenter");
	const FName UnusedPointsPinLabel = TEXT("UnusedPoints");
	constexpr double OccupancyCellSize = 1000.0;
}

namespace SymmetricalDefensesInternal
{
	using namespace EnemyBaseSetupShared;

	/** @brief One candidate defensive position: its XY and its distance to the defended center. */
	struct FSymCandidate
	{
		FVector2D Position = FVector2D::ZeroVector;
		int32 SourceIndex = INDEX_NONE;
		double DistanceSquaredToCenter = 0.0;
	};

	/** @brief Placement-side resolved entries; identical layout to the Fortified Building node. */
	struct FSymResolved
	{
		TArray<FEnemyResolvedEntry> Buildings;
		TArray<FEnemyResolvedEntry> Sandbags;
		TArray<FEnemyResolvedEntry> Obstacles;
		TArray<FEnemyResolvedEntry> Wire;
		TArray<TArray<FEnemyResolvedEntry>> DecoratorProfiles;
	};

	// --- Candidate collection & symmetry selection ---

	TArray<FPCGPoint> CollectCandidatePoints(FPCGContext* Context)
	{
		TArray<FPCGPoint> Points;
		for (const FPCGTaggedData& Input :
			Context->InputData.GetInputsByPin(SymmetricalDefensesConstants::CandidatePointsPinLabel))
		{
			if (const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data))
			{
				Points.Append(PointData->GetPoints());
			}
		}
		return Points;
	}

	TArray<FSymCandidate> BuildEligibleCandidates(
		const TArray<FPCGPoint>& CandidatePoints,
		const FVector2D& Center,
		const TFunction<bool(const FVector2D&)>& ExclusionPredicate)
	{
		TArray<FSymCandidate> Candidates;
		for (int32 CandidateIndex = 0; CandidateIndex < CandidatePoints.Num(); ++CandidateIndex)
		{
			const FVector2D Position(CandidatePoints[CandidateIndex].Transform.GetLocation());
			if (ExclusionPredicate && ExclusionPredicate(Position))
			{
				continue;
			}
			FSymCandidate& Candidate = Candidates.Emplace_GetRef();
			Candidate.Position = Position;
			Candidate.SourceIndex = CandidateIndex;
			Candidate.DistanceSquaredToCenter = FVector2D::DistSquared(Position, Center);
		}

		Candidates.Sort([](const FSymCandidate& Left, const FSymCandidate& Right)
		{
			if (not FMath::IsNearlyEqual(Left.DistanceSquaredToCenter, Right.DistanceSquaredToCenter))
			{
				return Left.DistanceSquaredToCenter < Right.DistanceSquaredToCenter;
			}
			return Left.SourceIndex < Right.SourceIndex;
		});
		return Candidates;
	}

	int32 FindNearestAvailable(
		const TArray<FSymCandidate>& Candidates,
		const FVector2D& Target,
		const TArray<bool>& Used,
		const double Tolerance)
	{
		int32 BestIndex = INDEX_NONE;
		double BestDistanceSquared = Tolerance * Tolerance;
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			if (Used[CandidateIndex])
			{
				continue;
			}
			const double DistanceSquared = FVector2D::DistSquared(Candidates[CandidateIndex].Position, Target);
			if (DistanceSquared <= BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				BestIndex = CandidateIndex;
			}
		}
		return BestIndex;
	}

	bool AddUniqueMember(const int32 CandidateIndex, TArray<int32>& OutGroup)
	{
		if (CandidateIndex == INDEX_NONE)
		{
			return false;
		}
		OutGroup.AddUnique(CandidateIndex);
		return true;
	}

	/** @brief Builds a group from the ideal symmetric targets of a seed; empty if any target is unmatched. */
	TArray<int32> BuildGroupFromTargets(
		const TArray<FVector2D>& Targets,
		const TArray<FSymCandidate>& Candidates,
		const TArray<bool>& Used,
		const double Tolerance)
	{
		TArray<int32> Group;
		for (const FVector2D& Target : Targets)
		{
			const int32 MatchIndex = FindNearestAvailable(Candidates, Target, Used, Tolerance);
			if (not AddUniqueMember(MatchIndex, Group))
			{
				return {};
			}
		}
		return Group;
	}

	TArray<FVector2D> RectangularTargets(const FVector2D& SeedOffset, const FVector2D& Center)
	{
		const double AbsX = FMath::Abs(SeedOffset.X);
		const double AbsY = FMath::Abs(SeedOffset.Y);
		return {
			Center + FVector2D(AbsX, AbsY),
			Center + FVector2D(-AbsX, AbsY),
			Center + FVector2D(AbsX, -AbsY),
			Center + FVector2D(-AbsX, -AbsY)
		};
	}

	TArray<FVector2D> RadialTargets(const FVector2D& SeedOffset, const FVector2D& Center, const int32 Count)
	{
		const double Radius = SeedOffset.Size();
		const double BaseAngle = FMath::Atan2(SeedOffset.Y, SeedOffset.X);
		TArray<FVector2D> Targets;
		for (int32 Step = 0; Step < FMath::Max(2, Count); ++Step)
		{
			const double Angle = BaseAngle + 2.0 * PI * Step / FMath::Max(2, Count);
			Targets.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		return Targets;
	}

	TArray<FVector2D> AxisMirroredTargets(
		const FVector2D& SeedOffset,
		const FVector2D& Center,
		const FVector2D& AxisDirection)
	{
		// Reflection of the offset across the axis line: R(v) = 2 (v . d) d - v.
		const FVector2D Mirrored =
			AxisDirection * (2.0 * FVector2D::DotProduct(SeedOffset, AxisDirection)) - SeedOffset;
		return { Center + SeedOffset, Center + Mirrored };
	}

	bool GroupTooClose(
		const TArray<int32>& Group,
		const TArray<FSymCandidate>& Candidates,
		const TArray<FVector2D>& SelectedPositions,
		const double Spacing)
	{
		const double MinDistanceSquared = FMath::Square(Spacing);
		for (int32 GroupPos = 0; GroupPos < Group.Num(); ++GroupPos)
		{
			const FVector2D& Position = Candidates[Group[GroupPos]].Position;
			for (int32 OtherPos = GroupPos + 1; OtherPos < Group.Num(); ++OtherPos)
			{
				if (FVector2D::DistSquared(Position, Candidates[Group[OtherPos]].Position) < MinDistanceSquared)
				{
					return true;
				}
			}
			for (const FVector2D& Selected : SelectedPositions)
			{
				if (FVector2D::DistSquared(Position, Selected) < MinDistanceSquared)
				{
					return true;
				}
			}
		}
		return false;
	}

	TArray<int32> SelectSymmetricPositions(
		const UPCGSymmetricalDefensesSettings& Settings,
		const TArray<FSymCandidate>& Candidates,
		const FVector2D& Center,
		const FVector2D& AxisDirection)
	{
		const int32 Amount = FMath::Max(0, Settings.AmountToPlace);
		const double Tolerance = FMath::Max(1.0f, Settings.MatchTolerance);
		const double Spacing = FMath::Max(0.0f, Settings.PositionSpacing);

		TArray<bool> Used;
		Used.Init(false, Candidates.Num());
		TArray<int32> Selected;
		TArray<FVector2D> SelectedPositions;

		for (int32 SeedIndex = 0; SeedIndex < Candidates.Num() && Selected.Num() < Amount; ++SeedIndex)
		{
			if (Used[SeedIndex])
			{
				continue;
			}

			const FVector2D SeedOffset = Candidates[SeedIndex].Position - Center;
			TArray<FVector2D> Targets;
			switch (Settings.DefenseFormation)
			{
			case EEnemyDefenseFormation::RadialSymmetrical:
				Targets = RadialTargets(SeedOffset, Center, Settings.RadialSymmetryCount);
				break;
			case EEnemyDefenseFormation::AxisMirrored:
				Targets = AxisMirroredTargets(SeedOffset, Center, AxisDirection);
				break;
			case EEnemyDefenseFormation::RectangularSymmetrical:
			default:
				Targets = RectangularTargets(SeedOffset, Center);
				break;
			}

			const TArray<int32> Group = BuildGroupFromTargets(Targets, Candidates, Used, Tolerance);
			if (Group.IsEmpty() || Selected.Num() + Group.Num() > Amount
				|| GroupTooClose(Group, Candidates, SelectedPositions, Spacing))
			{
				continue;
			}
			for (const int32 MemberIndex : Group)
			{
				Used[MemberIndex] = true;
				Selected.Add(MemberIndex);
				SelectedPositions.Add(Candidates[MemberIndex].Position);
			}
		}
		return Selected;
	}

	// --- Asset resolution & placement (mirrors the Fortified Building node) ---

	void ResolveSymAssets(
		FPCGContext* Context,
		const UPCGSymmetricalDefensesSettings& Settings,
		UWorld& World,
		FEnemyBaseSetupAssets& OutAssets,
		FSymResolved& OutResolved)
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

	void BuildFortifiedCluster(
		const UPCGSymmetricalDefensesSettings& Settings,
		const FSymResolved& Resolved,
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

	void GenerateSymmetricalDefenses(
		const UPCGSymmetricalDefensesSettings& Settings,
		const FSymResolved& Resolved,
		const TArray<FSymCandidate>& Candidates,
		const TArray<int32>& Selected,
		const FVector2D& Center,
		FEnemyBaseSetupResult& OutResult)
	{
		FEnemyOccupancyGrid Occupancy(SymmetricalDefensesConstants::OccupancyCellSize);
		FRandomStream Random(Settings.RandomSeed);
		FEnemyBaseSetupBuilder Builder(Random, Occupancy, OutResult);

		// Facing references the defended center: AwayFromTarget faces outward, AimTowardsTarget inward.
		const TArray<FVector2D> FacingTargets = { Center };

		for (const int32 CandidateIndex : Selected)
		{
			FEnemyDefenseFrame Frame;
			Frame.Origin = Candidates[CandidateIndex].Position;
			Frame.FacingYaw = ComputeFacingYaw(
				Settings.FacingMode, Frame.Origin, 0.0, Settings.FixedYawDegrees, FacingTargets);
			BuildFortifiedCluster(Settings, Resolved, Builder, Frame);
		}
	}

	void EmitUnusedPoints(
		FPCGContext* Context,
		const TArray<FPCGPoint>& CandidatePoints,
		const TArray<FSymCandidate>& Candidates,
		const TArray<int32>& Selected)
	{
		TArray<bool> UsedSource;
		UsedSource.Init(false, CandidatePoints.Num());
		for (const int32 CandidateIndex : Selected)
		{
			const int32 SourceIndex = Candidates[CandidateIndex].SourceIndex;
			if (CandidatePoints.IsValidIndex(SourceIndex))
			{
				UsedSource[SourceIndex] = true;
			}
		}

		TArray<FPCGPoint> UnusedPoints;
		for (int32 PointIndex = 0; PointIndex < CandidatePoints.Num(); ++PointIndex)
		{
			if (not UsedSource[PointIndex])
			{
				UnusedPoints.Add(CandidatePoints[PointIndex]);
			}
		}

		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		PointData->InitializeFromData(nullptr);
		PointData->SetPoints(UnusedPoints);
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Pin = SymmetricalDefensesConstants::UnusedPointsPinLabel;
		Output.Data = PointData;
	}

	void EmitSymOutputs(
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

	FVector2D ResolveAxisDirection(const TArray<FVector2D>& AimTargets, const FVector2D& Center)
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
FName UPCGSymmetricalDefensesSettings::GetDefaultNodeName() const
{
	return FName(TEXT("SymmetricalDefenses"));
}

FText UPCGSymmetricalDefensesSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Symmetrical Defenses");
}

FText UPCGSymmetricalDefensesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Selects a symmetric set of positions from CandidatePoints around the DefendCenter "
		"(rectangular mirror, radial ring, or single-axis mirror) and builds a fortified building "
		"with sandbags, obstacle belt, wire and decorators at each, facing outward or inward. Unused "
		"candidates pass through on UnusedPoints. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGSymmetricalDefensesSettings::CreateElement() const
{
	return MakeShared<FPCGSymmetricalDefensesElement>();
}

TArray<FPCGPinProperties> UPCGSymmetricalDefensesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(SymmetricalDefensesConstants::CandidatePointsPinLabel, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace_GetRef(SymmetricalDefensesConstants::DefendCenterPinLabel, EPCGDataType::Spatial).SetRequiredPin();
	// Optional: the axis direction for AxisMirrored (and a facing override target).
	Pins.Emplace(EnemyBaseSetupShared::Pins::AimTarget, EPCGDataType::Spatial);
	Pins.Emplace(EnemyBaseSetupShared::Pins::Exclusion, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGSymmetricalDefensesSettings::OutputPinProperties() const
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
	Pins.Emplace(SymmetricalDefensesConstants::UnusedPointsPinLabel, EPCGDataType::Point);
	return Pins;
}

// ---------------------------------------------------------------------------
// Element
// ---------------------------------------------------------------------------

bool FPCGSymmetricalDefensesElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	using namespace SymmetricalDefensesInternal;

	const UPCGSymmetricalDefensesSettings* Settings =
		Context->GetInputSettings<UPCGSymmetricalDefensesSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	const TArray<FBox> CenterBounds =
		EnemyBaseSetupShared::CollectSpatialBounds(Context, SymmetricalDefensesConstants::DefendCenterPinLabel);
	if (CenterBounds.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingCenter",
			"SymmetricalDefenses: DefendCenter requires valid spatial data."));
		return true;
	}
	FBox UnionBounds(EForceInit::ForceInit);
	for (const FBox& Bounds : CenterBounds)
	{
		UnionBounds += Bounds;
	}
	const FVector2D Center(UnionBounds.GetCenter());

	const TArray<FPCGPoint> CandidatePoints = CollectCandidatePoints(Context);
	const TArray<FVector2D> AimTargets =
		EnemyBaseSetupShared::CollectAimTargets(Context, EnemyBaseSetupShared::Pins::AimTarget);
	const TFunction<bool(const FVector2D&)> ExclusionPredicate =
		EnemyBaseSetupShared::BuildExclusionPredicate(Context, EnemyBaseSetupShared::Pins::Exclusion);

	const TArray<FSymCandidate> Candidates = BuildEligibleCandidates(CandidatePoints, Center, ExclusionPredicate);
	const FVector2D AxisDirection = ResolveAxisDirection(AimTargets, Center);
	const TArray<int32> Selected = SelectSymmetricPositions(*Settings, Candidates, Center, AxisDirection);

	FEnemyBaseSetupAssets Assets;
	FSymResolved Resolved;
	ResolveSymAssets(Context, *Settings, World, Assets, Resolved);

	FEnemyBaseSetupResult Result;
	GenerateSymmetricalDefenses(*Settings, Resolved, Candidates, Selected, Center, Result);

	TArray<AActor*> SpawnedActors;
	EnemyBaseSetupShared::SpawnSetupActors(World, Result, Assets, SpawnedActors);
	EnemyBaseSetupShared::SpawnDecalsActor(World, Result, Assets, SpawnedActors);
	EnemyBaseSetupShared::RegisterManagedActors(Context, SpawnedActors);

	EmitSymOutputs(Context, World, Result, Assets, Settings->RandomSeed, SpawnedActors);
	EmitUnusedPoints(Context, CandidatePoints, Candidates, Selected);
	return true;
}

#undef LOCTEXT_NAMESPACE
