// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreateRadixiteField.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Helpers/PCGHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#define LOCTEXT_NAMESPACE "PCGCreateRadixiteField"

namespace RadixiteFieldPCGConstants
{
	const FName ExcludedBoundsPinLabel = TEXT("Excluded Bounds");
	const FName FieldBoundsPinLabel = TEXT("Field Bounds");
	const FName PlacementsPinLabel = TEXT("Placements");
	const FName FieldIndexAttributeName = TEXT("RadixiteFieldIndex");
	const FName PlacementRoleAttributeName = TEXT("RadixitePlacementRole");

	constexpr double InspectionDepth = -100000.0;
	constexpr double SpatialSampleHalfExtent = 5.0;
	constexpr double MinimumUsableBoundsSize = 1.0;
	constexpr double FullCircleRadians = 2.0 * PI;
	constexpr double FullCircleDegrees = 360.0;
	constexpr double MinimumFieldRadius = 100.0;
	constexpr double MinimumSampleSpacing = 25.0;
	constexpr double MinimumScale = 0.01;
	constexpr int32 MinimumSplinePoints = 6;
	constexpr int32 MaximumSplinePoints = 64;
	constexpr int32 MaximumSamplesPerAxis = 128;
}

#if WITH_EDITOR
FName UPCGCreateRadixiteFieldSettings::GetDefaultNodeName() const
{
	return FName(TEXT("CreateRadixiteField"));
}

FText UPCGCreateRadixiteFieldSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Radixite Field");
}

FText UPCGCreateRadixiteFieldSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Builds non-overlapping radixite fields from candidate points. Each accepted candidate creates "
		"one closed temporary spline, terrain-aligned growth centers and crystals, and ground-projected "
		"decals. Excluded Bounds can contain any spatial PCG data. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGCreateRadixiteFieldSettings::CreateElement() const
{
	return MakeShared<FPCGCreateRadixiteFieldElement>();
}

TArray<FPCGPinProperties> UPCGCreateRadixiteFieldSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(RadixiteFieldPCGConstants::ExcludedBoundsPinLabel, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreateRadixiteFieldSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(RadixiteFieldPCGConstants::FieldBoundsPinLabel, EPCGDataType::Point);
	Pins.Emplace(RadixiteFieldPCGConstants::PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

namespace
{
	enum class ERadixitePlacementRole : uint8
	{
		GrowthCenter,
		RegularCrystal,
		Decal
	};

	struct FResolvedRadixiteActor
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		double FootprintRadius = 0.0;
		double Weight = 1.0;
		double MinScale = 1.0;
		double MaxScale = 1.0;
		double ZOffset = 0.0;
		double YawOffsetDegrees = 0.0;
	};

	struct FResolvedRadixiteDecal
	{
		UMaterialInterface* Material = nullptr;
		double Weight = 1.0;
		FVector2D MinSize = FVector2D::ZeroVector;
		FVector2D MaxSize = FVector2D::ZeroVector;
		double ProjectionDepth = 0.0;
		double SurfaceOffset = 0.0;
	};

	struct FRadixiteGroundResult
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
	};

	struct FRadixiteFieldBoundary
	{
		FVector Center = FVector::ZeroVector;
		TArray<FVector> Points;
		FBox2D Bounds = FBox2D(EForceInit::ForceInit);
		double MaximumRadius = 0.0;
	};

	struct FPlacedRadixiteActor
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		FTransform Transform = FTransform::Identity;
		FVector2D Center = FVector2D::ZeroVector;
		double FootprintRadius = 0.0;
		int32 FieldIndex = INDEX_NONE;
		ERadixitePlacementRole Role = ERadixitePlacementRole::GrowthCenter;
	};

	struct FRadixiteDecalSpawn
	{
		UMaterialInterface* Material = nullptr;
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		FVector2D Size = FVector2D::ZeroVector;
		double ProjectionDepth = 0.0;
		double YawRadians = 0.0;
		int32 FieldIndex = INDEX_NONE;
	};

	struct FGeneratedRadixiteField
	{
		FRadixiteFieldBoundary Boundary;
		TArray<FPlacedRadixiteActor> Actors;
		TArray<FRadixiteDecalSpawn> Decals;
	};

	FName PlacementRoleToName(const ERadixitePlacementRole Role)
	{
		switch (Role)
		{
		case ERadixitePlacementRole::GrowthCenter:
			return TEXT("GrowthCenter");
		case ERadixitePlacementRole::RegularCrystal:
			return TEXT("RegularCrystal");
		case ERadixitePlacementRole::Decal:
			return TEXT("Decal");
		default:
			return NAME_None;
		}
	}

	AActor* SpawnRadixiteInspectionActor(UWorld& World, UClass& ActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		return World.SpawnActor<AActor>(
			&ActorClass,
			FTransform(FVector(0.0, 0.0, RadixiteFieldPCGConstants::InspectionDepth)),
			SpawnParameters);
	}

	bool TryMeasureActor(UWorld& World, UClass& ActorClass, FBox& OutLocalBounds)
	{
		AActor* InspectionActor = SpawnRadixiteInspectionActor(World, ActorClass);
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
		return BoundsSize.X >= RadixiteFieldPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Y >= RadixiteFieldPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Z >= RadixiteFieldPCGConstants::MinimumUsableBoundsSize;
	}

	void ResolveActorEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FRadixiteFieldActorEntry>& Entries,
		const FText& EntryRole,
		TArray<FResolvedRadixiteActor>& OutActors)
	{
		for (const FRadixiteFieldActorEntry& Entry : Entries)
		{
			UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureActor(World, *ActorClass, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
					LOCTEXT("InvalidActorEntry", "CreateRadixiteField: skipped an invalid or bounds-less {0} actor."),
					EntryRole));
				continue;
			}

			FResolvedRadixiteActor& Resolved = OutActors.Emplace_GetRef();
			Resolved.ActorClass = ActorClass;
			Resolved.LocalBounds = LocalBounds;
			Resolved.FootprintRadius = Entry.FootprintRadiusOverride > 0.0f
				? Entry.FootprintRadiusOverride
				: 0.5 * FVector2D(LocalBounds.GetSize().X, LocalBounds.GetSize().Y).Size();
			Resolved.Weight = FMath::Max(0.0f, Entry.Weight);
			Resolved.MinScale = FMath::Max(RadixiteFieldPCGConstants::MinimumScale,
				FMath::Min(Entry.MinUniformScale, Entry.MaxUniformScale));
			Resolved.MaxScale = FMath::Max(Resolved.MinScale,
				FMath::Max(Entry.MinUniformScale, Entry.MaxUniformScale));
			Resolved.ZOffset = Entry.ZOffset;
			Resolved.YawOffsetDegrees = Entry.YawOffsetDegrees;
		}
	}

	void ResolveDecalEntries(
		FPCGContext* Context,
		const TArray<FRadixiteFieldDecalEntry>& Entries,
		TArray<FResolvedRadixiteDecal>& OutDecals)
	{
		for (const FRadixiteFieldDecalEntry& Entry : Entries)
		{
			UMaterialInterface* Material = Entry.Material.LoadSynchronous();
			if (not IsValid(Material))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("InvalidDecalEntry", "CreateRadixiteField: skipped a decal with no valid material."));
				continue;
			}

			FResolvedRadixiteDecal& Resolved = OutDecals.Emplace_GetRef();
			Resolved.Material = Material;
			Resolved.Weight = FMath::Max(0.0f, Entry.Weight);
			Resolved.MinSize.X = FMath::Max(1.0f, FMath::Min(Entry.MinSize.X, Entry.MaxSize.X));
			Resolved.MinSize.Y = FMath::Max(1.0f, FMath::Min(Entry.MinSize.Y, Entry.MaxSize.Y));
			Resolved.MaxSize.X = FMath::Max(Resolved.MinSize.X, FMath::Max(Entry.MinSize.X, Entry.MaxSize.X));
			Resolved.MaxSize.Y = FMath::Max(Resolved.MinSize.Y, FMath::Max(Entry.MinSize.Y, Entry.MaxSize.Y));
			Resolved.ProjectionDepth = FMath::Max(1.0f, Entry.ProjectionDepth);
			Resolved.SurfaceOffset = Entry.SurfaceOffset;
		}
	}

	template <typename AssetType>
	int32 PickWeightedAssetIndex(const TArray<AssetType>& Assets, FRandomStream& Random)
	{
		double TotalWeight = 0.0;
		for (const AssetType& Asset : Assets)
		{
			TotalWeight += Asset.Weight;
		}
		if (TotalWeight <= 0.0)
		{
			return Assets.IsEmpty() ? INDEX_NONE : Random.RandRange(0, Assets.Num() - 1);
		}

		double Roll = Random.FRandRange(0.0f, static_cast<float>(TotalWeight));
		for (int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
		{
			Roll -= Assets[AssetIndex].Weight;
			if (Roll <= 0.0)
			{
				return AssetIndex;
			}
		}
		return Assets.Num() - 1;
	}

	TArray<FVector> CollectCandidatePositions(FPCGContext* Context)
	{
		TArray<FVector> Positions;
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
				Positions.Add(Point.Transform.GetLocation());
			}
		}
		return Positions;
	}

	TArray<const UPCGSpatialData*> CollectExclusions(FPCGContext* Context)
	{
		TArray<const UPCGSpatialData*> Exclusions;
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetInputsByPin(RadixiteFieldPCGConstants::ExcludedBoundsPinLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
			{
				Exclusions.Add(SpatialData);
			}
		}
		return Exclusions;
	}

	bool DoesRadixiteSpatialDataContainPosition(const UPCGSpatialData& SpatialData, const FVector& Position)
	{
		const FBox SampleBounds(
			FVector(-RadixiteFieldPCGConstants::SpatialSampleHalfExtent),
			FVector(RadixiteFieldPCGConstants::SpatialSampleHalfExtent));
		FPCGPoint SampledPoint;
		return SpatialData.SamplePoint(FTransform(Position), SampleBounds, SampledPoint, nullptr)
			&& SampledPoint.Density > 0.0f;
	}

	bool IsPositionExcludedFromRadixiteField(
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FVector& Position)
	{
		for (const UPCGSpatialData* Exclusion : Exclusions)
		{
			if (Exclusion != nullptr && DoesRadixiteSpatialDataContainPosition(*Exclusion, Position))
			{
				return true;
			}
		}
		return false;
	}

	bool TryProjectToGround(
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		const FVector& Position,
		FRadixiteGroundResult& OutGround)
	{
		FHitResult Hit;
		const FVector TraceStart = Position + FVector::UpVector * Settings.GroundTraceUp;
		const FVector TraceEnd = Position - FVector::UpVector * Settings.GroundTraceDown;
		const FCollisionQueryParams QueryParameters;
		if (not World.LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParameters))
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

	bool IsPointInBoundary(const FRadixiteFieldBoundary& Boundary, const FVector2D& Position)
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

	double DistanceToBoundaryEdge(const FRadixiteFieldBoundary& Boundary, const FVector2D& Position)
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
		const FRadixiteFieldBoundary& Boundary,
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
		const FRadixiteFieldBoundary& First,
		const FRadixiteFieldBoundary& Second,
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

	bool IsBoundaryClearOfFields(
		const FRadixiteFieldBoundary& Candidate,
		const TArray<FGeneratedRadixiteField>& Fields,
		const double Clearance)
	{
		for (const FGeneratedRadixiteField& Field : Fields)
		{
			if (BoundariesOverlap(Candidate, Field.Boundary, Clearance))
			{
				return false;
			}
		}
		return true;
	}

	FRadixiteFieldBoundary MakePointBoundsBoundary(const FPCGPoint& Point)
	{
		FRadixiteFieldBoundary Boundary;
		const double MinimumX = FMath::IsNearlyEqual(Point.BoundsMin.X, Point.BoundsMax.X)
			? Point.BoundsMin.X - RadixiteFieldPCGConstants::SpatialSampleHalfExtent
			: Point.BoundsMin.X;
		const double MaximumX = FMath::IsNearlyEqual(Point.BoundsMin.X, Point.BoundsMax.X)
			? Point.BoundsMax.X + RadixiteFieldPCGConstants::SpatialSampleHalfExtent
			: Point.BoundsMax.X;
		const double MinimumY = FMath::IsNearlyEqual(Point.BoundsMin.Y, Point.BoundsMax.Y)
			? Point.BoundsMin.Y - RadixiteFieldPCGConstants::SpatialSampleHalfExtent
			: Point.BoundsMin.Y;
		const double MaximumY = FMath::IsNearlyEqual(Point.BoundsMin.Y, Point.BoundsMax.Y)
			? Point.BoundsMax.Y + RadixiteFieldPCGConstants::SpatialSampleHalfExtent
			: Point.BoundsMax.Y;
		const FVector LocalCorners[] =
		{
			FVector(MinimumX, MinimumY, 0.0),
			FVector(MaximumX, MinimumY, 0.0),
			FVector(MaximumX, MaximumY, 0.0),
			FVector(MinimumX, MaximumY, 0.0)
		};
		for (const FVector& LocalCorner : LocalCorners)
		{
			const FVector WorldCorner = Point.Transform.TransformPosition(LocalCorner);
			Boundary.Points.Add(WorldCorner);
			Boundary.Bounds += FVector2D(WorldCorner);
		}
		Boundary.Center = Point.Transform.GetLocation();
		return Boundary;
	}

	bool BoundaryOverlapsExcludedPointBounds(
		const FRadixiteFieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions)
	{
		for (const UPCGSpatialData* Exclusion : Exclusions)
		{
			const UPCGPointData* PointData = Cast<UPCGPointData>(Exclusion);
			if (PointData == nullptr)
			{
				continue;
			}
			for (const FPCGPoint& Point : PointData->GetPoints())
			{
				const FRadixiteFieldBoundary PointBounds = MakePointBoundsBoundary(Point);
				if (BoundariesOverlap(Boundary, PointBounds, 0.0))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IsBoundaryClearOfExclusions(
		const FRadixiteFieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const double RequestedSpacing)
	{
		if (Exclusions.IsEmpty())
		{
			return true;
		}
		if (IsPositionExcludedFromRadixiteField(Exclusions, Boundary.Center)
			|| BoundaryOverlapsExcludedPointBounds(Boundary, Exclusions))
		{
			return false;
		}

		const double Spacing = FMath::Max(
			RadixiteFieldPCGConstants::MinimumSampleSpacing, RequestedSpacing);
		const int32 SamplesX = FMath::Clamp(
			FMath::CeilToInt32(Boundary.Bounds.GetSize().X / Spacing),
			1, RadixiteFieldPCGConstants::MaximumSamplesPerAxis);
		const int32 SamplesY = FMath::Clamp(
			FMath::CeilToInt32(Boundary.Bounds.GetSize().Y / Spacing),
			1, RadixiteFieldPCGConstants::MaximumSamplesPerAxis);
		for (int32 SampleX = 0; SampleX <= SamplesX; ++SampleX)
		{
			for (int32 SampleY = 0; SampleY <= SamplesY; ++SampleY)
			{
				const FVector2D Position(
					FMath::Lerp(Boundary.Bounds.Min.X, Boundary.Bounds.Max.X,
						static_cast<double>(SampleX) / SamplesX),
					FMath::Lerp(Boundary.Bounds.Min.Y, Boundary.Bounds.Max.Y,
						static_cast<double>(SampleY) / SamplesY));
				if (IsPointInBoundary(Boundary, Position)
					&& IsPositionExcludedFromRadixiteField(Exclusions, FVector(Position, Boundary.Center.Z)))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool TryCreateBoundary(
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		const FVector& Candidate,
		FRandomStream& Random,
		FRadixiteFieldBoundary& OutBoundary)
	{
		FRadixiteGroundResult CenterGround;
		if (not TryProjectToGround(World, Settings, Candidate, CenterGround))
		{
			return false;
		}

		OutBoundary.Center = CenterGround.Position;
		const double MinimumRadius = FMath::Max(RadixiteFieldPCGConstants::MinimumFieldRadius,
			FMath::Min(Settings.MinFieldRadius, Settings.MaxFieldRadius));
		const double MaximumRadius = FMath::Max(MinimumRadius,
			FMath::Max(Settings.MinFieldRadius, Settings.MaxFieldRadius));
		const double BaseRadius = Random.FRandRange(static_cast<float>(MinimumRadius), static_cast<float>(MaximumRadius));
		const int32 PointCount = FMath::Clamp(Settings.SplinePointCount,
			RadixiteFieldPCGConstants::MinimumSplinePoints, RadixiteFieldPCGConstants::MaximumSplinePoints);
		const double StartingAngle = Random.FRandRange(0.0f, static_cast<float>(RadixiteFieldPCGConstants::FullCircleRadians));

		OutBoundary.Points.Reserve(PointCount);
		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const double Angle = StartingAngle + RadixiteFieldPCGConstants::FullCircleRadians * PointIndex / PointCount;
			const double RadiusVariation = Random.FRandRange(-Settings.FieldBoundaryVariation,
				Settings.FieldBoundaryVariation);
			const double Radius = BaseRadius * (1.0 + RadiusVariation);
			const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
			FRadixiteGroundResult BoundaryGround;
			if (not TryProjectToGround(World, Settings,
				FVector(FVector2D(CenterGround.Position) + Direction * Radius, CenterGround.Position.Z), BoundaryGround))
			{
				return false;
			}
			OutBoundary.Points.Add(BoundaryGround.Position);
			OutBoundary.Bounds += FVector2D(BoundaryGround.Position);
			OutBoundary.MaximumRadius = FMath::Max(OutBoundary.MaximumRadius, Radius);
		}
		return true;
	}

	FTransform MakeGroundedActorTransform(
		const FResolvedRadixiteActor& Asset,
		const FRadixiteGroundResult& Ground,
		const double UniformScale,
		const double YawDegrees,
		const bool bAlignToGround)
	{
		FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(YawDegrees + Asset.YawOffsetDegrees));
		if (bAlignToGround)
		{
			Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Ground.Normal) * Rotation;
		}

		double RotatedMinimumZ = TNumericLimits<double>::Max();
		for (int32 XIndex = 0; XIndex < 2; ++XIndex)
		{
			for (int32 YIndex = 0; YIndex < 2; ++YIndex)
			{
				for (int32 ZIndex = 0; ZIndex < 2; ++ZIndex)
				{
					const FVector Corner(
						(XIndex == 0 ? Asset.LocalBounds.Min.X : Asset.LocalBounds.Max.X) * UniformScale,
						(YIndex == 0 ? Asset.LocalBounds.Min.Y : Asset.LocalBounds.Max.Y) * UniformScale,
						(ZIndex == 0 ? Asset.LocalBounds.Min.Z : Asset.LocalBounds.Max.Z) * UniformScale);
					RotatedMinimumZ = FMath::Min(RotatedMinimumZ, Rotation.RotateVector(Corner).Z);
				}
			}
		}
		const FVector Location = Ground.Position + FVector::UpVector * (Asset.ZOffset - RotatedMinimumZ);
		return FTransform(Rotation, Location, FVector(UniformScale));
	}

	bool IsActorPlacementClear(
		const FVector2D& Center,
		const double Radius,
		const TArray<FPlacedRadixiteActor>& Placements,
		const double ExtraSpacing)
	{
		for (const FPlacedRadixiteActor& Placement : Placements)
		{
			if (FVector2D::Distance(Center, Placement.Center)
				< Radius + Placement.FootprintRadius + ExtraSpacing)
			{
				return false;
			}
		}
		return true;
	}

	bool TryBuildActorPlacement(
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		const FRadixiteFieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedRadixiteActor>& ExistingPlacements,
		const FResolvedRadixiteActor& Asset,
		const FVector2D& DesiredCenter,
		const double ExtraSpacing,
		const int32 FieldIndex,
		const ERadixitePlacementRole Role,
		FRandomStream& Random,
		FPlacedRadixiteActor& OutPlacement)
	{
		const double Scale = Random.FRandRange(static_cast<float>(Asset.MinScale), static_cast<float>(Asset.MaxScale));
		const double Radius = Asset.FootprintRadius * Scale;
		if (not IsDiskInsideBoundary(Boundary, DesiredCenter, Radius)
			|| IsPositionExcludedFromRadixiteField(Exclusions, FVector(DesiredCenter, Boundary.Center.Z))
			|| not IsActorPlacementClear(DesiredCenter, Radius, ExistingPlacements, ExtraSpacing))
		{
			return false;
		}

		FRadixiteGroundResult Ground;
		if (not TryProjectToGround(World, Settings, FVector(DesiredCenter, Boundary.Center.Z), Ground))
		{
			return false;
		}
		OutPlacement.ActorClass = Asset.ActorClass;
		OutPlacement.LocalBounds = Asset.LocalBounds;
		OutPlacement.Transform = MakeGroundedActorTransform(Asset, Ground, Scale,
			Random.FRandRange(0.0f, static_cast<float>(RadixiteFieldPCGConstants::FullCircleDegrees)),
			Settings.bAlignActorsToGround);
		OutPlacement.Center = DesiredCenter;
		OutPlacement.FootprintRadius = Radius;
		OutPlacement.FieldIndex = FieldIndex;
		OutPlacement.Role = Role;
		return true;
	}

	FVector2D MakeRandomPointInField(
		const FRadixiteFieldBoundary& Boundary,
		const double MaximumRadiusMultiplier,
		FRandomStream& Random)
	{
		const double Angle = Random.FRandRange(0.0f, static_cast<float>(RadixiteFieldPCGConstants::FullCircleRadians));
		const double Radius = Boundary.MaximumRadius * MaximumRadiusMultiplier * FMath::Sqrt(Random.FRand());
		return FVector2D(Boundary.Center) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	}

	bool PlaceGrowthCenters(
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		const FRadixiteFieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedRadixiteActor>& Assets,
		const int32 FieldIndex,
		FRandomStream& Random,
		TArray<FPlacedRadixiteActor>& OutPlacements)
	{
		if (Assets.IsEmpty())
		{
			return false;
		}

		const int32 DesiredCount = FMath::Max(1, Settings.GrowthCentersPerField);
		for (int32 CenterIndex = 0; CenterIndex < DesiredCount; ++CenterIndex)
		{
			bool bPlaced = false;
			const int32 Attempts = CenterIndex == 0 ? 1 : FMath::Max(1, Settings.PlacementAttemptsPerItem);
			for (int32 Attempt = 0; Attempt < Attempts && not bPlaced; ++Attempt)
			{
				const FVector2D Position = CenterIndex == 0
					? FVector2D(Boundary.Center)
					: MakeRandomPointInField(Boundary, 0.55, Random);
				const int32 AssetIndex = PickWeightedAssetIndex(Assets, Random);
				FPlacedRadixiteActor Placement;
				bPlaced = TryBuildActorPlacement(World, Settings, Boundary, Exclusions, OutPlacements,
					Assets[AssetIndex], Position, Settings.MinGrowthCenterSpacing, FieldIndex,
					ERadixitePlacementRole::GrowthCenter, Random, Placement);
				if (bPlaced)
				{
					OutPlacements.Add(MoveTemp(Placement));
				}
			}
			if (CenterIndex == 0 && not bPlaced)
			{
				return false;
			}
		}
		return true;
	}

	bool IsCrystalConnectedToCluster(
		const FVector2D& Position,
		const TArray<FVector2D>& ClusterPositions,
		const double MaximumDistance)
	{
		if (ClusterPositions.IsEmpty() || MaximumDistance <= 0.0)
		{
			return true;
		}
		for (const FVector2D& ExistingPosition : ClusterPositions)
		{
			if (FVector2D::Distance(Position, ExistingPosition) <= MaximumDistance)
			{
				return true;
			}
		}
		return false;
	}

	void PlaceCrystalsAroundCenter(
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		const FRadixiteFieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedRadixiteActor>& Assets,
		const FVector2D& GrowthCenter,
		const int32 FieldIndex,
		FRandomStream& Random,
		TArray<FPlacedRadixiteActor>& InOutPlacements)
	{
		const int32 MinimumCount = FMath::Max(0,
			FMath::Min(Settings.MinCrystalsPerGrowthCenter, Settings.MaxCrystalsPerGrowthCenter));
		const int32 MaximumCount = FMath::Max(MinimumCount,
			FMath::Max(Settings.MinCrystalsPerGrowthCenter, Settings.MaxCrystalsPerGrowthCenter));
		const int32 DesiredCount = Random.RandRange(MinimumCount, MaximumCount);
		const double MinimumRadius = FMath::Max(0.0f,
			FMath::Min(Settings.MinGrowthNodeToCrystalDistance, Settings.MaxGrowthNodeToCrystalDistance));
		const double MaximumRadius = FMath::Max(MinimumRadius,
			FMath::Max(Settings.MinGrowthNodeToCrystalDistance, Settings.MaxGrowthNodeToCrystalDistance));
		TArray<FVector2D> ClusterPositions;

		for (int32 CrystalIndex = 0; CrystalIndex < DesiredCount; ++CrystalIndex)
		{
			for (int32 Attempt = 0; Attempt < FMath::Max(1, Settings.PlacementAttemptsPerItem); ++Attempt)
			{
				const double Angle = Random.FRandRange(0.0f,
					static_cast<float>(RadixiteFieldPCGConstants::FullCircleRadians));
				const double Radius = Random.FRandRange(static_cast<float>(MinimumRadius), static_cast<float>(MaximumRadius));
				const FVector2D Position = GrowthCenter
					+ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
				if (not IsCrystalConnectedToCluster(Position, ClusterPositions, Settings.MaxIndividualCrystalDistance))
				{
					continue;
				}

				const int32 AssetIndex = PickWeightedAssetIndex(Assets, Random);
				FPlacedRadixiteActor Placement;
				if (not TryBuildActorPlacement(World, Settings, Boundary, Exclusions, InOutPlacements,
					Assets[AssetIndex], Position, Settings.MinIndividualCrystalDistance, FieldIndex,
					ERadixitePlacementRole::RegularCrystal, Random, Placement))
				{
					continue;
				}
				InOutPlacements.Add(MoveTemp(Placement));
				ClusterPositions.Add(Position);
				break;
			}
		}
	}

	void PlaceRegularCrystals(
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		const FRadixiteFieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedRadixiteActor>& Assets,
		const int32 FieldIndex,
		FRandomStream& Random,
		TArray<FPlacedRadixiteActor>& InOutPlacements)
	{
		if (Assets.IsEmpty())
		{
			return;
		}

		TArray<FVector2D> GrowthCenters;
		for (const FPlacedRadixiteActor& Placement : InOutPlacements)
		{
			if (Placement.Role == ERadixitePlacementRole::GrowthCenter)
			{
				GrowthCenters.Add(Placement.Center);
			}
		}
		for (const FVector2D& GrowthCenter : GrowthCenters)
		{
			PlaceCrystalsAroundCenter(World, Settings, Boundary, Exclusions, Assets,
				GrowthCenter, FieldIndex, Random, InOutPlacements);
		}
	}

	void PlaceDecals(
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		const FRadixiteFieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedRadixiteDecal>& Assets,
		const int32 FieldIndex,
		FRandomStream& Random,
		TArray<FRadixiteDecalSpawn>& OutDecals)
	{
		if (Assets.IsEmpty())
		{
			return;
		}

		const int32 MinimumCount = FMath::Max(0, FMath::Min(Settings.MinDecalsPerField, Settings.MaxDecalsPerField));
		const int32 MaximumCount = FMath::Max(MinimumCount, FMath::Max(Settings.MinDecalsPerField, Settings.MaxDecalsPerField));
		const int32 DesiredCount = Random.RandRange(MinimumCount, MaximumCount);
		for (int32 DecalIndex = 0; DecalIndex < DesiredCount; ++DecalIndex)
		{
			for (int32 Attempt = 0; Attempt < FMath::Max(1, Settings.PlacementAttemptsPerItem); ++Attempt)
			{
				const int32 AssetIndex = PickWeightedAssetIndex(Assets, Random);
				const FResolvedRadixiteDecal& Asset = Assets[AssetIndex];
				const FVector2D Size(
					Random.FRandRange(static_cast<float>(Asset.MinSize.X), static_cast<float>(Asset.MaxSize.X)),
					Random.FRandRange(static_cast<float>(Asset.MinSize.Y), static_cast<float>(Asset.MaxSize.Y)));
				const FVector2D Position = MakeRandomPointInField(Boundary, 0.95, Random);
				const double FootprintRadius = 0.5 * Size.Size();
				if (not IsDiskInsideBoundary(Boundary, Position, FootprintRadius)
					|| IsPositionExcludedFromRadixiteField(Exclusions, FVector(Position, Boundary.Center.Z)))
				{
					continue;
				}

				FRadixiteGroundResult Ground;
				if (not TryProjectToGround(World, Settings, FVector(Position, Boundary.Center.Z), Ground))
				{
					continue;
				}
				FRadixiteDecalSpawn& Decal = OutDecals.Emplace_GetRef();
				Decal.Material = Asset.Material;
				Decal.Position = Ground.Position + Ground.Normal * Asset.SurfaceOffset;
				Decal.Normal = Ground.Normal;
				Decal.Size = Size;
				Decal.ProjectionDepth = Asset.ProjectionDepth;
				Decal.YawRadians = Random.FRandRange(0.0f,
					static_cast<float>(RadixiteFieldPCGConstants::FullCircleRadians));
				Decal.FieldIndex = FieldIndex;
				break;
			}
		}
	}

	bool TryGenerateField(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		const FVector& Candidate,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedRadixiteActor>& GrowthAssets,
		const TArray<FResolvedRadixiteActor>& CrystalAssets,
		const TArray<FResolvedRadixiteDecal>& DecalAssets,
		const TArray<FGeneratedRadixiteField>& ExistingFields,
		const int32 FieldIndex,
		FRandomStream& Random,
		FGeneratedRadixiteField& OutField)
	{
		if (not TryCreateBoundary(World, Settings, Candidate, Random, OutField.Boundary)
			|| not IsBoundaryClearOfFields(OutField.Boundary, ExistingFields, Settings.FieldClearance)
			|| not IsBoundaryClearOfExclusions(OutField.Boundary, Exclusions, Settings.ExclusionSampleSpacing))
		{
			return false;
		}

		if (not PlaceGrowthCenters(World, Settings, OutField.Boundary, Exclusions,
			GrowthAssets, FieldIndex, Random, OutField.Actors))
		{
			return false;
		}
		PlaceRegularCrystals(World, Settings, OutField.Boundary, Exclusions,
			CrystalAssets, FieldIndex, Random, OutField.Actors);
		PlaceDecals(World, Settings, OutField.Boundary, Exclusions,
			DecalAssets, FieldIndex, Random, OutField.Decals);
		return true;
	}

	void ShuffleCandidates(TArray<FVector>& Candidates, FRandomStream& Random)
	{
		for (int32 CandidateIndex = Candidates.Num() - 1; CandidateIndex > 0; --CandidateIndex)
		{
			const int32 SwapIndex = Random.RandRange(0, CandidateIndex);
			Candidates.Swap(CandidateIndex, SwapIndex);
		}
	}

	void GenerateFields(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateRadixiteFieldSettings& Settings,
		TArray<FVector> Candidates,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedRadixiteActor>& GrowthAssets,
		const TArray<FResolvedRadixiteActor>& CrystalAssets,
		const TArray<FResolvedRadixiteDecal>& DecalAssets,
		FRandomStream& Random,
		TArray<FGeneratedRadixiteField>& OutFields)
	{
		const int32 MinimumFields = FMath::Max(0, FMath::Min(Settings.MinFieldsToCreate, Settings.MaxFieldsToCreate));
		const int32 MaximumFields = FMath::Max(MinimumFields, FMath::Max(Settings.MinFieldsToCreate, Settings.MaxFieldsToCreate));
		const int32 DesiredFields = Random.RandRange(MinimumFields, MaximumFields);
		ShuffleCandidates(Candidates, Random);

		for (const FVector& Candidate : Candidates)
		{
			if (OutFields.Num() >= DesiredFields)
			{
				break;
			}
			FGeneratedRadixiteField Field;
			if (TryGenerateField(Context, World, Settings, Candidate, Exclusions, GrowthAssets,
				CrystalAssets, DecalAssets, OutFields, OutFields.Num(), Random, Field))
			{
				OutFields.Add(MoveTemp(Field));
			}
		}

		if (OutFields.Num() < MinimumFields)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("BelowMinimumFields",
					"CreateRadixiteField: created {0} fields, below the configured minimum {1}; remaining candidates overlapped exclusions, other fields, or invalid terrain."),
				FText::AsNumber(OutFields.Num()), FText::AsNumber(MinimumFields)));
		}
	}

	AActor* SpawnFieldSplineActor(
		UWorld& World,
		const FRadixiteFieldBoundary& Boundary,
		const int32 FieldIndex)
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
		SplineActor->SetActorLabel(FString::Printf(TEXT("PCG_RadixiteFieldSpline_%d"), FieldIndex));
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

	AActor* SpawnRadixiteActor(UWorld& World, const FPlacedRadixiteActor& Placement)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World.SpawnActor<AActor>(Placement.ActorClass, Placement.Transform, SpawnParameters);
		if (not IsValid(Actor))
		{
			return nullptr;
		}
		Actor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("PCG_RadixiteField_%s_%d"),
			*PlacementRoleToName(Placement.Role).ToString(), Placement.FieldIndex));
#endif
		return Actor;
	}

	AActor* SpawnFieldDecalActor(
		UWorld& World,
		const TArray<FRadixiteDecalSpawn>& Decals,
		const int32 FieldIndex)
	{
		if (Decals.IsEmpty())
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Anchor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (not IsValid(Anchor))
		{
			return nullptr;
		}
		Anchor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		Anchor->SetActorLabel(FString::Printf(TEXT("PCG_RadixiteFieldDecals_%d"), FieldIndex));
#endif
		USceneComponent* Root = NewObject<USceneComponent>(Anchor);
		Anchor->SetRootComponent(Root);
		Anchor->AddInstanceComponent(Root);
		Root->RegisterComponent();

		for (const FRadixiteDecalSpawn& Decal : Decals)
		{
			const FQuat SurfaceRotation = FQuat::FindBetweenNormals(FVector::ForwardVector, -Decal.Normal);
			const FQuat YawRotation(Decal.Normal, Decal.YawRadians);
			UDecalComponent* DecalComponent = NewObject<UDecalComponent>(Anchor);
			DecalComponent->SetMobility(EComponentMobility::Static);
			DecalComponent->SetDecalMaterial(Decal.Material);
			DecalComponent->DecalSize = FVector(Decal.ProjectionDepth, Decal.Size.X, Decal.Size.Y);
			DecalComponent->SetWorldTransform(FTransform(YawRotation * SurfaceRotation, Decal.Position));
			DecalComponent->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
			Anchor->AddInstanceComponent(DecalComponent);
			DecalComponent->RegisterComponent();
		}
		return Anchor;
	}

	void SpawnAndRegisterFields(
		UPCGComponent& SourceComponent,
		UWorld& World,
		const TArray<FGeneratedRadixiteField>& Fields)
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(&SourceComponent);
		for (int32 FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
		{
			const FGeneratedRadixiteField& Field = Fields[FieldIndex];
			if (AActor* SplineActor = SpawnFieldSplineActor(World, Field.Boundary, FieldIndex))
			{
				ManagedActors->GeneratedActors.Add(SplineActor);
			}
			for (const FPlacedRadixiteActor& Placement : Field.Actors)
			{
				if (AActor* Actor = SpawnRadixiteActor(World, Placement))
				{
					ManagedActors->GeneratedActors.Add(Actor);
				}
			}
			if (AActor* DecalActor = SpawnFieldDecalActor(World, Field.Decals, FieldIndex))
			{
				ManagedActors->GeneratedActors.Add(DecalActor);
			}
		}
		if (not ManagedActors->GeneratedActors.IsEmpty())
		{
			SourceComponent.AddToManagedResources(ManagedActors);
		}
	}

	UPCGPointData* CreateRadixiteOutputPointData(FPCGContext* Context, const FName PinLabel)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		PointData->InitializeFromData(nullptr);
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = PinLabel;
		return PointData;
	}

	void EmitFieldBounds(
		FPCGContext* Context,
		const TArray<FGeneratedRadixiteField>& Fields,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateRadixiteOutputPointData(
			Context, RadixiteFieldPCGConstants::FieldBoundsPinLabel);
		FPCGMetadataAttribute<int32>* FieldIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			RadixiteFieldPCGConstants::FieldIndexAttributeName, INDEX_NONE, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Fields.Num());
		for (int32 FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
		{
			const FRadixiteFieldBoundary& Boundary = Fields[FieldIndex].Boundary;
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(Boundary.Center);
			Point.BoundsMin = FVector(Boundary.Bounds.Min - FVector2D(Boundary.Center), 0.0);
			Point.BoundsMax = FVector(Boundary.Bounds.Max - FVector2D(Boundary.Center), 0.0);
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, FieldIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			FieldIndexAttribute->SetValue(Point.MetadataEntry, FieldIndex);
		}
	}

	void EmitPlacementPoint(
		UPCGPointData& PointData,
		FPCGMetadataAttribute<int32>& FieldIndexAttribute,
		FPCGMetadataAttribute<FName>& RoleAttribute,
		const FTransform& Transform,
		const FBox& LocalBounds,
		const int32 FieldIndex,
		const ERadixitePlacementRole Role,
		const int32 Seed)
	{
		FPCGPoint& Point = PointData.GetMutablePoints().Emplace_GetRef();
		Point.Transform = Transform;
		Point.BoundsMin = LocalBounds.Min;
		Point.BoundsMax = LocalBounds.Max;
		Point.Density = 1.0f;
		Point.Seed = Seed;
		PointData.Metadata->InitializeOnSet(Point.MetadataEntry);
		FieldIndexAttribute.SetValue(Point.MetadataEntry, FieldIndex);
		RoleAttribute.SetValue(Point.MetadataEntry, PlacementRoleToName(Role));
	}

	void EmitPlacements(
		FPCGContext* Context,
		const TArray<FGeneratedRadixiteField>& Fields,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateRadixiteOutputPointData(
			Context, RadixiteFieldPCGConstants::PlacementsPinLabel);
		FPCGMetadataAttribute<int32>* FieldIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			RadixiteFieldPCGConstants::FieldIndexAttributeName, INDEX_NONE, false, false);
		FPCGMetadataAttribute<FName>* RoleAttribute = PointData->Metadata->CreateAttribute<FName>(
			RadixiteFieldPCGConstants::PlacementRoleAttributeName, NAME_None, false, false);
		int32 PlacementIndex = 0;
		for (const FGeneratedRadixiteField& Field : Fields)
		{
			for (const FPlacedRadixiteActor& Placement : Field.Actors)
			{
				EmitPlacementPoint(*PointData, *FieldIndexAttribute, *RoleAttribute,
					Placement.Transform, Placement.LocalBounds, Placement.FieldIndex, Placement.Role,
					PCGHelpers::ComputeSeed(RandomSeed, PlacementIndex++));
			}
			for (const FRadixiteDecalSpawn& Decal : Field.Decals)
			{
				const FBox DecalBounds(
					FVector(-Decal.Size.X * 0.5, -Decal.Size.Y * 0.5, 0.0),
					FVector(Decal.Size.X * 0.5, Decal.Size.Y * 0.5, 0.0));
				EmitPlacementPoint(*PointData, *FieldIndexAttribute, *RoleAttribute,
					FTransform(Decal.Position), DecalBounds, Decal.FieldIndex, ERadixitePlacementRole::Decal,
					PCGHelpers::ComputeSeed(RandomSeed, PlacementIndex++));
			}
		}
	}
}

bool FPCGCreateRadixiteFieldElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	const UPCGCreateRadixiteFieldSettings* Settings =
		Context->GetInputSettings<UPCGCreateRadixiteFieldSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	TArray<FVector> Candidates = CollectCandidatePositions(Context);
	if (Candidates.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("NoCandidates", "CreateRadixiteField: the In pin contains no candidate points."));
		EmitFieldBounds(Context, {}, Settings->RandomSeed);
		EmitPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	TArray<FResolvedRadixiteActor> GrowthAssets;
	TArray<FResolvedRadixiteActor> CrystalAssets;
	TArray<FResolvedRadixiteDecal> DecalAssets;
	ResolveActorEntries(Context, World, Settings->GrowthNodeActors,
		LOCTEXT("GrowthNodeRole", "growth node"), GrowthAssets);
	ResolveActorEntries(Context, World, Settings->RadixiteCrystalActors,
		LOCTEXT("CrystalRole", "regular crystal"), CrystalAssets);
	ResolveDecalEntries(Context, Settings->Decals, DecalAssets);
	if (GrowthAssets.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("NoGrowthAssets", "CreateRadixiteField: no valid growth node actors are configured."));
		EmitFieldBounds(Context, {}, Settings->RandomSeed);
		EmitPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	FRandomStream Random(Settings->RandomSeed);
	TArray<FGeneratedRadixiteField> Fields;
	GenerateFields(Context, World, *Settings, MoveTemp(Candidates), CollectExclusions(Context),
		GrowthAssets, CrystalAssets, DecalAssets, Random, Fields);
	SpawnAndRegisterFields(*SourceComponent, World, Fields);
	EmitFieldBounds(Context, Fields, Settings->RandomSeed);
	EmitPlacements(Context, Fields, Settings->RandomSeed);
	return true;
}

#undef LOCTEXT_NAMESPACE
