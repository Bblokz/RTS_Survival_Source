// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreateDivision.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Components/SplineComponent.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Helpers/PCGHelpers.h"
#include "LandscapeProxy.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#define LOCTEXT_NAMESPACE "PCGCreateDivision"

namespace DivisionPCGConstants
{
	const FName FaceTowardsPinLabel = TEXT("Face Towards");
	const FName ExcludedBoundsPinLabel = TEXT("Excluded Bounds");
	const FName DivisionBoundsPinLabel = TEXT("Division Bounds");
	const FName PlacementsPinLabel = TEXT("Placements");
	const FName DivisionIndexAttributeName = TEXT("DivisionIndex");
	const FName UnitCategoryAttributeName = TEXT("DivisionUnitCategory");
	const FName TankCategoryName = TEXT("Tank");
	const FName SquadCategoryName = TEXT("Squad");

	constexpr double InspectionDepth = -100000.0;
	constexpr double SpatialSampleHalfExtent = 5.0;
	constexpr double MinimumUsableBoundsSize = 1.0;
	constexpr double FullCircleRadians = 2.0 * PI;
	constexpr double SemiCircleRadians = PI;
	constexpr double MinimumAreaRadius = 200.0;
	constexpr double MinimumSampleSpacing = 25.0;
	constexpr double MinimumAdjustmentStep = 25.0;
	// Keeps the boundary usable even when the designer maxes the radial variation out.
	constexpr double MinimumBoundaryRadiusScale = 0.2;
	constexpr int32 MinimumSplinePoints = 6;
	constexpr int32 MaximumSplinePoints = 64;
	constexpr int32 MaximumSamplesPerAxis = 128;
	constexpr int32 MaximumUnitsPerDivision = 512;
	constexpr int32 AdjustmentDirections = 8;
}

#if WITH_EDITOR
FName UPCGCreateDivisionSettings::GetDefaultNodeName() const
{
	return FName(TEXT("CreateDivision"));
}

FText UPCGCreateDivisionSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Division");
}

FText UPCGCreateDivisionSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Builds spline-bounded division areas from candidate points and fills each with a formation "
		"of tank and squad units snapped to the landscape. Units rotate toward the Face Towards "
		"point (falling back to the candidate point's own rotation) and individually nudge away "
		"from Excluded Bounds. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGCreateDivisionSettings::CreateElement() const
{
	return MakeShared<FPCGCreateDivisionElement>();
}

TArray<FPCGPinProperties> UPCGCreateDivisionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(DivisionPCGConstants::FaceTowardsPinLabel, EPCGDataType::Point);
	Pins.Emplace(DivisionPCGConstants::ExcludedBoundsPinLabel, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreateDivisionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(DivisionPCGConstants::DivisionBoundsPinLabel, EPCGDataType::Point);
	Pins.Emplace(DivisionPCGConstants::PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

/**
 * @brief Uniquely named helper namespace: sibling custom nodes share unity build blobs, so
 * anonymous-namespace helpers with generic names would collide across .cpp files.
 */
namespace CreateDivisionPCGInternal
{
	struct FResolvedDivisionUnit
	{
		UClass* UnitClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		double FootprintRadius = 0.0;
		double ZOffset = 0.0;
		double YawOffsetDegrees = 0.0;
		int32 Count = 1;
		bool bIsTank = true;
	};

	struct FDivisionGroundResult
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
	};

	struct FDivisionBoundary
	{
		FVector Center = FVector::ZeroVector;
		TArray<FVector> Points;
		FBox2D Bounds = FBox2D(EForceInit::ForceInit);
		double MaximumRadius = 0.0;
	};

	struct FPlacedDivisionUnit
	{
		UClass* UnitClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		FTransform Transform = FTransform::Identity;
		FVector2D Center = FVector2D::ZeroVector;
		double FootprintRadius = 0.0;
		int32 DivisionIndex = INDEX_NONE;
		bool bIsTank = true;
	};

	struct FGeneratedDivision
	{
		FDivisionBoundary Boundary;
		TArray<FPlacedDivisionUnit> Units;
		int32 SkippedUnits = 0;
	};

	FName UnitCategoryToName(const bool bIsTank)
	{
		return bIsTank ? DivisionPCGConstants::TankCategoryName : DivisionPCGConstants::SquadCategoryName;
	}

	AActor* SpawnDivisionInspectionActor(UWorld& World, UClass& ActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		return World.SpawnActor<AActor>(
			&ActorClass,
			FTransform(FVector(0.0, 0.0, DivisionPCGConstants::InspectionDepth)),
			SpawnParameters);
	}

	bool TryMeasureUnitBounds(UWorld& World, UClass& ActorClass, FBox& OutLocalBounds)
	{
		AActor* InspectionActor = SpawnDivisionInspectionActor(World, ActorClass);
		if (not IsValid(InspectionActor))
		{
			return false;
		}

		OutLocalBounds = InspectionActor->GetComponentsBoundingBox(true, true)
			.ShiftBy(-InspectionActor->GetActorLocation());
		InspectionActor->Destroy();
		if (not OutLocalBounds.IsValid)
		{
			return false;
		}

		const FVector BoundsSize = OutLocalBounds.GetSize();
		return BoundsSize.X >= DivisionPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Y >= DivisionPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Z >= DivisionPCGConstants::MinimumUsableBoundsSize;
	}

	void ResolveUnitEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FDivisionUnitEntry>& Entries,
		const bool bIsTank,
		TArray<FResolvedDivisionUnit>& OutUnits)
	{
		for (const FDivisionUnitEntry& Entry : Entries)
		{
			UClass* UnitClass = Entry.UnitClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (UnitClass == nullptr || not UnitClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureUnitBounds(World, *UnitClass, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
					LOCTEXT("InvalidUnitEntry", "CreateDivision: skipped an invalid or bounds-less {0} entry."),
					bIsTank ? LOCTEXT("TankRole", "tank") : LOCTEXT("SquadRole", "squad")));
				continue;
			}

			FResolvedDivisionUnit& Resolved = OutUnits.Emplace_GetRef();
			Resolved.UnitClass = UnitClass;
			Resolved.LocalBounds = LocalBounds;
			Resolved.FootprintRadius = Entry.FootprintRadiusOverride > 0.0f
				? Entry.FootprintRadiusOverride
				: 0.5 * FVector2D(LocalBounds.GetSize().X, LocalBounds.GetSize().Y).Size();
			Resolved.ZOffset = Entry.ZOffset;
			Resolved.YawOffsetDegrees = Entry.YawOffsetDegrees;
			Resolved.Count = FMath::Max(1, Entry.Count);
			Resolved.bIsTank = bIsTank;
		}
	}

	TArray<FTransform> CollectCandidateTransforms(FPCGContext* Context)
	{
		TArray<FTransform> Candidates;
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
			if (PointData == nullptr)
			{
				continue;
			}
			for (const FPCGPoint& Point : PointData->GetPoints())
			{
				Candidates.Add(Point.Transform);
			}
		}
		return Candidates;
	}

	TArray<FVector2D> CollectFaceTargets(FPCGContext* Context)
	{
		TArray<FVector2D> Targets;
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetInputsByPin(DivisionPCGConstants::FaceTowardsPinLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
			if (PointData == nullptr)
			{
				continue;
			}
			for (const FPCGPoint& Point : PointData->GetPoints())
			{
				Targets.Add(FVector2D(Point.Transform.GetLocation()));
			}
		}
		return Targets;
	}

	TArray<const UPCGSpatialData*> CollectExclusions(FPCGContext* Context)
	{
		TArray<const UPCGSpatialData*> Exclusions;
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetInputsByPin(DivisionPCGConstants::ExcludedBoundsPinLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
			{
				Exclusions.Add(SpatialData);
			}
		}
		return Exclusions;
	}

	bool DoesDivisionSpatialDataContainPosition(const UPCGSpatialData& SpatialData, const FVector& Position)
	{
		const FBox SampleBounds(
			FVector(-DivisionPCGConstants::SpatialSampleHalfExtent),
			FVector(DivisionPCGConstants::SpatialSampleHalfExtent));
		FPCGPoint SampledPoint;
		return SpatialData.SamplePoint(FTransform(Position), SampleBounds, SampledPoint, nullptr)
			&& SampledPoint.Density > 0.0f;
	}

	bool IsPositionExcludedFromDivision(
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FVector& Position)
	{
		for (const UPCGSpatialData* Exclusion : Exclusions)
		{
			if (Exclusion != nullptr && DoesDivisionSpatialDataContainPosition(*Exclusion, Position))
			{
				return true;
			}
		}
		return false;
	}

	/** @brief Tests the unit's center and the compass points of its footprint so edges cannot clip exclusions. */
	bool IsUnitFootprintExcluded(
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FVector2D& Center,
		const double FootprintRadius,
		const double ZReference)
	{
		if (Exclusions.IsEmpty())
		{
			return false;
		}
		if (IsPositionExcludedFromDivision(Exclusions, FVector(Center, ZReference)))
		{
			return true;
		}
		if (FootprintRadius <= 0.0)
		{
			return false;
		}

		const FVector2D CompassDirections[] =
		{
			FVector2D(1.0, 0.0), FVector2D(-1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(0.0, -1.0)
		};
		for (const FVector2D& Direction : CompassDirections)
		{
			if (IsPositionExcludedFromDivision(Exclusions,
				FVector(Center + Direction * FootprintRadius, ZReference)))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief Resolves terrain exclusively from XY so the temporary spline's coarse Z cannot affect
	 * placement, and traces only the matching landscape proxy to avoid snapping onto other actors.
	 */
	bool TryTraceLandscape(
		UWorld& World,
		const UPCGCreateDivisionSettings& Settings,
		const FVector& Position,
		FHitResult& OutHit)
	{
		const FCollisionQueryParams QueryParameters;
		for (TActorIterator<ALandscapeProxy> LandscapeIterator(&World); LandscapeIterator; ++LandscapeIterator)
		{
			const ALandscapeProxy* Landscape = *LandscapeIterator;
			if (not IsValid(Landscape))
			{
				continue;
			}

			const TOptional<float> LandscapeHeight = Landscape->GetHeightAtLocation(
				FVector(Position.X, Position.Y, 0.0));
			if (not LandscapeHeight.IsSet())
			{
				continue;
			}

			const FVector TracePosition(Position.X, Position.Y, LandscapeHeight.GetValue());
			const FVector TraceStart = TracePosition + FVector::UpVector * Settings.GroundTraceUp;
			const FVector TraceEnd = TracePosition - FVector::UpVector * Settings.GroundTraceDown;
			if (Landscape->ActorLineTraceSingle(
				OutHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParameters))
			{
				return true;
			}
		}

		return false;
	}

	bool TryProjectToGround(
		UWorld& World,
		const UPCGCreateDivisionSettings& Settings,
		const FVector& Position,
		FDivisionGroundResult& OutGround)
	{
		FHitResult Hit;
		if (not TryTraceLandscape(World, Settings, Position, Hit))
		{
			return false;
		}

		const double SlopeDegrees = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(static_cast<double>(Hit.ImpactNormal.Z), -1.0, 1.0)));
		if (SlopeDegrees > Settings.MaxGroundSlopeDegrees)
		{
			return false;
		}
		OutGround.Position = Hit.ImpactPoint;
		OutGround.Normal = Hit.ImpactNormal.GetSafeNormal();
		return true;
	}

	bool IsPointInBoundary(const FDivisionBoundary& Boundary, const FVector2D& Position)
	{
		bool bInside = false;
		for (int32 CurrentIndex = 0, PreviousIndex = Boundary.Points.Num() - 1;
			CurrentIndex < Boundary.Points.Num(); PreviousIndex = CurrentIndex++)
		{
			const FVector& Current = Boundary.Points[CurrentIndex];
			const FVector& Previous = Boundary.Points[PreviousIndex];
			const bool bCrosses = (Current.Y > Position.Y) != (Previous.Y > Position.Y);
			if (bCrosses && Position.X < (Previous.X - Current.X) * (Position.Y - Current.Y)
				/ (Previous.Y - Current.Y) + Current.X)
			{
				bInside = not bInside;
			}
		}
		return bInside;
	}

	double DistancePointToSegment(
		const FVector2D& Position,
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const double SegmentSizeSquared = Segment.SizeSquared();
		if (SegmentSizeSquared <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D::Distance(Position, SegmentStart);
		}
		const double Alpha = FMath::Clamp(FVector2D::DotProduct(Position - SegmentStart, Segment)
			/ SegmentSizeSquared, 0.0, 1.0);
		return FVector2D::Distance(Position, SegmentStart + Segment * Alpha);
	}

	double DistanceToBoundaryEdge(const FDivisionBoundary& Boundary, const FVector2D& Position)
	{
		double MinimumDistance = TNumericLimits<double>::Max();
		for (int32 PointIndex = 0; PointIndex < Boundary.Points.Num(); ++PointIndex)
		{
			const int32 NextIndex = (PointIndex + 1) % Boundary.Points.Num();
			MinimumDistance = FMath::Min(MinimumDistance, DistancePointToSegment(
				Position,
				FVector2D(Boundary.Points[PointIndex]),
				FVector2D(Boundary.Points[NextIndex])));
		}
		return MinimumDistance;
	}

	bool IsDiskInsideBoundary(
		const FDivisionBoundary& Boundary,
		const FVector2D& Position,
		const double Radius)
	{
		return IsPointInBoundary(Boundary, Position) && DistanceToBoundaryEdge(Boundary, Position) >= Radius;
	}

	double Orientation(const FVector2D& First, const FVector2D& Second, const FVector2D& Third)
	{
		return FVector2D::CrossProduct(Second - First, Third - First);
	}

	bool IsPointOnSegment(
		const FVector2D& Position,
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd)
	{
		return FMath::Abs(Orientation(SegmentStart, SegmentEnd, Position)) <= KINDA_SMALL_NUMBER
			&& Position.X >= FMath::Min(SegmentStart.X, SegmentEnd.X) - KINDA_SMALL_NUMBER
			&& Position.X <= FMath::Max(SegmentStart.X, SegmentEnd.X) + KINDA_SMALL_NUMBER
			&& Position.Y >= FMath::Min(SegmentStart.Y, SegmentEnd.Y) - KINDA_SMALL_NUMBER
			&& Position.Y <= FMath::Max(SegmentStart.Y, SegmentEnd.Y) + KINDA_SMALL_NUMBER;
	}

	bool SegmentsIntersect(
		const FVector2D& FirstStart,
		const FVector2D& FirstEnd,
		const FVector2D& SecondStart,
		const FVector2D& SecondEnd)
	{
		const double FirstSideStart = Orientation(FirstStart, FirstEnd, SecondStart);
		const double FirstSideEnd = Orientation(FirstStart, FirstEnd, SecondEnd);
		const double SecondSideStart = Orientation(SecondStart, SecondEnd, FirstStart);
		const double SecondSideEnd = Orientation(SecondStart, SecondEnd, FirstEnd);
		const bool bCrossesFirst = (FirstSideStart > KINDA_SMALL_NUMBER && FirstSideEnd < -KINDA_SMALL_NUMBER)
			|| (FirstSideStart < -KINDA_SMALL_NUMBER && FirstSideEnd > KINDA_SMALL_NUMBER);
		const bool bCrossesSecond = (SecondSideStart > KINDA_SMALL_NUMBER && SecondSideEnd < -KINDA_SMALL_NUMBER)
			|| (SecondSideStart < -KINDA_SMALL_NUMBER && SecondSideEnd > KINDA_SMALL_NUMBER);
		if (bCrossesFirst && bCrossesSecond)
		{
			return true;
		}
		return IsPointOnSegment(SecondStart, FirstStart, FirstEnd)
			|| IsPointOnSegment(SecondEnd, FirstStart, FirstEnd)
			|| IsPointOnSegment(FirstStart, SecondStart, SecondEnd)
			|| IsPointOnSegment(FirstEnd, SecondStart, SecondEnd);
	}

	bool BoundariesOverlap(
		const FDivisionBoundary& First,
		const FDivisionBoundary& Second,
		const double Clearance)
	{
		const FBox2D ExpandedFirst(First.Bounds.Min - FVector2D(Clearance),
			First.Bounds.Max + FVector2D(Clearance));
		if (not ExpandedFirst.Intersect(Second.Bounds))
		{
			return false;
		}

		if (IsPointInBoundary(First, FVector2D(Second.Points[0]))
			|| IsPointInBoundary(Second, FVector2D(First.Points[0])))
		{
			return true;
		}

		for (int32 FirstIndex = 0; FirstIndex < First.Points.Num(); ++FirstIndex)
		{
			const FVector2D FirstStart(First.Points[FirstIndex]);
			const FVector2D FirstEnd(First.Points[(FirstIndex + 1) % First.Points.Num()]);
			for (int32 SecondIndex = 0; SecondIndex < Second.Points.Num(); ++SecondIndex)
			{
				const FVector2D SecondStart(Second.Points[SecondIndex]);
				const FVector2D SecondEnd(Second.Points[(SecondIndex + 1) % Second.Points.Num()]);
				if (SegmentsIntersect(FirstStart, FirstEnd, SecondStart, SecondEnd)
					|| DistancePointToSegment(FirstStart, SecondStart, SecondEnd) < Clearance
					|| DistancePointToSegment(SecondStart, FirstStart, FirstEnd) < Clearance)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IsBoundaryClearOfDivisions(
		const FDivisionBoundary& Candidate,
		const TArray<FGeneratedDivision>& Divisions,
		const double Clearance)
	{
		for (const FGeneratedDivision& Division : Divisions)
		{
			if (BoundariesOverlap(Candidate, Division.Boundary, Clearance))
			{
				return false;
			}
		}
		return true;
	}

	/** @return Fraction of the grid samples inside the boundary that hit exclusion data. */
	double ComputeExcludedAreaFraction(
		const FDivisionBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const double RequestedSpacing)
	{
		if (Exclusions.IsEmpty())
		{
			return 0.0;
		}

		const double Spacing = FMath::Max(
			DivisionPCGConstants::MinimumSampleSpacing, RequestedSpacing);
		const int32 SamplesX = FMath::Clamp(
			FMath::CeilToInt32(Boundary.Bounds.GetSize().X / Spacing),
			1, DivisionPCGConstants::MaximumSamplesPerAxis);
		const int32 SamplesY = FMath::Clamp(
			FMath::CeilToInt32(Boundary.Bounds.GetSize().Y / Spacing),
			1, DivisionPCGConstants::MaximumSamplesPerAxis);
		int32 InsideSamples = 0;
		int32 ExcludedSamples = 0;
		for (int32 SampleX = 0; SampleX <= SamplesX; ++SampleX)
		{
			for (int32 SampleY = 0; SampleY <= SamplesY; ++SampleY)
			{
				const FVector2D Position(
					FMath::Lerp(Boundary.Bounds.Min.X, Boundary.Bounds.Max.X,
						static_cast<double>(SampleX) / SamplesX),
					FMath::Lerp(Boundary.Bounds.Min.Y, Boundary.Bounds.Max.Y,
						static_cast<double>(SampleY) / SamplesY));
				if (not IsPointInBoundary(Boundary, Position))
				{
					continue;
				}
				++InsideSamples;
				if (IsPositionExcludedFromDivision(Exclusions, FVector(Position, Boundary.Center.Z)))
				{
					++ExcludedSamples;
				}
			}
		}
		return InsideSamples > 0 ? static_cast<double>(ExcludedSamples) / InsideSamples : 0.0;
	}

	bool TryCreateBoundary(
		UWorld& World,
		const UPCGCreateDivisionSettings& Settings,
		const FVector& ProjectedCenter,
		const double BaseRadius,
		FRandomStream& Random,
		FDivisionBoundary& OutBoundary)
	{
		OutBoundary.Center = ProjectedCenter;
		const int32 PointCount = FMath::Clamp(Settings.SplinePointCount,
			DivisionPCGConstants::MinimumSplinePoints, DivisionPCGConstants::MaximumSplinePoints);
		const float Variation = FMath::Clamp(Settings.AreaBoundaryVariation, 0.0f, 0.8f);
		const double StartingAngle = Random.FRandRange(0.0f,
			static_cast<float>(DivisionPCGConstants::FullCircleRadians));

		OutBoundary.Points.Reserve(PointCount);
		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const double Angle = StartingAngle
				+ DivisionPCGConstants::FullCircleRadians * PointIndex / PointCount;
			const double RadiusVariation = Random.FRandRange(-Variation, Variation);
			const double Radius = BaseRadius * (1.0 + RadiusVariation);
			const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
			FDivisionGroundResult BoundaryGround;
			if (not TryProjectToGround(World, Settings,
				FVector(FVector2D(ProjectedCenter) + Direction * Radius, ProjectedCenter.Z), BoundaryGround))
			{
				return false;
			}
			OutBoundary.Points.Add(BoundaryGround.Position);
			OutBoundary.Bounds += FVector2D(BoundaryGround.Position);
			OutBoundary.MaximumRadius = FMath::Max(OutBoundary.MaximumRadius, Radius);
		}
		return true;
	}

	/**
	 * @brief Expands the resolved entries into one ordered roster: TanksFirst / SquadsFirst claim
	 * the leading formation slots for that category, Mixed shuffles both together deterministically.
	 */
	void BuildDivisionRoster(
		FPCGContext* Context,
		const UPCGCreateDivisionSettings& Settings,
		const TArray<FResolvedDivisionUnit>& TankUnits,
		const TArray<FResolvedDivisionUnit>& SquadUnits,
		FRandomStream& Random,
		TArray<const FResolvedDivisionUnit*>& OutRoster)
	{
		TArray<const FResolvedDivisionUnit*> Tanks;
		TArray<const FResolvedDivisionUnit*> Squads;
		for (const FResolvedDivisionUnit& Unit : TankUnits)
		{
			for (int32 CopyIndex = 0; CopyIndex < Unit.Count; ++CopyIndex)
			{
				Tanks.Add(&Unit);
			}
		}
		for (const FResolvedDivisionUnit& Unit : SquadUnits)
		{
			for (int32 CopyIndex = 0; CopyIndex < Unit.Count; ++CopyIndex)
			{
				Squads.Add(&Unit);
			}
		}

		switch (Settings.SpawnOrdering)
		{
		case EDivisionSpawnOrdering::SquadsFirst:
			OutRoster.Append(Squads);
			OutRoster.Append(Tanks);
			break;
		case EDivisionSpawnOrdering::Mixed:
			OutRoster.Append(Tanks);
			OutRoster.Append(Squads);
			for (int32 UnitIndex = OutRoster.Num() - 1; UnitIndex > 0; --UnitIndex)
			{
				OutRoster.Swap(UnitIndex, Random.RandRange(0, UnitIndex));
			}
			break;
		case EDivisionSpawnOrdering::TanksFirst:
		default:
			OutRoster.Append(Tanks);
			OutRoster.Append(Squads);
			break;
		}

		if (OutRoster.Num() > DivisionPCGConstants::MaximumUnitsPerDivision)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("RosterTruncated",
					"CreateDivision: the configured roster of {0} units exceeds the maximum of {1}; extra units are dropped."),
				FText::AsNumber(OutRoster.Num()),
				FText::AsNumber(DivisionPCGConstants::MaximumUnitsPerDivision)));
			OutRoster.SetNum(DivisionPCGConstants::MaximumUnitsPerDivision);
		}
	}

	/**
	 * @brief Lays the formation slots out around the origin, ordered front to back so the roster's
	 * leading units form the vanguard. Rectangular stacks centered rows away from the facing
	 * direction; SemiSpheres builds concentric arcs bulging toward it, outermost arc first.
	 */
	TArray<FVector2D> ComputeFormationSlots(
		const UPCGCreateDivisionSettings& Settings,
		const int32 UnitCount,
		const double ForwardRadians)
	{
		TArray<FVector2D> Slots;
		Slots.Reserve(UnitCount);
		const FVector2D Forward(FMath::Cos(ForwardRadians), FMath::Sin(ForwardRadians));

		if (Settings.FormationShape == EDivisionFormationShape::Rectangular)
		{
			const FVector2D Right(-Forward.Y, Forward.X);
			const int32 UnitsPerRow = FMath::Max(1, Settings.UnitsPerRow);
			const int32 RowCount = FMath::DivideAndRoundUp(UnitCount, UnitsPerRow);
			for (int32 Row = 0; Row < RowCount; ++Row)
			{
				const int32 UnitsInRow = FMath::Min(UnitsPerRow, UnitCount - Row * UnitsPerRow);
				const double Depth = (0.5 * (RowCount - 1) - Row) * Settings.OffsetBetweenRows;
				for (int32 Column = 0; Column < UnitsInRow; ++Column)
				{
					const double Lateral = (Column - 0.5 * (UnitsInRow - 1)) * Settings.OffsetBetweenUnits;
					Slots.Add(Forward * Depth + Right * Lateral);
				}
			}
			return Slots;
		}

		const int32 UnitsPerArc = FMath::Max(1, Settings.UnitsPerSemiSphere);
		const int32 ArcCount = FMath::DivideAndRoundUp(UnitCount, UnitsPerArc);
		const double InnerRadius = Settings.InnerSemiSphereRadius > 0.0f
			? Settings.InnerSemiSphereRadius
			: FMath::Max(static_cast<double>(Settings.OffsetBetweenUnits),
				(UnitsPerArc - 1) * Settings.OffsetBetweenUnits / DivisionPCGConstants::SemiCircleRadians);
		const double ArcYawOffset = FMath::DegreesToRadians(Settings.SemiSphereYawOffsetDegrees);
		int32 RemainingUnits = UnitCount;
		for (int32 ArcOrder = 0; ArcOrder < ArcCount; ++ArcOrder)
		{
			// Outermost arc first; the final (innermost) arc absorbs the partial remainder.
			const double Radius = InnerRadius + (ArcCount - 1 - ArcOrder) * Settings.OffsetBetweenRows;
			const int32 UnitsInArc = FMath::Min(UnitsPerArc, RemainingUnits);
			RemainingUnits -= UnitsInArc;
			for (int32 UnitIndex = 0; UnitIndex < UnitsInArc; ++UnitIndex)
			{
				const double Alpha = UnitsInArc == 1
					? 0.5
					: static_cast<double>(UnitIndex) / (UnitsInArc - 1);
				const double Angle = ForwardRadians + ArcYawOffset
					+ (Alpha - 0.5) * DivisionPCGConstants::SemiCircleRadians;
				Slots.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}
		}
		return Slots;
	}

	double ComputeNearestTargetYawDegrees(
		const FVector2D& Position,
		const TArray<FVector2D>& FaceTargets,
		const double FallbackYawDegrees)
	{
		if (FaceTargets.IsEmpty())
		{
			return FallbackYawDegrees;
		}

		FVector2D NearestTarget = FaceTargets[0];
		double NearestDistanceSquared = TNumericLimits<double>::Max();
		for (const FVector2D& Target : FaceTargets)
		{
			const double DistanceSquared = FVector2D::DistSquared(Position, Target);
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				NearestTarget = Target;
			}
		}

		const FVector2D ToTarget = NearestTarget - Position;
		if (ToTarget.IsNearlyZero())
		{
			return FallbackYawDegrees;
		}
		return FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
	}

	FTransform MakeGroundedUnitTransform(
		const FResolvedDivisionUnit& Unit,
		const FDivisionGroundResult& Ground,
		const double YawDegrees,
		const bool bAlignToGround)
	{
		FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(YawDegrees + Unit.YawOffsetDegrees));
		if (bAlignToGround)
		{
			Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Ground.Normal) * Rotation;
		}

		const FVector LocalSurfaceAnchor(
			Unit.LocalBounds.GetCenter().X,
			Unit.LocalBounds.GetCenter().Y,
			Unit.LocalBounds.Min.Z);
		const FVector RotatedSurfaceAnchor = Rotation.RotateVector(LocalSurfaceAnchor);
		const FVector OffsetDirection = bAlignToGround ? Ground.Normal : FVector::UpVector;
		const FVector Location = Ground.Position + OffsetDirection * Unit.ZOffset - RotatedSurfaceAnchor;
		return FTransform(Rotation, Location, FVector::OneVector);
	}

	/** @brief Physical non-overlap only: nudged units may sit closer than the designer offset. */
	bool IsUnitPlacementClear(
		const FVector2D& Center,
		const double FootprintRadius,
		const TArray<FPlacedDivisionUnit>& Placements)
	{
		for (const FPlacedDivisionUnit& Placement : Placements)
		{
			if (FVector2D::Distance(Center, Placement.Center)
				< FootprintRadius + Placement.FootprintRadius)
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * @brief Places one unit on its formation slot, or on the nearest valid nudged position when
	 * the slot clips exclusions, the boundary, blocked terrain, or an already placed unit.
	 */
	bool TryPlaceUnit(
		UWorld& World,
		const UPCGCreateDivisionSettings& Settings,
		const FDivisionBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedDivisionUnit>& ExistingUnits,
		const FResolvedDivisionUnit& Unit,
		const FVector2D& DesiredCenter,
		const TArray<FVector2D>& FaceTargets,
		const double FallbackYawDegrees,
		const int32 DivisionIndex,
		FPlacedDivisionUnit& OutPlacement)
	{
		TArray<FVector2D> Candidates;
		Candidates.Add(DesiredCenter);
		if (Settings.bAdjustPlacementsToAvoidExclusions && Settings.MaxPlacementAdjustment > 0.0f)
		{
			const double Step = FMath::Max(
				DivisionPCGConstants::MinimumAdjustmentStep,
				static_cast<double>(Settings.PlacementAdjustmentStep));
			for (int32 Ring = 1; Ring * Step <= Settings.MaxPlacementAdjustment + KINDA_SMALL_NUMBER; ++Ring)
			{
				// Staggering alternate rings spreads the probes instead of retesting the same rays.
				const double RingStagger = Ring * DivisionPCGConstants::SemiCircleRadians
					/ DivisionPCGConstants::AdjustmentDirections;
				for (int32 Direction = 0; Direction < DivisionPCGConstants::AdjustmentDirections; ++Direction)
				{
					const double Angle = RingStagger + DivisionPCGConstants::FullCircleRadians
						* Direction / DivisionPCGConstants::AdjustmentDirections;
					Candidates.Add(DesiredCenter
						+ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Ring * Step);
				}
			}
		}

		for (const FVector2D& Candidate : Candidates)
		{
			if (not IsDiskInsideBoundary(Boundary, Candidate, Unit.FootprintRadius)
				|| IsUnitFootprintExcluded(Exclusions, Candidate, Unit.FootprintRadius, Boundary.Center.Z)
				|| not IsUnitPlacementClear(Candidate, Unit.FootprintRadius, ExistingUnits))
			{
				continue;
			}

			FDivisionGroundResult Ground;
			if (not TryProjectToGround(World, Settings, FVector(Candidate, Boundary.Center.Z), Ground))
			{
				continue;
			}

			const double YawDegrees = ComputeNearestTargetYawDegrees(
				Candidate, FaceTargets, FallbackYawDegrees);
			OutPlacement.UnitClass = Unit.UnitClass;
			OutPlacement.LocalBounds = Unit.LocalBounds;
			OutPlacement.Transform = MakeGroundedUnitTransform(
				Unit, Ground, YawDegrees, Settings.bAlignUnitsToGround);
			OutPlacement.Center = Candidate;
			OutPlacement.FootprintRadius = Unit.FootprintRadius;
			OutPlacement.DivisionIndex = DivisionIndex;
			OutPlacement.bIsTank = Unit.bIsTank;
			return true;
		}
		return false;
	}

	bool TryGenerateDivision(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateDivisionSettings& Settings,
		const FTransform& Candidate,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FVector2D>& FaceTargets,
		const TArray<FResolvedDivisionUnit>& TankUnits,
		const TArray<FResolvedDivisionUnit>& SquadUnits,
		const TArray<FGeneratedDivision>& ExistingDivisions,
		const int32 DivisionIndex,
		FRandomStream& Random,
		FGeneratedDivision& OutDivision)
	{
		FDivisionGroundResult CenterGround;
		if (not TryProjectToGround(World, Settings, Candidate.GetLocation(), CenterGround))
		{
			return false;
		}

		TArray<const FResolvedDivisionUnit*> Roster;
		BuildDivisionRoster(Context, Settings, TankUnits, SquadUnits, Random, Roster);
		if (Roster.IsEmpty())
		{
			return false;
		}

		const FVector2D Center2D(CenterGround.Position);
		const double FallbackYawDegrees = Candidate.Rotator().Yaw;
		const double ForwardRadians = FMath::DegreesToRadians(ComputeNearestTargetYawDegrees(
			Center2D, FaceTargets, FallbackYawDegrees));
		const TArray<FVector2D> Slots = ComputeFormationSlots(Settings, Roster.Num(), ForwardRadians);

		double MaxFootprintRadius = 0.0;
		for (const FResolvedDivisionUnit* Unit : Roster)
		{
			MaxFootprintRadius = FMath::Max(MaxFootprintRadius, Unit->FootprintRadius);
		}
		double FormationExtent = 0.0;
		for (const FVector2D& Slot : Slots)
		{
			FormationExtent = FMath::Max(FormationExtent, Slot.Size());
		}

		// Divide by the worst-case inward variation so even a pinched spline edge still
		// leaves AreaPadding of free space around the outermost slot.
		const double CoreRadius = FormationExtent + MaxFootprintRadius + Settings.AreaPadding;
		const double WorstCaseRadiusScale = FMath::Max(
			DivisionPCGConstants::MinimumBoundaryRadiusScale,
			1.0 - FMath::Clamp(Settings.AreaBoundaryVariation, 0.0f, 0.8f));
		const double BaseRadius = FMath::Max(
			DivisionPCGConstants::MinimumAreaRadius, CoreRadius / WorstCaseRadiusScale);

		if (not TryCreateBoundary(World, Settings, CenterGround.Position, BaseRadius, Random, OutDivision.Boundary)
			|| not IsBoundaryClearOfDivisions(OutDivision.Boundary, ExistingDivisions, Settings.DivisionClearance))
		{
			return false;
		}

		if (IsPositionExcludedFromDivision(Exclusions, OutDivision.Boundary.Center)
			|| ComputeExcludedAreaFraction(OutDivision.Boundary, Exclusions, Settings.ExclusionSampleSpacing)
				> Settings.MaxExcludedAreaFraction)
		{
			return false;
		}

		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			FPlacedDivisionUnit Placement;
			if (TryPlaceUnit(World, Settings, OutDivision.Boundary, Exclusions, OutDivision.Units,
				*Roster[SlotIndex], Center2D + Slots[SlotIndex], FaceTargets, FallbackYawDegrees,
				DivisionIndex, Placement))
			{
				OutDivision.Units.Add(MoveTemp(Placement));
			}
			else
			{
				++OutDivision.SkippedUnits;
			}
		}

		if (OutDivision.Units.IsEmpty())
		{
			return false;
		}
		if (OutDivision.SkippedUnits > 0)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("SkippedUnits",
					"CreateDivision: division {0} skipped {1} of {2} units; their slots and every nudge within MaxPlacementAdjustment overlapped exclusions, other units, or invalid terrain."),
				FText::AsNumber(DivisionIndex), FText::AsNumber(OutDivision.SkippedUnits),
				FText::AsNumber(Slots.Num())));
		}
		return true;
	}

	void ShuffleCandidates(TArray<FTransform>& Candidates, FRandomStream& Random)
	{
		for (int32 CandidateIndex = Candidates.Num() - 1; CandidateIndex > 0; --CandidateIndex)
		{
			const int32 SwapIndex = Random.RandRange(0, CandidateIndex);
			Candidates.Swap(CandidateIndex, SwapIndex);
		}
	}

	void GenerateDivisions(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateDivisionSettings& Settings,
		TArray<FTransform> Candidates,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FVector2D>& FaceTargets,
		const TArray<FResolvedDivisionUnit>& TankUnits,
		const TArray<FResolvedDivisionUnit>& SquadUnits,
		FRandomStream& Random,
		TArray<FGeneratedDivision>& OutDivisions)
	{
		const int32 MinimumDivisions = FMath::Max(0,
			FMath::Min(Settings.MinDivisionsToCreate, Settings.MaxDivisionsToCreate));
		const int32 MaximumDivisions = FMath::Max(MinimumDivisions,
			FMath::Max(Settings.MinDivisionsToCreate, Settings.MaxDivisionsToCreate));
		const int32 DesiredDivisions = Random.RandRange(MinimumDivisions, MaximumDivisions);
		ShuffleCandidates(Candidates, Random);

		for (const FTransform& Candidate : Candidates)
		{
			if (OutDivisions.Num() >= DesiredDivisions)
			{
				break;
			}
			FGeneratedDivision Division;
			if (TryGenerateDivision(Context, World, Settings, Candidate, Exclusions, FaceTargets,
				TankUnits, SquadUnits, OutDivisions, OutDivisions.Num(), Random, Division))
			{
				OutDivisions.Add(MoveTemp(Division));
			}
		}

		if (OutDivisions.Num() < MinimumDivisions)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("BelowMinimumDivisions",
					"CreateDivision: created {0} divisions, below the configured minimum {1}; remaining candidates overlapped exclusions, other divisions, or invalid terrain."),
				FText::AsNumber(OutDivisions.Num()), FText::AsNumber(MinimumDivisions)));
		}
	}

	AActor* SpawnDivisionSplineActor(
		UWorld& World,
		const FDivisionBoundary& Boundary,
		const int32 DivisionIndex)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* SplineActor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (not IsValid(SplineActor))
		{
			return nullptr;
		}
		SplineActor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		SplineActor->SetActorLabel(FString::Printf(TEXT("PCG_DivisionSpline_%d"), DivisionIndex));
#endif

		USplineComponent* Spline = NewObject<USplineComponent>(SplineActor);
		SplineActor->SetRootComponent(Spline);
		SplineActor->AddInstanceComponent(Spline);
		Spline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Spline->RegisterComponent();
		Spline->ClearSplinePoints(false);
		for (const FVector& Point : Boundary.Points)
		{
			Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
		}
		for (int32 PointIndex = 0; PointIndex < Spline->GetNumberOfSplinePoints(); ++PointIndex)
		{
			Spline->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
		}
		Spline->SetClosedLoop(true, false);
		Spline->UpdateSpline();
		return SplineActor;
	}

	AActor* SpawnDivisionUnitActor(
		UWorld& World,
		const FPlacedDivisionUnit& Placement,
		const int32 UnitIndex)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World.SpawnActor<AActor>(Placement.UnitClass, Placement.Transform, SpawnParameters);
		if (not IsValid(Actor))
		{
			return nullptr;
		}
		Actor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("PCG_Division%d_%s_%d"),
			Placement.DivisionIndex, *UnitCategoryToName(Placement.bIsTank).ToString(), UnitIndex));
#endif
		return Actor;
	}

	void SpawnAndRegisterDivisions(
		UPCGComponent& SourceComponent,
		UWorld& World,
		const TArray<FGeneratedDivision>& Divisions)
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(&SourceComponent);
		for (int32 DivisionIndex = 0; DivisionIndex < Divisions.Num(); ++DivisionIndex)
		{
			const FGeneratedDivision& Division = Divisions[DivisionIndex];
			if (AActor* SplineActor = SpawnDivisionSplineActor(World, Division.Boundary, DivisionIndex))
			{
				ManagedActors->GeneratedActors.Add(SplineActor);
			}
			for (int32 UnitIndex = 0; UnitIndex < Division.Units.Num(); ++UnitIndex)
			{
				if (AActor* UnitActor = SpawnDivisionUnitActor(World, Division.Units[UnitIndex], UnitIndex))
				{
					ManagedActors->GeneratedActors.Add(UnitActor);
				}
			}
		}
		if (not ManagedActors->GeneratedActors.IsEmpty())
		{
			SourceComponent.AddToManagedResources(ManagedActors);
		}
	}

	UPCGPointData* CreateDivisionOutputPointData(FPCGContext* Context, const FName PinLabel)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		PointData->InitializeFromData(nullptr);
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = PinLabel;
		return PointData;
	}

	void EmitDivisionBounds(
		FPCGContext* Context,
		const TArray<FGeneratedDivision>& Divisions,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateDivisionOutputPointData(
			Context, DivisionPCGConstants::DivisionBoundsPinLabel);
		FPCGMetadataAttribute<int32>* DivisionIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			DivisionPCGConstants::DivisionIndexAttributeName, INDEX_NONE, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Divisions.Num());
		for (int32 DivisionIndex = 0; DivisionIndex < Divisions.Num(); ++DivisionIndex)
		{
			const FDivisionBoundary& Boundary = Divisions[DivisionIndex].Boundary;
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(Boundary.Center);
			Point.BoundsMin = FVector(Boundary.Bounds.Min - FVector2D(Boundary.Center), 0.0);
			Point.BoundsMax = FVector(Boundary.Bounds.Max - FVector2D(Boundary.Center), 0.0);
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, DivisionIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			DivisionIndexAttribute->SetValue(Point.MetadataEntry, DivisionIndex);
		}
	}

	void EmitDivisionPlacements(
		FPCGContext* Context,
		const TArray<FGeneratedDivision>& Divisions,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateDivisionOutputPointData(
			Context, DivisionPCGConstants::PlacementsPinLabel);
		FPCGMetadataAttribute<int32>* DivisionIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			DivisionPCGConstants::DivisionIndexAttributeName, INDEX_NONE, false, false);
		FPCGMetadataAttribute<FName>* CategoryAttribute = PointData->Metadata->CreateAttribute<FName>(
			DivisionPCGConstants::UnitCategoryAttributeName, NAME_None, false, false);
		int32 PlacementIndex = 0;
		for (const FGeneratedDivision& Division : Divisions)
		{
			for (const FPlacedDivisionUnit& Placement : Division.Units)
			{
				FPCGPoint& Point = PointData->GetMutablePoints().Emplace_GetRef();
				Point.Transform = Placement.Transform;
				Point.BoundsMin = Placement.LocalBounds.Min;
				Point.BoundsMax = Placement.LocalBounds.Max;
				Point.Density = 1.0f;
				Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, PlacementIndex++);
				PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
				DivisionIndexAttribute->SetValue(Point.MetadataEntry, Placement.DivisionIndex);
				CategoryAttribute->SetValue(Point.MetadataEntry, UnitCategoryToName(Placement.bIsTank));
			}
		}
	}
}

bool FPCGCreateDivisionElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace CreateDivisionPCGInternal;
	check(Context);

	const UPCGCreateDivisionSettings* Settings =
		Context->GetInputSettings<UPCGCreateDivisionSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	TArray<FTransform> Candidates = CollectCandidateTransforms(Context);
	if (Candidates.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("NoCandidates", "CreateDivision: the In pin contains no candidate points."));
		EmitDivisionBounds(Context, {}, Settings->RandomSeed);
		EmitDivisionPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	TArray<FResolvedDivisionUnit> TankUnits;
	TArray<FResolvedDivisionUnit> SquadUnits;
	ResolveUnitEntries(Context, World, Settings->TankUnits, true, TankUnits);
	ResolveUnitEntries(Context, World, Settings->SquadUnits, false, SquadUnits);
	if (TankUnits.IsEmpty() && SquadUnits.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("NoUnits", "CreateDivision: no valid tank or squad unit Blueprints are configured."));
		EmitDivisionBounds(Context, {}, Settings->RandomSeed);
		EmitDivisionPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	FRandomStream Random(Settings->RandomSeed);
	TArray<FGeneratedDivision> Divisions;
	GenerateDivisions(Context, World, *Settings, MoveTemp(Candidates), CollectExclusions(Context),
		CollectFaceTargets(Context), TankUnits, SquadUnits, Random, Divisions);
	SpawnAndRegisterDivisions(*SourceComponent, World, Divisions);
	EmitDivisionBounds(Context, Divisions, Settings->RandomSeed);
	EmitDivisionPlacements(Context, Divisions, Settings->RandomSeed);
	return true;
}

#undef LOCTEXT_NAMESPACE
