// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreateBattlefieldArea.h"

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
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Helpers/PCGHelpers.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#define LOCTEXT_NAMESPACE "PCGCreateBattlefieldArea"

namespace BattlefieldAreaPCGConstants
{
	const FName ExcludedBoundsPinLabel = TEXT("Excluded Bounds");
	const FName AreaBoundsPinLabel = TEXT("Area Bounds");
	const FName PlacementsPinLabel = TEXT("Placements");
	const FName AreaIndexAttributeName = TEXT("BattlefieldAreaIndex");
	const FName PlacementRoleAttributeName = TEXT("BattlefieldPlacementRole");
	const FName SliceAttributeName = TEXT("BattlefieldSlice");

	constexpr double InspectionDepth = -100000.0;
	constexpr double SpatialSampleHalfExtent = 5.0;
	constexpr double MinimumUsableBoundsSize = 1.0;
	constexpr double MinimumAreaDimension = 1000.0;
	constexpr double MinimumSampleSpacing = 25.0;
	constexpr double MinimumScale = 0.01;
	constexpr double FullCircleDegrees = 360.0;
	constexpr int32 MinimumBoundaryStations = 4;
	constexpr int32 MaximumBoundaryStations = 32;
	constexpr int32 MaximumSamplesPerAxis = 128;
}

#if WITH_EDITOR
FName UPCGCreateBattlefieldAreaSettings::GetDefaultNodeName() const
{
	return FName(TEXT("CreateBattlefieldArea"));
}

FText UPCGCreateBattlefieldAreaSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Battlefield Areas");
}

FText UPCGCreateBattlefieldAreaSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Creates deterministic XY-only non-convex battlefield spline areas split into Soviet weaponry, no man's "
		"land, and German weaponry. Bounds-measured actors, grouped obstacles, spoils, trees, and decals "
		"share collision prevention and snap independently to landscape elevation.");
}
#endif

FPCGElementPtr UPCGCreateBattlefieldAreaSettings::CreateElement() const
{
	return MakeShared<FPCGCreateBattlefieldAreaElement>();
}

TArray<FPCGPinProperties> UPCGCreateBattlefieldAreaSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(BattlefieldAreaPCGConstants::ExcludedBoundsPinLabel, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreateBattlefieldAreaSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(BattlefieldAreaPCGConstants::AreaBoundsPinLabel, EPCGDataType::Point);
	Pins.Emplace(BattlefieldAreaPCGConstants::PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

namespace BattlefieldAreaPCGInternal
{
	enum class EBattlefieldPlacementRole : uint8
	{
		SovietWeaponry,
		BarbedWire,
		Hedgehog,
		GermanWeaponry,
		SovietSpoils,
		GermanSpoils,
		Tree,
		Decal
	};

	enum class EBattlefieldSlice : uint8
	{
		Soviet,
		NoMansLand,
		German,
		Global
	};

	struct FBattlefieldCandidate
	{
		FTransform Transform = FTransform::Identity;
	};

	struct FResolvedBattlefieldActor
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		FVector2D FootprintSize = FVector2D::ZeroVector;
		double Weight = 1.0;
		double MinScale = 1.0;
		double MaxScale = 1.0;
		double ZOffset = 0.0;
		double YawOffsetDegrees = 0.0;
		double AuthoredLengthYawDegrees = 0.0;
	};

	struct FResolvedBattlefieldDecal
	{
		UMaterialInterface* Material = nullptr;
		FVector2D DefaultSize = FVector2D::ZeroVector;
		double Weight = 1.0;
		double ProjectionDepth = 0.0;
		double SurfaceOffset = 0.0;
	};

	struct FBattlefieldGroundResult
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
	};

	struct FBattlefieldBoundary
	{
		FVector Center = FVector::ZeroVector;
		FVector2D Forward = FVector2D(1.0, 0.0);
		FVector2D Side = FVector2D(0.0, 1.0);
		FVector2D SovietRange = FVector2D::ZeroVector;
		FVector2D NoMansLandRange = FVector2D::ZeroVector;
		FVector2D GermanRange = FVector2D::ZeroVector;
		TArray<FVector> Points;
		FBox2D Bounds = FBox2D(EForceInit::ForceInit);
		double Length = 0.0;
		double Width = 0.0;
	};

	struct FBattlefieldFootprint
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D HalfExtents = FVector2D::ZeroVector;
		double YawRadians = 0.0;
	};

	struct FPlacedBattlefieldActor
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		FTransform Transform = FTransform::Identity;
		FVector2D Center = FVector2D::ZeroVector;
		double FootprintRadius = 0.0;
		FBattlefieldFootprint Footprint;
		int32 AreaIndex = INDEX_NONE;
		EBattlefieldPlacementRole Role = EBattlefieldPlacementRole::SovietWeaponry;
		EBattlefieldSlice Slice = EBattlefieldSlice::Soviet;
	};

	struct FBattlefieldDecalSpawn
	{
		UMaterialInterface* Material = nullptr;
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		FVector2D Size = FVector2D::ZeroVector;
		double ProjectionDepth = 0.0;
		double YawRadians = 0.0;
		double FootprintRadius = 0.0;
		FBattlefieldFootprint Footprint;
		int32 AreaIndex = INDEX_NONE;
	};

	struct FGeneratedBattlefieldArea
	{
		FBattlefieldBoundary Boundary;
		TArray<FPlacedBattlefieldActor> Actors;
		TArray<FBattlefieldDecalSpawn> Decals;
	};

	FName PlacementRoleToName(const EBattlefieldPlacementRole Role)
	{
		switch (Role)
		{
		case EBattlefieldPlacementRole::SovietWeaponry:
			return TEXT("SovietWeaponry");
		case EBattlefieldPlacementRole::BarbedWire:
			return TEXT("BarbedWire");
		case EBattlefieldPlacementRole::Hedgehog:
			return TEXT("Hedgehog");
		case EBattlefieldPlacementRole::GermanWeaponry:
			return TEXT("GermanWeaponry");
		case EBattlefieldPlacementRole::SovietSpoils:
			return TEXT("SovietSpoilsOfWar");
		case EBattlefieldPlacementRole::GermanSpoils:
			return TEXT("GermanSpoilsOfWar");
		case EBattlefieldPlacementRole::Tree:
			return TEXT("Tree");
		case EBattlefieldPlacementRole::Decal:
			return TEXT("Decal");
		default:
			return NAME_None;
		}
	}

	FName SliceToName(const EBattlefieldSlice Slice)
	{
		switch (Slice)
		{
		case EBattlefieldSlice::Soviet:
			return TEXT("Soviet");
		case EBattlefieldSlice::NoMansLand:
			return TEXT("NoMansLand");
		case EBattlefieldSlice::German:
			return TEXT("German");
		case EBattlefieldSlice::Global:
			return TEXT("Global");
		default:
			return NAME_None;
		}
	}

	double LineDirectionToYawDegrees(const EBattlefieldLineDirection Direction)
	{
		switch (Direction)
		{
		case EBattlefieldLineDirection::PositiveX:
			return 0.0;
		case EBattlefieldLineDirection::NegativeX:
			return 180.0;
		case EBattlefieldLineDirection::PositiveY:
			return 90.0;
		case EBattlefieldLineDirection::NegativeY:
			return -90.0;
		default:
			return 0.0;
		}
	}

	AActor* SpawnInspectionActor(UWorld& World, UClass& ActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		return World.SpawnActor<AActor>(
			&ActorClass,
			FTransform(FVector(0.0, 0.0, BattlefieldAreaPCGConstants::InspectionDepth)),
			SpawnParameters);
	}

	bool TryMeasureActor(UWorld& World, UClass& ActorClass, FBox& OutLocalBounds)
	{
		AActor* InspectionActor = SpawnInspectionActor(World, ActorClass);
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
		return BoundsSize.X >= BattlefieldAreaPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Y >= BattlefieldAreaPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Z >= BattlefieldAreaPCGConstants::MinimumUsableBoundsSize;
	}

	bool TryResolveActorEntry(
		UWorld& World,
		const FBattlefieldActorEntry& Entry,
		FResolvedBattlefieldActor& OutActor)
	{
		UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
		FBox LocalBounds(EForceInit::ForceInit);
		if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
			|| not TryMeasureActor(World, *ActorClass, LocalBounds))
		{
			return false;
		}

		const FVector BoundsSize = LocalBounds.GetSize();
		OutActor.ActorClass = ActorClass;
		OutActor.LocalBounds = LocalBounds;
		OutActor.FootprintSize.X = Entry.FootprintOverrideX > 0.0f ? Entry.FootprintOverrideX : BoundsSize.X;
		OutActor.FootprintSize.Y = Entry.FootprintOverrideY > 0.0f ? Entry.FootprintOverrideY : BoundsSize.Y;
		OutActor.Weight = FMath::Max(0.0f, Entry.Weight);
		OutActor.MinScale = FMath::Max(BattlefieldAreaPCGConstants::MinimumScale,
			FMath::Min(Entry.MinUniformScale, Entry.MaxUniformScale));
		OutActor.MaxScale = FMath::Max(OutActor.MinScale,
			FMath::Max(Entry.MinUniformScale, Entry.MaxUniformScale));
		OutActor.ZOffset = Entry.ZOffset;
		OutActor.YawOffsetDegrees = Entry.YawOffsetDegrees;
		return true;
	}

	void ResolveActorEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FBattlefieldActorEntry>& Entries,
		const FText& EntryRole,
		TArray<FResolvedBattlefieldActor>& OutActors)
	{
		for (const FBattlefieldActorEntry& Entry : Entries)
		{
			FResolvedBattlefieldActor Resolved;
			if (TryResolveActorEntry(World, Entry, Resolved))
			{
				OutActors.Add(MoveTemp(Resolved));
				continue;
			}

			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("InvalidActorEntry", "CreateBattlefieldArea: skipped an invalid or bounds-less {0} actor."),
				EntryRole));
		}
	}

	void ResolveLineActorEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FBattlefieldLineActorEntry>& Entries,
		TArray<FResolvedBattlefieldActor>& OutActors)
	{
		for (const FBattlefieldLineActorEntry& Entry : Entries)
		{
			FResolvedBattlefieldActor Resolved;
			if (not TryResolveActorEntry(World, Entry, Resolved))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("InvalidLineActorEntry",
					"CreateBattlefieldArea: skipped an invalid or bounds-less barbed-wire actor."));
				continue;
			}
			Resolved.AuthoredLengthYawDegrees = LineDirectionToYawDegrees(Entry.AuthoredLengthDirection);
			OutActors.Add(MoveTemp(Resolved));
		}
	}

	void ResolveDecalEntries(
		FPCGContext* Context,
		const TArray<FBattlefieldDecalEntry>& Entries,
		TArray<FResolvedBattlefieldDecal>& OutDecals)
	{
		for (const FBattlefieldDecalEntry& Entry : Entries)
		{
			UMaterialInterface* Material = Entry.Material.LoadSynchronous();
			if (not IsValid(Material))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("InvalidDecalEntry",
					"CreateBattlefieldArea: skipped a decal with no valid material."));
				continue;
			}

			FResolvedBattlefieldDecal& Resolved = OutDecals.Emplace_GetRef();
			Resolved.Material = Material;
			Resolved.Weight = FMath::Max(0.0f, Entry.Weight);
			Resolved.DefaultSize.X = FMath::Max(1.0f, Entry.DefaultSize.X);
			Resolved.DefaultSize.Y = FMath::Max(1.0f, Entry.DefaultSize.Y);
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

	TArray<FBattlefieldCandidate> CollectCandidates(FPCGContext* Context)
	{
		TArray<FBattlefieldCandidate> Candidates;
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
				Candidates.Add({Point.Transform});
			}
		}
		return Candidates;
	}

	TArray<const UPCGSpatialData*> CollectExclusions(FPCGContext* Context)
	{
		TArray<const UPCGSpatialData*> Exclusions;
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(
			BattlefieldAreaPCGConstants::ExcludedBoundsPinLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
			{
				Exclusions.Add(SpatialData);
			}
		}
		return Exclusions;
	}

	bool SpatialContains(const UPCGSpatialData& SpatialData, const FVector& Position)
	{
		const FBox SampleBounds(
			FVector(-BattlefieldAreaPCGConstants::SpatialSampleHalfExtent),
			FVector(BattlefieldAreaPCGConstants::SpatialSampleHalfExtent));
		FPCGPoint SampledPoint;
		return SpatialData.SamplePoint(FTransform(Position), SampleBounds, SampledPoint, nullptr)
			&& SampledPoint.Density > 0.0f;
	}

	bool IsExcluded(const TArray<const UPCGSpatialData*>& Exclusions, const FVector& Position)
	{
		for (const UPCGSpatialData* Exclusion : Exclusions)
		{
			if (Exclusion != nullptr && SpatialContains(*Exclusion, Position))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief Resolves the landscape from XY before tracing so the area's reference Z cannot affect grounding.
	 * @param World World containing the landscape proxies.
	 * @param Settings Trace distances used around the landscape-reported height.
	 * @param Position Placement whose XY selects the landscape; its Z is intentionally ignored.
	 * @param OutHit Receives the landscape surface hit.
	 * @return True when a landscape covers the requested XY and its collision surface was hit.
	 */
	bool TryTraceLandscape(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
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

			const FVector LandscapePosition(Position.X, Position.Y, LandscapeHeight.GetValue());
			const FVector TraceStart = LandscapePosition + FVector::UpVector * Settings.GroundTraceUp;
			const FVector TraceEnd = LandscapePosition - FVector::UpVector * Settings.GroundTraceDown;
			if (Landscape->ActorLineTraceSingle(
				OutHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParameters))
			{
				return true;
			}
		}

		return false;
	}

	bool TryProjectToLandscape(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FVector& Position,
		FBattlefieldGroundResult& OutGround)
	{
		FHitResult LandscapeHit;
		if (not TryTraceLandscape(World, Settings, Position, LandscapeHit))
		{
			return false;
		}

		const double SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(
			FMath::Clamp(static_cast<double>(LandscapeHit.ImpactNormal.Z), -1.0, 1.0)));
		if (SlopeDegrees > Settings.MaxGroundSlopeDegrees)
		{
			return false;
		}
		OutGround.Position = LandscapeHit.ImpactPoint;
		OutGround.Normal = LandscapeHit.ImpactNormal.GetSafeNormal();
		return true;
	}

	bool IsPointInBoundary(const FBattlefieldBoundary& Boundary, const FVector2D& Position)
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

	double DistanceToBoundaryEdge(const FBattlefieldBoundary& Boundary, const FVector2D& Position)
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
		const FBattlefieldBoundary& Boundary,
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
		const FBattlefieldBoundary& First,
		const FBattlefieldBoundary& Second,
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

	bool IsBoundaryClearOfAreas(
		const FBattlefieldBoundary& Candidate,
		const TArray<FGeneratedBattlefieldArea>& Areas,
		const double Clearance)
	{
		for (const FGeneratedBattlefieldArea& Area : Areas)
		{
			if (BoundariesOverlap(Candidate, Area.Boundary, Clearance))
			{
				return false;
			}
		}
		return true;
	}

	FBattlefieldBoundary MakePointBoundsBoundary(const FPCGPoint& Point)
	{
		FBattlefieldBoundary Boundary;
		const double MinimumX = FMath::IsNearlyEqual(Point.BoundsMin.X, Point.BoundsMax.X)
			? Point.BoundsMin.X - BattlefieldAreaPCGConstants::SpatialSampleHalfExtent : Point.BoundsMin.X;
		const double MaximumX = FMath::IsNearlyEqual(Point.BoundsMin.X, Point.BoundsMax.X)
			? Point.BoundsMax.X + BattlefieldAreaPCGConstants::SpatialSampleHalfExtent : Point.BoundsMax.X;
		const double MinimumY = FMath::IsNearlyEqual(Point.BoundsMin.Y, Point.BoundsMax.Y)
			? Point.BoundsMin.Y - BattlefieldAreaPCGConstants::SpatialSampleHalfExtent : Point.BoundsMin.Y;
		const double MaximumY = FMath::IsNearlyEqual(Point.BoundsMin.Y, Point.BoundsMax.Y)
			? Point.BoundsMax.Y + BattlefieldAreaPCGConstants::SpatialSampleHalfExtent : Point.BoundsMax.Y;
		const FVector LocalCorners[] = {
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
		const FBattlefieldBoundary& Boundary,
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
				if (BoundariesOverlap(Boundary, MakePointBoundsBoundary(Point), 0.0))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IsBoundaryClearOfExclusions(
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const double RequestedSpacing)
	{
		if (Exclusions.IsEmpty())
		{
			return true;
		}
		if (IsExcluded(Exclusions, Boundary.Center)
			|| BoundaryOverlapsExcludedPointBounds(Boundary, Exclusions))
		{
			return false;
		}

		const double Spacing = FMath::Max(BattlefieldAreaPCGConstants::MinimumSampleSpacing, RequestedSpacing);
		const int32 SamplesX = FMath::Clamp(FMath::CeilToInt32(Boundary.Bounds.GetSize().X / Spacing),
			1, BattlefieldAreaPCGConstants::MaximumSamplesPerAxis);
		const int32 SamplesY = FMath::Clamp(FMath::CeilToInt32(Boundary.Bounds.GetSize().Y / Spacing),
			1, BattlefieldAreaPCGConstants::MaximumSamplesPerAxis);
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
					&& IsExcluded(Exclusions, FVector(Position, Boundary.Center.Z)))
				{
					return false;
				}
			}
		}
		return true;
	}

	void ApplyBoundaryIndentations(
		TArray<double>& UpperWidths,
		TArray<double>& LowerWidths,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const double HalfWidth,
		FRandomStream& Random)
	{
		const int32 MinimumIndentations = FMath::Max(0,
			FMath::Min(Settings.MinBoundaryIndentations, Settings.MaxBoundaryIndentations));
		const int32 MaximumIndentations = FMath::Max(MinimumIndentations,
			FMath::Max(Settings.MinBoundaryIndentations, Settings.MaxBoundaryIndentations));
		const int32 AvailableStations = FMath::Max(0, UpperWidths.Num() - 2);
		const int32 DesiredIndentations = FMath::Min(AvailableStations * 2,
			Random.RandRange(MinimumIndentations, MaximumIndentations));
		TSet<int32> UsedIndentKeys;
		for (int32 IndentIndex = 0; IndentIndex < DesiredIndentations; ++IndentIndex)
		{
			int32 Key = INDEX_NONE;
			for (int32 Attempt = 0; Attempt < AvailableStations * 4; ++Attempt)
			{
				const int32 Station = Random.RandRange(1, UpperWidths.Num() - 2);
				const int32 CandidateKey = Station * 2 + Random.RandRange(0, 1);
				if (not UsedIndentKeys.Contains(CandidateKey))
				{
					Key = CandidateKey;
					break;
				}
			}
			if (Key == INDEX_NONE || UsedIndentKeys.Contains(Key))
			{
				continue;
			}
			UsedIndentKeys.Add(Key);

			TArray<double>& Widths = Key % 2 == 0 ? UpperWidths : LowerWidths;
			const int32 Station = Key / 2;
			const double NeighborWidth = 0.5 * (Widths[Station - 1] + Widths[Station + 1]);
			const double IndentedWidth = NeighborWidth - HalfWidth * Settings.BoundaryIndentDepth;
			Widths[Station] = FMath::Max(HalfWidth * 0.2, FMath::Min(Widths[Station], IndentedWidth));
		}
	}

	void InitializeSliceRanges(
		const UPCGCreateBattlefieldAreaSettings& Settings,
		FBattlefieldBoundary& OutBoundary)
	{
		const double SovietWeight = FMath::Max(0.01f, Settings.SovietSliceProportion);
		const double NoMansLandWeight = FMath::Max(0.01f, Settings.NoMansLandSliceProportion);
		const double GermanWeight = FMath::Max(0.01f, Settings.GermanSliceProportion);
		const double TotalWeight = SovietWeight + NoMansLandWeight + GermanWeight;
		const double MinimumX = -0.5 * OutBoundary.Length;
		const double SovietEnd = MinimumX + OutBoundary.Length * SovietWeight / TotalWeight;
		const double NoMansLandEnd = SovietEnd + OutBoundary.Length * NoMansLandWeight / TotalWeight;
		OutBoundary.SovietRange = FVector2D(MinimumX, SovietEnd);
		OutBoundary.NoMansLandRange = FVector2D(SovietEnd, NoMansLandEnd);
		OutBoundary.GermanRange = FVector2D(NoMansLandEnd, 0.5 * OutBoundary.Length);
	}

	void AddBoundaryPoint(
		const FVector2D& WorldPosition,
		FBattlefieldBoundary& OutBoundary)
	{
		OutBoundary.Points.Add(FVector(WorldPosition, OutBoundary.Center.Z));
		OutBoundary.Bounds += WorldPosition;
	}

	void BuildBoundaryPoints(
		const TArray<double>& UpperWidths,
		const TArray<double>& LowerWidths,
		FBattlefieldBoundary& OutBoundary)
	{
		const int32 StationCount = UpperWidths.Num();
		OutBoundary.Points.Reserve(StationCount * 2);
		for (int32 Station = 0; Station < StationCount; ++Station)
		{
			const double Alpha = static_cast<double>(Station) / (StationCount - 1);
			const double LocalX = FMath::Lerp(-0.5 * OutBoundary.Length, 0.5 * OutBoundary.Length, Alpha);
			const FVector2D Position = FVector2D(OutBoundary.Center) + OutBoundary.Forward * LocalX
				+ OutBoundary.Side * UpperWidths[Station];
			AddBoundaryPoint(Position, OutBoundary);
		}
		for (int32 Station = StationCount - 1; Station >= 0; --Station)
		{
			const double Alpha = static_cast<double>(Station) / (StationCount - 1);
			const double LocalX = FMath::Lerp(-0.5 * OutBoundary.Length, 0.5 * OutBoundary.Length, Alpha);
			const FVector2D Position = FVector2D(OutBoundary.Center) + OutBoundary.Forward * LocalX
				- OutBoundary.Side * LowerWidths[Station];
			AddBoundaryPoint(Position, OutBoundary);
		}
	}

	void CreateBoundary(
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldCandidate& Candidate,
		FRandomStream& Random,
		FBattlefieldBoundary& OutBoundary)
	{
		const double MinLength = FMath::Max(BattlefieldAreaPCGConstants::MinimumAreaDimension,
			FMath::Min(Settings.MinAreaLength, Settings.MaxAreaLength));
		const double MaxLength = FMath::Max(MinLength, FMath::Max(Settings.MinAreaLength, Settings.MaxAreaLength));
		const double MinWidth = FMath::Max(BattlefieldAreaPCGConstants::MinimumAreaDimension,
			FMath::Min(Settings.MinAreaWidth, Settings.MaxAreaWidth));
		const double MaxWidth = FMath::Max(MinWidth, FMath::Max(Settings.MinAreaWidth, Settings.MaxAreaWidth));
		const double YawDegrees = Settings.bUseInputPointRotation
			? Candidate.Transform.Rotator().Yaw
			: Random.FRandRange(0.0f, static_cast<float>(BattlefieldAreaPCGConstants::FullCircleDegrees));
		OutBoundary.Center = Candidate.Transform.GetLocation();
		OutBoundary.Length = Random.FRandRange(static_cast<float>(MinLength), static_cast<float>(MaxLength));
		OutBoundary.Width = Random.FRandRange(static_cast<float>(MinWidth), static_cast<float>(MaxWidth));
		OutBoundary.Forward = FVector2D(FMath::Cos(FMath::DegreesToRadians(YawDegrees)),
			FMath::Sin(FMath::DegreesToRadians(YawDegrees)));
		OutBoundary.Side = FVector2D(-OutBoundary.Forward.Y, OutBoundary.Forward.X);
		InitializeSliceRanges(Settings, OutBoundary);

		const int32 StationCount = FMath::Clamp(Settings.BoundaryStationsPerSide,
			BattlefieldAreaPCGConstants::MinimumBoundaryStations,
			BattlefieldAreaPCGConstants::MaximumBoundaryStations);
		const double HalfWidth = 0.5 * OutBoundary.Width;
		TArray<double> UpperWidths;
		TArray<double> LowerWidths;
		UpperWidths.Reserve(StationCount);
		LowerWidths.Reserve(StationCount);
		for (int32 Station = 0; Station < StationCount; ++Station)
		{
			UpperWidths.Add(HalfWidth * (1.0 + Random.FRandRange(
				-Settings.BoundaryWidthVariation, Settings.BoundaryWidthVariation)));
			LowerWidths.Add(HalfWidth * (1.0 + Random.FRandRange(
				-Settings.BoundaryWidthVariation, Settings.BoundaryWidthVariation)));
		}
		ApplyBoundaryIndentations(UpperWidths, LowerWidths, Settings, HalfWidth, Random);
		BuildBoundaryPoints(UpperWidths, LowerWidths, OutBoundary);
	}

	FVector2D LocalToWorld(const FBattlefieldBoundary& Boundary, const FVector2D& LocalPosition)
	{
		return FVector2D(Boundary.Center) + Boundary.Forward * LocalPosition.X + Boundary.Side * LocalPosition.Y;
	}

	FVector2D MakeRandomLocalPoint(
		const FBattlefieldBoundary& Boundary,
		const FVector2D& SliceRange,
		const double Spread,
		const double Padding,
		FRandomStream& Random)
	{
		const double SafeMinX = FMath::Min(SliceRange.X + Padding, SliceRange.Y);
		const double SafeMaxX = FMath::Max(SafeMinX, SliceRange.Y - Padding);
		const double CenterX = 0.5 * (SafeMinX + SafeMaxX);
		const double HalfRangeX = 0.5 * (SafeMaxX - SafeMinX) * FMath::Clamp(Spread, 0.05, 1.0);
		const double HalfRangeY = FMath::Max(0.0, 0.5 * Boundary.Width - Padding)
			* FMath::Clamp(Spread, 0.05, 1.0);
		return FVector2D(
			Random.FRandRange(static_cast<float>(CenterX - HalfRangeX), static_cast<float>(CenterX + HalfRangeX)),
			Random.FRandRange(static_cast<float>(-HalfRangeY), static_cast<float>(HalfRangeY)));
	}

	FQuat MakeActorRotation(
		const FResolvedBattlefieldActor& Asset,
		const FBattlefieldGroundResult& Ground,
		const double YawDegrees,
		const bool bAlignToGround)
	{
		FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(YawDegrees + Asset.YawOffsetDegrees));
		if (bAlignToGround)
		{
			Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Ground.Normal) * Rotation;
		}
		return Rotation;
	}

	FTransform MakeGroundedActorTransform(
		const FResolvedBattlefieldActor& Asset,
		const FBattlefieldGroundResult& Ground,
		const double UniformScale,
		const double YawDegrees,
		const bool bAlignToGround)
	{
		const FQuat Rotation = MakeActorRotation(Asset, Ground, YawDegrees, bAlignToGround);
		const double ScaledMinimumLocalZ = Asset.LocalBounds.Min.Z * UniformScale;
		const double GroundNormalZ = bAlignToGround
			? FMath::Max(static_cast<double>(Ground.Normal.Z), UE_DOUBLE_SMALL_NUMBER)
			: 1.0;
		const double LocationZ = Ground.Position.Z + Asset.ZOffset - ScaledMinimumLocalZ / GroundNormalZ;
		const FVector Location(Ground.Position.X, Ground.Position.Y, LocationZ);
		return FTransform(Rotation, Location, FVector(UniformScale));
	}

	FVector2D GetFootprintAxis(const FBattlefieldFootprint& Footprint, const bool bSideAxis)
	{
		const double AxisYaw = Footprint.YawRadians + (bSideAxis ? 0.5 * PI : 0.0);
		return FVector2D(FMath::Cos(AxisYaw), FMath::Sin(AxisYaw));
	}

	double GetProjectedFootprintRadius(
		const FBattlefieldFootprint& Footprint,
		const FVector2D& Axis)
	{
		return FMath::Abs(FVector2D::DotProduct(Axis, GetFootprintAxis(Footprint, false)))
				* Footprint.HalfExtents.X
			+ FMath::Abs(FVector2D::DotProduct(Axis, GetFootprintAxis(Footprint, true)))
				* Footprint.HalfExtents.Y;
	}

	bool HasSeparatingAxis(
		const FBattlefieldFootprint& First,
		const FBattlefieldFootprint& Second,
		const FVector2D& Axis,
		const double Clearance)
	{
		const double CenterDistance = FMath::Abs(FVector2D::DotProduct(Second.Center - First.Center, Axis));
		return CenterDistance >= GetProjectedFootprintRadius(First, Axis)
			+ GetProjectedFootprintRadius(Second, Axis) + Clearance;
	}

	bool FootprintsOverlap(
		const FBattlefieldFootprint& First,
		const FBattlefieldFootprint& Second,
		const double Clearance)
	{
		return not HasSeparatingAxis(First, Second, GetFootprintAxis(First, false), Clearance)
			&& not HasSeparatingAxis(First, Second, GetFootprintAxis(First, true), Clearance)
			&& not HasSeparatingAxis(First, Second, GetFootprintAxis(Second, false), Clearance)
			&& not HasSeparatingAxis(First, Second, GetFootprintAxis(Second, true), Clearance);
	}

	bool IsPlacementClear(
		const FBattlefieldFootprint& Footprint,
		const TArray<FBattlefieldFootprint>& Occupied,
		const double Clearance)
	{
		for (const FBattlefieldFootprint& Existing : Occupied)
		{
			if (FootprintsOverlap(Footprint, Existing, Clearance))
			{
				return false;
			}
		}
		return true;
	}

	bool TryMakeActorPlacement(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FResolvedBattlefieldActor& Asset,
		const FVector2D& Center,
		const double UniformScale,
		const double YawDegrees,
		const int32 AreaIndex,
		const EBattlefieldPlacementRole Role,
		const EBattlefieldSlice Slice,
		const TArray<FBattlefieldFootprint>& Occupied,
		FPlacedBattlefieldActor& OutPlacement)
	{
		const double Radius = 0.5 * Asset.FootprintSize.Size() * UniformScale;
		const double FootprintYawRadians = FMath::DegreesToRadians(YawDegrees + Asset.YawOffsetDegrees);
		const FVector2D LocalBoundsCenter(Asset.LocalBounds.GetCenter());
		const FVector2D BoundsCenterOffset(
			LocalBoundsCenter.X * FMath::Cos(FootprintYawRadians)
				- LocalBoundsCenter.Y * FMath::Sin(FootprintYawRadians),
			LocalBoundsCenter.X * FMath::Sin(FootprintYawRadians)
				+ LocalBoundsCenter.Y * FMath::Cos(FootprintYawRadians));
		FBattlefieldFootprint Footprint;
		Footprint.Center = Center + BoundsCenterOffset * UniformScale;
		Footprint.HalfExtents = 0.5 * Asset.FootprintSize * UniformScale;
		Footprint.YawRadians = FootprintYawRadians;
		if (not IsDiskInsideBoundary(Boundary, Footprint.Center, Radius)
			|| IsExcluded(Exclusions, FVector(Footprint.Center, Boundary.Center.Z))
			|| not IsPlacementClear(Footprint, Occupied, Settings.PlacementClearance))
		{
			return false;
		}

		FBattlefieldGroundResult Ground;
		if (not TryProjectToLandscape(World, Settings, FVector(Center, 0.0), Ground))
		{
			return false;
		}
		OutPlacement.ActorClass = Asset.ActorClass;
		OutPlacement.LocalBounds = Asset.LocalBounds;
		OutPlacement.Transform = MakeGroundedActorTransform(
			Asset, Ground, UniformScale, YawDegrees, Settings.bAlignActorsToGround);
		OutPlacement.Center = Center;
		OutPlacement.FootprintRadius = Radius;
		OutPlacement.Footprint = Footprint;
		OutPlacement.AreaIndex = AreaIndex;
		OutPlacement.Role = Role;
		OutPlacement.Slice = Slice;
		return true;
	}

	void AddActorPlacement(
		FPlacedBattlefieldActor&& Placement,
		TArray<FPlacedBattlefieldActor>& OutActors,
		TArray<FBattlefieldFootprint>& InOutOccupied)
	{
		InOutOccupied.Add(Placement.Footprint);
		OutActors.Add(MoveTemp(Placement));
	}

	int32 GetRandomCount(const int32 First, const int32 Second, FRandomStream& Random)
	{
		const int32 Minimum = FMath::Max(0, FMath::Min(First, Second));
		const int32 Maximum = FMath::Max(Minimum, FMath::Max(First, Second));
		return Random.RandRange(Minimum, Maximum);
	}

	double GetBoundaryForwardYawDegrees(const FBattlefieldBoundary& Boundary)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Boundary.Forward.Y, Boundary.Forward.X));
	}

	void PlaceScatteredActors(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedBattlefieldActor>& Assets,
		const FVector2D& SliceRange,
		const int32 MinimumCount,
		const int32 MaximumCount,
		const double Spread,
		const double BaseYawDegrees,
		const int32 AreaIndex,
		const EBattlefieldPlacementRole Role,
		const EBattlefieldSlice Slice,
		FRandomStream& Random,
		TArray<FPlacedBattlefieldActor>& OutActors,
		TArray<FBattlefieldFootprint>& InOutOccupied)
	{
		if (Assets.IsEmpty())
		{
			return;
		}
		const int32 DesiredCount = GetRandomCount(MinimumCount, MaximumCount, Random);
		for (int32 ItemIndex = 0; ItemIndex < DesiredCount; ++ItemIndex)
		{
			for (int32 Attempt = 0; Attempt < FMath::Max(1, Settings.PlacementAttemptsPerItem); ++Attempt)
			{
				const int32 AssetIndex = PickWeightedAssetIndex(Assets, Random);
				const FResolvedBattlefieldActor& Asset = Assets[AssetIndex];
				const double Scale = Random.FRandRange(static_cast<float>(Asset.MinScale), static_cast<float>(Asset.MaxScale));
				const FVector2D LocalPosition = MakeRandomLocalPoint(
					Boundary, SliceRange, Spread, Settings.SliceEdgePadding, Random);
				FPlacedBattlefieldActor Placement;
				if (TryMakeActorPlacement(World, Settings, Boundary, Exclusions, Asset,
					LocalToWorld(Boundary, LocalPosition), Scale, BaseYawDegrees, AreaIndex,
					Role, Slice, InOutOccupied, Placement))
				{
					AddActorPlacement(MoveTemp(Placement), OutActors, InOutOccupied);
					break;
				}
			}
		}
	}

	double GetLineLength(const FResolvedBattlefieldActor& Asset)
	{
		const double AbsoluteYaw = FMath::Abs(FMath::Fmod(Asset.AuthoredLengthYawDegrees, 180.0));
		return FMath::IsNearlyEqual(AbsoluteYaw, 90.0) ? Asset.FootprintSize.Y : Asset.FootprintSize.X;
	}

	bool TryBuildBarbedWireRun(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FResolvedBattlefieldActor& Asset,
		const int32 PieceCount,
		const int32 AreaIndex,
		FRandomStream& Random,
		const TArray<FBattlefieldFootprint>& Occupied,
		TArray<FPlacedBattlefieldActor>& OutRun)
	{
		const FVector2D AnchorLocal = MakeRandomLocalPoint(
			Boundary, Boundary.NoMansLandRange, 1.0, Settings.SliceEdgePadding, Random);
		const double BoundaryYaw = GetBoundaryForwardYawDegrees(Boundary);
		const double RunYaw = BoundaryYaw + 90.0 + Random.FRandRange(
			-Settings.BarbedWireMaxRunAngleDegrees, Settings.BarbedWireMaxRunAngleDegrees);
		const FVector2D RunDirection(FMath::Cos(FMath::DegreesToRadians(RunYaw)),
			FMath::Sin(FMath::DegreesToRadians(RunYaw)));
		const double Scale = Random.FRandRange(static_cast<float>(Asset.MinScale), static_cast<float>(Asset.MaxScale));
		const double PieceGap = FMath::Max(Settings.BarbedWirePieceGap, Settings.PlacementClearance);
		const double Step = FMath::Max(1.0, GetLineLength(Asset) * Scale + PieceGap);
		TArray<FBattlefieldFootprint> StagedOccupied = Occupied;
		for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
		{
			const double Offset = (PieceIndex - 0.5 * (PieceCount - 1)) * Step;
			const FVector2D Center = LocalToWorld(Boundary, AnchorLocal) + RunDirection * Offset;
			FPlacedBattlefieldActor Placement;
			const double ActorYaw = RunYaw - Asset.AuthoredLengthYawDegrees;
			if (not TryMakeActorPlacement(World, Settings, Boundary, Exclusions, Asset, Center,
				Scale, ActorYaw, AreaIndex, EBattlefieldPlacementRole::BarbedWire,
				EBattlefieldSlice::NoMansLand, StagedOccupied, Placement))
			{
				return false;
			}
			StagedOccupied.Add(Placement.Footprint);
			OutRun.Add(MoveTemp(Placement));
		}
		return true;
	}

	void PlaceBarbedWireRuns(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedBattlefieldActor>& Assets,
		const int32 AreaIndex,
		FRandomStream& Random,
		TArray<FPlacedBattlefieldActor>& OutActors,
		TArray<FBattlefieldFootprint>& InOutOccupied)
	{
		if (Assets.IsEmpty())
		{
			return;
		}
		const int32 RunCount = GetRandomCount(
			Settings.MinBarbedWireRunsPerArea, Settings.MaxBarbedWireRunsPerArea, Random);
		for (int32 RunIndex = 0; RunIndex < RunCount; ++RunIndex)
		{
			const int32 PieceCount = FMath::Max(1, GetRandomCount(
				Settings.MinBarbedWirePiecesPerRun, Settings.MaxBarbedWirePiecesPerRun, Random));
			for (int32 Attempt = 0; Attempt < FMath::Max(1, Settings.PlacementAttemptsPerItem); ++Attempt)
			{
				const int32 AssetIndex = PickWeightedAssetIndex(Assets, Random);
				TArray<FPlacedBattlefieldActor> Run;
				if (not TryBuildBarbedWireRun(World, Settings, Boundary, Exclusions, Assets[AssetIndex],
					PieceCount, AreaIndex, Random, InOutOccupied, Run))
				{
					continue;
				}
				for (FPlacedBattlefieldActor& Placement : Run)
				{
					AddActorPlacement(MoveTemp(Placement), OutActors, InOutOccupied);
				}
				break;
			}
		}
	}

	bool TryBuildHedgehogGroup(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedBattlefieldActor>& Assets,
		const int32 HedgehogCount,
		const int32 AreaIndex,
		FRandomStream& Random,
		const TArray<FBattlefieldFootprint>& Occupied,
		TArray<FPlacedBattlefieldActor>& OutGroup)
	{
		const FVector2D AnchorLocal = MakeRandomLocalPoint(
			Boundary, Boundary.NoMansLandRange, 1.0, Settings.SliceEdgePadding, Random);
		TArray<FBattlefieldFootprint> StagedOccupied = Occupied;
		TArray<FVector2D> GroupCenters;
		for (int32 HedgehogIndex = 0; HedgehogIndex < HedgehogCount; ++HedgehogIndex)
		{
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < FMath::Max(1, Settings.PlacementAttemptsPerItem); ++Attempt)
			{
				const FVector2D Parent = GroupCenters.IsEmpty()
					? LocalToWorld(Boundary, AnchorLocal)
					: GroupCenters[Random.RandRange(0, GroupCenters.Num() - 1)];
				const double Distance = GroupCenters.IsEmpty() ? 0.0 : Random.FRandRange(
					FMath::Min(Settings.MinHedgehogSpacing, Settings.MaxHedgehogSpacing),
					FMath::Max(Settings.MinHedgehogSpacing, Settings.MaxHedgehogSpacing));
				const double Angle = Random.FRandRange(0.0f, static_cast<float>(2.0 * PI));
				const FVector2D Center = Parent + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
				const FResolvedBattlefieldActor& Asset = Assets[PickWeightedAssetIndex(Assets, Random)];
				const double Scale = Random.FRandRange(static_cast<float>(Asset.MinScale), static_cast<float>(Asset.MaxScale));
				FPlacedBattlefieldActor Placement;
				if (TryMakeActorPlacement(World, Settings, Boundary, Exclusions, Asset, Center, Scale,
					Random.FRandRange(0.0f, static_cast<float>(BattlefieldAreaPCGConstants::FullCircleDegrees)),
					AreaIndex, EBattlefieldPlacementRole::Hedgehog, EBattlefieldSlice::NoMansLand,
					StagedOccupied, Placement))
				{
					StagedOccupied.Add(Placement.Footprint);
					GroupCenters.Add(Placement.Center);
					OutGroup.Add(MoveTemp(Placement));
					bPlaced = true;
					break;
				}
			}
			if (not bPlaced)
			{
				return false;
			}
		}
		return true;
	}

	void PlaceHedgehogGroups(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedBattlefieldActor>& Assets,
		const int32 AreaIndex,
		FRandomStream& Random,
		TArray<FPlacedBattlefieldActor>& OutActors,
		TArray<FBattlefieldFootprint>& InOutOccupied)
	{
		if (Assets.IsEmpty())
		{
			return;
		}
		const int32 GroupCount = GetRandomCount(
			Settings.MinHedgehogGroupsPerArea, Settings.MaxHedgehogGroupsPerArea, Random);
		for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			const int32 HedgehogCount = FMath::Max(1, GetRandomCount(
				Settings.MinHedgehogsPerGroup, Settings.MaxHedgehogsPerGroup, Random));
			for (int32 Attempt = 0; Attempt < FMath::Max(1, Settings.PlacementAttemptsPerItem); ++Attempt)
			{
				TArray<FPlacedBattlefieldActor> Group;
				if (not TryBuildHedgehogGroup(World, Settings, Boundary, Exclusions, Assets,
					HedgehogCount, AreaIndex, Random, InOutOccupied, Group))
				{
					continue;
				}
				for (FPlacedBattlefieldActor& Placement : Group)
				{
					AddActorPlacement(MoveTemp(Placement), OutActors, InOutOccupied);
				}
				break;
			}
		}
	}

	void PlaceDecals(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedBattlefieldDecal>& Assets,
		const int32 AreaIndex,
		FRandomStream& Random,
		TArray<FBattlefieldDecalSpawn>& OutDecals,
		TArray<FBattlefieldFootprint>& InOutOccupied)
	{
		if (Assets.IsEmpty())
		{
			return;
		}
		const int32 DesiredCount = GetRandomCount(Settings.MinDecalsPerArea, Settings.MaxDecalsPerArea, Random);
		const FVector2D WholeAreaRange(-0.5 * Boundary.Length, 0.5 * Boundary.Length);
		for (int32 DecalIndex = 0; DecalIndex < DesiredCount; ++DecalIndex)
		{
			for (int32 Attempt = 0; Attempt < FMath::Max(1, Settings.PlacementAttemptsPerItem); ++Attempt)
			{
				const FResolvedBattlefieldDecal& Asset = Assets[PickWeightedAssetIndex(Assets, Random)];
				const FVector2D Size = Asset.DefaultSize * FMath::Max(0.01f, Settings.GlobalDecalSizeMultiplier);
				const double Radius = 0.5 * Size.Size();
				const FVector2D LocalPosition = MakeRandomLocalPoint(Boundary, WholeAreaRange, 1.0, 0.0, Random);
				const FVector2D Center = LocalToWorld(Boundary, LocalPosition);
				FBattlefieldFootprint Footprint;
				Footprint.Center = Center;
				Footprint.HalfExtents = 0.5 * Size;
				Footprint.YawRadians = Random.FRandRange(0.0f, static_cast<float>(2.0 * PI));
				if (not IsDiskInsideBoundary(Boundary, Center, Radius)
					|| IsExcluded(Exclusions, FVector(Center, Boundary.Center.Z))
					|| not IsPlacementClear(Footprint, InOutOccupied, Settings.PlacementClearance))
				{
					continue;
				}
				FBattlefieldGroundResult Ground;
				if (not TryProjectToLandscape(World, Settings, FVector(Center, 0.0), Ground))
				{
					continue;
				}
				FBattlefieldDecalSpawn& Decal = OutDecals.Emplace_GetRef();
				Decal.Material = Asset.Material;
				Decal.Position = Ground.Position + Ground.Normal * Asset.SurfaceOffset;
				Decal.Normal = Ground.Normal;
				Decal.Size = Size;
				Decal.ProjectionDepth = Asset.ProjectionDepth;
				Decal.YawRadians = Footprint.YawRadians;
				Decal.FootprintRadius = Radius;
				Decal.Footprint = Footprint;
				Decal.AreaIndex = AreaIndex;
				InOutOccupied.Add(Footprint);
				break;
			}
		}
	}

	struct FResolvedBattlefieldAssets
	{
		TArray<FResolvedBattlefieldActor> SovietWeaponry;
		TArray<FResolvedBattlefieldActor> GermanWeaponry;
		TArray<FResolvedBattlefieldActor> BarbedWire;
		TArray<FResolvedBattlefieldActor> Hedgehogs;
		TArray<FResolvedBattlefieldActor> SovietSpoils;
		TArray<FResolvedBattlefieldActor> GermanSpoils;
		TArray<FResolvedBattlefieldActor> Trees;
		TArray<FResolvedBattlefieldDecal> Decals;
	};

	void ResolveAllAssets(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		FResolvedBattlefieldAssets& OutAssets)
	{
		ResolveActorEntries(Context, World, Settings.SovietWeaponry,
			LOCTEXT("SovietWeaponRole", "Soviet weaponry"), OutAssets.SovietWeaponry);
		ResolveActorEntries(Context, World, Settings.GermanWeaponry,
			LOCTEXT("GermanWeaponRole", "German weaponry"), OutAssets.GermanWeaponry);
		ResolveLineActorEntries(Context, World, Settings.BarbedWire, OutAssets.BarbedWire);
		ResolveActorEntries(Context, World, Settings.Hedgehogs,
			LOCTEXT("HedgehogRole", "hedgehog"), OutAssets.Hedgehogs);
		ResolveActorEntries(Context, World, Settings.SovietSpoilsOfWar,
			LOCTEXT("SovietSpoilsRole", "Soviet spoils-of-war"), OutAssets.SovietSpoils);
		ResolveActorEntries(Context, World, Settings.GermanSpoilsOfWar,
			LOCTEXT("GermanSpoilsRole", "German spoils-of-war"), OutAssets.GermanSpoils);
		ResolveActorEntries(Context, World, Settings.Trees,
			LOCTEXT("TreeRole", "tree"), OutAssets.Trees);
		ResolveDecalEntries(Context, Settings.Decals, OutAssets.Decals);
	}

	void PlaceFactionSections(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FResolvedBattlefieldAssets& Assets,
		const int32 AreaIndex,
		FRandomStream& Random,
		TArray<FPlacedBattlefieldActor>& OutActors,
		TArray<FBattlefieldFootprint>& InOutOccupied)
	{
		const double SovietFacingYaw = GetBoundaryForwardYawDegrees(Boundary);
		const double GermanFacingYaw = SovietFacingYaw + 180.0;
		PlaceScatteredActors(World, Settings, Boundary, Exclusions, Assets.SovietWeaponry,
			Boundary.SovietRange, Settings.MinSovietWeaponryPerArea, Settings.MaxSovietWeaponryPerArea,
			Settings.SovietWeaponrySpread, SovietFacingYaw, AreaIndex,
			EBattlefieldPlacementRole::SovietWeaponry, EBattlefieldSlice::Soviet,
			Random, OutActors, InOutOccupied);
		PlaceScatteredActors(World, Settings, Boundary, Exclusions, Assets.GermanWeaponry,
			Boundary.GermanRange, Settings.MinGermanWeaponryPerArea, Settings.MaxGermanWeaponryPerArea,
			Settings.GermanWeaponrySpread, GermanFacingYaw, AreaIndex,
			EBattlefieldPlacementRole::GermanWeaponry, EBattlefieldSlice::German,
			Random, OutActors, InOutOccupied);
	}

	void PlaceSpoilsAndTrees(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const FBattlefieldBoundary& Boundary,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FResolvedBattlefieldAssets& Assets,
		const int32 AreaIndex,
		FRandomStream& Random,
		TArray<FPlacedBattlefieldActor>& OutActors,
		TArray<FBattlefieldFootprint>& InOutOccupied)
	{
		const double RandomYaw = Random.FRandRange(0.0f,
			static_cast<float>(BattlefieldAreaPCGConstants::FullCircleDegrees));
		PlaceScatteredActors(World, Settings, Boundary, Exclusions, Assets.SovietSpoils,
			Boundary.SovietRange, Settings.MinSovietSpoilsPerArea, Settings.MaxSovietSpoilsPerArea,
			1.0, RandomYaw, AreaIndex, EBattlefieldPlacementRole::SovietSpoils,
			EBattlefieldSlice::Soviet, Random, OutActors, InOutOccupied);
		PlaceScatteredActors(World, Settings, Boundary, Exclusions, Assets.GermanSpoils,
			Boundary.GermanRange, Settings.MinGermanSpoilsPerArea, Settings.MaxGermanSpoilsPerArea,
			1.0, RandomYaw, AreaIndex, EBattlefieldPlacementRole::GermanSpoils,
			EBattlefieldSlice::German, Random, OutActors, InOutOccupied);
		const FVector2D WholeAreaRange(-0.5 * Boundary.Length, 0.5 * Boundary.Length);
		PlaceScatteredActors(World, Settings, Boundary, Exclusions, Assets.Trees, WholeAreaRange,
			Settings.MinTreesPerArea, Settings.MaxTreesPerArea, 1.0, RandomYaw, AreaIndex,
			EBattlefieldPlacementRole::Tree, EBattlefieldSlice::Global,
			Random, OutActors, InOutOccupied);
	}

	void GenerateAreaContents(
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FResolvedBattlefieldAssets& Assets,
		const int32 AreaIndex,
		FRandomStream& Random,
		FGeneratedBattlefieldArea& OutArea)
	{
		TArray<FBattlefieldFootprint> Occupied;
		PlaceFactionSections(World, Settings, OutArea.Boundary, Exclusions, Assets,
			AreaIndex, Random, OutArea.Actors, Occupied);
		PlaceBarbedWireRuns(World, Settings, OutArea.Boundary, Exclusions, Assets.BarbedWire,
			AreaIndex, Random, OutArea.Actors, Occupied);
		PlaceHedgehogGroups(World, Settings, OutArea.Boundary, Exclusions, Assets.Hedgehogs,
			AreaIndex, Random, OutArea.Actors, Occupied);
		PlaceSpoilsAndTrees(World, Settings, OutArea.Boundary, Exclusions, Assets,
			AreaIndex, Random, OutArea.Actors, Occupied);
		PlaceDecals(World, Settings, OutArea.Boundary, Exclusions, Assets.Decals,
			AreaIndex, Random, OutArea.Decals, Occupied);
	}

	void ShuffleCandidates(TArray<FBattlefieldCandidate>& Candidates, FRandomStream& Random)
	{
		for (int32 CandidateIndex = Candidates.Num() - 1; CandidateIndex > 0; --CandidateIndex)
		{
			Candidates.Swap(CandidateIndex, Random.RandRange(0, CandidateIndex));
		}
	}

	void GenerateAreas(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateBattlefieldAreaSettings& Settings,
		TArray<FBattlefieldCandidate> Candidates,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FResolvedBattlefieldAssets& Assets,
		FRandomStream& Random,
		TArray<FGeneratedBattlefieldArea>& OutAreas)
	{
		const int32 MinimumAreas = FMath::Max(0, FMath::Min(Settings.MinAreasToCreate, Settings.MaxAreasToCreate));
		const int32 MaximumAreas = FMath::Max(MinimumAreas,
			FMath::Max(Settings.MinAreasToCreate, Settings.MaxAreasToCreate));
		const int32 DesiredAreas = Random.RandRange(MinimumAreas, MaximumAreas);
		ShuffleCandidates(Candidates, Random);
		for (const FBattlefieldCandidate& Candidate : Candidates)
		{
			if (OutAreas.Num() >= DesiredAreas)
			{
				break;
			}
			FGeneratedBattlefieldArea Area;
			CreateBoundary(Settings, Candidate, Random, Area.Boundary);
			if (not IsBoundaryClearOfAreas(Area.Boundary, OutAreas, Settings.AreaClearance)
				|| not IsBoundaryClearOfExclusions(Area.Boundary, Exclusions, Settings.ExclusionSampleSpacing))
			{
				continue;
			}
			GenerateAreaContents(World, Settings, Exclusions, Assets, OutAreas.Num(), Random, Area);
			OutAreas.Add(MoveTemp(Area));
		}

		if (OutAreas.Num() < MinimumAreas)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("BelowMinimumAreas",
					"CreateBattlefieldArea: created {0} areas, below the configured minimum {1}; remaining "
					"candidates overlapped exclusions, other areas, or invalid terrain."),
				FText::AsNumber(OutAreas.Num()), FText::AsNumber(MinimumAreas)));
		}
	}

	AActor* SpawnAreaSplineActor(
		UWorld& World,
		const FBattlefieldBoundary& Boundary,
		const int32 AreaIndex)
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
		SplineActor->SetActorLabel(FString::Printf(TEXT("PCG_BattlefieldAreaSpline_%d"), AreaIndex));
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

	AActor* SpawnBattlefieldActor(UWorld& World, const FPlacedBattlefieldActor& Placement)
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
		Actor->SetActorLabel(FString::Printf(TEXT("PCG_Battlefield_%s_%d"),
			*PlacementRoleToName(Placement.Role).ToString(), Placement.AreaIndex));
#endif
		return Actor;
	}

	AActor* SpawnDecalAnchor(
		UWorld& World,
		const TArray<FBattlefieldDecalSpawn>& Decals,
		const int32 AreaIndex)
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
		Anchor->SetActorLabel(FString::Printf(TEXT("PCG_BattlefieldDecals_%d"), AreaIndex));
#endif
		USceneComponent* Root = NewObject<USceneComponent>(Anchor);
		Anchor->SetRootComponent(Root);
		Anchor->AddInstanceComponent(Root);
		Root->RegisterComponent();
		for (const FBattlefieldDecalSpawn& Decal : Decals)
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

	void SpawnAndRegisterAreas(
		UPCGComponent& SourceComponent,
		UWorld& World,
		const TArray<FGeneratedBattlefieldArea>& Areas)
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(&SourceComponent);
		for (int32 AreaIndex = 0; AreaIndex < Areas.Num(); ++AreaIndex)
		{
			const FGeneratedBattlefieldArea& Area = Areas[AreaIndex];
			if (AActor* SplineActor = SpawnAreaSplineActor(World, Area.Boundary, AreaIndex))
			{
				ManagedActors->GeneratedActors.Add(SplineActor);
			}
			for (const FPlacedBattlefieldActor& Placement : Area.Actors)
			{
				if (AActor* Actor = SpawnBattlefieldActor(World, Placement))
				{
					ManagedActors->GeneratedActors.Add(Actor);
				}
			}
			if (AActor* DecalAnchor = SpawnDecalAnchor(World, Area.Decals, AreaIndex))
			{
				ManagedActors->GeneratedActors.Add(DecalAnchor);
			}
		}
		if (not ManagedActors->GeneratedActors.IsEmpty())
		{
			SourceComponent.AddToManagedResources(ManagedActors);
		}
	}

	UPCGPointData* CreateOutputPointData(FPCGContext* Context, const FName PinLabel)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		PointData->InitializeFromData(nullptr);
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = PinLabel;
		return PointData;
	}

	void EmitAreaBounds(
		FPCGContext* Context,
		const TArray<FGeneratedBattlefieldArea>& Areas,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateOutputPointData(Context, BattlefieldAreaPCGConstants::AreaBoundsPinLabel);
		FPCGMetadataAttribute<int32>* AreaIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			BattlefieldAreaPCGConstants::AreaIndexAttributeName, INDEX_NONE, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Areas.Num());
		for (int32 AreaIndex = 0; AreaIndex < Areas.Num(); ++AreaIndex)
		{
			const FBattlefieldBoundary& Boundary = Areas[AreaIndex].Boundary;
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(Boundary.Center);
			Point.BoundsMin = FVector(Boundary.Bounds.Min - FVector2D(Boundary.Center), 0.0);
			Point.BoundsMax = FVector(Boundary.Bounds.Max - FVector2D(Boundary.Center), 0.0);
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, AreaIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			AreaIndexAttribute->SetValue(Point.MetadataEntry, AreaIndex);
		}
	}

	void EmitActorPlacement(
		UPCGPointData& PointData,
		FPCGMetadataAttribute<int32>& AreaIndexAttribute,
		FPCGMetadataAttribute<FName>& RoleAttribute,
		FPCGMetadataAttribute<FName>& SliceAttribute,
		const FPlacedBattlefieldActor& Placement,
		const int32 Seed)
	{
		FPCGPoint& Point = PointData.GetMutablePoints().Emplace_GetRef();
		Point.Transform = Placement.Transform;
		Point.BoundsMin = Placement.LocalBounds.Min;
		Point.BoundsMax = Placement.LocalBounds.Max;
		Point.Density = 1.0f;
		Point.Seed = Seed;
		PointData.Metadata->InitializeOnSet(Point.MetadataEntry);
		AreaIndexAttribute.SetValue(Point.MetadataEntry, Placement.AreaIndex);
		RoleAttribute.SetValue(Point.MetadataEntry, PlacementRoleToName(Placement.Role));
		SliceAttribute.SetValue(Point.MetadataEntry, SliceToName(Placement.Slice));
	}

	void EmitDecalPlacement(
		UPCGPointData& PointData,
		FPCGMetadataAttribute<int32>& AreaIndexAttribute,
		FPCGMetadataAttribute<FName>& RoleAttribute,
		FPCGMetadataAttribute<FName>& SliceAttribute,
		const FBattlefieldDecalSpawn& Decal,
		const int32 Seed)
	{
		FPCGPoint& Point = PointData.GetMutablePoints().Emplace_GetRef();
		Point.Transform = FTransform(FRotator(0.0, FMath::RadiansToDegrees(Decal.YawRadians), 0.0), Decal.Position);
		Point.BoundsMin = FVector(-0.5 * Decal.Size.X, -0.5 * Decal.Size.Y, 0.0);
		Point.BoundsMax = FVector(0.5 * Decal.Size.X, 0.5 * Decal.Size.Y, Decal.ProjectionDepth);
		Point.Density = 1.0f;
		Point.Seed = Seed;
		PointData.Metadata->InitializeOnSet(Point.MetadataEntry);
		AreaIndexAttribute.SetValue(Point.MetadataEntry, Decal.AreaIndex);
		RoleAttribute.SetValue(Point.MetadataEntry, PlacementRoleToName(EBattlefieldPlacementRole::Decal));
		SliceAttribute.SetValue(Point.MetadataEntry, SliceToName(EBattlefieldSlice::Global));
	}

	void EmitPlacements(
		FPCGContext* Context,
		const TArray<FGeneratedBattlefieldArea>& Areas,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateOutputPointData(Context, BattlefieldAreaPCGConstants::PlacementsPinLabel);
		FPCGMetadataAttribute<int32>* AreaIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			BattlefieldAreaPCGConstants::AreaIndexAttributeName, INDEX_NONE, false, false);
		FPCGMetadataAttribute<FName>* RoleAttribute = PointData->Metadata->CreateAttribute<FName>(
			BattlefieldAreaPCGConstants::PlacementRoleAttributeName, NAME_None, false, false);
		FPCGMetadataAttribute<FName>* SliceAttribute = PointData->Metadata->CreateAttribute<FName>(
			BattlefieldAreaPCGConstants::SliceAttributeName, NAME_None, false, false);
		int32 PlacementIndex = 0;
		for (const FGeneratedBattlefieldArea& Area : Areas)
		{
			for (const FPlacedBattlefieldActor& Placement : Area.Actors)
			{
				EmitActorPlacement(*PointData, *AreaIndexAttribute, *RoleAttribute, *SliceAttribute,
					Placement, PCGHelpers::ComputeSeed(RandomSeed, PlacementIndex++));
			}
			for (const FBattlefieldDecalSpawn& Decal : Area.Decals)
			{
				EmitDecalPlacement(*PointData, *AreaIndexAttribute, *RoleAttribute, *SliceAttribute,
					Decal, PCGHelpers::ComputeSeed(RandomSeed, PlacementIndex++));
			}
		}
	}
}

bool FPCGCreateBattlefieldAreaElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	using namespace BattlefieldAreaPCGInternal;

	const UPCGCreateBattlefieldAreaSettings* Settings =
		Context->GetInputSettings<UPCGCreateBattlefieldAreaSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	TArray<FBattlefieldCandidate> Candidates = CollectCandidates(Context);
	if (Candidates.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("NoCandidates", "CreateBattlefieldArea: the In pin contains no candidate points."));
		EmitAreaBounds(Context, {}, Settings->RandomSeed);
		EmitPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	FResolvedBattlefieldAssets Assets;
	ResolveAllAssets(Context, World, *Settings, Assets);
	FRandomStream Random(Settings->RandomSeed);
	TArray<FGeneratedBattlefieldArea> Areas;
	GenerateAreas(Context, World, *Settings, MoveTemp(Candidates), CollectExclusions(Context),
		Assets, Random, Areas);
	SpawnAndRegisterAreas(*SourceComponent, World, Areas);
	EmitAreaBounds(Context, Areas, Settings->RandomSeed);
	EmitPlacements(Context, Areas, Settings->RandomSeed);
	return true;
}

#undef LOCTEXT_NAMESPACE
