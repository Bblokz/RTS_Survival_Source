// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreateFuelSupply.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPolyLineData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGCreateFuelSupply"

namespace FuelSupplyPCGConstants
{
	const FName ExclusionPinLabel = TEXT("Exclusion");
	const FName OccupiedBoundsPinLabel = TEXT("OccupiedBounds");
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
	constexpr int32 MinimumRouteSearchCells = 100;
	constexpr int32 MaximumFootprintSamplesPerAxis = 32;
	constexpr int32 MinimumSplineSegmentSamples = 2;
	constexpr int32 MaximumSplineSegmentSamples = 128;
	constexpr int32 MaximumLoopCandidatesPerDepot = 2;
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
		"Creates bounds-aware supply depots and fuel tanks, routes a connector-authored destructible pipe "
		"network around exclusions, and encloses tanks with standalone Blueprint fences. Fence openings "
		"are derived from the generated pipe crossings. Deterministic from RandomSeed.");
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
	};

	struct FResolvedFuelPipe
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		EFuelCardinalDirection StartConnectorDirection = EFuelCardinalDirection::NegativeX;
		EFuelCardinalDirection EndConnectorDirection = EFuelCardinalDirection::PositiveX;
		bool bCanReverse = true;
		double Weight = 1.0;
		double EndOverlap = 0.0;
		double ZOffset = 0.0;
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
	};

	struct FFuelNetworkEdge
	{
		int32 StartIndex = INDEX_NONE;
		int32 EndIndex = INDEX_NONE;
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

	struct FFuelPipeCornerPlacement
	{
		int32 RoutePointIndex = INDEX_NONE;
		double IncomingTrim = 0.0;
		double OutgoingTrim = 0.0;
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
		if (OverrideX > 0.0)
		{
			const double HalfOverrideX = OverrideX * 0.5;
			InOutLocalBounds.Min.X = -HalfOverrideX;
			InOutLocalBounds.Max.X = HalfOverrideX;
		}
		if (OverrideY > 0.0)
		{
			const double HalfOverrideY = OverrideY * 0.5;
			InOutLocalBounds.Min.Y = -HalfOverrideY;
			InOutLocalBounds.Max.Y = HalfOverrideY;
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

	bool IsValidConnectorPair(const FResolvedFuelPipe& Pipe)
	{
		const FIntPoint Start = CardinalDirectionToVector(Pipe.StartConnectorDirection);
		const FIntPoint End = CardinalDirectionToVector(Pipe.EndConnectorDirection);
		return Start != End;
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

	void ResolvePipeEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FFuelPipeActorEntry>& Entries,
		TArray<FResolvedFuelPipe>& OutAssets)
	{
		for (const FFuelPipeActorEntry& Entry : Entries)
		{
			UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureActorClass(
					World, *ActorClass, Entry.FootprintOverrideX, Entry.FootprintOverrideY, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("InvalidFuelPipe", "CreateFuelSupply: skipped an invalid or bounds-less fuel-pipe Blueprint."));
				continue;
			}

			FResolvedFuelPipe& Asset = OutAssets.Emplace_GetRef();
			Asset.ActorClass = ActorClass;
			Asset.LocalBounds = LocalBounds;
			Asset.StartConnectorDirection = Entry.StartConnectorDirection;
			Asset.EndConnectorDirection = Entry.EndConnectorDirection;
			Asset.bCanReverse = Entry.bCanReverse;
			Asset.Weight = FMath::Max(0.01, static_cast<double>(Entry.Weight));
			Asset.EndOverlap = FMath::Max(0.0, static_cast<double>(Entry.EndOverlap));
			Asset.ZOffset = Entry.ZOffset;
			if (not IsValidConnectorPair(Asset))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("InvalidPipeConnectorPair",
					"CreateFuelSupply: skipped a pipe whose start and end connectors use the same side."));
				OutAssets.Pop(EAllowShrinking::No);
			}
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

	int32 SelectWeightedFence(const TArray<FResolvedFuelFence>& Assets, FRandomStream& Random)
	{
		double TotalWeight = 0.0;
		for (const FResolvedFuelFence& Asset : Assets)
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

		FHitResult Hit;
		const FVector Start = Position + FVector::UpVector * FuelSupplyPCGConstants::GroundTraceUp;
		const FVector End = Position - FVector::UpVector * FuelSupplyPCGConstants::GroundTraceDown;
		FCollisionQueryParams QueryParameters;
		if (World.LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, QueryParameters))
		{
			return {Hit.ImpactPoint, Hit.ImpactNormal, true};
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
		const FVector2D& DesiredVisualCenter,
		const double GroundZ,
		const FQuat& Rotation,
		const FVector& Scale,
		const double ZOffset)
	{
		const FVector ScaledLocalCenter = LocalBounds.GetCenter() * Scale;
		const FVector RotatedCenter = Rotation.RotateVector(ScaledLocalCenter);
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
			DesiredVisualCenter.X - RotatedCenter.X,
			DesiredVisualCenter.Y - RotatedCenter.Y,
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
			Asset.LocalBounds, DesiredCenter, Ground.Position.Z, Rotation, FVector::OneVector, Asset.ZOffset);
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

	void PlaceIndustrialDressing(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedFuelActor>& Assets,
		const TArray<int32>& DepotIndices,
		FRandomStream& Random,
		TArray<FPlacedFuelActor>& InOutPlacements)
	{
		if (Assets.IsEmpty())
		{
			return;
		}

		for (const int32 DepotIndex : DepotIndices)
		{
			const int32 DesiredCount = Random.RandRange(
				FMath::Min(Settings.MinDressingActorsPerDepot, Settings.MaxDressingActorsPerDepot),
				FMath::Max(Settings.MinDressingActorsPerDepot, Settings.MaxDressingActorsPerDepot));
			for (int32 DesiredIndex = 0; DesiredIndex < DesiredCount; ++DesiredIndex)
			{
				for (int32 AttemptIndex = 0; AttemptIndex < Settings.PlacementAttemptsPerActor; ++AttemptIndex)
				{
					const double Angle = Random.FRandRange(0.0f, 2.0f * PI);
					const double Radius = Settings.DressingRadius * FMath::Sqrt(Random.FRand());
					const FVector2D Center = InOutPlacements[DepotIndex].VisualCenter
						+ Radius * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
					const int32 AssetIndex = SelectWeightedActor(Assets, Random);
					FPlacedFuelActor Placement;
					if (TryBuildActorPlacement(
						World, Settings, Assets[AssetIndex], Center,
						MakeGeneratedYawDegrees(Random, Settings.bUseRightAngleActorRotations), AreaBounds.GetCenter().Z,
						IsInsideArea, Exclusions, InOutPlacements, EFuelPlacedRole::IndustrialDressing,
						Settings.bAlignIndustrialDressingToGround, Placement))
					{
						InOutPlacements.Add(MoveTemp(Placement));
						break;
					}
				}
			}
		}
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
		for (const int32 TankIndex : TankIndices)
		{
			int32 ClosestDepot = DepotIndices[0];
			double ClosestDistanceSquared = TNumericLimits<double>::Max();
			for (const int32 DepotIndex : DepotIndices)
			{
				const double DistanceSquared = FVector2D::DistSquared(
					Placements[TankIndex].VisualCenter, Placements[DepotIndex].VisualCenter);
				if (DistanceSquared < ClosestDistanceSquared)
				{
					ClosestDistanceSquared = DistanceSquared;
					ClosestDepot = DepotIndex;
				}
			}
			OutEdges.Add({TankIndex, ClosestDepot});
		}
	}

	double DistanceToBoxBoundary(const FBox2D& Box, const FVector2D& Direction)
	{
		const FVector2D Extent = Box.GetExtent();
		const double XDistance = FMath::Abs(Direction.X) > FuelSupplyPCGConstants::DirectionTolerance
			? Extent.X / FMath::Abs(Direction.X)
			: TNumericLimits<double>::Max();
		const double YDistance = FMath::Abs(Direction.Y) > FuelSupplyPCGConstants::DirectionTolerance
			? Extent.Y / FMath::Abs(Direction.Y)
			: TNumericLimits<double>::Max();
		return FMath::Min(XDistance, YDistance);
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

	void TryAddRouteNeighbor(
		const FFuelRoutingGrid& Grid,
		const FIntPoint& NeighborCell,
		const int32 CurrentIndex,
		const double PipeRadius,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& Placements,
		const FFuelNetworkEdge& Edge,
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
		if (NeighborIndex != Grid.EndIndex && not IsRoutePointValid(
			NeighborPosition, Grid.AreaBounds.GetCenter().Z, PipeRadius, IsInsideArea, Exclusions,
			Placements, Edge.StartIndex, Edge.EndIndex))
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
					Placements, Edge, Closed, Costs, OutPrevious, Open);
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

	bool FindRoute(
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedFuelActor>& Placements,
		const FFuelNetworkEdge& Edge,
		const double PipeRadius,
		TArray<FVector2D>& OutRoute)
	{
		const FPlacedFuelActor& StartPlacement = Placements[Edge.StartIndex];
		const FPlacedFuelActor& EndPlacement = Placements[Edge.EndIndex];
		const FVector2D Direction = (EndPlacement.VisualCenter - StartPlacement.VisualCenter).GetSafeNormal();
		const FVector2D Start = StartPlacement.VisualCenter + Direction
			* (DistanceToBoxBoundary(StartPlacement.Footprint, Direction) + Settings.ConnectionClearance);
		const FVector2D End = EndPlacement.VisualCenter - Direction
			* (DistanceToBoxBoundary(EndPlacement.Footprint, -Direction) + Settings.ConnectionClearance);
		TArray<int32> Previous;
		const FFuelRoutingGrid Grid = BuildRoutingGrid(Settings, AreaBounds, Start, End);
		if (not SearchRoutingGrid(Grid, PipeRadius, IsInsideArea, Exclusions, Placements, Edge, Previous))
		{
			return false;
		}
		ReconstructRoute(Grid, Previous, Start, End, OutRoute);
		return OutRoute.Num() >= 2;
	}

	struct FMatchedFuelPipe
	{
		int32 PipeIndex = INDEX_NONE;
		double YawRadians = 0.0;
		double ConnectionLength = 0.0;

		bool IsValid() const
		{
			return PipeIndex != INDEX_NONE;
		}
	};

	FVector2D CardinalDirectionToVector2D(const EFuelCardinalDirection Direction)
	{
		const FIntPoint DirectionVector = CardinalDirectionToVector(Direction);
		return FVector2D(DirectionVector.X, DirectionVector.Y);
	}

	FVector2D GetLocalConnectorPosition(const FResolvedFuelPipe& Pipe, const EFuelCardinalDirection Direction)
	{
		const FVector2D DirectionVector = CardinalDirectionToVector2D(Direction);
		const FVector BoundsExtent = Pipe.LocalBounds.GetExtent();
		return FVector2D(DirectionVector.X * BoundsExtent.X, DirectionVector.Y * BoundsExtent.Y);
	}

	bool TryMatchPipeOrientation(
		const FResolvedFuelPipe& Pipe,
		const FVector2D& DesiredStartOutward,
		const FVector2D& DesiredEndOutward,
		double& OutYawRadians)
	{
		const int32 NumTraversalDirections = Pipe.bCanReverse ? 2 : 1;
		for (int32 TraversalIndex = 0; TraversalIndex < NumTraversalDirections; ++TraversalIndex)
		{
			const bool bReverse = TraversalIndex == 1;
			const EFuelCardinalDirection StartDirection =
				bReverse ? Pipe.EndConnectorDirection : Pipe.StartConnectorDirection;
			const EFuelCardinalDirection EndDirection =
				bReverse ? Pipe.StartConnectorDirection : Pipe.EndConnectorDirection;
			const FVector2D LocalStart = CardinalDirectionToVector2D(StartDirection);
			const FVector2D LocalEnd = CardinalDirectionToVector2D(EndDirection);
			const double DesiredStartYaw = FMath::Atan2(DesiredStartOutward.Y, DesiredStartOutward.X);
			const double LocalStartYaw = FMath::Atan2(LocalStart.Y, LocalStart.X);
			const double CandidateYaw = DesiredStartYaw - LocalStartYaw;
			const FVector2D RotatedEnd = LocalEnd.GetRotated(FMath::RadiansToDegrees(CandidateYaw));
			if (FVector2D::DotProduct(RotatedEnd, DesiredEndOutward) > 1.0 - FuelSupplyPCGConstants::DirectionTolerance)
			{
				OutYawRadians = CandidateYaw;
				return true;
			}
		}
		return false;
	}

	double GetPipeConnectionLength(const FResolvedFuelPipe& Pipe)
	{
		return FVector2D::Distance(
			GetLocalConnectorPosition(Pipe, Pipe.StartConnectorDirection),
			GetLocalConnectorPosition(Pipe, Pipe.EndConnectorDirection));
	}

	FMatchedFuelPipe MatchStraightPipe(
		const TArray<FResolvedFuelPipe>& Pipes,
		const int32 PipeIndex,
		const FVector2D& FlowDirection)
	{
		FMatchedFuelPipe Match;
		double YawRadians = 0.0;
		if (Pipes.IsValidIndex(PipeIndex) && TryMatchPipeOrientation(
			Pipes[PipeIndex], -FlowDirection, FlowDirection, YawRadians))
		{
			Match.PipeIndex = PipeIndex;
			Match.YawRadians = YawRadians;
			Match.ConnectionLength = GetPipeConnectionLength(Pipes[PipeIndex]);
		}
		return Match;
	}

	bool CanStraightPipeCloseLength(
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FResolvedFuelPipe>& Pipes,
		const FVector2D& FlowDirection,
		const double Length)
	{
		const double MinimumScale = Settings.bAllowPipeLengthScaling
			? FMath::Min(Settings.MinPipeLengthScale, Settings.MaxPipeLengthScale) : 1.0;
		const double MaximumScale = Settings.bAllowPipeLengthScaling
			? FMath::Max(Settings.MinPipeLengthScale, Settings.MaxPipeLengthScale) : 1.0;
		for (int32 PipeIndex = 0; PipeIndex < Pipes.Num(); ++PipeIndex)
		{
			const FMatchedFuelPipe Match = MatchStraightPipe(Pipes, PipeIndex, FlowDirection);
			if (Match.IsValid() && Length >= Match.ConnectionLength * MinimumScale
				&& Length <= Match.ConnectionLength * MaximumScale + Pipes[PipeIndex].EndOverlap)
			{
				return true;
			}
		}
		return false;
	}

	FMatchedFuelPipe SelectStraightPipeForLength(
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FResolvedFuelPipe>& Pipes,
		const FVector2D& FlowDirection,
		const double RemainingLength)
	{
		FMatchedFuelPipe BestMatch;
		double BestScore = TNumericLimits<double>::Max();
		for (int32 PipeIndex = 0; PipeIndex < Pipes.Num(); ++PipeIndex)
		{
			FMatchedFuelPipe Match = MatchStraightPipe(Pipes, PipeIndex, FlowDirection);
			if (not Match.IsValid())
			{
				continue;
			}
			const FResolvedFuelPipe& Pipe = Pipes[PipeIndex];
			const double Advance = FMath::Max(
				FuelSupplyPCGConstants::MinimumRouteSegmentLength, Match.ConnectionLength - Pipe.EndOverlap);
			const double ResidualLength = RemainingLength - Advance;
			const double MinimumScale = Settings.bAllowPipeLengthScaling
				? FMath::Min(Settings.MinPipeLengthScale, Settings.MaxPipeLengthScale) : 1.0;
			const double MaximumScale = Settings.bAllowPipeLengthScaling
				? FMath::Max(Settings.MinPipeLengthScale, Settings.MaxPipeLengthScale) : 1.0;
			const bool bClosesNow = RemainingLength >= Match.ConnectionLength * MinimumScale
				&& RemainingLength <= Match.ConnectionLength * MaximumScale + Pipe.EndOverlap;
			const bool bLeavesClosableRemainder = ResidualLength > 0.0
				&& CanStraightPipeCloseLength(Settings, Pipes, FlowDirection, ResidualLength);
			const double FitPenalty = bClosesNow ? 0.0 : (bLeavesClosableRemainder ? 1.0 : 2.0);
			const double Score = FitPenalty * RemainingLength
				+ FMath::Abs(RemainingLength - Match.ConnectionLength) / Pipe.Weight;
			if (Score < BestScore)
			{
				BestScore = Score;
				BestMatch = Match;
			}
		}
		return BestMatch;
	}

	FPlacedFuelActor MakePipePlacement(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FResolvedFuelPipe& Pipe,
		const FVector2D& Center,
		const double YawRadians,
		const double LengthScale,
		const double AreaHeight)
	{
		const FQuat Rotation(FVector::UpVector, YawRadians);
		FVector Scale = FVector::OneVector;
		const FVector2D ConnectorDelta = GetLocalConnectorPosition(Pipe, Pipe.EndConnectorDirection)
			- GetLocalConnectorPosition(Pipe, Pipe.StartConnectorDirection);
		if (FMath::Abs(ConnectorDelta.Y) > FMath::Abs(ConnectorDelta.X))
		{
			Scale.Y = LengthScale;
		}
		else
		{
			Scale.X = LengthScale;
		}
		const FFuelGroundResult Ground = ProjectToGround(
			World, FVector(Center, AreaHeight), Settings.bProjectActorsToGround);

		FPlacedFuelActor Placement;
		Placement.ActorClass = Pipe.ActorClass;
		Placement.LocalBounds = Pipe.LocalBounds;
		Placement.Transform = MakeBoundsAlignedTransform(
			Pipe.LocalBounds, Center, Ground.Position.Z, Rotation, Scale, Pipe.ZOffset);
		Placement.Footprint = ComputeFootprint(Pipe.LocalBounds, Placement.Transform);
		Placement.VisualCenter = Center;
		Placement.Role = EFuelPlacedRole::FuelPipe;
		return Placement;
	}

	FMatchedFuelPipe SelectCornerPipe(
		const TArray<FResolvedFuelPipe>& Pipes,
		const FVector2D& Incoming,
		const FVector2D& Outgoing,
		FRandomStream& Random)
	{
		TArray<FMatchedFuelPipe> Candidates;
		double TotalWeight = 0.0;
		for (int32 PipeIndex = 0; PipeIndex < Pipes.Num(); ++PipeIndex)
		{
			double YawRadians = 0.0;
			if (TryMatchPipeOrientation(Pipes[PipeIndex], -Incoming, Outgoing, YawRadians))
			{
				Candidates.Add({PipeIndex, YawRadians, GetPipeConnectionLength(Pipes[PipeIndex])});
				TotalWeight += Pipes[PipeIndex].Weight;
			}
		}
		if (Candidates.IsEmpty())
		{
			return FMatchedFuelPipe();
		}

		double Roll = Random.FRandRange(0.0f, static_cast<float>(TotalWeight));
		for (const FMatchedFuelPipe& Candidate : Candidates)
		{
			Roll -= Pipes[Candidate.PipeIndex].Weight;
			if (Roll <= 0.0)
			{
				return Candidate;
			}
		}
		return Candidates.Last();
	}

	void BuildCornerPlacements(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FResolvedFuelPipe>& Pipes,
		const TArray<FVector2D>& Route,
		const double AreaHeight,
		FRandomStream& Random,
		TArray<FFuelPipeCornerPlacement>& OutCorners,
		TArray<FPlacedFuelActor>& OutPipePlacements)
	{
		for (int32 PointIndex = 1; PointIndex + 1 < Route.Num(); ++PointIndex)
		{
			const FVector2D Incoming = (Route[PointIndex] - Route[PointIndex - 1]).GetSafeNormal();
			const FVector2D Outgoing = (Route[PointIndex + 1] - Route[PointIndex]).GetSafeNormal();
			const double Cross = Incoming.X * Outgoing.Y - Incoming.Y * Outgoing.X;
			if (FMath::Abs(Cross) < FuelSupplyPCGConstants::DirectionTolerance)
			{
				continue;
			}
			const FMatchedFuelPipe Match = SelectCornerPipe(Pipes, Incoming, Outgoing, Random);
			if (not Match.IsValid())
			{
				continue;
			}

			const FResolvedFuelPipe& Pipe = Pipes[Match.PipeIndex];
			FPlacedFuelActor Placement = MakePipePlacement(
				World, Settings, Pipe, Route[PointIndex], Match.YawRadians, 1.0, AreaHeight);
			const FVector2D Extent = Placement.Footprint.GetExtent();
			FFuelPipeCornerPlacement& Corner = OutCorners.Emplace_GetRef();
			Corner.RoutePointIndex = PointIndex;
			Corner.IncomingTrim = FMath::Abs(Incoming.X) * Extent.X + FMath::Abs(Incoming.Y) * Extent.Y;
			Corner.OutgoingTrim = FMath::Abs(Outgoing.X) * Extent.X + FMath::Abs(Outgoing.Y) * Extent.Y;
			OutPipePlacements.Add(MoveTemp(Placement));
		}
	}

	const FFuelPipeCornerPlacement* FindCorner(
		const TArray<FFuelPipeCornerPlacement>& Corners,
		const int32 RoutePointIndex)
	{
		return Corners.FindByPredicate([RoutePointIndex](const FFuelPipeCornerPlacement& Corner)
		{
			return Corner.RoutePointIndex == RoutePointIndex;
		});
	}

	void TileStraightPipeSegment(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FResolvedFuelPipe>& Pipes,
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd,
		const double AreaHeight,
		TArray<FPlacedFuelActor>& OutPipePlacements)
	{
		const FVector2D Direction = (SegmentEnd - SegmentStart).GetSafeNormal();
		const double SegmentLength = FVector2D::Distance(SegmentStart, SegmentEnd);
		double Cursor = 0.0;
		while (Cursor + FuelSupplyPCGConstants::MinimumRouteSegmentLength < SegmentLength)
		{
			const double Remaining = SegmentLength - Cursor;
			const FMatchedFuelPipe Match = SelectStraightPipeForLength(Settings, Pipes, Direction, Remaining);
			if (not Match.IsValid())
			{
				return;
			}
			const FResolvedFuelPipe& Pipe = Pipes[Match.PipeIndex];
			const double NominalLength = Match.ConnectionLength;
			double LengthScale = 1.0;
			const double MinimumScale = FMath::Min(Settings.MinPipeLengthScale, Settings.MaxPipeLengthScale);
			const double MaximumScale = FMath::Max(Settings.MinPipeLengthScale, Settings.MaxPipeLengthScale);
			if (Settings.bAllowPipeLengthScaling && Remaining <= NominalLength * MaximumScale)
			{
				const double DesiredScale = Remaining / NominalLength;
				if (DesiredScale < MinimumScale)
				{
					break;
				}
				LengthScale = FMath::Min(DesiredScale, MaximumScale);
			}
			else if (NominalLength > Remaining + Pipe.EndOverlap)
			{
				break;
			}

			const double PieceLength = NominalLength * LengthScale;
			const FVector2D Center = SegmentStart + Direction * (Cursor + PieceLength * 0.5);
			OutPipePlacements.Add(MakePipePlacement(
				World, Settings, Pipe, Center, Match.YawRadians, LengthScale, AreaHeight));
			const double Advance = FMath::Max(
				FuelSupplyPCGConstants::MinimumRouteSegmentLength, PieceLength - Pipe.EndOverlap);
			Cursor += Advance;
			if (PieceLength >= Remaining - FuelSupplyPCGConstants::MinimumRouteSegmentLength)
			{
				break;
			}
		}
	}

	void TileStraightPipes(
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FResolvedFuelPipe>& Pipes,
		const TArray<FVector2D>& Route,
		const TArray<FFuelPipeCornerPlacement>& Corners,
		const double AreaHeight,
		TArray<FPlacedFuelActor>& OutPipePlacements)
	{
		for (int32 SegmentIndex = 0; SegmentIndex + 1 < Route.Num(); ++SegmentIndex)
		{
			const FVector2D Direction = (Route[SegmentIndex + 1] - Route[SegmentIndex]).GetSafeNormal();
			double StartTrim = 0.0;
			double EndTrim = 0.0;
			if (const FFuelPipeCornerPlacement* StartCorner = FindCorner(Corners, SegmentIndex))
			{
				StartTrim = StartCorner->OutgoingTrim;
			}
			if (const FFuelPipeCornerPlacement* EndCorner = FindCorner(Corners, SegmentIndex + 1))
			{
				EndTrim = EndCorner->IncomingTrim;
			}

			const FVector2D SegmentStart = Route[SegmentIndex] + Direction * StartTrim;
			const FVector2D SegmentEnd = Route[SegmentIndex + 1] - Direction * EndTrim;
			if (FVector2D::Distance(SegmentStart, SegmentEnd)
				< FuelSupplyPCGConstants::MinimumRouteSegmentLength)
			{
				continue;
			}
			TileStraightPipeSegment(
				World, Settings, Pipes, SegmentStart, SegmentEnd, AreaHeight, OutPipePlacements);
		}
	}

	AActor* SpawnManagedActor(UWorld& World, const FPlacedFuelActor& Placement)
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
		Actor->SetActorLabel(FString::Printf(TEXT("PCG_FuelSupply_%s"), *RoleToName(Placement.Role).ToString()));
#endif
		return Actor;
	}

	void SpawnAndRegisterActors(
		UPCGComponent& SourceComponent,
		UWorld& World,
		const TArray<FPlacedFuelActor>& Placements)
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(&SourceComponent);
		for (const FPlacedFuelActor& Placement : Placements)
		{
			if (AActor* Actor = SpawnManagedActor(World, Placement))
			{
				ManagedActors->GeneratedActors.Add(Actor);
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
		TArray<FResolvedFuelActor> Dressing;
		TArray<FResolvedFuelPipe> Pipes;
	};

	struct FGeneratedFuelSupply
	{
		TArray<FPlacedFuelActor> Placements;
		TArray<int32> DepotIndices;
		TArray<int32> TankIndices;
		TArray<TArray<FVector2D>> Routes;
	};

	bool ResolveFuelSupplyAssets(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		FResolvedFuelSupplyAssets& OutAssets)
	{
		ResolveActorEntries(Context, World, Settings.SupplyDepots,
			LOCTEXT("SupplyDepotRole", "supply-depot"), OutAssets.Depots);
		ResolveActorEntries(Context, World, Settings.FuelTanks,
			LOCTEXT("FuelTankRole", "fuel-tank"), OutAssets.Tanks);
		ResolveActorEntries(Context, World, Settings.IndustrialDressing,
			LOCTEXT("DressingRole", "industrial-dressing"), OutAssets.Dressing);
		ResolvePipeEntries(Context, World, Settings.FuelPipes, OutAssets.Pipes);
		const bool bHasStraightPipe = OutAssets.Pipes.FindByPredicate([](const FResolvedFuelPipe& Pipe)
		{
			return Pipe.PieceType == EFuelPipePieceType::Straight;
		}) != nullptr;
		if (OutAssets.Depots.IsEmpty() || not bHasStraightPipe)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("MissingCoreFuelAssets",
				"CreateFuelSupply: at least one valid supply-depot and straight-pipe Blueprint are required."));
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
		PlaceIndustrialDressing(
			World, Settings, AreaBounds, IsInsideArea, Exclusions, Assets.Dressing,
			OutGenerated.DepotIndices, Random, OutGenerated.Placements);
		return true;
	}

	double GetPipeRouteRadius(
		const UPCGCreateFuelSupplySettings& Settings,
		const TArray<FResolvedFuelPipe>& Pipes)
	{
		double MaximumPipeHalfWidth = 0.0;
		for (const FResolvedFuelPipe& Pipe : Pipes)
		{
			MaximumPipeHalfWidth = FMath::Max(MaximumPipeHalfWidth, Pipe.GetHalfWidth());
		}
		return MaximumPipeHalfWidth + Settings.PipeObstacleClearance;
	}

	void GenerateFuelPipeNetwork(
		FPCGContext* Context,
		UWorld& World,
		const UPCGCreateFuelSupplySettings& Settings,
		const FBox& AreaBounds,
		const FFuelSupplyAreaFunction& IsInsideArea,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FResolvedFuelPipe>& Pipes,
		FRandomStream& Random,
		FGeneratedFuelSupply& InOutGenerated)
	{
		TArray<FFuelNetworkEdge> Edges;
		BuildDepotSpanningEdges(InOutGenerated.Placements, InOutGenerated.DepotIndices, Edges);
		AddDepotLoopEdges(Settings, InOutGenerated.Placements, InOutGenerated.DepotIndices, Random, Edges);
		AddTankEdges(InOutGenerated.Placements, InOutGenerated.DepotIndices, InOutGenerated.TankIndices, Edges);

		const double RouteRadius = GetPipeRouteRadius(Settings, Pipes);
		TArray<FPlacedFuelActor> RoutingObstacles = InOutGenerated.Placements;
		for (const FFuelNetworkEdge& Edge : Edges)
		{
			TArray<FVector2D> Route;
			if (not FindRoute(Settings, AreaBounds, IsInsideArea, Exclusions, RoutingObstacles, Edge, RouteRadius, Route))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FuelRouteFailed",
					"CreateFuelSupply: an obstacle-free pipe route could not be found; that connection was skipped."));
				continue;
			}

			TArray<FPlacedFuelActor> NewPipePlacements;
			TArray<FFuelPipeCornerPlacement> Corners;
			BuildCornerPlacements(
				World, Settings, Pipes, Route, AreaBounds.GetCenter().Z, Random, Corners, NewPipePlacements);
			TileStraightPipes(
				World, Settings, Pipes, Route, Corners, AreaBounds.GetCenter().Z, NewPipePlacements);
			RoutingObstacles.Append(NewPipePlacements);
			InOutGenerated.Placements.Append(MoveTemp(NewPipePlacements));
			InOutGenerated.Routes.Add(MoveTemp(Route));
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
		Context, World, *Settings, AreaBounds, IsInsideArea, Exclusions, Assets.Pipes, Random, Generated);
	SpawnAndRegisterActors(*SourceComponent, World, Generated.Placements);
	EmitOccupiedBounds(Context, Generated.Placements, Settings->RandomSeed);
	EmitPipeRoutes(Context, Generated.Routes, AreaBounds.GetCenter().Z, Settings->RandomSeed);
	return true;
}

#undef LOCTEXT_NAMESPACE
