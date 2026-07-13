// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreateMetalField.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGCreateMetalField"

namespace MetalFieldPCGConstants
{
	const FName ExcludedBoundsPinLabel = TEXT("Excluded Bounds");
	const FName FieldBoundsPinLabel = TEXT("Field Bounds");
	const FName PlacementsPinLabel = TEXT("Placements");
	const FName FieldTypeAttributeName = TEXT("MetalFieldType");
	const FName FieldIndexAttributeName = TEXT("MetalFieldIndex");
	const FName PlacementRoleAttributeName = TEXT("MetalPlacementRole");

	constexpr double InspectionDepth = -100000.0;
	constexpr double SpatialSampleHalfExtent = 5.0;
	constexpr double MinimumUsableBoundsSize = 1.0;
	constexpr double FullCircleRadians = 2.0 * PI;
	constexpr double FullCircleDegrees = 360.0;
	constexpr double RightAngleDegrees = 90.0;
	constexpr double MinimumFieldRadius = 100.0;
	constexpr double MinimumScale = 0.01;
	constexpr double MinimumFenceSegmentLength = 10.0;
	constexpr double DirectionTolerance = 0.001;
	constexpr double DiscScatterFraction = 0.9;
	constexpr int32 FenceSideCount = 4;
}

#if WITH_EDITOR
FName UPCGCreateMetalFieldSettings::GetDefaultNodeName() const
{
	return FName(TEXT("CreateMetalField"));
}

FText UPCGCreateMetalFieldSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Metal Field");
}

FText UPCGCreateMetalFieldSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Builds non-overlapping metal resource fields from candidate points. Each accepted field is "
		"deterministically one of three kinds: a connected cluster of metal towers with scattered "
		"auxiliaries, a single metal building enclosed by a chosen fence, or a loose scatter of metal "
		"objects with auxiliaries. Excluded Bounds can contain any spatial PCG data. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGCreateMetalFieldSettings::CreateElement() const
{
	return MakeShared<FPCGCreateMetalFieldElement>();
}

TArray<FPCGPinProperties> UPCGCreateMetalFieldSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(MetalFieldPCGConstants::ExcludedBoundsPinLabel, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreateMetalFieldSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(MetalFieldPCGConstants::FieldBoundsPinLabel, EPCGDataType::Point);
	Pins.Emplace(MetalFieldPCGConstants::PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

// All internal helpers live in a uniquely named namespace (rather than an anonymous one) so their
// generic names cannot collide with sibling PCG nodes when merged into the same unity-build blob.
namespace MetalFieldPCGInternal
{
	enum class EMetalFieldType : uint8
	{
		TowerCluster,
		Building,
		RegularScatter
	};

	enum class EMetalPlacementRole : uint8
	{
		Tower,
		TowerAuxiliary,
		Building,
		Fence,
		RegularMetal,
		RegularAuxiliary
	};

	struct FResolvedMetalActor
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		double Weight = 1.0;
		double MinScale = 1.0;
		double MaxScale = 1.0;
		double ZOffset = 0.0;
		double YawOffsetDegrees = 0.0;
	};

	struct FResolvedMetalFence
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		EMetalFenceDirection AuthoredForwardDirection = EMetalFenceDirection::PositiveX;
		double Weight = 1.0;
		double EndOverlap = 0.0;
		double ZOffset = 0.0;
	};

	struct FResolvedMetalAssets
	{
		TArray<FResolvedMetalActor> Towers;
		TArray<FResolvedMetalActor> TowerAuxiliaries;
		TArray<FResolvedMetalActor> Buildings;
		TArray<FResolvedMetalFence> Fences;
		TArray<FResolvedMetalActor> RegularObjects;
		TArray<FResolvedMetalActor> RegularAuxiliaries;
	};

	struct FMetalGroundResult
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
	};

	struct FPlacedMetalActor
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		FTransform Transform = FTransform::Identity;
		FBox2D Footprint = FBox2D(EForceInit::ForceInit);
		FVector2D Center = FVector2D::ZeroVector;
		double FootprintRadius = 0.0;
		int32 FieldIndex = INDEX_NONE;
		EMetalPlacementRole Role = EMetalPlacementRole::Tower;
	};

	struct FGeneratedMetalField
	{
		FVector Center = FVector::ZeroVector;
		double OuterRadius = 0.0;
		EMetalFieldType Type = EMetalFieldType::RegularScatter;
	};

	FName FieldTypeToName(const EMetalFieldType Type)
	{
		switch (Type)
		{
		case EMetalFieldType::TowerCluster:
			return TEXT("TowerCluster");
		case EMetalFieldType::Building:
			return TEXT("Building");
		case EMetalFieldType::RegularScatter:
			return TEXT("RegularScatter");
		default:
			return NAME_None;
		}
	}

	FName PlacementRoleToName(const EMetalPlacementRole Role)
	{
		switch (Role)
		{
		case EMetalPlacementRole::Tower:
			return TEXT("Tower");
		case EMetalPlacementRole::TowerAuxiliary:
			return TEXT("TowerAuxiliary");
		case EMetalPlacementRole::Building:
			return TEXT("Building");
		case EMetalPlacementRole::Fence:
			return TEXT("Fence");
		case EMetalPlacementRole::RegularMetal:
			return TEXT("RegularMetal");
		case EMetalPlacementRole::RegularAuxiliary:
			return TEXT("RegularAuxiliary");
		default:
			return NAME_None;
		}
	}

	FVector2D MetalCardinalToVector2D(const EMetalFenceDirection Direction)
	{
		switch (Direction)
		{
		case EMetalFenceDirection::PositiveX:
			return FVector2D(1.0, 0.0);
		case EMetalFenceDirection::NegativeX:
			return FVector2D(-1.0, 0.0);
		case EMetalFenceDirection::PositiveY:
			return FVector2D(0.0, 1.0);
		case EMetalFenceDirection::NegativeY:
			return FVector2D(0.0, -1.0);
		default:
			return FVector2D(1.0, 0.0);
		}
	}

	// -----------------------------------------------------------------------
	// Blueprint measurement
	// -----------------------------------------------------------------------

	AActor* SpawnInspectionActor(UWorld& World, UClass& ActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		return World.SpawnActor<AActor>(
			&ActorClass, FTransform(FVector(0.0, 0.0, MetalFieldPCGConstants::InspectionDepth)), SpawnParameters);
	}

	/** @brief Scales measured bounds so the requested footprint size is honored while preserving the pivot ratio. */
	void ApplyFootprintOverrides(FBox& InOutLocalBounds, const double OverrideX, const double OverrideY)
	{
		const FVector MeasuredSize = InOutLocalBounds.GetSize();
		if (OverrideX > 0.0 && MeasuredSize.X >= MetalFieldPCGConstants::MinimumUsableBoundsSize)
		{
			const double PivotRelativeScaleX = OverrideX / MeasuredSize.X;
			InOutLocalBounds.Min.X *= PivotRelativeScaleX;
			InOutLocalBounds.Max.X *= PivotRelativeScaleX;
		}
		if (OverrideY > 0.0 && MeasuredSize.Y >= MetalFieldPCGConstants::MinimumUsableBoundsSize)
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

		OutLocalBounds = InspectionActor->GetComponentsBoundingBox(true, true)
			.ShiftBy(-InspectionActor->GetActorLocation());
		InspectionActor->Destroy();
		if (not OutLocalBounds.IsValid)
		{
			return false;
		}

		ApplyFootprintOverrides(OutLocalBounds, FootprintOverrideX, FootprintOverrideY);
		const FVector BoundsSize = OutLocalBounds.GetSize();
		return BoundsSize.X >= MetalFieldPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Y >= MetalFieldPCGConstants::MinimumUsableBoundsSize
			&& BoundsSize.Z >= MetalFieldPCGConstants::MinimumUsableBoundsSize;
	}

	void ResolveActorEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FMetalFieldActorEntry>& Entries,
		const FText& EntryRole,
		TArray<FResolvedMetalActor>& OutActors)
	{
		for (const FMetalFieldActorEntry& Entry : Entries)
		{
			UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureActorClass(
					World, *ActorClass, Entry.FootprintOverrideX, Entry.FootprintOverrideY, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
					LOCTEXT("InvalidActorEntry", "CreateMetalField: skipped an invalid or bounds-less {0} Blueprint."),
					EntryRole));
				continue;
			}

			FResolvedMetalActor& Resolved = OutActors.Emplace_GetRef();
			Resolved.ActorClass = ActorClass;
			Resolved.LocalBounds = LocalBounds;
			Resolved.Weight = FMath::Max(0.01, static_cast<double>(Entry.Weight));
			Resolved.MinScale = FMath::Max(MetalFieldPCGConstants::MinimumScale,
				FMath::Min(Entry.MinUniformScale, Entry.MaxUniformScale));
			Resolved.MaxScale = FMath::Max(Resolved.MinScale,
				FMath::Max(Entry.MinUniformScale, Entry.MaxUniformScale));
			Resolved.ZOffset = Entry.ZOffset;
			Resolved.YawOffsetDegrees = Entry.YawOffsetDegrees;
		}
	}

	void ResolveFenceEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FMetalFieldFenceEntry>& Entries,
		TArray<FResolvedMetalFence>& OutFences)
	{
		for (const FMetalFieldFenceEntry& Entry : Entries)
		{
			UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureActorClass(
					World, *ActorClass, Entry.FootprintOverrideX, Entry.FootprintOverrideY, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("InvalidFenceEntry", "CreateMetalField: skipped an invalid or bounds-less fence Blueprint."));
				continue;
			}

			FResolvedMetalFence& Resolved = OutFences.Emplace_GetRef();
			Resolved.ActorClass = ActorClass;
			Resolved.LocalBounds = LocalBounds;
			Resolved.AuthoredForwardDirection = Entry.AuthoredForwardDirection;
			Resolved.Weight = FMath::Max(0.01, static_cast<double>(Entry.Weight));
			Resolved.EndOverlap = FMath::Max(0.0, static_cast<double>(Entry.EndOverlap));
			Resolved.ZOffset = Entry.ZOffset;
		}
	}

	template <typename AssetType>
	int32 PickWeightedAssetIndex(const TArray<AssetType>& Assets, FRandomStream& Random)
	{
		if (Assets.IsEmpty())
		{
			return INDEX_NONE;
		}
		double TotalWeight = 0.0;
		for (const AssetType& Asset : Assets)
		{
			TotalWeight += Asset.Weight;
		}
		if (TotalWeight <= 0.0)
		{
			return Random.RandRange(0, Assets.Num() - 1);
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

	// -----------------------------------------------------------------------
	// Inputs, exclusions, ground
	// -----------------------------------------------------------------------

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
			Context->InputData.GetInputsByPin(MetalFieldPCGConstants::ExcludedBoundsPinLabel);
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
			FVector(-MetalFieldPCGConstants::SpatialSampleHalfExtent),
			FVector(MetalFieldPCGConstants::SpatialSampleHalfExtent));
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

	/** @brief Traces down for ground and rejects slopes steeper than the configured maximum. */
	bool TryProjectToGround(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FVector& Position,
		FMetalGroundResult& OutGround)
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

	/** @brief Ground query that never rejects a slope; used for thin fence pieces that follow the terrain. */
	FMetalGroundResult ProjectGroundLenient(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FVector& Position)
	{
		FHitResult Hit;
		const FVector TraceStart = Position + FVector::UpVector * Settings.GroundTraceUp;
		const FVector TraceEnd = Position - FVector::UpVector * Settings.GroundTraceDown;
		const FCollisionQueryParams QueryParameters;
		if (World.LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParameters))
		{
			return {Hit.ImpactPoint, Hit.ImpactNormal.GetSafeNormal()};
		}
		return {Position, FVector::UpVector};
	}

	// -----------------------------------------------------------------------
	// Transforms and footprints
	// -----------------------------------------------------------------------

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

	bool IsFootprintClear(
		const FBox2D& Footprint,
		const TArray<FPlacedMetalActor>& Placements,
		const double Clearance)
	{
		for (const FPlacedMetalActor& Placement : Placements)
		{
			if (BoxesOverlapWithClearance(Footprint, Placement.Footprint, Clearance))
			{
				return false;
			}
		}
		return true;
	}

	double FootprintRadiusOf(const FBox2D& Footprint)
	{
		return 0.5 * Footprint.GetSize().Size();
	}

	FVector2D MakeRandomPointInDisc(const FVector2D& Center, const double Radius, FRandomStream& Random)
	{
		const double Angle = Random.FRandRange(0.0f, static_cast<float>(MetalFieldPCGConstants::FullCircleRadians));
		const double SampledRadius = Radius * FMath::Sqrt(Random.FRand());
		return Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * SampledRadius;
	}

	double MakeGeneratedYawDegrees(FRandomStream& Random, const bool bRightAngles)
	{
		if (bRightAngles)
		{
			return MetalFieldPCGConstants::RightAngleDegrees * Random.RandRange(0, 3);
		}
		return Random.FRandRange(0.0f, static_cast<float>(MetalFieldPCGConstants::FullCircleDegrees));
	}

	/**
	 * @brief Attempts to place one measured actor, honoring the field disc, exclusions, ground slope,
	 * and footprint clearance against everything already placed in the field.
	 */
	bool TryBuildActorPlacement(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalActor& Asset,
		const FVector2D& DesiredCenter,
		const double GeneratedYawDegrees,
		const double FieldCenterZ,
		const bool bConstrainToDisc,
		const FVector2D& FieldCenter,
		const double FieldRadius,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const TArray<FPlacedMetalActor>& ExistingPlacements,
		const double ExtraClearance,
		const int32 FieldIndex,
		const EMetalPlacementRole Role,
		FRandomStream& Random,
		FPlacedMetalActor& OutPlacement)
	{
		if (IsExcluded(Exclusions, FVector(DesiredCenter, FieldCenterZ)))
		{
			return false;
		}

		FMetalGroundResult Ground;
		if (not TryProjectToGround(World, Settings, FVector(DesiredCenter, FieldCenterZ), Ground))
		{
			return false;
		}

		const double Scale = Random.FRandRange(static_cast<float>(Asset.MinScale), static_cast<float>(Asset.MaxScale));
		FQuat Rotation(FVector::UpVector,
			FMath::DegreesToRadians(GeneratedYawDegrees + Asset.YawOffsetDegrees));
		if (Settings.bAlignActorsToGround)
		{
			Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Ground.Normal) * Rotation;
		}

		const FTransform Transform = MakeBoundsAlignedTransform(
			Asset.LocalBounds, Asset.LocalBounds.GetCenter(), DesiredCenter,
			Ground.Position.Z, Rotation, FVector(Scale), Asset.ZOffset);
		const FBox2D Footprint = ComputeFootprint(Asset.LocalBounds, Transform);
		const double Radius = FootprintRadiusOf(Footprint);
		if (bConstrainToDisc
			&& FVector2D::Distance(DesiredCenter, FieldCenter) + Radius > FieldRadius)
		{
			return false;
		}
		if (not IsFootprintClear(Footprint, ExistingPlacements, Settings.MinActorClearance + ExtraClearance))
		{
			return false;
		}

		OutPlacement.ActorClass = Asset.ActorClass;
		OutPlacement.LocalBounds = Asset.LocalBounds;
		OutPlacement.Transform = Transform;
		OutPlacement.Footprint = Footprint;
		OutPlacement.Center = DesiredCenter;
		OutPlacement.FootprintRadius = Radius;
		OutPlacement.FieldIndex = FieldIndex;
		OutPlacement.Role = Role;
		return true;
	}

	double ComputeFieldOuterRadius(const FVector2D& Center, const TArray<FPlacedMetalActor>& Placements)
	{
		double OuterRadius = 0.0;
		for (const FPlacedMetalActor& Placement : Placements)
		{
			OuterRadius = FMath::Max(OuterRadius,
				FVector2D::Distance(Center, Placement.Center) + Placement.FootprintRadius);
		}
		return OuterRadius;
	}

	// -----------------------------------------------------------------------
	// Shared primary / auxiliary scatter
	// -----------------------------------------------------------------------

	bool IsWithinDistanceOfAny(
		const FVector2D& Position,
		const TArray<FVector2D>& Others,
		const double MaximumDistance)
	{
		for (const FVector2D& Other : Others)
		{
			if (FVector2D::Distance(Position, Other) <= MaximumDistance)
			{
				return true;
			}
		}
		return false;
	}

	bool IsFartherThanAll(
		const FVector2D& Position,
		const TArray<FVector2D>& Others,
		const double MinimumDistance)
	{
		for (const FVector2D& Other : Others)
		{
			if (FVector2D::Distance(Position, Other) < MinimumDistance)
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * @brief Scatters the primary actors (towers or metal objects) inside the field disc. The first sits at
	 * the field center; each later one keeps at least MinSpacing from siblings and stays within MaxSpacing of
	 * one of them so the field reads as a single connected cluster.
	 */
	void PlacePrimaries(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const TArray<FResolvedMetalActor>& Assets,
		const FVector2D& FieldCenter,
		const double FieldRadius,
		const double FieldCenterZ,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const int32 MinCount,
		const int32 MaxCount,
		const double MinSpacing,
		const double MaxSpacing,
		const int32 FieldIndex,
		const EMetalPlacementRole Role,
		FRandomStream& Random,
		TArray<FVector2D>& OutCenters,
		TArray<FPlacedMetalActor>& InOutFieldPlacements)
	{
		if (Assets.IsEmpty())
		{
			return;
		}
		const int32 ClampedMin = FMath::Max(1, FMath::Min(MinCount, MaxCount));
		const int32 ClampedMax = FMath::Max(ClampedMin, FMath::Max(MinCount, MaxCount));
		const int32 DesiredCount = Random.RandRange(ClampedMin, ClampedMax);
		const int32 Attempts = FMath::Max(1, Settings.PlacementAttemptsPerItem);

		for (int32 PrimaryIndex = 0; PrimaryIndex < DesiredCount; ++PrimaryIndex)
		{
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < Attempts && not bPlaced; ++Attempt)
			{
				const FVector2D Position = (PrimaryIndex == 0 && Attempt == 0)
					? FieldCenter
					: MakeRandomPointInDisc(
						FieldCenter, FieldRadius * MetalFieldPCGConstants::DiscScatterFraction, Random);
				if (PrimaryIndex > 0
					&& (not IsFartherThanAll(Position, OutCenters, MinSpacing)
						|| not IsWithinDistanceOfAny(Position, OutCenters, MaxSpacing)))
				{
					continue;
				}

				const int32 AssetIndex = PickWeightedAssetIndex(Assets, Random);
				FPlacedMetalActor Placement;
				if (TryBuildActorPlacement(
					World, Settings, Assets[AssetIndex], Position,
					MakeGeneratedYawDegrees(Random, false), FieldCenterZ, true, FieldCenter, FieldRadius,
					Exclusions, InOutFieldPlacements, 0.0, FieldIndex, Role, Random, Placement))
				{
					InOutFieldPlacements.Add(MoveTemp(Placement));
					OutCenters.Add(Position);
					bPlaced = true;
				}
			}
		}
	}

	/**
	 * @brief Scatters auxiliaries in a ring around the primaries. Each auxiliary keeps MinSpacing from every
	 * other auxiliary, and beyond an owning primary's first it stays within MaxSpacing of a sibling so the
	 * auxiliaries clump around their primary rather than smearing across the field.
	 */
	void PlaceAuxiliaries(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const TArray<FResolvedMetalActor>& Assets,
		const TArray<FVector2D>& PrimaryCenters,
		const FVector2D& FieldCenter,
		const double FieldRadius,
		const double FieldCenterZ,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const int32 MinCount,
		const int32 MaxCount,
		const double MinToPrimary,
		const double MaxToPrimary,
		const double MinSpacing,
		const double MaxSpacing,
		const int32 FieldIndex,
		const EMetalPlacementRole Role,
		FRandomStream& Random,
		TArray<FPlacedMetalActor>& InOutFieldPlacements)
	{
		if (Assets.IsEmpty() || PrimaryCenters.IsEmpty())
		{
			return;
		}
		const int32 ClampedMin = FMath::Max(0, FMath::Min(MinCount, MaxCount));
		const int32 ClampedMax = FMath::Max(ClampedMin, FMath::Max(MinCount, MaxCount));
		const int32 DesiredCount = Random.RandRange(ClampedMin, ClampedMax);
		const double InnerRadius = FMath::Max(0.0, FMath::Min(MinToPrimary, MaxToPrimary));
		const double OuterRadius = FMath::Max(InnerRadius, FMath::Max(MinToPrimary, MaxToPrimary));
		const int32 Attempts = FMath::Max(1, Settings.PlacementAttemptsPerItem);

		TArray<FVector2D> AllAuxiliaries;
		TArray<TArray<FVector2D>> AuxiliariesByPrimary;
		AuxiliariesByPrimary.SetNum(PrimaryCenters.Num());

		for (int32 AuxiliaryIndex = 0; AuxiliaryIndex < DesiredCount; ++AuxiliaryIndex)
		{
			const int32 PrimaryIndex = Random.RandRange(0, PrimaryCenters.Num() - 1);
			const FVector2D Primary = PrimaryCenters[PrimaryIndex];
			for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
			{
				const double Angle = Random.FRandRange(
					0.0f, static_cast<float>(MetalFieldPCGConstants::FullCircleRadians));
				const double SampledRadius = Random.FRandRange(
					static_cast<float>(InnerRadius), static_cast<float>(OuterRadius));
				const FVector2D Position = Primary + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * SampledRadius;
				if (not IsFartherThanAll(Position, AllAuxiliaries, MinSpacing))
				{
					continue;
				}
				const TArray<FVector2D>& Siblings = AuxiliariesByPrimary[PrimaryIndex];
				if (not Siblings.IsEmpty() && not IsWithinDistanceOfAny(Position, Siblings, MaxSpacing))
				{
					continue;
				}

				const int32 AssetIndex = PickWeightedAssetIndex(Assets, Random);
				FPlacedMetalActor Placement;
				if (TryBuildActorPlacement(
					World, Settings, Assets[AssetIndex], Position,
					MakeGeneratedYawDegrees(Random, false), FieldCenterZ, true, FieldCenter, FieldRadius,
					Exclusions, InOutFieldPlacements, 0.0, FieldIndex, Role, Random, Placement))
				{
					InOutFieldPlacements.Add(MoveTemp(Placement));
					AllAuxiliaries.Add(Position);
					AuxiliariesByPrimary[PrimaryIndex].Add(Position);
					break;
				}
			}
		}
	}

	// -----------------------------------------------------------------------
	// Building fence enclosure
	// -----------------------------------------------------------------------

	double GetFenceLength(const FResolvedMetalFence& Fence)
	{
		const FVector2D Forward = MetalCardinalToVector2D(Fence.AuthoredForwardDirection);
		return FMath::Abs(Forward.X) > 0.5 ? Fence.LocalBounds.GetSize().X : Fence.LocalBounds.GetSize().Y;
	}

	double SnapFenceSideLength(const FResolvedMetalFence& Fence, const double DesiredLength)
	{
		const double FenceLength = GetFenceLength(Fence);
		const double Advance = FMath::Max(MetalFieldPCGConstants::MinimumFenceSegmentLength, FenceLength - Fence.EndOverlap);
		const double RequiredAdvance = FMath::Max(0.0, DesiredLength - Fence.EndOverlap);
		const int32 PieceCount = FMath::Max(1, FMath::CeilToInt32(RequiredAdvance / Advance));
		return Fence.EndOverlap + PieceCount * Advance;
	}

	FPlacedMetalActor MakeFencePlacement(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalFence& Fence,
		const FVector2D& Center,
		const FVector2D& SideDirection,
		const double FieldCenterZ,
		const int32 FieldIndex)
	{
		const FVector2D AuthoredForward = MetalCardinalToVector2D(Fence.AuthoredForwardDirection);
		const double YawRadians = FMath::Atan2(SideDirection.Y, SideDirection.X)
			- FMath::Atan2(AuthoredForward.Y, AuthoredForward.X);
		const FQuat Rotation(FVector::UpVector, YawRadians);
		const FMetalGroundResult Ground = ProjectGroundLenient(World, Settings, FVector(Center, FieldCenterZ));

		FPlacedMetalActor Placement;
		Placement.ActorClass = Fence.ActorClass;
		Placement.LocalBounds = Fence.LocalBounds;
		Placement.Transform = MakeBoundsAlignedTransform(
			Fence.LocalBounds, Fence.LocalBounds.GetCenter(), Center,
			Ground.Position.Z, Rotation, FVector::OneVector, Fence.ZOffset);
		Placement.Footprint = ComputeFootprint(Fence.LocalBounds, Placement.Transform);
		Placement.Center = Center;
		Placement.FootprintRadius = FootprintRadiusOf(Placement.Footprint);
		Placement.FieldIndex = FieldIndex;
		Placement.Role = EMetalPlacementRole::Fence;
		return Placement;
	}

	bool IsFenceFootprintClearOfNonFencePlacements(
		const FBox2D& Footprint,
		const TArray<FPlacedMetalActor>& Placements,
		const double Clearance)
	{
		for (const FPlacedMetalActor& Placement : Placements)
		{
			if (Placement.Role == EMetalPlacementRole::Fence)
			{
				continue;
			}

			if (BoxesOverlapWithClearance(Footprint, Placement.Footprint, Clearance))
			{
				return false;
			}
		}
		return true;
	}

	void TileFenceSide(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalFence& Fence,
		const FVector2D& SideStart,
		const FVector2D& SideEnd,
		const double FieldCenterZ,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const int32 FieldIndex,
		TArray<FPlacedMetalActor>& InOutFieldPlacements)
	{
		const FVector2D SideDirection = (SideEnd - SideStart).GetSafeNormal();
		const double SideLength = FVector2D::Distance(SideStart, SideEnd);
		const double FenceLength = GetFenceLength(Fence);
		double Cursor = 0.0;
		while (Cursor + MetalFieldPCGConstants::MinimumFenceSegmentLength < SideLength)
		{
			const double RemainingLength = SideLength - Cursor;
			const FVector2D Center = SideStart + SideDirection * (Cursor + FenceLength * 0.5);
			FPlacedMetalActor Placement = MakeFencePlacement(
				World, Settings, Fence, Center, SideDirection, FieldCenterZ, FieldIndex);
			const bool bCanPlace = not IsExcluded(Exclusions, FVector(Center, FieldCenterZ))
				&& IsFenceFootprintClearOfNonFencePlacements(
					Placement.Footprint, InOutFieldPlacements, Settings.MinActorClearance);
			if (bCanPlace)
			{
				InOutFieldPlacements.Add(MoveTemp(Placement));
			}
			if (FenceLength >= RemainingLength + Fence.EndOverlap)
			{
				break;
			}
			Cursor += FMath::Max(MetalFieldPCGConstants::MinimumFenceSegmentLength, FenceLength - Fence.EndOverlap);
		}
	}

	void EncloseBuildingWithFence(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalFence& Fence,
		const FBox2D& BuildingFootprint,
		const double FieldCenterZ,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const int32 FieldIndex,
		TArray<FPlacedMetalActor>& InOutFieldPlacements)
	{
		const double CornerClearance = FMath::Max(0.0, static_cast<double>(Settings.FenceCornerClearance));
		const FBox2D DesiredBounds(
			BuildingFootprint.Min - FVector2D(Settings.FenceDistanceFromBuilding),
			BuildingFootprint.Max + FVector2D(Settings.FenceDistanceFromBuilding));
		const FVector2D DesiredSize = DesiredBounds.GetSize();
		const double SnappedSideX = SnapFenceSideLength(Fence, FMath::Max(0.0, DesiredSize.X - 2.0 * CornerClearance));
		const double SnappedSideY = SnapFenceSideLength(Fence, FMath::Max(0.0, DesiredSize.Y - 2.0 * CornerClearance));
		const FVector2D SnappedExtent(
			0.5 * (SnappedSideX + 2.0 * CornerClearance),
			0.5 * (SnappedSideY + 2.0 * CornerClearance));
		const FVector2D Center = DesiredBounds.GetCenter();
		const FBox2D FenceBounds(Center - SnappedExtent, Center + SnappedExtent);

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
		for (int32 SideIndex = 0; SideIndex < MetalFieldPCGConstants::FenceSideCount; ++SideIndex)
		{
			TileFenceSide(World, Settings, Fence, Corners[SideIndex], SideEnds[SideIndex],
				FieldCenterZ, Exclusions, FieldIndex, InOutFieldPlacements);
		}
	}

	// -----------------------------------------------------------------------
	// Per-field generators
	// -----------------------------------------------------------------------

	bool GenerateTowerField(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalAssets& Assets,
		const FVector& FieldCenter,
		const double FieldRadius,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const int32 FieldIndex,
		FRandomStream& Random,
		TArray<FPlacedMetalActor>& OutFieldPlacements)
	{
		const FVector2D FieldCenter2D(FieldCenter);
		TArray<FVector2D> TowerCenters;
		PlacePrimaries(World, Settings, Assets.Towers, FieldCenter2D, FieldRadius, FieldCenter.Z, Exclusions,
			Settings.MinTowersPerField, Settings.MaxTowersPerField,
			Settings.MinTowerToTowerDistance, Settings.MaxTowerToTowerDistance,
			FieldIndex, EMetalPlacementRole::Tower, Random, TowerCenters, OutFieldPlacements);
		if (TowerCenters.IsEmpty())
		{
			return false;
		}

		PlaceAuxiliaries(World, Settings, Assets.TowerAuxiliaries, TowerCenters, FieldCenter2D, FieldRadius,
			FieldCenter.Z, Exclusions, Settings.MinTowerAuxiliariesPerField, Settings.MaxTowerAuxiliariesPerField,
			Settings.MinTowerAuxiliaryToTowerDistance, Settings.MaxTowerAuxiliaryToTowerDistance,
			Settings.MinTowerAuxiliarySpacing, Settings.MaxTowerAuxiliarySpacing,
			FieldIndex, EMetalPlacementRole::TowerAuxiliary, Random, OutFieldPlacements);
		return true;
	}

	bool GenerateBuildingField(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalAssets& Assets,
		const FVector& FieldCenter,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const int32 FieldIndex,
		FRandomStream& Random,
		TArray<FPlacedMetalActor>& OutFieldPlacements)
	{
		const FVector2D FieldCenter2D(FieldCenter);
		const int32 BuildingIndex = PickWeightedAssetIndex(Assets.Buildings, Random);
		if (BuildingIndex == INDEX_NONE)
		{
			return false;
		}

		FPlacedMetalActor Building;
		if (not TryBuildActorPlacement(
			World, Settings, Assets.Buildings[BuildingIndex], FieldCenter2D,
			MakeGeneratedYawDegrees(Random, true), FieldCenter.Z, false, FieldCenter2D, 0.0,
			Exclusions, OutFieldPlacements, 0.0, FieldIndex, EMetalPlacementRole::Building, Random, Building))
		{
			return false;
		}
		const FBox2D BuildingFootprint = Building.Footprint;
		OutFieldPlacements.Add(MoveTemp(Building));

		if (Settings.bEncloseBuildingWithFence && not Assets.Fences.IsEmpty())
		{
			const int32 FenceIndex = PickWeightedAssetIndex(Assets.Fences, Random);
			EncloseBuildingWithFence(World, Settings, Assets.Fences[FenceIndex], BuildingFootprint,
				FieldCenter.Z, Exclusions, FieldIndex, OutFieldPlacements);
		}
		return true;
	}

	bool GenerateRegularField(
		UWorld& World,
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalAssets& Assets,
		const FVector& FieldCenter,
		const double FieldRadius,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const int32 FieldIndex,
		FRandomStream& Random,
		TArray<FPlacedMetalActor>& OutFieldPlacements)
	{
		const FVector2D FieldCenter2D(FieldCenter);
		TArray<FVector2D> ObjectCenters;
		PlacePrimaries(World, Settings, Assets.RegularObjects, FieldCenter2D, FieldRadius, FieldCenter.Z, Exclusions,
			Settings.MinRegularMetalObjects, Settings.MaxRegularMetalObjects,
			Settings.MinRegularMetalSpacing, Settings.MaxRegularMetalSpacing,
			FieldIndex, EMetalPlacementRole::RegularMetal, Random, ObjectCenters, OutFieldPlacements);
		if (ObjectCenters.IsEmpty())
		{
			return false;
		}

		PlaceAuxiliaries(World, Settings, Assets.RegularAuxiliaries, ObjectCenters, FieldCenter2D, FieldRadius,
			FieldCenter.Z, Exclusions, Settings.MinRegularAuxiliariesPerField, Settings.MaxRegularAuxiliariesPerField,
			Settings.MinRegularAuxiliaryToObjectDistance, Settings.MaxRegularAuxiliaryToObjectDistance,
			Settings.MinRegularAuxiliarySpacing, Settings.MaxRegularAuxiliarySpacing,
			FieldIndex, EMetalPlacementRole::RegularAuxiliary, Random, OutFieldPlacements);
		return true;
	}

	/** @brief Deterministically picks a field type, considering only the types whose primary assets exist. */
	bool PickFieldType(
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalAssets& Assets,
		FRandomStream& Random,
		EMetalFieldType& OutType)
	{
		struct FWeightedType
		{
			EMetalFieldType Type;
			double Weight;
		};
		TArray<FWeightedType> Options;
		if (not Assets.Towers.IsEmpty() && Settings.TowerFieldWeight > 0.0f)
		{
			Options.Add({EMetalFieldType::TowerCluster, Settings.TowerFieldWeight});
		}
		if (not Assets.Buildings.IsEmpty() && Settings.BuildingFieldWeight > 0.0f)
		{
			Options.Add({EMetalFieldType::Building, Settings.BuildingFieldWeight});
		}
		if (not Assets.RegularObjects.IsEmpty() && Settings.RegularMetalFieldWeight > 0.0f)
		{
			Options.Add({EMetalFieldType::RegularScatter, Settings.RegularMetalFieldWeight});
		}
		if (Options.IsEmpty())
		{
			return false;
		}

		double TotalWeight = 0.0;
		for (const FWeightedType& Option : Options)
		{
			TotalWeight += Option.Weight;
		}
		double Roll = Random.FRandRange(0.0f, static_cast<float>(TotalWeight));
		for (const FWeightedType& Option : Options)
		{
			Roll -= Option.Weight;
			if (Roll <= 0.0)
			{
				OutType = Option.Type;
				return true;
			}
		}
		OutType = Options.Last().Type;
		return true;
	}

	double EstimateFieldRadius(
		const UPCGCreateMetalFieldSettings& Settings,
		const FResolvedMetalAssets& Assets,
		const EMetalFieldType Type,
		const double SampledFieldRadius)
	{
		if (Type != EMetalFieldType::Building)
		{
			return SampledFieldRadius;
		}
		double MaxBuildingRadius = 0.0;
		for (const FResolvedMetalActor& Building : Assets.Buildings)
		{
			const FVector Size = Building.LocalBounds.GetSize();
			MaxBuildingRadius = FMath::Max(MaxBuildingRadius, 0.5 * FVector2D(Size.X, Size.Y).Size());
		}
		double MaxFenceLength = 0.0;
		for (const FResolvedMetalFence& Fence : Assets.Fences)
		{
			MaxFenceLength = FMath::Max(MaxFenceLength, GetFenceLength(Fence));
		}
		return MaxBuildingRadius + Settings.FenceDistanceFromBuilding + MaxFenceLength;
	}

	bool IsClearOfExistingFields(
		const FVector2D& Center,
		const double EstimatedRadius,
		const double Clearance,
		const TArray<FGeneratedMetalField>& Fields)
	{
		for (const FGeneratedMetalField& Field : Fields)
		{
			const double RequiredDistance = EstimatedRadius + Field.OuterRadius + Clearance;
			if (FVector2D::DistSquared(Center, FVector2D(Field.Center)) < RequiredDistance * RequiredDistance)
			{
				return false;
			}
		}
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
		const UPCGCreateMetalFieldSettings& Settings,
		TArray<FVector> Candidates,
		const TArray<const UPCGSpatialData*>& Exclusions,
		const FResolvedMetalAssets& Assets,
		FRandomStream& Random,
		TArray<FGeneratedMetalField>& OutFields,
		TArray<FPlacedMetalActor>& OutPlacements)
	{
		const int32 MinFields = FMath::Max(0, FMath::Min(Settings.MinFieldsToCreate, Settings.MaxFieldsToCreate));
		const int32 MaxFields = FMath::Max(MinFields, FMath::Max(Settings.MinFieldsToCreate, Settings.MaxFieldsToCreate));
		const int32 DesiredFields = Random.RandRange(MinFields, MaxFields);
		const double MinFieldRadius = FMath::Max(MetalFieldPCGConstants::MinimumFieldRadius,
			FMath::Min(Settings.MinFieldRadius, Settings.MaxFieldRadius));
		const double MaxFieldRadius = FMath::Max(MinFieldRadius,
			FMath::Max(Settings.MinFieldRadius, Settings.MaxFieldRadius));
		ShuffleCandidates(Candidates, Random);

		for (const FVector& Candidate : Candidates)
		{
			if (OutFields.Num() >= DesiredFields)
			{
				break;
			}

			FMetalGroundResult Ground;
			if (not TryProjectToGround(World, Settings, Candidate, Ground)
				|| IsExcluded(Exclusions, Ground.Position))
			{
				continue;
			}

			EMetalFieldType FieldType = EMetalFieldType::RegularScatter;
			if (not PickFieldType(Settings, Assets, Random, FieldType))
			{
				continue;
			}

			const double SampledFieldRadius = Random.FRandRange(
				static_cast<float>(MinFieldRadius), static_cast<float>(MaxFieldRadius));
			const double EstimatedRadius = EstimateFieldRadius(Settings, Assets, FieldType, SampledFieldRadius);
			if (not IsClearOfExistingFields(
				FVector2D(Ground.Position), EstimatedRadius, Settings.FieldClearance, OutFields))
			{
				continue;
			}

			const int32 FieldIndex = OutFields.Num();
			TArray<FPlacedMetalActor> FieldPlacements;
			bool bGenerated = false;
			switch (FieldType)
			{
			case EMetalFieldType::TowerCluster:
				bGenerated = GenerateTowerField(World, Settings, Assets, Ground.Position, SampledFieldRadius,
					Exclusions, FieldIndex, Random, FieldPlacements);
				break;
			case EMetalFieldType::Building:
				bGenerated = GenerateBuildingField(World, Settings, Assets, Ground.Position,
					Exclusions, FieldIndex, Random, FieldPlacements);
				break;
			case EMetalFieldType::RegularScatter:
				bGenerated = GenerateRegularField(World, Settings, Assets, Ground.Position, SampledFieldRadius,
					Exclusions, FieldIndex, Random, FieldPlacements);
				break;
			}

			if (not bGenerated || FieldPlacements.IsEmpty())
			{
				continue;
			}

			FGeneratedMetalField& Field = OutFields.Emplace_GetRef();
			Field.Center = Ground.Position;
			Field.Type = FieldType;
			Field.OuterRadius = FMath::Max(
				ComputeFieldOuterRadius(FVector2D(Ground.Position), FieldPlacements),
				FieldType == EMetalFieldType::Building ? 0.0 : SampledFieldRadius);
			OutPlacements.Append(MoveTemp(FieldPlacements));
		}

		if (OutFields.Num() < MinFields)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("BelowMinimumFields",
					"CreateMetalField: created {0} fields, below the configured minimum {1}; remaining candidates "
					"overlapped exclusions, other fields, or invalid terrain."),
				FText::AsNumber(OutFields.Num()), FText::AsNumber(MinFields)));
		}
	}

	// -----------------------------------------------------------------------
	// Spawning and output
	// -----------------------------------------------------------------------

	AActor* SpawnMetalActor(UWorld& World, const FPlacedMetalActor& Placement)
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
		Actor->SetActorLabel(FString::Printf(TEXT("PCG_MetalField_%s_%d"),
			*PlacementRoleToName(Placement.Role).ToString(), Placement.FieldIndex));
#endif
		return Actor;
	}

	void SpawnAndRegisterPlacements(
		UPCGComponent& SourceComponent,
		UWorld& World,
		const TArray<FPlacedMetalActor>& Placements)
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(&SourceComponent);
		for (const FPlacedMetalActor& Placement : Placements)
		{
			if (AActor* Actor = SpawnMetalActor(World, Placement))
			{
				ManagedActors->GeneratedActors.Add(Actor);
			}
		}
		if (not ManagedActors->GeneratedActors.IsEmpty())
		{
			SourceComponent.AddToManagedResources(ManagedActors);
		}
	}

	UPCGPointData* CreateMetalOutputPointData(FPCGContext* Context, const FName PinLabel)
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
		const TArray<FGeneratedMetalField>& Fields,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateMetalOutputPointData(Context, MetalFieldPCGConstants::FieldBoundsPinLabel);
		FPCGMetadataAttribute<FName>* FieldTypeAttribute = PointData->Metadata->CreateAttribute<FName>(
			MetalFieldPCGConstants::FieldTypeAttributeName, NAME_None, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Fields.Num());
		for (int32 FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
		{
			const FGeneratedMetalField& Field = Fields[FieldIndex];
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(Field.Center);
			Point.BoundsMin = FVector(-Field.OuterRadius, -Field.OuterRadius, 0.0);
			Point.BoundsMax = FVector(Field.OuterRadius, Field.OuterRadius, 0.0);
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, FieldIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			FieldTypeAttribute->SetValue(Point.MetadataEntry, FieldTypeToName(Field.Type));
		}
	}

	void EmitPlacements(
		FPCGContext* Context,
		const TArray<FPlacedMetalActor>& Placements,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateMetalOutputPointData(Context, MetalFieldPCGConstants::PlacementsPinLabel);
		FPCGMetadataAttribute<int32>* FieldIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			MetalFieldPCGConstants::FieldIndexAttributeName, INDEX_NONE, false, false);
		FPCGMetadataAttribute<FName>* RoleAttribute = PointData->Metadata->CreateAttribute<FName>(
			MetalFieldPCGConstants::PlacementRoleAttributeName, NAME_None, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Placements.Num());
		for (int32 PlacementIndex = 0; PlacementIndex < Placements.Num(); ++PlacementIndex)
		{
			const FPlacedMetalActor& Placement = Placements[PlacementIndex];
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = Placement.Transform;
			Point.BoundsMin = Placement.LocalBounds.Min;
			Point.BoundsMax = Placement.LocalBounds.Max;
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, PlacementIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			FieldIndexAttribute->SetValue(Point.MetadataEntry, Placement.FieldIndex);
			RoleAttribute->SetValue(Point.MetadataEntry, PlacementRoleToName(Placement.Role));
		}
	}
}

bool FPCGCreateMetalFieldElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	using namespace MetalFieldPCGInternal;

	const UPCGCreateMetalFieldSettings* Settings = Context->GetInputSettings<UPCGCreateMetalFieldSettings>();
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
			LOCTEXT("NoCandidates", "CreateMetalField: the In pin contains no candidate points."));
		EmitFieldBounds(Context, {}, Settings->RandomSeed);
		EmitPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	FResolvedMetalAssets Assets;
	ResolveActorEntries(Context, World, Settings->MetalTowerActors, LOCTEXT("TowerRole", "metal tower"), Assets.Towers);
	ResolveActorEntries(Context, World, Settings->MetalTowerAuxiliaryActors,
		LOCTEXT("TowerAuxRole", "tower auxiliary"), Assets.TowerAuxiliaries);
	ResolveActorEntries(Context, World, Settings->MetalBuildingActors,
		LOCTEXT("BuildingRole", "metal building"), Assets.Buildings);
	ResolveFenceEntries(Context, World, Settings->MetalBuildingFences, Assets.Fences);
	ResolveActorEntries(Context, World, Settings->RegularMetalActors,
		LOCTEXT("RegularRole", "regular metal object"), Assets.RegularObjects);
	ResolveActorEntries(Context, World, Settings->RegularMetalAuxiliaryActors,
		LOCTEXT("RegularAuxRole", "regular metal auxiliary"), Assets.RegularAuxiliaries);

	if (Assets.Towers.IsEmpty() && Assets.Buildings.IsEmpty() && Assets.RegularObjects.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoPrimaryAssets",
			"CreateMetalField: no valid tower, building, or regular metal actors are configured; nothing to place."));
		EmitFieldBounds(Context, {}, Settings->RandomSeed);
		EmitPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	FRandomStream Random(Settings->RandomSeed);
	TArray<FGeneratedMetalField> Fields;
	TArray<FPlacedMetalActor> Placements;
	GenerateFields(Context, World, *Settings, MoveTemp(Candidates), CollectExclusions(Context),
		Assets, Random, Fields, Placements);

	SpawnAndRegisterPlacements(*SourceComponent, World, Placements);
	EmitFieldBounds(Context, Fields, Settings->RandomSeed);
	EmitPlacements(Context, Placements, Settings->RandomSeed);
	return true;
}

#undef LOCTEXT_NAMESPACE
