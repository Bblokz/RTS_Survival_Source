// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreateFuelSupply.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPolyLineData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGSplineInteriorSurfaceData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"
#include "Components/SplineComponent.h"
#include "RTS_Survival/Environment/Splines/DestructibleSpline/DestructibleSplineActor.h"

#define LOCTEXT_NAMESPACE "PCGCreateFuelSupply"

namespace FuelSupplyPCGConstants
{
	const FName ExclusionPinLabel = TEXT("Exclusion");
	const FName OccupiedBoundsPinLabel = TEXT("OccupiedBounds");
	const FName FacilityAreaPinLabel = TEXT("FacilityArea");
	const FName PipeRoutesPinLabel = TEXT("PipeRoutes");
	const FName RoleAttributeName = TEXT("FuelSupplyRole");
	const FName NetworkAttributeName = TEXT("NetworkIndex");

	constexpr double InspectionDepth = -100000.0;
	constexpr double GroundTraceUp = 15000.0;
	constexpr double GroundTraceDown = 40000.0;
	constexpr double SpatialSampleHalfExtent = 5.0;
	constexpr double MinimumUsableBoundsSize = 1.0;
	constexpr double DirectionTolerance = 0.001;
	constexpr double MinimumRouteSegmentLength = 10.0;
	constexpr double RightAngleDegrees = 90.0;
	constexpr double FullCircleDegrees = 360.0;
	constexpr double MinimumRouteGridSize = 100.0;
	constexpr double FootprintSampleSpacing = 200.0;
	constexpr double SplineAreaSampleSpacing = 500.0;
	constexpr double MinimumPipeSplineSampleSpacing = 1.0;
	constexpr double PipeBoundsHalfHeight = 5.0;
	constexpr double CurvinessReductionDivisor = 4.0;
	constexpr int32 MinimumRouteSearchCells = 100;
	constexpr int32 MaximumFootprintSamplesPerAxis = 32;
	constexpr int32 MinimumSplineSegmentSamples = 2;
	constexpr int32 MaximumSplineSegmentSamples = 128;
	constexpr int32 MaximumLoopCandidatesPerDepot = 2;
	constexpr int32 CoordinateAxisCount = 2;
	constexpr int32 FenceSideCount = 4;
	constexpr int32 PipeCurveSafetyAttempts = 5;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FName UPCGCreateFuelSupplySettings::GetDefaultNodeName() const
{
	return FName(TEXT("CreateFuelSupply"));
}

FText UPCGCreateFuelSupplySettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Fuel Supply Network");
}

FText UPCGCreateFuelSupplySettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Creates bounds-aware supply depots and fuel tanks, routes designer-authored destructible spline pipes "
		"around exclusions and each other, and encloses tanks with standalone Blueprint fences. Fence openings "
		"are derived from the generated pipe crossings. FacilityArea provides one continuous exclusion "
		"surface covering the complete generated facility. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGCreateFuelSupplySettings::CreateElement() const
{
	return MakeShared<FPCGCreateFuelSupplyElement>();
}

TArray<FPCGPinProperties> UPCGCreateFuelSupplySettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	Pins.Emplace(FuelSupplyPCGConstants::ExclusionPinLabel, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreateFuelSupplySettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(FuelSupplyPCGConstants::OccupiedBoundsPinLabel, EPCGDataType::Point);
	Pins.Emplace(FuelSupplyPCGConstants::FacilityAreaPinLabel, EPCGDataType::Surface);
	Pins.Emplace(FuelSupplyPCGConstants::PipeRoutesPinLabel, EPCGDataType::Point);
	return Pins;
}

namespace
{
	using FFuelSupplyAreaFunction = TFunction<bool(const FVector&)>;

	enum class EFuelPlacedRole : uint8
	{
		SupplyDepot,
		FuelTank,
		FuelPipe,
		FuelFence
	};

	struct FResolvedFuelActor
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		double Weight = 1.0;
		double BoundsClearance = 0.0;
		double ZOffset = 0.0;
		double YawOffsetDegrees = 0.0;
		int32 PipeConnections = 0;
	};

	struct FResolvedFuelFence
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		EFuelCardinalDirection AuthoredForwardDirection = EFuelCardinalDirection::PositiveX;
		double Weight = 1.0;
		double EndOverlap = 0.0;
		double ZOffset = 0.0;
	};

	struct FPlacedFuelActor
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		FTransform Transform = FTransform::Identity;
		FBox2D Footprint = FBox2D(EForceInit::ForceInit);
		FVector2D VisualCenter = FVector2D::ZeroVector;
		EFuelPlacedRole Role = EFuelPlacedRole::SupplyDepot;
		int32 FenceOwnerTankIndex = INDEX_NONE;
		int32 PipeConnections = 0;
	};

	struct FFuelNetworkEdge
	{
		int32 StartIndex = INDEX_NONE;
		int32 EndIndex = INDEX_NONE;
		double StartLaneOffset = 0.0;
		double EndLaneOffset = 0.0;
	};

	struct FGeneratedFuelPipeSpline
	{
		UClass* ActorClass = nullptr;
		TArray<FVector> WorldPoints;
		TArray<FVector> WorldTangents;
	};

	struct FFuelGroundResult
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		bool bHitGround = false;
	};

	struct FFuelRouteOpenNode
	{
		int32 CellIndex = INDEX_NONE;
		double Score = 0.0;
	};

	struct FFuelTankFenceGroup
	{
		FBox2D DesiredBounds = FBox2D(EForceInit::ForceInit);
		int32 OwnerTankIndex = INDEX_NONE;
	};

	FName RoleToName(const EFuelPlacedRole Role)
	{
		switch (Role)
		{
		case EFuelPlacedRole::SupplyDepot:
			return TEXT("SupplyDepot");
		case EFuelPlacedRole::FuelTank:
			return TEXT("FuelTank");
		case EFuelPlacedRole::FuelPipe:
			return TEXT("FuelPipe");
		case EFuelPlacedRole::FuelFence:
			return TEXT("FuelFence");
		default:
			return NAME_None;
		}
	}

	AActor* SpawnInspectionActor(UWorld& World, UClass& ActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		return World.SpawnActor<AActor>(
			&ActorClass, FTransform(FVector(0.0, 0.0, FuelSupplyPCGConstants::InspectionDepth)), SpawnParameters);
	}

	void ApplyFootprintOverrides(FBox& InOutLocalBounds, const double OverrideX, const double OverrideY)
	{
		const FVector MeasuredSize = InOutLocalBounds.GetSize();
		if (OverrideX > 0.0 && MeasuredSize.X >= FuelSupplyPCGConstants::MinimumUsableBoundsSize)
		{
			const double PivotRelativeScaleX = OverrideX / MeasuredSize.X;
			InOutLocalBounds.Min.X *= PivotRelativeScaleX;
			InOutLocalBounds.Max.X *= PivotRelativeScaleX;
		}
		if (OverrideY > 0.0 && MeasuredSize.Y >= FuelSupplyPCGConstants::MinimumUsableBoundsSize)
		{
			const double PivotRelativeScaleY = OverrideY / MeasuredSize.Y;
			InOutLocalBounds.Min.Y *= PivotRelativeScaleY;
			InOutLocalBounds.Max.Y *= PivotRelativeScaleY;
		}
	}

	bool TryMeasureActorClass(
		UWorld& World,
		UClass& ActorClass,
		const double FootprintOverrideX,
		const double FootprintOverrideY,
		FBox& OutLocalBounds)
	{
		AActor* InspectionActor = SpawnInspectionActor(World, ActorClass);
		if (not IsValid(InspectionActor))
		{
			return false;
		}

		OutLocalBounds = InspectionActor->GetComponentsBoundingBox(true, true).ShiftBy(-InspectionActor->GetActorLocation());
		InspectionActor->Destroy();
		if (not OutLocalBounds.IsValid)
		{
			return false;
		}

		ApplyFootprintOverrides(OutLocalBounds, FootprintOverrideX, FootprintOverrideY);
		const FVector BoundsSize = OutLocalBounds.GetSize();
		return BoundsSize.X >= FuelSupplyPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Y >= FuelSupplyPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Z >= FuelSupplyPCGConstants::MinimumUsableBoundsSize;
	}

	FIntPoint CardinalDirectionToVector(const EFuelCardinalDirection Direction)
	{
		switch (Direction)
		{
		case EFuelCardinalDirection::PositiveX:
			return FIntPoint(1, 0);
		case EFuelCardinalDirection::NegativeX:
			return FIntPoint(-1, 0);
		case EFuelCardinalDirection::PositiveY:
			return FIntPoint(0, 1);
		case EFuelCardinalDirection::NegativeY:
			return FIntPoint(0, -1);
		default:
			return FIntPoint::ZeroValue;
		}
	}

	void ResolveActorEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FFuelSupplyActorEntry>& Entries,
		const FText& RoleName,
		TArray<FResolvedFuelActor>& OutAssets)
	{
		for (const FFuelSupplyActorEntry& Entry : Entries)
		{
			UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureActorClass(
					World, *ActorClass, Entry.FootprintOverrideX, Entry.FootprintOverrideY, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
					LOCTEXT("InvalidFuelActor", "CreateFuelSupply: skipped an invalid or bounds-less {0} Blueprint."), RoleName));
				continue;
			}

			FResolvedFuelActor& Asset = OutAssets.Emplace_GetRef();
			Asset.ActorClass = ActorClass;
			Asset.LocalBounds = LocalBounds;
			Asset.Weight = FMath::Max(0.01, static_cast<double>(Entry.Weight));
			Asset.BoundsClearance = FMath::Max(0.0, static_cast<double>(Entry.BoundsClearance));
			Asset.ZOffset = Entry.ZOffset;
			Asset.YawOffsetDegrees = Entry.YawOffsetDegrees;
		}
	}

	void ResolveTankEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FFuelTankActorEntry>& Entries,
		TArray<FResolvedFuelActor>& OutAssets)
	{
		for (const FFuelTankActorEntry& Entry : Entries)
		{
			UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureActorClass(
					World, *ActorClass, Entry.FootprintOverrideX, Entry.FootprintOverrideY, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("InvalidFuelTank",
					"CreateFuelSupply: skipped an invalid or bounds-less fuel-tank Blueprint."));
				continue;
			}

			FResolvedFuelActor& Asset = OutAssets.Emplace_GetRef();
			Asset.ActorClass = ActorClass;
			Asset.LocalBounds = LocalBounds;
			Asset.Weight = FMath::Max(0.01, static_cast<double>(Entry.Weight));
			Asset.BoundsClearance = FMath::Max(0.0, static_cast<double>(Entry.BoundsClearance));
			Asset.ZOffset = Entry.ZOffset;
			Asset.YawOffsetDegrees = Entry.YawOffsetDegrees;
			Asset.PipeConnections = FMath::Max(1, Entry.PipeConnections);
		}
	}

	void ResolveFenceEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FFuelFenceActorEntry>& Entries,
		TArray<FResolvedFuelFence>& OutAssets)
	{
		for (const FFuelFenceActorEntry& Entry : Entries)
		{
			UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureActorClass(
					World, *ActorClass, Entry.FootprintOverrideX, Entry.FootprintOverrideY, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("InvalidFuelFence", "CreateFuelSupply: skipped an invalid or bounds-less fence Blueprint."));
				continue;
			}

			FResolvedFuelFence& Asset = OutAssets.Emplace_GetRef();
			Asset.ActorClass = ActorClass;
			Asset.LocalBounds = LocalBounds;
			Asset.AuthoredForwardDirection = Entry.AuthoredForwardDirection;
			Asset.Weight = FMath::Max(0.01, static_cast<double>(Entry.Weight));
			Asset.EndOverlap = FMath::Max(0.0, static_cast<double>(Entry.EndOverlap));
			Asset.ZOffset = Entry.ZOffset;
		}
	}

	int32 SelectWeightedActor(const TArray<FResolvedFuelActor>& Assets, FRandomStream& Random)
	{
		double TotalWeight = 0.0;
		for (const FResolvedFuelActor& Asset : Assets)
		{
			TotalWeight += Asset.Weight;
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

	bool TryGetArea(FPCGContext* Context, const UPCGSpatialData*& OutAreaData, FBox& OutBounds)
	{
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (SpatialData == nullptr || not SpatialData->GetBounds().IsValid)
			{
				continue;
			}
			OutAreaData = SpatialData;
			OutBounds = SpatialData->GetBounds();
			return true;
		}
		return false;
	}

	TArray<const UPCGSpatialData*> GetExclusionData(FPCGContext* Context)
	{
		TArray<const UPCGSpatialData*> Result;
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetInputsByPin(FuelSupplyPCGConstants::ExclusionPinLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
			{
				Result.Add(SpatialData);
			}
		}
		return Result;
	}

	bool SpatialContains(const UPCGSpatialData& SpatialData, const FVector& Position)
	{
		const FBox SampleBounds(
			FVector(-FuelSupplyPCGConstants::SpatialSampleHalfExtent),
			FVector(FuelSupplyPCGConstants::SpatialSampleHalfExtent));
		FPCGPoint SampledPoint;
		return SpatialData.SamplePoint(FTransform(Position), SampleBounds, SampledPoint, nullptr)
			&& SampledPoint.Density > 0.0f;
	}

	/** @brief Even-odd ray crossing for a spline area sampled into a world-space polygon. */
	bool IsPointInFuelSupplyPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Position)
	{
		bool bInside = false;
		for (int32 CurrentIndex = 0, PreviousIndex = Polygon.Num() - 1;
			CurrentIndex < Polygon.Num();
			PreviousIndex = CurrentIndex++)
		{
			const FVector2D& CurrentVertex = Polygon[CurrentIndex];
			const FVector2D& PreviousVertex = Polygon[PreviousIndex];
			const bool bCrossesRay = (CurrentVertex.Y > Position.Y) != (PreviousVertex.Y > Position.Y);
			if (bCrossesRay && Position.X < (PreviousVertex.X - CurrentVertex.X)
				* (Position.Y - CurrentVertex.Y) / (PreviousVertex.Y - CurrentVertex.Y) + CurrentVertex.X)
			{
				bInside = not bInside;
			}
		}
		return bInside;
	}

	FFuelSupplyAreaFunction BuildFuelSupplyAreaFunction(const UPCGSpatialData& AreaData)
	{
		if (const UPCGPolyLineData* PolyLineData = Cast<UPCGPolyLineData>(&AreaData))
		{
			TArray<FVector2D> Polygon;
			for (int32 SegmentIndex = 0; SegmentIndex < PolyLineData->GetNumSegments(); ++SegmentIndex)
			{
				const double SegmentLength = PolyLineData->GetSegmentLength(SegmentIndex);
				const int32 NumSamples = FMath::Clamp(
					FMath::CeilToInt32(SegmentLength / FuelSupplyPCGConstants::SplineAreaSampleSpacing),
					FuelSupplyPCGConstants::MinimumSplineSegmentSamples,
					FuelSupplyPCGConstants::MaximumSplineSegmentSamples);
				for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
				{
					const double Distance = SegmentLength * SampleIndex / NumSamples;
					const FVector SamplePosition =
						PolyLineData->GetTransformAtDistance(SegmentIndex, Distance, true).GetLocation();
					Polygon.Add(FVector2D(SamplePosition.X, SamplePosition.Y));
				}
			}

			if (Polygon.Num() >= 3)
			{
				return [Polygon = MoveTemp(Polygon)](const FVector& Position)
				{
					return IsPointInFuelSupplyPolygon(Polygon, FVector2D(Position.X, Position.Y));
				};
			}
		}

		const UPCGSpatialData* GenericAreaData = &AreaData;
		return [GenericAreaData](const FVector& Position)
		{
			return SpatialContains(*GenericAreaData, Position);
		};
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

	FFuelGroundResult ProjectToGround(UWorld& World, const FVector& Position, const bool bProject)
	{
		if (not bProject)
		{
			return {Position, FVector::UpVector, false};
		}

		const FVector Start = Position + FVector::UpVector * FuelSupplyPCGConstants::GroundTraceUp;
		const FVector End = Position - FVector::UpVector * FuelSupplyPCGConstants::GroundTraceDown;
		FCollisionQueryParams QueryParameters;
		TArray<FHitResult> Hits;
		World.LineTraceMultiByChannel(Hits, Start, End, ECC_WorldStatic, QueryParameters);
		for (const FHitResult& Hit : Hits)
		{
			const AActor* HitActor = Hit.GetActor();
			if (IsValid(HitActor) && HitActor->IsA<ALandscapeProxy>())
			{
				return {Hit.ImpactPoint, Hit.ImpactNormal, true};
			}
		}
		return {Position, FVector::UpVector, false};
	}

	double MakeGeneratedYawDegrees(FRandomStream& Random, const bool bRightAngles)
	{
		if (bRightAngles)
		{
			return FuelSupplyPCGConstants::RightAngleDegrees * Random.RandRange(0, 3);
		}
		return Random.FRandRange(0.0f, FuelSupplyPCGConstants::FullCircleDegrees);
	}

	FTransform MakeBoundsAlignedTransform(
		const FBox& LocalBounds,
		const FVector& LocalAnchor,
		const FVector2D& DesiredWorldAnchor,
		const double GroundZ,
		const FQuat& Rotation,
		const FVector& Scale,
		const double ZOffset)
	{
		const FVector ScaledLocalAnchor = LocalAnchor * Scale;
		const FVector RotatedAnchor = Rotation.RotateVector(ScaledLocalAnchor);
		double RotatedMinimumZ = TNumericLimits<double>::Max();
		for (int32 XIndex = 0; XIndex < 2; ++XIndex)
		{
			for (int32 YIndex = 0; YIndex < 2; ++YIndex)
			{
				for (int32 ZIndex = 0; ZIndex < 2; ++ZIndex)
				{
					const FVector LocalCorner(
						(XIndex == 0 ? LocalBounds.Min.X : LocalBounds.Max.X) * Scale.X,
						(YIndex == 0 ? LocalBounds.Min.Y : LocalBounds.Max.Y) * Scale.Y,
						(ZIndex == 0 ? LocalBounds.Min.Z : LocalBounds.Max.Z) * Scale.Z);
					RotatedMinimumZ = FMath::Min(RotatedMinimumZ, Rotation.RotateVector(LocalCorner).Z);
				}
			}
		}
		const FVector Location(
			DesiredWorldAnchor.X - RotatedAnchor.X,
			DesiredWorldAnchor.Y - RotatedAnchor.Y,
			GroundZ + ZOffset - RotatedMinimumZ);
		return FTransform(Rotation, Location, Scale);
	}

	FBox2D ComputeFootprint(const FBox& LocalBounds, const FTransform& Transform)
	{
		FBox2D Result(EForceInit::ForceInit);
		for (int32 XIndex = 0; XIndex < 2; ++XIndex)
		{
			for (int32 YIndex = 0; YIndex < 2; ++YIndex)
			{
				const FVector LocalCorner(
					XIndex == 0 ? LocalBounds.Min.X : LocalBounds.Max.X,
					YIndex == 0 ? LocalBounds.Min.Y : LocalBounds.Max.Y,
					LocalBounds.GetCenter().Z);
				const FVector WorldCorner = Transform.TransformPosition(LocalCorner);
				Result += FVector2D(WorldCorner.X, WorldCorner.Y);
			}
		}
		return Result;
	}

	bool BoxesOverlapWithClearance(const FBox2D& First, const FBox2D& Second, const double Clearance)
	{
		const FBox2D ExpandedFirst(First.Min - FVector2D(Clearance), First.Max + FVector2D(Clearance));
		return ExpandedFirst.Intersect(Second);
	}

	bool IsFootprintSpatiallyValid(
		const FBox2D& Footprint,
		const double Height,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions)
	{
		const int32 NumSamplesX = FMath::Clamp(
			FMath::CeilToInt32(Footprint.GetSize().X / FuelSupplyPCGConstants::FootprintSampleSpacing),
			1, FuelSupplyPCGConstants::MaximumFootprintSamplesPerAxis);
		const int32 NumSamplesY = FMath::Clamp(
			FMath::CeilToInt32(Footprint.GetSize().Y / FuelSupplyPCGConstants::FootprintSampleSpacing),
			1, FuelSupplyPCGConstants::MaximumFootprintSamplesPerAxis);
		for (int32 SampleX = 0; SampleX <= NumSamplesX; ++SampleX)
		{
			for (int32 SampleY = 0; SampleY <= NumSamplesY; ++SampleY)
			{
				const FVector2D Sample(
					FMath::Lerp(Footprint.Min.X, Footprint.Max.X, static_cast<double>(SampleX) / NumSamplesX),
					FMath::Lerp(Footprint.Min.Y, Footprint.Max.Y, static_cast<double>(SampleY) / NumSamplesY));
				const FVector WorldSample(Sample, Height);
				if (not IsInsideArea(WorldSample) || IsExcluded(Exclusions, WorldSample))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool IsPlacementClear(
		const FBox2D& Footprint,
		const TArray<FPlacedFuelActor>& Placements,
		const double Clearance)
	{
		for (const FPlacedFuelActor& Placement : Placements)
		{
			if (BoxesOverlapWithClearance(Footprint, Placement.Footprint, Clearance))
			{
				return false;
			}
		}
		return true;
	}

	bool TryBuildActorPlacement(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FResolvedFuelActor& Asset,
		const FVector2D& DesiredCenter,
		const double GeneratedYawDegrees,
		const double AreaHeight,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& ExistingPlacements,
		const EFuelPlacedRole Role,
		const bool bAlignToGround,
		FPlacedFuelActor& OutPlacement)
	{
		const FFuelGroundResult Ground = ProjectToGround(
			World, FVector(DesiredCenter, AreaHeight), Settings.bProjectActorsToGround);
		const double SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Ground.Normal.Z, -1.0, 1.0)));
		if (SlopeDegrees > Settings.MaxGroundSlopeDegrees)
		{
			return false;
		}

		FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(GeneratedYawDegrees + Asset.YawOffsetDegrees));
		if (bAlignToGround)
		{
			Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Ground.Normal) * Rotation;
		}

		const FTransform Transform = MakeBoundsAlignedTransform(
			Asset.LocalBounds, Asset.LocalBounds.GetCenter(), DesiredCenter,
			Ground.Position.Z, Rotation, FVector::OneVector,
			Asset.ZOffset + static_cast<double>(Settings.GlobalActorZOffset));
		const FBox2D Footprint = ComputeFootprint(Asset.LocalBounds, Transform);
		const double Clearance = Settings.GlobalActorClearance + Asset.BoundsClearance;
		if (not IsFootprintSpatiallyValid(Footprint, Ground.Position.Z, IsInsideArea, Exclusions)
			|| not IsPlacementClear(Footprint, ExistingPlacements, Clearance))
		{
			return false;
		}

		OutPlacement.ActorClass = Asset.ActorClass;
		OutPlacement.LocalBounds = Asset.LocalBounds;
		OutPlacement.Transform = Transform;
		OutPlacement.Footprint = Footprint;
		OutPlacement.VisualCenter = DesiredCenter;
		OutPlacement.Role = Role;
		OutPlacement.PipeConnections = Asset.PipeConnections;
		return true;
	}

	FVector2D RandomAreaPosition(const FBox& AreaBounds, FRandomStream& Random)
	{
		return FVector2D(
			Random.FRandRange(AreaBounds.Min.X, AreaBounds.Max.X),
			Random.FRandRange(AreaBounds.Min.Y, AreaBounds.Max.Y));
	}

	int32 PlaceFacilities(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedFuelActor>& Assets,
		const int32 DesiredCount,
		const EFuelPlacedRole Role,
		const TArray<int32>& DepotIndices,
		FRandomStream& Random,
		TArray<FPlacedFuelActor>& InOutPlacements)
	{
		if (Assets.IsEmpty())
		{
			return 0;
		}

		int32 NumPlaced = 0;
		for (int32 DesiredIndex = 0; DesiredIndex < DesiredCount; ++DesiredIndex)
		{
			for (int32 AttemptIndex = 0; AttemptIndex < Settings.PlacementAttemptsPerActor; ++AttemptIndex)
			{
				const int32 AssetIndex = SelectWeightedActor(Assets, Random);
				FVector2D DesiredCenter = RandomAreaPosition(AreaBounds, Random);
				if (Role == EFuelPlacedRole::FuelTank && not DepotIndices.IsEmpty())
				{
					const FPlacedFuelActor& Depot = InOutPlacements[DepotIndices[Random.RandRange(0, DepotIndices.Num() - 1)]];
					const double Angle = Random.FRandRange(0.0f, 2.0f * PI);
					const double Radius = Settings.FuelTankClusterRadius * FMath::Sqrt(Random.FRand());
					DesiredCenter = Depot.VisualCenter + Radius * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
				}

				FPlacedFuelActor Placement;
				if (TryBuildActorPlacement(
					World, Settings, Assets[AssetIndex], DesiredCenter,
					MakeGeneratedYawDegrees(Random, Settings.bUseRightAngleActorRotations), AreaBounds.GetCenter().Z,
					IsInsideArea, Exclusions, InOutPlacements, Role, false, Placement))
				{
					InOutPlacements.Add(MoveTemp(Placement));
					++NumPlaced;
					break;
				}
			}
		}
		return NumPlaced;
	}

	bool EdgeExists(const TArray<FFuelNetworkEdge>& Edges, const int32 First, const int32 Second)
	{
		for (const FFuelNetworkEdge& Edge : Edges)
		{
			if ((Edge.StartIndex == First && Edge.EndIndex == Second)
				|| (Edge.StartIndex == Second && Edge.EndIndex == First))
			{
				return true;
			}
		}
		return false;
	}

	void BuildDepotSpanningEdges(
		const TArray<FPlacedFuelActor>& Placements,
		const TArray<int32>& DepotIndices,
		TArray<FFuelNetworkEdge>& OutEdges)
	{
		if (DepotIndices.Num() < 2)
		{
			return;
		}

		TSet<int32> Connected;
		Connected.Add(DepotIndices[0]);
		while (Connected.Num() < DepotIndices.Num())
		{
			double BestDistanceSquared = TNumericLimits<double>::Max();
			FFuelNetworkEdge BestEdge;
			for (const int32 ConnectedIndex : Connected)
			{
				for (const int32 CandidateIndex : DepotIndices)
				{
					if (Connected.Contains(CandidateIndex))
					{
						continue;
					}
					const double DistanceSquared = FVector2D::DistSquared(
						Placements[ConnectedIndex].VisualCenter, Placements[CandidateIndex].VisualCenter);
					if (DistanceSquared < BestDistanceSquared)
					{
						BestDistanceSquared = DistanceSquared;
						BestEdge = {ConnectedIndex, CandidateIndex};
					}
				}
			}
			if (BestEdge.EndIndex == INDEX_NONE)
			{
				break;
			}
			OutEdges.Add(BestEdge);
			Connected.Add(BestEdge.EndIndex);
		}
	}

	void AddDepotLoopEdges(
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FPlacedFuelActor>& Placements,
		const TArray<int32>& DepotIndices,
		FRandomStream& Random,
		TArray<FFuelNetworkEdge>& InOutEdges)
	{
		for (const int32 DepotIndex : DepotIndices)
		{
			TArray<int32> OtherDepots = DepotIndices;
			OtherDepots.Remove(DepotIndex);
			OtherDepots.Sort([&Placements, DepotIndex](const int32 First, const int32 Second)
			{
				return FVector2D::DistSquared(Placements[DepotIndex].VisualCenter, Placements[First].VisualCenter)
					< FVector2D::DistSquared(Placements[DepotIndex].VisualCenter, Placements[Second].VisualCenter);
			});

			const int32 NumCandidates = FMath::Min(FuelSupplyPCGConstants::MaximumLoopCandidatesPerDepot, OtherDepots.Num());
			for (int32 CandidateIndex = 0; CandidateIndex < NumCandidates; ++CandidateIndex)
			{
				const int32 OtherDepotIndex = OtherDepots[CandidateIndex];
				if (not EdgeExists(InOutEdges, DepotIndex, OtherDepotIndex)
					&& Random.FRand() <= Settings.DepotLoopChance)
				{
					InOutEdges.Add({DepotIndex, OtherDepotIndex});
				}
			}
		}
	}

	double GetPipeConnectionLaneOffset(
		const UPCGCreateFuelSupplySettings& Settings,
		const int32 LaneIndex)
	{
		if (LaneIndex == 0)
		{
			return 0.0;
		}

		const int32 LaneMagnitude = (LaneIndex + 1) / 2;
		const double LaneSign = LaneIndex % 2 == 1 ? 1.0 : -1.0;
		const double PipeWidth = FMath::Max(1.0, static_cast<double>(Settings.FuelPipeSizeY));
		const double Separation = FMath::Max(0.0, static_cast<double>(Settings.PipeSeparation));
		return LaneSign * LaneMagnitude * (PipeWidth + Separation);
	}

	int32 FindClosestPlacement(
		const TArray<FPlacedFuelActor>& Placements,
		const int32 SourceIndex,
		const TArray<int32>& CandidateIndices)
	{
		int32 ClosestIndex = INDEX_NONE;
		double ClosestDistanceSquared = TNumericLimits<double>::Max();
		for (const int32 CandidateIndex : CandidateIndices)
		{
			if (CandidateIndex == SourceIndex)
			{
				continue;
			}
			const double DistanceSquared = FVector2D::DistSquared(
				Placements[SourceIndex].VisualCenter, Placements[CandidateIndex].VisualCenter);
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestIndex = CandidateIndex;
			}
		}
		return ClosestIndex;
	}

	void AddTankEdges(
		const TArray<FPlacedFuelActor>& Placements,
		const TArray<int32>& DepotIndices,
		const TArray<int32>& TankIndices,
		TArray<FFuelNetworkEdge>& OutEdges)
	{
		if (DepotIndices.IsEmpty())
		{
			return;
		}

		// Create every tank's required depot connection before any tank-to-tank connections.
		for (const int32 TankIndex : TankIndices)
		{
			const int32 ClosestDepot = FindClosestPlacement(Placements, TankIndex, DepotIndices);
			if (ClosestDepot != INDEX_NONE)
			{
				OutEdges.Add({TankIndex, ClosestDepot});
			}
		}

		// The second connection is always the nearest other tank when one exists.
		for (const int32 TankIndex : TankIndices)
		{
			if (Placements[TankIndex].PipeConnections < 2)
			{
				continue;
			}
			const int32 ClosestTank = FindClosestPlacement(Placements, TankIndex, TankIndices);
			if (ClosestTank != INDEX_NONE && not EdgeExists(OutEdges, TankIndex, ClosestTank))
			{
				OutEdges.Add({TankIndex, ClosestTank});
			}
		}

		// Additional requests alternate beside the two required connection routes.
		for (const int32 TankIndex : TankIndices)
		{
			const int32 ClosestDepot = FindClosestPlacement(Placements, TankIndex, DepotIndices);
			const int32 ClosestTank = FindClosestPlacement(Placements, TankIndex, TankIndices);
			for (int32 ConnectionIndex = 2;
				ConnectionIndex < Placements[TankIndex].PipeConnections;
				++ConnectionIndex)
			{
				const bool bUsesDepotConnection = ConnectionIndex % 2 == 0 || ClosestTank == INDEX_NONE;
				const int32 TargetIndex = bUsesDepotConnection ? ClosestDepot : ClosestTank;
				if (TargetIndex != INDEX_NONE)
				{
					OutEdges.Add({TankIndex, TargetIndex});
				}
			}
		}
	}

	void AssignPipeConnectionLanes(
		const UPCGCreateFuelSupplySettings& Settings,
		const int32 NumPlacements,
		TArray<FFuelNetworkEdge>& InOutEdges)
	{
		TArray<int32> NextLaneByPlacement;
		NextLaneByPlacement.Init(0, NumPlacements);
		for (FFuelNetworkEdge& Edge : InOutEdges)
		{
			if (NextLaneByPlacement.IsValidIndex(Edge.StartIndex))
			{
				Edge.StartLaneOffset = GetPipeConnectionLaneOffset(
					Settings, NextLaneByPlacement[Edge.StartIndex]++);
			}
			if (NextLaneByPlacement.IsValidIndex(Edge.EndIndex))
			{
				Edge.EndLaneOffset = GetPipeConnectionLaneOffset(
					Settings, NextLaneByPlacement[Edge.EndIndex]++);
			}
		}
	}

	bool IsRoutePointValid(
		const FVector2D& Position,
		const double Height,
		const double Radius,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& Placements,
		const int32 StartPlacementIndex,
		const int32 EndPlacementIndex)
	{
		const FVector2D Offsets[] = {
			FVector2D::ZeroVector,
			FVector2D(Radius, 0.0),
			FVector2D(-Radius, 0.0),
			FVector2D(0.0, Radius),
			FVector2D(0.0, -Radius)
		};
		for (const FVector2D& Offset : Offsets)
		{
			const FVector Sample(Position + Offset, Height);
			if (not IsInsideArea(Sample) || IsExcluded(Exclusions, Sample))
			{
				return false;
			}
		}

		for (int32 PlacementIndex = 0; PlacementIndex < Placements.Num(); ++PlacementIndex)
		{
			if (PlacementIndex == StartPlacementIndex || PlacementIndex == EndPlacementIndex)
			{
				continue;
			}
			const FBox2D Expanded(
				Placements[PlacementIndex].Footprint.Min - FVector2D(Radius),
				Placements[PlacementIndex].Footprint.Max + FVector2D(Radius));
			if (Expanded.IsInside(Position))
			{
				return false;
			}
		}
		return true;
	}

	FIntPoint PositionToCell(const FVector2D& Position, const FBox& AreaBounds, const double CellSize, const FIntPoint GridSize)
	{
		return FIntPoint(
			FMath::Clamp(FMath::RoundToInt32((Position.X - AreaBounds.Min.X) / CellSize), 0, GridSize.X - 1),
			FMath::Clamp(FMath::RoundToInt32((Position.Y - AreaBounds.Min.Y) / CellSize), 0, GridSize.Y - 1));
	}

	FVector2D CellToPosition(const FIntPoint Cell, const FBox& AreaBounds, const double CellSize)
	{
		return FVector2D(AreaBounds.Min.X + Cell.X * CellSize, AreaBounds.Min.Y + Cell.Y * CellSize);
	}

	void SimplifyRoute(TArray<FVector2D>& InOutRoute)
	{
		for (int32 PointIndex = InOutRoute.Num() - 2; PointIndex > 0; --PointIndex)
		{
			const FVector2D Incoming = (InOutRoute[PointIndex] - InOutRoute[PointIndex - 1]).GetSafeNormal();
			const FVector2D Outgoing = (InOutRoute[PointIndex + 1] - InOutRoute[PointIndex]).GetSafeNormal();
			if (FMath::Abs(Incoming.X * Outgoing.Y - Incoming.Y * Outgoing.X)
				< FuelSupplyPCGConstants::DirectionTolerance)
			{
				InOutRoute.RemoveAt(PointIndex);
			}
		}
	}

	struct FFuelRoutingGrid
	{
		FBox AreaBounds = FBox(EForceInit::ForceInit);
		FIntPoint GridSize = FIntPoint::ZeroValue;
		FIntPoint StartCell = FIntPoint::ZeroValue;
		FIntPoint EndCell = FIntPoint::ZeroValue;
		int32 StartIndex = INDEX_NONE;
		int32 EndIndex = INDEX_NONE;
		double CellSize = 0.0;
	};

	FFuelRoutingGrid BuildRoutingGrid(
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FVector2D& Start,
		const FVector2D& End)
	{
		FFuelRoutingGrid Grid;
		Grid.AreaBounds = AreaBounds;
		Grid.CellSize = FMath::Max(
			FuelSupplyPCGConstants::MinimumRouteGridSize, static_cast<double>(Settings.RouteGridSize));
		Grid.GridSize = FIntPoint(
			FMath::CeilToInt32(AreaBounds.GetSize().X / Grid.CellSize) + 1,
			FMath::CeilToInt32(AreaBounds.GetSize().Y / Grid.CellSize) + 1);
		const int64 InitialCellCount = static_cast<int64>(Grid.GridSize.X) * Grid.GridSize.Y;
		const int32 MaximumSearchCells = FMath::Max(
			FuelSupplyPCGConstants::MinimumRouteSearchCells, Settings.MaxRouteSearchCells);
		if (InitialCellCount > MaximumSearchCells)
		{
			Grid.CellSize *= FMath::Sqrt(static_cast<double>(InitialCellCount) / MaximumSearchCells);
			Grid.GridSize.X = FMath::CeilToInt32(AreaBounds.GetSize().X / Grid.CellSize) + 1;
			Grid.GridSize.Y = FMath::CeilToInt32(AreaBounds.GetSize().Y / Grid.CellSize) + 1;
		}

		Grid.StartCell = PositionToCell(Start, AreaBounds, Grid.CellSize, Grid.GridSize);
		Grid.EndCell = PositionToCell(End, AreaBounds, Grid.CellSize, Grid.GridSize);
		Grid.StartIndex = Grid.StartCell.Y * Grid.GridSize.X + Grid.StartCell.X;
		Grid.EndIndex = Grid.EndCell.Y * Grid.GridSize.X + Grid.EndCell.X;
		return Grid;
	}

	bool DoesSegmentOverlapExistingPipeRoutes(
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd,
		const TArray<TArray<FVector2D>>& ExistingPipeRoutes,
		const double MinimumCenterDistance)
	{
		const FVector Start3D(SegmentStart, 0.0);
		const FVector End3D(SegmentEnd, 0.0);
		for (const TArray<FVector2D>& ExistingRoute : ExistingPipeRoutes)
		{
			for (int32 PointIndex = 0; PointIndex + 1 < ExistingRoute.Num(); ++PointIndex)
			{
				FVector ClosestOnNewSegment;
				FVector ClosestOnExistingSegment;
				FMath::SegmentDistToSegmentSafe(
					Start3D,
					End3D,
					FVector(ExistingRoute[PointIndex], 0.0),
					FVector(ExistingRoute[PointIndex + 1], 0.0),
					ClosestOnNewSegment,
					ClosestOnExistingSegment);
				if (FVector::DistSquared(ClosestOnNewSegment, ClosestOnExistingSegment)
					< FMath::Square(MinimumCenterDistance))
				{
					return true;
				}
			}
		}
		return false;
	}

	void TryAddRouteNeighbor(
		const FFuelRoutingGrid& Grid,
		const FIntPoint& NeighborCell,
		const int32 CurrentIndex,
		const double PipeRadius,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& Placements,
		const FFuelNetworkEdge& Edge,
		const TArray<TArray<FVector2D>>& ExistingPipeRoutes,
		const double MinimumPipeCenterDistance,
		TBitArray<>& Closed,
		TArray<double>& Costs,
		TArray<int32>& Previous,
		TArray<FFuelRouteOpenNode>& Open)
	{
		if (NeighborCell.X < 0 || NeighborCell.Y < 0
			|| NeighborCell.X >= Grid.GridSize.X || NeighborCell.Y >= Grid.GridSize.Y)
		{
			return;
		}
		const int32 NeighborIndex = NeighborCell.Y * Grid.GridSize.X + NeighborCell.X;
		if (Closed[NeighborIndex])
		{
			return;
		}

		const FVector2D NeighborPosition = CellToPosition(NeighborCell, Grid.AreaBounds, Grid.CellSize);
		const FIntPoint CurrentCell(CurrentIndex % Grid.GridSize.X, CurrentIndex / Grid.GridSize.X);
		const FVector2D CurrentPosition = CellToPosition(CurrentCell, Grid.AreaBounds, Grid.CellSize);
		if (NeighborIndex != Grid.EndIndex && not IsRoutePointValid(
			NeighborPosition, Grid.AreaBounds.GetCenter().Z, PipeRadius, IsInsideArea, Exclusions,
			Placements, Edge.StartIndex, Edge.EndIndex))
		{
			return;
		}
		if (DoesSegmentOverlapExistingPipeRoutes(
			CurrentPosition, NeighborPosition, ExistingPipeRoutes, MinimumPipeCenterDistance))
		{
			return;
		}

		const double NewCost = Costs[CurrentIndex] + Grid.CellSize;
		if (NewCost >= Costs[NeighborIndex])
		{
			return;
		}
		Costs[NeighborIndex] = NewCost;
		Previous[NeighborIndex] = CurrentIndex;
		const double Heuristic = Grid.CellSize * (FMath::Abs(NeighborCell.X - Grid.EndCell.X)
			+ FMath::Abs(NeighborCell.Y - Grid.EndCell.Y));
		Open.Add({NeighborIndex, NewCost + Heuristic});
	}

	bool SearchRoutingGrid(
		const FFuelRoutingGrid& Grid,
		const double PipeRadius,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& Placements,
		const FFuelNetworkEdge& Edge,
		const TArray<TArray<FVector2D>>& ExistingPipeRoutes,
		const double MinimumPipeCenterDistance,
		TArray<int32>& OutPrevious)
	{
		const int32 NumCells = Grid.GridSize.X * Grid.GridSize.Y;
		TArray<double> Costs;
		Costs.Init(TNumericLimits<double>::Max(), NumCells);
		OutPrevious.Init(INDEX_NONE, NumCells);
		TBitArray<> Closed(false, NumCells);
		TArray<FFuelRouteOpenNode> Open;
		Costs[Grid.StartIndex] = 0.0;
		Open.Add({Grid.StartIndex, 0.0});
		const FIntPoint NeighborOffsets[] = {
			FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)
		};

		while (not Open.IsEmpty())
		{
			Open.Sort([](const FFuelRouteOpenNode& First, const FFuelRouteOpenNode& Second)
			{
				return First.Score > Second.Score;
			});
			const int32 CurrentIndex = Open.Pop(EAllowShrinking::No).CellIndex;
			if (Closed[CurrentIndex])
			{
				continue;
			}
			if (CurrentIndex == Grid.EndIndex)
			{
				break;
			}
			Closed[CurrentIndex] = true;
			const FIntPoint CurrentCell(CurrentIndex % Grid.GridSize.X, CurrentIndex / Grid.GridSize.X);
			for (const FIntPoint& Offset : NeighborOffsets)
			{
				TryAddRouteNeighbor(
					Grid, CurrentCell + Offset, CurrentIndex, PipeRadius, IsInsideArea, Exclusions,
					Placements, Edge, ExistingPipeRoutes, MinimumPipeCenterDistance,
					Closed, Costs, OutPrevious, Open);
			}
		}
		return Grid.EndIndex == Grid.StartIndex || OutPrevious[Grid.EndIndex] != INDEX_NONE;
	}

	void ReconstructRoute(
		const FFuelRoutingGrid& Grid,
		const TArray<int32>& Previous,
		const FVector2D& Start,
		const FVector2D& End,
		TArray<FVector2D>& OutRoute)
	{
		TArray<FVector2D> ReverseRoute;
		for (int32 CellIndex = Grid.EndIndex;
			CellIndex != INDEX_NONE && CellIndex != Grid.StartIndex;
			CellIndex = Previous[CellIndex])
		{
			const FIntPoint Cell(CellIndex % Grid.GridSize.X, CellIndex / Grid.GridSize.X);
			ReverseRoute.Add(CellToPosition(Cell, Grid.AreaBounds, Grid.CellSize));
		}
		OutRoute.Add(Start);
		for (int32 PointIndex = ReverseRoute.Num() - 1; PointIndex >= 0; --PointIndex)
		{
			OutRoute.Add(ReverseRoute[PointIndex]);
		}
		OutRoute.Add(End);
		SimplifyRoute(OutRoute);
	}

	double DistanceFromPointToBoxBoundary(
		const FBox2D& Box,
		const FVector2D& Position,
		const FVector2D& Direction)
	{
		const double XDistance = Direction.X > FuelSupplyPCGConstants::DirectionTolerance
			? (Box.Max.X - Position.X) / Direction.X
			: (Direction.X < -FuelSupplyPCGConstants::DirectionTolerance
				? (Box.Min.X - Position.X) / Direction.X : TNumericLimits<double>::Max());
		const double YDistance = Direction.Y > FuelSupplyPCGConstants::DirectionTolerance
			? (Box.Max.Y - Position.Y) / Direction.Y
			: (Direction.Y < -FuelSupplyPCGConstants::DirectionTolerance
				? (Box.Min.Y - Position.Y) / Direction.Y : TNumericLimits<double>::Max());
		return FMath::Max(0.0, FMath::Min(XDistance, YDistance));
	}

	FVector2D MakeConnectionPoint(
		const FPlacedFuelActor& Placement,
		const FVector2D& OutwardDirection,
		const FVector2D& LaneDirection,
		const double LaneOffset,
		const double ConnectionOverlap)
	{
		FVector2D OffsetCenter = Placement.VisualCenter + LaneDirection * LaneOffset;
		const FVector2D FootprintExtent = Placement.Footprint.GetExtent();
		const FVector2D ConnectionInset(
			FMath::Min(ConnectionOverlap, FootprintExtent.X),
			FMath::Min(ConnectionOverlap, FootprintExtent.Y));
		OffsetCenter.X = FMath::Clamp(
			OffsetCenter.X,
			Placement.Footprint.Min.X + ConnectionInset.X,
			Placement.Footprint.Max.X - ConnectionInset.X);
		OffsetCenter.Y = FMath::Clamp(
			OffsetCenter.Y,
			Placement.Footprint.Min.Y + ConnectionInset.Y,
			Placement.Footprint.Max.Y - ConnectionInset.Y);
		const double BoundaryDistance = DistanceFromPointToBoxBoundary(
			Placement.Footprint, OffsetCenter, OutwardDirection);
		const double ConnectionDistance = FMath::Max(0.0, BoundaryDistance - ConnectionOverlap);
		return OffsetCenter + OutwardDirection * ConnectionDistance;
	}

	bool FindRoute(
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& Placements,
		const FFuelNetworkEdge& Edge,
		const double PipeRadius,
		const TArray<TArray<FVector2D>>& ExistingPipeRoutes,
		const double MinimumPipeCenterDistance,
		TArray<FVector2D>& OutRoute)
	{
		const FPlacedFuelActor& StartPlacement = Placements[Edge.StartIndex];
		const FPlacedFuelActor& EndPlacement = Placements[Edge.EndIndex];
		const FVector2D Direction = (EndPlacement.VisualCenter - StartPlacement.VisualCenter).GetSafeNormal();
		const FVector2D LaneDirection(-Direction.Y, Direction.X);
		const double ConnectionOverlap = FMath::Max(0.0, static_cast<double>(Settings.ConnectionOverlap));
		const FVector2D Start = MakeConnectionPoint(
			StartPlacement, Direction, LaneDirection, Edge.StartLaneOffset, ConnectionOverlap);
		const FVector2D End = MakeConnectionPoint(
			EndPlacement, -Direction, LaneDirection, Edge.EndLaneOffset, ConnectionOverlap);
		TArray<int32> Previous;
		const FFuelRoutingGrid Grid = BuildRoutingGrid(Settings, AreaBounds, Start, End);
		if (not SearchRoutingGrid(
			Grid, PipeRadius, IsInsideArea, Exclusions, Placements, Edge,
			ExistingPipeRoutes, MinimumPipeCenterDistance, Previous))
		{
			return false;
		}
		ReconstructRoute(Grid, Previous, Start, End, OutRoute);
		return OutRoute.Num() >= 2;
	}


	FVector2D CardinalDirectionToVector2D(const EFuelCardinalDirection Direction)
	{
		const FIntPoint DirectionVector = CardinalDirectionToVector(Direction);
		return FVector2D(DirectionVector.X, DirectionVector.Y);
	}

	void BuildSubdividedPipeRoute(
		const TArray<FVector2D>& Route,
		const double MaximumSegmentLength,
		TArray<FVector2D>& OutPoints)
	{
		if (Route.IsEmpty())
		{
			return;
		}

		OutPoints.Add(Route[0]);
		for (int32 SegmentIndex = 0; SegmentIndex + 1 < Route.Num(); ++SegmentIndex)
		{
			const FVector2D& SegmentStart = Route[SegmentIndex];
			const FVector2D& SegmentEnd = Route[SegmentIndex + 1];
			const double SegmentLength = FVector2D::Distance(SegmentStart, SegmentEnd);
			const int32 NumPieces = FMath::Max(1, FMath::CeilToInt32(SegmentLength / MaximumSegmentLength));
			for (int32 PieceIndex = 1; PieceIndex <= NumPieces; ++PieceIndex)
			{
				OutPoints.Add(FMath::Lerp(
					SegmentStart, SegmentEnd, static_cast<double>(PieceIndex) / NumPieces));
			}
		}
	}

	void BuildPipeSplineTangents(
		const TArray<FVector2D>& Points,
		const double Curviness,
		TArray<FVector2D>& OutTangents)
	{
		OutTangents.Init(FVector2D::ZeroVector, Points.Num());
		if (Points.Num() < 2 || Curviness <= 0.0)
		{
			return;
		}

		OutTangents[0] = (Points[1] - Points[0]) * Curviness;
		OutTangents.Last() = (Points.Last() - Points[Points.Num() - 2]) * Curviness;
		for (int32 PointIndex = 1; PointIndex + 1 < Points.Num(); ++PointIndex)
		{
			OutTangents[PointIndex] = 0.5 * (Points[PointIndex + 1] - Points[PointIndex - 1]) * Curviness;
		}
	}

	FVector2D EvaluatePipeSplineSegment(
		const FVector2D& Start,
		const FVector2D& StartTangent,
		const FVector2D& End,
		const FVector2D& EndTangent,
		const double Alpha)
	{
		const double AlphaSquared = Alpha * Alpha;
		const double AlphaCubed = AlphaSquared * Alpha;
		return Start * (2.0 * AlphaCubed - 3.0 * AlphaSquared + 1.0)
			+ StartTangent * (AlphaCubed - 2.0 * AlphaSquared + Alpha)
			+ End * (-2.0 * AlphaCubed + 3.0 * AlphaSquared)
			+ EndTangent * (AlphaCubed - AlphaSquared);
	}

	void SamplePipeSplinePath(
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FVector2D>& Points,
		const TArray<FVector2D>& Tangents,
		TArray<FVector2D>& OutSampledPath)
	{
		if (Points.Num() < 2 || Points.Num() != Tangents.Num())
		{
			return;
		}

		const double PipeSizeX = FMath::Max(1.0, static_cast<double>(Settings.FuelPipeSizeX));
		const double PipeSizeY = FMath::Max(1.0, static_cast<double>(Settings.FuelPipeSizeY));
		const double SampleSpacing = FMath::Max(
			FuelSupplyPCGConstants::MinimumPipeSplineSampleSpacing,
			FMath::Min(PipeSizeX / FuelSupplyPCGConstants::CurvinessReductionDivisor, PipeSizeY * 0.5));
		OutSampledPath.Add(Points[0]);
		for (int32 SegmentIndex = 0; SegmentIndex + 1 < Points.Num(); ++SegmentIndex)
		{
			const double ChordLength = FVector2D::Distance(Points[SegmentIndex], Points[SegmentIndex + 1]);
			const int32 NumSamples = FMath::Max(1, FMath::CeilToInt32(ChordLength / SampleSpacing));
			for (int32 SampleIndex = 1; SampleIndex <= NumSamples; ++SampleIndex)
			{
				const double Alpha = static_cast<double>(SampleIndex) / NumSamples;
				OutSampledPath.Add(EvaluatePipeSplineSegment(
					Points[SegmentIndex], Tangents[SegmentIndex],
					Points[SegmentIndex + 1], Tangents[SegmentIndex + 1], Alpha));
			}
		}
	}

	bool IsPipeSplinePathValid(
		const TArray<FVector2D>& SampledPath,
		const double AreaHeight,
		const double PipeRadius,
		const double MinimumPipeCenterDistance,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& Placements,
		const FFuelNetworkEdge& Edge,
		const TArray<TArray<FVector2D>>& ExistingPipeRoutes)
	{
		for (const FVector2D& Point : SampledPath)
		{
			if (not IsRoutePointValid(
				Point, AreaHeight, PipeRadius, IsInsideArea, Exclusions,
				Placements, Edge.StartIndex, Edge.EndIndex))
			{
				return false;
			}
		}
		for (int32 PointIndex = 0; PointIndex + 1 < SampledPath.Num(); ++PointIndex)
		{
			if (DoesSegmentOverlapExistingPipeRoutes(
				SampledPath[PointIndex], SampledPath[PointIndex + 1],
				ExistingPipeRoutes, MinimumPipeCenterDistance))
			{
				return false;
			}
		}
		return true;
	}

	void ProjectPipeSplineToTerrain(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const double AreaHeight,
		const double Curviness,
		const TArray<FVector2D>& Points,
		const TArray<FVector2D>& Tangents,
		FGeneratedFuelPipeSpline& OutSpline)
	{
		const double ZOffset = Settings.GlobalActorZOffset + Settings.FuelPipeZOffset;
		OutSpline.WorldPoints.Reserve(Points.Num());
		for (const FVector2D& Point : Points)
		{
			const FFuelGroundResult Ground = ProjectToGround(
				World, FVector(Point, AreaHeight), Settings.bProjectActorsToGround);
			OutSpline.WorldPoints.Add(FVector(Point, Ground.Position.Z + ZOffset));
		}

		OutSpline.WorldTangents.Reserve(Tangents.Num());
		for (int32 PointIndex = 0; PointIndex < Tangents.Num(); ++PointIndex)
		{
			double ZTangent = 0.0;
			if (Curviness > 0.0 && OutSpline.WorldPoints.Num() >= 2)
			{
				if (PointIndex == 0)
				{
					ZTangent = (OutSpline.WorldPoints[1].Z - OutSpline.WorldPoints[0].Z) * Curviness;
				}
				else if (PointIndex + 1 == OutSpline.WorldPoints.Num())
				{
					ZTangent = (OutSpline.WorldPoints[PointIndex].Z
						- OutSpline.WorldPoints[PointIndex - 1].Z) * Curviness;
				}
				else
				{
					ZTangent = 0.5 * (OutSpline.WorldPoints[PointIndex + 1].Z
						- OutSpline.WorldPoints[PointIndex - 1].Z) * Curviness;
				}
			}
			OutSpline.WorldTangents.Add(FVector(Tangents[PointIndex], ZTangent));
		}
	}

	bool TryBuildSafePipeSpline(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const double AreaHeight,
		const double PipeRadius,
		const double MinimumPipeCenterDistance,
		const TArray<FVector2D>& Route,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& Placements,
		const FFuelNetworkEdge& Edge,
		const TArray<TArray<FVector2D>>& ExistingPipeRoutes,
		UClass& PipeClass,
		FGeneratedFuelPipeSpline& OutSpline,
		TArray<FVector2D>& OutSampledPath)
	{
		TArray<FVector2D> PipePoints;
		BuildSubdividedPipeRoute(
			Route, FMath::Max(1.0, static_cast<double>(Settings.FuelPipeSizeX)), PipePoints);
		for (int32 AttemptIndex = 0;
			AttemptIndex < FuelSupplyPCGConstants::PipeCurveSafetyAttempts;
			++AttemptIndex)
		{
			const double CurvinessScale = 1.0
				- static_cast<double>(AttemptIndex)
				/ (FuelSupplyPCGConstants::PipeCurveSafetyAttempts - 1);
			const double Curviness = FMath::Clamp(
				static_cast<double>(Settings.PipeCurviness), 0.0, 1.0) * CurvinessScale;
			TArray<FVector2D> Tangents;
			BuildPipeSplineTangents(PipePoints, Curviness, Tangents);
			TArray<FVector2D> SampledPath;
			SamplePipeSplinePath(Settings, PipePoints, Tangents, SampledPath);
			if (not IsPipeSplinePathValid(
				SampledPath, AreaHeight, PipeRadius, MinimumPipeCenterDistance,
				IsInsideArea, Exclusions, Placements, Edge, ExistingPipeRoutes))
			{
				continue;
			}

			OutSpline.ActorClass = &PipeClass;
			ProjectPipeSplineToTerrain(
				World, Settings, AreaHeight, Curviness, PipePoints, Tangents, OutSpline);
			OutSampledPath = MoveTemp(SampledPath);
			return true;
		}
		return false;
	}

	void AppendPipeOccupiedBounds(
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FVector2D>& SampledPath,
		const double Height,
		TArray<FPlacedFuelActor>& InOutPlacements)
	{
		const double HalfWidth = 0.5 * FMath::Max(1.0, static_cast<double>(Settings.FuelPipeSizeY));
		for (int32 PointIndex = 0; PointIndex + 1 < SampledPath.Num(); ++PointIndex)
		{
			const FVector2D& Start = SampledPath[PointIndex];
			const FVector2D& End = SampledPath[PointIndex + 1];
			const FVector2D Direction = (End - Start).GetSafeNormal();
			const double SegmentLength = FVector2D::Distance(Start, End);
			if (SegmentLength < FuelSupplyPCGConstants::MinimumRouteSegmentLength)
			{
				continue;
			}

			FPlacedFuelActor& Placement = InOutPlacements.Emplace_GetRef();
			Placement.LocalBounds = FBox(
				FVector(-0.5 * SegmentLength, -HalfWidth, -FuelSupplyPCGConstants::PipeBoundsHalfHeight),
				FVector(0.5 * SegmentLength, HalfWidth, FuelSupplyPCGConstants::PipeBoundsHalfHeight));
			Placement.Transform = FTransform(
				FQuat(FVector::UpVector, FMath::Atan2(Direction.Y, Direction.X)),
				FVector(0.5 * (Start + End), Height));
			Placement.Footprint = FBox2D(
				FVector2D(FMath::Min(Start.X, End.X) - HalfWidth, FMath::Min(Start.Y, End.Y) - HalfWidth),
				FVector2D(FMath::Max(Start.X, End.X) + HalfWidth, FMath::Max(Start.Y, End.Y) + HalfWidth));
			Placement.VisualCenter = 0.5 * (Start + End);
			Placement.Role = EFuelPlacedRole::FuelPipe;
		}
	}

	bool ConfigureGeneratedSpline(
		ADestructibleSplineActor& SplineActor,
		const FGeneratedFuelPipeSpline& GeneratedSpline)
	{
		if (SplineActor.Spline == nullptr
			|| GeneratedSpline.WorldPoints.Num() < 2
			|| GeneratedSpline.WorldPoints.Num() != GeneratedSpline.WorldTangents.Num())
		{
			return false;
		}

		SplineActor.Spline->ClearSplinePoints(false);
		for (int32 PointIndex = 0; PointIndex < GeneratedSpline.WorldPoints.Num(); ++PointIndex)
		{
			SplineActor.Spline->AddSplinePoint(
				GeneratedSpline.WorldPoints[PointIndex], ESplineCoordinateSpace::World, false);
			const FVector& Tangent = GeneratedSpline.WorldTangents[PointIndex];
			const ESplinePointType::Type PointType = Tangent.IsNearlyZero()
				? ESplinePointType::Linear : ESplinePointType::CurveCustomTangent;
			SplineActor.Spline->SetSplinePointType(PointIndex, PointType, false);
			SplineActor.Spline->SetTangentsAtSplinePoint(
				PointIndex, Tangent, Tangent, ESplineCoordinateSpace::World, false);
		}
		SplineActor.Spline->UpdateSpline();
		return true;
	}

	ADestructibleSplineActor* SpawnManagedPipeSpline(
		UWorld& World,
		const FGeneratedFuelPipeSpline& GeneratedSpline)
	{
		if (GeneratedSpline.ActorClass == nullptr
			|| not GeneratedSpline.ActorClass->IsChildOf(ADestructibleSplineActor::StaticClass()))
		{
			return nullptr;
		}

		const TSubclassOf<ADestructibleSplineActor> SplineClass(GeneratedSpline.ActorClass);
		ADestructibleSplineActor* SplineActor = World.SpawnActorDeferred<ADestructibleSplineActor>(
			SplineClass, FTransform::Identity, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (not IsValid(SplineActor) || not ConfigureGeneratedSpline(*SplineActor, GeneratedSpline))
		{
			if (IsValid(SplineActor))
			{
				SplineActor->Destroy();
			}
			return nullptr;
		}

		SplineActor->FinishSpawning(FTransform::Identity);
		if (not ConfigureGeneratedSpline(*SplineActor, GeneratedSpline))
		{
			SplineActor->Destroy();
			return nullptr;
		}
		SplineActor->RebuildSpline();
		SplineActor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		SplineActor->SetActorLabel(TEXT("PCG_FuelSupply_FuelPipeSpline"));
#endif
		return SplineActor;
	}

	AActor* SpawnManagedActor(UWorld& World, const FPlacedFuelActor& Placement)
	{
		if (Placement.ActorClass == nullptr)
		{
			return nullptr;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World.SpawnActor<AActor>(Placement.ActorClass, Placement.Transform, SpawnParameters);
		if (not IsValid(Actor))
		{
			return nullptr;
		}
		Actor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("PCG_FuelSupply_%s"), *RoleToName(Placement.Role).ToString()));
#endif
		return Actor;
	}

	void SpawnAndRegisterActors(
		UPCGComponent& SourceComponent,
		UWorld& World,
		const TArray<FPlacedFuelActor>& Placements,
		const TArray<FGeneratedFuelPipeSpline>& PipeSplines)
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(&SourceComponent);
		for (const FPlacedFuelActor& Placement : Placements)
		{
			if (AActor* Actor = SpawnManagedActor(World, Placement))
			{
				ManagedActors->GeneratedActors.Add(Actor);
			}
		}
		for (const FGeneratedFuelPipeSpline& PipeSpline : PipeSplines)
		{
			if (ADestructibleSplineActor* SplineActor = SpawnManagedPipeSpline(World, PipeSpline))
			{
				ManagedActors->GeneratedActors.Add(SplineActor);
			}
		}
		if (not ManagedActors->GeneratedActors.IsEmpty())
		{
			SourceComponent.AddToManagedResources(ManagedActors);
		}
	}

	UPCGPointData* CreateFuelSupplyOutputPointData(FPCGContext* Context, const FName PinLabel)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		PointData->InitializeFromData(nullptr);
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = PinLabel;
		return PointData;
	}

	void EmitOccupiedBounds(FPCGContext* Context, const TArray<FPlacedFuelActor>& Placements, const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateFuelSupplyOutputPointData(Context, FuelSupplyPCGConstants::OccupiedBoundsPinLabel);
		FPCGMetadataAttribute<FName>* RoleAttribute = PointData->Metadata->CreateAttribute<FName>(
			FuelSupplyPCGConstants::RoleAttributeName, NAME_None, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Placements.Num());
		for (int32 PlacementIndex = 0; PlacementIndex < Placements.Num(); ++PlacementIndex)
		{
			const FPlacedFuelActor& Placement = Placements[PlacementIndex];
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = Placement.Transform;
			Point.BoundsMin = Placement.LocalBounds.Min;
			Point.BoundsMax = Placement.LocalBounds.Max;
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, PlacementIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			RoleAttribute->SetValue(Point.MetadataEntry, RoleToName(Placement.Role));
		}
	}

	void EmitFacilityArea(
		FPCGContext* Context,
		const TArray<FPlacedFuelActor>& Placements,
		const double Height)
	{
		FBox2D FacilityBounds(EForceInit::ForceInit);
		for (const FPlacedFuelActor& Placement : Placements)
		{
			FacilityBounds += Placement.Footprint.Min;
			FacilityBounds += Placement.Footprint.Max;
		}
		if (not FacilityBounds.bIsValid)
		{
			return;
		}

		const FVector2D Corners[] = {
			FacilityBounds.Min,
			FVector2D(FacilityBounds.Max.X, FacilityBounds.Min.Y),
			FacilityBounds.Max,
			FVector2D(FacilityBounds.Min.X, FacilityBounds.Max.Y)
		};
		TArray<FSplinePoint> SplinePoints;
		SplinePoints.Reserve(UE_ARRAY_COUNT(Corners));
		for (int32 CornerIndex = 0; CornerIndex < UE_ARRAY_COUNT(Corners); ++CornerIndex)
		{
			SplinePoints.Emplace(
				static_cast<float>(CornerIndex),
				FVector(Corners[CornerIndex], Height),
				ESplinePointType::Linear);
		}

		UPCGSplineData* SplineData = FPCGContext::NewObject_AnyThread<UPCGSplineData>(Context);
		SplineData->Initialize(SplinePoints, true, FTransform::Identity);
		UPCGSplineInteriorSurfaceData* FacilityArea =
			FPCGContext::NewObject_AnyThread<UPCGSplineInteriorSurfaceData>(Context);
		FacilityArea->Initialize(Context, SplineData);

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = FacilityArea;
		Output.Pin = FuelSupplyPCGConstants::FacilityAreaPinLabel;
	}

	void EmitPipeRoutes(FPCGContext* Context, const TArray<TArray<FVector2D>>& Routes, const double Height, const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateFuelSupplyOutputPointData(Context, FuelSupplyPCGConstants::PipeRoutesPinLabel);
		FPCGMetadataAttribute<int32>* NetworkAttribute = PointData->Metadata->CreateAttribute<int32>(
			FuelSupplyPCGConstants::NetworkAttributeName, INDEX_NONE, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		for (int32 RouteIndex = 0; RouteIndex < Routes.Num(); ++RouteIndex)
		{
			const TArray<FVector2D>& Route = Routes[RouteIndex];
			for (int32 PointIndex = 0; PointIndex < Route.Num(); ++PointIndex)
			{
				FPCGPoint& Point = Points.Emplace_GetRef();
				Point.Transform.SetLocation(FVector(Route[PointIndex], Height));
				Point.Density = 1.0f;
				Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, RouteIndex, PointIndex);
				PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
				NetworkAttribute->SetValue(Point.MetadataEntry, RouteIndex);
			}
		}
	}

	struct FResolvedFuelSupplyAssets
	{
		TArray<FResolvedFuelActor> Depots;
		TArray<FResolvedFuelActor> Tanks;
		UClass* PipeSplineClass = nullptr;
		TArray<FResolvedFuelFence> Fences;
	};

	struct FGeneratedFuelSupply
	{
		TArray<FPlacedFuelActor> Placements;
		TArray<int32> DepotIndices;
		TArray<int32> TankIndices;
		TArray<TArray<FVector2D>> Routes;
		TArray<FGeneratedFuelPipeSpline> PipeSplines;
	};

	bool ResolveFuelSupplyAssets(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		FResolvedFuelSupplyAssets& OutAssets)
	{
		ResolveActorEntries(Context, World, Settings.SupplyDepots,
			LOCTEXT("SupplyDepotRole", "supply-depot"), OutAssets.Depots);
		ResolveTankEntries(Context, World, Settings.FuelTanks, OutAssets.Tanks);
		OutAssets.PipeSplineClass = Settings.FuelPipeSplineClass.LoadSynchronous();
		ResolveFenceEntries(Context, World, Settings.FuelTankFences, OutAssets.Fences);
		const bool bHasValidPipeClass = OutAssets.PipeSplineClass != nullptr
			&& OutAssets.PipeSplineClass->IsChildOf(ADestructibleSplineActor::StaticClass());
		if (OutAssets.Depots.IsEmpty() || not bHasValidPipeClass)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("MissingCoreFuelAssets",
				"CreateFuelSupply: at least one valid supply depot and an ADestructibleSplineActor-derived pipe class are required."));
			return false;
		}
		return true;
	}

	bool GenerateFuelFacilityLayout(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FResolvedFuelSupplyAssets& Assets,
		FRandomStream& Random,
		FGeneratedFuelSupply& OutGenerated)
	{
		const int32 DepotCount = Random.RandRange(
			FMath::Min(Settings.MinSupplyDepots, Settings.MaxSupplyDepots),
			FMath::Max(Settings.MinSupplyDepots, Settings.MaxSupplyDepots));
		PlaceFacilities(World, Settings, AreaBounds, IsInsideArea, Exclusions, Assets.Depots, DepotCount,
			EFuelPlacedRole::SupplyDepot, OutGenerated.DepotIndices, Random, OutGenerated.Placements);
		for (int32 PlacementIndex = 0; PlacementIndex < OutGenerated.Placements.Num(); ++PlacementIndex)
		{
			OutGenerated.DepotIndices.Add(PlacementIndex);
		}
		if (OutGenerated.DepotIndices.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("NoPlacedFuelDepots",
				"CreateFuelSupply: no supply depot fit inside the area after bounds and exclusion checks."));
			return false;
		}

		const int32 TankCount = Random.RandRange(
			FMath::Min(Settings.MinFuelTanks, Settings.MaxFuelTanks),
			FMath::Max(Settings.MinFuelTanks, Settings.MaxFuelTanks));
		const int32 FirstTankIndex = OutGenerated.Placements.Num();
		PlaceFacilities(World, Settings, AreaBounds, IsInsideArea, Exclusions, Assets.Tanks, TankCount,
			EFuelPlacedRole::FuelTank, OutGenerated.DepotIndices, Random, OutGenerated.Placements);
		for (int32 PlacementIndex = FirstTankIndex; PlacementIndex < OutGenerated.Placements.Num(); ++PlacementIndex)
		{
			OutGenerated.TankIndices.Add(PlacementIndex);
		}
		return true;
	}

	double GetPipeRouteRadius(
		const UPCGCreateFuelSupplySettings& Settings)
	{
		const double PipeWidth = FMath::Max(1.0, static_cast<double>(Settings.FuelPipeSizeY));
		const double ObstacleClearance = FMath::Max(
			0.0, static_cast<double>(Settings.PipeObstacleClearance));
		return 0.5 * PipeWidth + ObstacleClearance;
	}

	void GenerateFuelPipeNetwork(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		UClass& PipeSplineClass,
		FRandomStream& Random,
		FGeneratedFuelSupply& InOutGenerated)
	{
		TArray<FFuelNetworkEdge> Edges;
		BuildDepotSpanningEdges(InOutGenerated.Placements, InOutGenerated.DepotIndices, Edges);
		AddDepotLoopEdges(Settings, InOutGenerated.Placements, InOutGenerated.DepotIndices, Random, Edges);
		AddTankEdges(
			InOutGenerated.Placements,
			InOutGenerated.DepotIndices, InOutGenerated.TankIndices, Edges);
		AssignPipeConnectionLanes(Settings, InOutGenerated.Placements.Num(), Edges);

		const double RouteRadius = GetPipeRouteRadius(Settings);
		const double PipeWidth = FMath::Max(1.0, static_cast<double>(Settings.FuelPipeSizeY));
		const double PipeSeparation = FMath::Max(0.0, static_cast<double>(Settings.PipeSeparation));
		const double MinimumPipeCenterDistance = PipeWidth + PipeSeparation;
		TArray<TArray<FVector2D>> ExistingPipeRoutes;
		for (const FFuelNetworkEdge& Edge : Edges)
		{
			TArray<FVector2D> Route;
			if (not FindRoute(
				Settings, AreaBounds, IsInsideArea, Exclusions, InOutGenerated.Placements,
				Edge, RouteRadius, ExistingPipeRoutes, MinimumPipeCenterDistance, Route))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FuelRouteFailed",
					"CreateFuelSupply: an obstacle-free pipe route could not be found; that connection was skipped."));
				continue;
			}

			FGeneratedFuelPipeSpline GeneratedSpline;
			TArray<FVector2D> SampledPath;
			if (not TryBuildSafePipeSpline(
				World, Settings, AreaBounds.GetCenter().Z, RouteRadius, MinimumPipeCenterDistance,
				Route, IsInsideArea, Exclusions, InOutGenerated.Placements, Edge,
				ExistingPipeRoutes, PipeSplineClass, GeneratedSpline, SampledPath))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FuelSplineCurveFailed",
					"CreateFuelSupply: a routed pipe could not be curved without overlapping an obstacle or another pipe; that connection was skipped."));
				continue;
			}

			ExistingPipeRoutes.Add(SampledPath);
			InOutGenerated.Routes.Add(MoveTemp(SampledPath));
			InOutGenerated.PipeSplines.Add(MoveTemp(GeneratedSpline));
		}

		for (const TArray<FVector2D>& PipeRoute : InOutGenerated.Routes)
		{
			AppendPipeOccupiedBounds(
				Settings, PipeRoute, AreaBounds.GetCenter().Z, InOutGenerated.Placements);
		}
	}

	double GetFenceLength(const FResolvedFuelFence& Fence)
	{
		const FIntPoint Forward = CardinalDirectionToVector(Fence.AuthoredForwardDirection);
		return Forward.X != 0 ? Fence.LocalBounds.GetSize().X : Fence.LocalBounds.GetSize().Y;
	}

	int32 SelectFenceForRemainingLength(
		const TArray<FResolvedFuelFence>& Fences,
		const double RemainingLength)
	{
		constexpr double OvershootScoreMultiplier = 2.0;
		int32 BestFenceIndex = INDEX_NONE;
		double BestScore = TNumericLimits<double>::Max();
		for (int32 FenceIndex = 0; FenceIndex < Fences.Num(); ++FenceIndex)
		{
			const FResolvedFuelFence& Fence = Fences[FenceIndex];
			const double FenceLength = GetFenceLength(Fence);
			const double OvershootPenalty = FenceLength > RemainingLength ? OvershootScoreMultiplier : 1.0;
			const double Score = OvershootPenalty * FMath::Abs(RemainingLength - FenceLength) / Fence.Weight;
			if (Score < BestScore)
			{
				BestScore = Score;
				BestFenceIndex = FenceIndex;
			}
		}
		return BestFenceIndex;
	}

	double SnapFenceSideLengthToPieces(
		const TArray<FResolvedFuelFence>& Fences,
		const double DesiredLength)
	{
		const int32 FenceIndex = SelectFenceForRemainingLength(Fences, DesiredLength);
		if (FenceIndex == INDEX_NONE)
		{
			return DesiredLength;
		}

		const FResolvedFuelFence& Fence = Fences[FenceIndex];
		const double FenceLength = GetFenceLength(Fence);
		const double Advance = FMath::Max(
			FuelSupplyPCGConstants::MinimumRouteSegmentLength, FenceLength - Fence.EndOverlap);
		const double RequiredAdvance = FMath::Max(0.0, DesiredLength - Fence.EndOverlap);
		const int32 PieceCount = FMath::Max(1, FMath::CeilToInt32(RequiredAdvance / Advance));
		return Fence.EndOverlap + PieceCount * Advance;
	}

	double GetMaximumFenceLength(const TArray<FResolvedFuelFence>& Fences)
	{
		double MaximumLength = 0.0;
		for (const FResolvedFuelFence& Fence : Fences)
		{
			MaximumLength = FMath::Max(MaximumLength, GetFenceLength(Fence));
		}
		return MaximumLength;
	}

	bool AbsorbOverlappingFenceGroups(
		FBox2D& InOutDesiredBounds,
		int32& InOutOwnerTankIndex,
		const double MergeClearance,
		TArray<FFuelTankFenceGroup>& InOutGroups)
	{
		bool bAbsorbedGroup = false;
		for (int32 GroupIndex = InOutGroups.Num() - 1; GroupIndex >= 0; --GroupIndex)
		{
			const FFuelTankFenceGroup& Group = InOutGroups[GroupIndex];
			if (not BoxesOverlapWithClearance(InOutDesiredBounds, Group.DesiredBounds, MergeClearance))
			{
				continue;
			}

			InOutDesiredBounds += Group.DesiredBounds.Min;
			InOutDesiredBounds += Group.DesiredBounds.Max;
			InOutOwnerTankIndex = Group.OwnerTankIndex;
			InOutGroups.RemoveAt(GroupIndex, 1, EAllowShrinking::No);
			bAbsorbedGroup = true;
		}
		return bAbsorbedGroup;
	}

	void BuildFuelTankFenceGroups(
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FResolvedFuelFence>& Fences,
		const FGeneratedFuelSupply& Generated,
		TArray<FFuelTankFenceGroup>& OutGroups)
	{
		const double MergeClearance = GetMaximumFenceLength(Fences);
		for (const int32 TankIndex : Generated.TankIndices)
		{
			if (not Generated.Placements.IsValidIndex(TankIndex))
			{
				continue;
			}

			const FBox2D TankBounds = Generated.Placements[TankIndex].Footprint;
			FBox2D DesiredBounds(
				TankBounds.Min - FVector2D(Settings.FenceDistanceFromTank),
				TankBounds.Max + FVector2D(Settings.FenceDistanceFromTank));
			int32 OwnerTankIndex = TankIndex;
			while (AbsorbOverlappingFenceGroups(
				DesiredBounds, OwnerTankIndex, MergeClearance, OutGroups))
			{
			}

			OutGroups.Add({DesiredBounds, OwnerTankIndex});
		}
	}

	FPlacedFuelActor MakeFencePlacement(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FResolvedFuelFence& Fence,
		const FVector2D& Center,
		const FVector2D& SideDirection,
		const double AreaHeight,
		const int32 TankIndex)
	{
		const FVector2D AuthoredForward = CardinalDirectionToVector2D(Fence.AuthoredForwardDirection);
		const double YawRadians = FMath::Atan2(SideDirection.Y, SideDirection.X)
			- FMath::Atan2(AuthoredForward.Y, AuthoredForward.X);
		const FQuat Rotation(FVector::UpVector, YawRadians);
		const FFuelGroundResult Ground = ProjectToGround(
			World, FVector(Center, AreaHeight), Settings.bProjectActorsToGround);

		FPlacedFuelActor Placement;
		Placement.ActorClass = Fence.ActorClass;
		Placement.LocalBounds = Fence.LocalBounds;
		Placement.Transform = MakeBoundsAlignedTransform(
			Fence.LocalBounds, Fence.LocalBounds.GetCenter(), Center,
			Ground.Position.Z, Rotation, FVector::OneVector,
			Fence.ZOffset + static_cast<double>(Settings.GlobalActorZOffset));
		Placement.Footprint = ComputeFootprint(Fence.LocalBounds, Placement.Transform);
		Placement.VisualCenter = Center;
		Placement.Role = EFuelPlacedRole::FuelFence;
		Placement.FenceOwnerTankIndex = TankIndex;
		return Placement;
	}

	bool DoesSegmentIntersectBox(const FVector2D& Start, const FVector2D& End, const FBox2D& Box)
	{
		double MinimumTime = 0.0;
		double MaximumTime = 1.0;
		const FVector2D Delta = End - Start;
		const double Starts[] = {Start.X, Start.Y};
		const double Deltas[] = {Delta.X, Delta.Y};
		const double Minimums[] = {Box.Min.X, Box.Min.Y};
		const double Maximums[] = {Box.Max.X, Box.Max.Y};
		for (int32 AxisIndex = 0; AxisIndex < FuelSupplyPCGConstants::CoordinateAxisCount; ++AxisIndex)
		{
			if (FMath::Abs(Deltas[AxisIndex]) < FuelSupplyPCGConstants::DirectionTolerance)
			{
				if (Starts[AxisIndex] < Minimums[AxisIndex] || Starts[AxisIndex] > Maximums[AxisIndex])
				{
					return false;
				}
				continue;
			}

			double FirstTime = (Minimums[AxisIndex] - Starts[AxisIndex]) / Deltas[AxisIndex];
			double SecondTime = (Maximums[AxisIndex] - Starts[AxisIndex]) / Deltas[AxisIndex];
			if (FirstTime > SecondTime)
			{
				Swap(FirstTime, SecondTime);
			}
			MinimumTime = FMath::Max(MinimumTime, FirstTime);
			MaximumTime = FMath::Min(MaximumTime, SecondTime);
			if (MinimumTime > MaximumTime)
			{
				return false;
			}
		}
		return true;
	}

	bool DoesFenceBlockPipeRoute(
		const FBox2D& FenceFootprint,
		const TArray<TArray<FVector2D>>& Routes,
		const double PipeGap)
	{
		const FBox2D GapBounds(
			FenceFootprint.Min - FVector2D(PipeGap),
			FenceFootprint.Max + FVector2D(PipeGap));
		for (const TArray<FVector2D>& Route : Routes)
		{
			for (int32 PointIndex = 0; PointIndex + 1 < Route.Num(); ++PointIndex)
			{
				if (DoesSegmentIntersectBox(Route[PointIndex], Route[PointIndex + 1], GapBounds))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IsFenceClearOfGeneratedActors(
		const FBox2D& FenceFootprint,
		const TArray<FPlacedFuelActor>& Placements,
		const int32 EnclosedTankIndex,
		const double Clearance)
	{
		for (int32 PlacementIndex = 0; PlacementIndex < Placements.Num(); ++PlacementIndex)
		{
			const FPlacedFuelActor& Placement = Placements[PlacementIndex];
			const bool bSameTankFence = Placement.Role == EFuelPlacedRole::FuelFence
				&& Placement.FenceOwnerTankIndex == EnclosedTankIndex;
			if (PlacementIndex == EnclosedTankIndex || Placement.Role == EFuelPlacedRole::FuelPipe
				|| bSameTankFence)
			{
				continue;
			}
			if (BoxesOverlapWithClearance(FenceFootprint, Placement.Footprint, Clearance))
			{
				return false;
			}
		}
		return true;
	}

	void TileFenceSide(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedFuelFence>& Fences,
		const FVector2D& SideStart,
		const FVector2D& SideEnd,
		const int32 TankIndex,
		FGeneratedFuelSupply& InOutGenerated)
	{
		const FVector2D SideDirection = (SideEnd - SideStart).GetSafeNormal();
		const double SideLength = FVector2D::Distance(SideStart, SideEnd);
		double Cursor = 0.0;
		while (Cursor + FuelSupplyPCGConstants::MinimumRouteSegmentLength < SideLength)
		{
			const double RemainingLength = SideLength - Cursor;
			const int32 FenceIndex = SelectFenceForRemainingLength(Fences, RemainingLength);
			if (FenceIndex == INDEX_NONE)
			{
				return;
			}

			const FResolvedFuelFence& Fence = Fences[FenceIndex];
			const double FenceLength = GetFenceLength(Fence);
			const FVector2D Center = SideStart + SideDirection * (Cursor + FenceLength * 0.5);
			FPlacedFuelActor Placement = MakeFencePlacement(
				World, Settings, Fence, Center, SideDirection, AreaBounds.GetCenter().Z, TankIndex);
			const bool bCanPlace = IsFootprintSpatiallyValid(
				Placement.Footprint, Placement.Transform.GetLocation().Z, IsInsideArea, Exclusions)
				&& IsFenceClearOfGeneratedActors(
					Placement.Footprint, InOutGenerated.Placements, TankIndex, Settings.FenceObstacleClearance)
				&& not DoesFenceBlockPipeRoute(
					Placement.Footprint, InOutGenerated.Routes, Settings.FencePipeGap);
			if (bCanPlace)
			{
				InOutGenerated.Placements.Add(MoveTemp(Placement));
			}
			if (FenceLength >= RemainingLength + Fence.EndOverlap)
			{
				break;
			}

			Cursor += FMath::Max(
				FuelSupplyPCGConstants::MinimumRouteSegmentLength, FenceLength - Fence.EndOverlap);
		}
	}

	void GenerateFuelTankFences(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedFuelFence>& Fences,
		FGeneratedFuelSupply& InOutGenerated)
	{
		if (not Settings.bGenerateFuelTankFences || Fences.IsEmpty())
		{
			return;
		}

		TArray<FFuelTankFenceGroup> FenceGroups;
		BuildFuelTankFenceGroups(Settings, Fences, InOutGenerated, FenceGroups);
		for (const FFuelTankFenceGroup& FenceGroup : FenceGroups)
		{
			const FBox2D DesiredFenceBounds = FenceGroup.DesiredBounds;
			const double CornerClearance = FMath::Max(0.0, static_cast<double>(Settings.FenceCornerClearance));
			const FVector2D DesiredFenceSize = DesiredFenceBounds.GetSize();
			const double SnappedSideLengthX = SnapFenceSideLengthToPieces(
				Fences, FMath::Max(0.0, DesiredFenceSize.X - 2.0 * CornerClearance));
			const double SnappedSideLengthY = SnapFenceSideLengthToPieces(
				Fences, FMath::Max(0.0, DesiredFenceSize.Y - 2.0 * CornerClearance));
			const FVector2D SnappedFenceExtent(
				0.5 * (SnappedSideLengthX + 2.0 * CornerClearance),
				0.5 * (SnappedSideLengthY + 2.0 * CornerClearance));
			const FVector2D FenceCenter = DesiredFenceBounds.GetCenter();
			const FBox2D FenceBounds(FenceCenter - SnappedFenceExtent, FenceCenter + SnappedFenceExtent);
			const FVector2D Corners[] = {
				FVector2D(FenceBounds.Min.X + CornerClearance, FenceBounds.Min.Y),
				FVector2D(FenceBounds.Max.X, FenceBounds.Min.Y + CornerClearance),
				FVector2D(FenceBounds.Max.X - CornerClearance, FenceBounds.Max.Y),
				FVector2D(FenceBounds.Min.X, FenceBounds.Max.Y - CornerClearance)
			};
			const FVector2D SideEnds[] = {
				FVector2D(FenceBounds.Max.X - CornerClearance, FenceBounds.Min.Y),
				FVector2D(FenceBounds.Max.X, FenceBounds.Max.Y - CornerClearance),
				FVector2D(FenceBounds.Min.X + CornerClearance, FenceBounds.Max.Y),
				FVector2D(FenceBounds.Min.X, FenceBounds.Min.Y + CornerClearance)
			};
			for (int32 SideIndex = 0; SideIndex < FuelSupplyPCGConstants::FenceSideCount; ++SideIndex)
			{
				TileFenceSide(
					World, Settings, AreaBounds, IsInsideArea, Exclusions, Fences,
					Corners[SideIndex], SideEnds[SideIndex], FenceGroup.OwnerTankIndex, InOutGenerated);
			}
		}
	}
}

bool FPCGCreateFuelSupplyElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	const UPCGCreateFuelSupplySettings* Settings = Context->GetInputSettings<UPCGCreateFuelSupplySettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	const UPCGSpatialData* AreaData = nullptr;
	FBox AreaBounds(EForceInit::ForceInit);
	if (not TryGetArea(Context, AreaData, AreaBounds))
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoFuelArea",
			"CreateFuelSupply: no valid spatial input area was provided."));
		return true;
	}

	FResolvedFuelSupplyAssets Assets;
	if (not ResolveFuelSupplyAssets(Context, World, *Settings, Assets))
	{
		return true;
	}

	FRandomStream Random(Settings->RandomSeed);
	const TArray<const UPCGSpatialData*> Exclusions = GetExclusionData(Context);
	const FFuelSupplyAreaFunction IsInsideArea = BuildFuelSupplyAreaFunction(*AreaData);
	FGeneratedFuelSupply Generated;
	if (not GenerateFuelFacilityLayout(
		Context, World, *Settings, AreaBounds, IsInsideArea, Exclusions, Assets, Random, Generated))
	{
		return true;
	}

	GenerateFuelPipeNetwork(
		Context, World, *Settings, AreaBounds, IsInsideArea, Exclusions,
		*Assets.PipeSplineClass, Random, Generated);
	GenerateFuelTankFences(
		World, *Settings, AreaBounds, IsInsideArea, Exclusions, Assets.Fences, Generated);
	SpawnAndRegisterActors(*SourceComponent, World, Generated.Placements, Generated.PipeSplines);
	EmitOccupiedBounds(Context, Generated.Placements, Settings->RandomSeed);
	EmitFacilityArea(Context, Generated.Placements, AreaBounds.GetCenter().Z);
	EmitPipeRoutes(Context, Generated.Routes, AreaBounds.GetCenter().Z, Settings->RandomSeed);
	return true;
}

#undef LOCTEXT_NAMESPACE
