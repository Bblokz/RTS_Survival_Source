// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreateHedgehogCorners.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGCreateHedgehogCorners"

namespace HedgehogCornersConstants
{
	const FName CollectiveVolumePinLabel = TEXT("CollectiveVolume");
	const FName PlacementsPinLabel = TEXT("Placements");
	const FName CornerIndexAttributeName = TEXT("CornerIndex");

	constexpr double InspectionDepth = -100000.0;
	constexpr double MinimumUsableBoundsSize = 1.0;
	constexpr double MinimumScale = 0.01;
	constexpr double FullCircleDegrees = 360.0;
	constexpr int32 CornerCount = 4;
}

// All internal helpers live in a uniquely named namespace (never an anonymous one) so their generic
// names cannot collide with sibling PCG nodes when merged into the same unity-build translation unit.
namespace HedgehogCornersInternal
{
	/** @brief A hedgehog Blueprint resolved to a spawnable class plus the measured data placement needs. */
	struct FResolvedHedgehog
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		double Weight = 1.0;
		double MinScale = 1.0;
		double MaxScale = 1.0;
		double ZOffset = 0.0;
		double YawOffsetDegrees = 0.0;

		// Circumscribed footprint radius at max scale; the in-volume margin that keeps any yaw inside.
		double MaxCircumRadius = 0.0;
	};

	/** @brief One hedgehog chosen for spawning: its transform, class and the world box it occupies. */
	struct FPlacedHedgehog
	{
		UClass* ActorClass = nullptr;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		FTransform Transform = FTransform::Identity;
		FBox WorldBounds = FBox(EForceInit::ForceInit);
		int32 CornerIndex = INDEX_NONE;
	};

	/** @brief The collective world box enclosing every hedgehog placed for one corner. */
	struct FCornerCluster
	{
		FBox Bounds = FBox(EForceInit::ForceInit);
		int32 CornerIndex = INDEX_NONE;
		int32 Count = 0;
	};

	/** @brief One corner of the rectangle with the two inward edge directions meeting there. */
	struct FCornerDefinition
	{
		FVector2D Corner = FVector2D::ZeroVector;
		FVector2D InwardDirX = FVector2D(1.0, 0.0);
		FVector2D InwardDirY = FVector2D(0.0, 1.0);
	};

	// -----------------------------------------------------------------------
	// Blueprint measurement
	// -----------------------------------------------------------------------

	AActor* SpawnInspectionActor(UWorld& World, UClass& ActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		return World.SpawnActor<AActor>(
			&ActorClass, FTransform(FVector(0.0, 0.0, HedgehogCornersConstants::InspectionDepth)), SpawnParameters);
	}

	/** @brief Scales measured bounds so the requested footprint size is honored while preserving the pivot ratio. */
	void ApplyFootprintOverrides(FBox& InOutLocalBounds, const double OverrideX, const double OverrideY)
	{
		const FVector MeasuredSize = InOutLocalBounds.GetSize();
		if (OverrideX > 0.0 && MeasuredSize.X >= HedgehogCornersConstants::MinimumUsableBoundsSize)
		{
			const double PivotRelativeScaleX = OverrideX / MeasuredSize.X;
			InOutLocalBounds.Min.X *= PivotRelativeScaleX;
			InOutLocalBounds.Max.X *= PivotRelativeScaleX;
		}
		if (OverrideY > 0.0 && MeasuredSize.Y >= HedgehogCornersConstants::MinimumUsableBoundsSize)
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
		return BoundsSize.X >= HedgehogCornersConstants::MinimumUsableBoundsSize
			&& BoundsSize.Y >= HedgehogCornersConstants::MinimumUsableBoundsSize
			&& BoundsSize.Z >= HedgehogCornersConstants::MinimumUsableBoundsSize;
	}

	void ResolveHedgehogEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FHedgehogCornersEntry>& Entries,
		TArray<FResolvedHedgehog>& OutHedgehogs)
	{
		for (const FHedgehogCornersEntry& Entry : Entries)
		{
			UClass* ActorClass = Entry.HedgehogClass.LoadSynchronous();
			FBox LocalBounds(EForceInit::ForceInit);
			if (ActorClass == nullptr || not ActorClass->IsChildOf(AActor::StaticClass())
				|| not TryMeasureActorClass(
					World, *ActorClass, Entry.FootprintOverrideX, Entry.FootprintOverrideY, LocalBounds))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("InvalidHedgehog", "CreateHedgehogCorners: skipped an invalid or bounds-less hedgehog Blueprint."));
				continue;
			}

			FResolvedHedgehog& Resolved = OutHedgehogs.Emplace_GetRef();
			Resolved.ActorClass = ActorClass;
			Resolved.LocalBounds = LocalBounds;
			Resolved.Weight = FMath::Max(0.01, static_cast<double>(Entry.Weight));
			Resolved.MinScale = FMath::Max(HedgehogCornersConstants::MinimumScale,
				FMath::Min(Entry.MinUniformScale, Entry.MaxUniformScale));
			Resolved.MaxScale = FMath::Max(Resolved.MinScale,
				FMath::Max(Entry.MinUniformScale, Entry.MaxUniformScale));
			Resolved.ZOffset = Entry.ZOffset;
			Resolved.YawOffsetDegrees = Entry.YawOffsetDegrees;

			const FVector BoundsSize = LocalBounds.GetSize();
			Resolved.MaxCircumRadius = 0.5 * FVector2D(BoundsSize.X, BoundsSize.Y).Size() * Resolved.MaxScale;
		}
	}

	int32 PickWeightedHedgehogIndex(const TArray<FResolvedHedgehog>& Hedgehogs, FRandomStream& Random)
	{
		if (Hedgehogs.IsEmpty())
		{
			return INDEX_NONE;
		}
		double TotalWeight = 0.0;
		for (const FResolvedHedgehog& Hedgehog : Hedgehogs)
		{
			TotalWeight += Hedgehog.Weight;
		}
		if (TotalWeight <= 0.0)
		{
			return Random.RandRange(0, Hedgehogs.Num() - 1);
		}

		double Roll = Random.FRandRange(0.0f, static_cast<float>(TotalWeight));
		for (int32 HedgehogIndex = 0; HedgehogIndex < Hedgehogs.Num(); ++HedgehogIndex)
		{
			Roll -= Hedgehogs[HedgehogIndex].Weight;
			if (Roll <= 0.0)
			{
				return HedgehogIndex;
			}
		}
		return Hedgehogs.Num() - 1;
	}

	// -----------------------------------------------------------------------
	// Transforms and footprints
	// -----------------------------------------------------------------------

	/**
	 * @brief Builds a yaw-only transform whose measured bottom rests on the FloorZ plane (+ ZOffset) and
	 * whose XY pivot lands on DesiredWorldAnchor, so the hedgehog stands on the volume floor.
	 */
	FTransform MakeBoundsAlignedTransform(
		const FBox& LocalBounds,
		const FVector2D& DesiredWorldAnchor,
		const double FloorZ,
		const FQuat& Rotation,
		const double Scale,
		const double ZOffset)
	{
		const FVector ScaledCenter = LocalBounds.GetCenter() * Scale;
		const FVector RotatedAnchor = Rotation.RotateVector(ScaledCenter);
		double RotatedMinimumZ = TNumericLimits<double>::Max();
		for (int32 XIndex = 0; XIndex < 2; ++XIndex)
		{
			for (int32 YIndex = 0; YIndex < 2; ++YIndex)
			{
				for (int32 ZIndex = 0; ZIndex < 2; ++ZIndex)
				{
					const FVector LocalCorner(
						(XIndex == 0 ? LocalBounds.Min.X : LocalBounds.Max.X) * Scale,
						(YIndex == 0 ? LocalBounds.Min.Y : LocalBounds.Max.Y) * Scale,
						(ZIndex == 0 ? LocalBounds.Min.Z : LocalBounds.Max.Z) * Scale);
					RotatedMinimumZ = FMath::Min(RotatedMinimumZ, Rotation.RotateVector(LocalCorner).Z);
				}
			}
		}
		const FVector Location(
			DesiredWorldAnchor.X - RotatedAnchor.X,
			DesiredWorldAnchor.Y - RotatedAnchor.Y,
			FloorZ + ZOffset - RotatedMinimumZ);
		return FTransform(Rotation, Location, FVector(Scale));
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

	// -----------------------------------------------------------------------
	// Corner pattern generation
	// -----------------------------------------------------------------------

	/** @return The distance from Origin to the InteriorBox wall along the (axis-aligned) unit Direction. */
	double ReachToInteriorWall(const FVector2D& Origin, const FVector2D& Direction, const FBox2D& InteriorBox)
	{
		if (Direction.X > 0.5)
		{
			return InteriorBox.Max.X - Origin.X;
		}
		if (Direction.X < -0.5)
		{
			return Origin.X - InteriorBox.Min.X;
		}
		if (Direction.Y > 0.5)
		{
			return InteriorBox.Max.Y - Origin.Y;
		}
		return Origin.Y - InteriorBox.Min.Y;
	}

	/**
	 * @brief Lays one arm of hedgehogs from Origin along Direction. Steps by HedgeOffset up to the
	 * requested arm length, clamped so the last hedgehog center never passes the interior wall (which
	 * keeps its footprint inside the volume). Appends placements and grows the corner cluster box.
	 */
	void PlaceArm(
		const TArray<FResolvedHedgehog>& Hedgehogs,
		const FVector2D& Origin,
		const FVector2D& Direction,
		const FBox2D& InteriorBox,
		const double FloorZ,
		const double RequestedArmLength,
		const double HedgeOffset,
		const bool bIncludeOrigin,
		const int32 CornerIndex,
		FRandomStream& Random,
		TArray<FPlacedHedgehog>& OutPlacements,
		FCornerCluster& InOutCluster)
	{
		const double ArmLength = FMath::Min(RequestedArmLength, FMath::Max(0.0, ReachToInteriorWall(Origin, Direction, InteriorBox)));
		const int32 StepCount = FMath::FloorToInt32(ArmLength / HedgeOffset);
		for (int32 StepIndex = bIncludeOrigin ? 0 : 1; StepIndex <= StepCount; ++StepIndex)
		{
			const FVector2D Center = Origin + Direction * (StepIndex * HedgeOffset);
			const int32 HedgehogIndex = PickWeightedHedgehogIndex(Hedgehogs, Random);
			if (HedgehogIndex == INDEX_NONE)
			{
				return;
			}
			const FResolvedHedgehog& Hedgehog = Hedgehogs[HedgehogIndex];

			const double Scale = Random.FRandRange(static_cast<float>(Hedgehog.MinScale), static_cast<float>(Hedgehog.MaxScale));
			const double YawDegrees = Random.FRandRange(0.0f, static_cast<float>(HedgehogCornersConstants::FullCircleDegrees))
				+ Hedgehog.YawOffsetDegrees;
			const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));

			const FTransform Transform = MakeBoundsAlignedTransform(
				Hedgehog.LocalBounds, Center, FloorZ, Rotation, Scale, Hedgehog.ZOffset);

			const FBox2D Footprint = ComputeFootprint(Hedgehog.LocalBounds, Transform);
			const double BottomZ = FloorZ + Hedgehog.ZOffset;
			const double TopZ = BottomZ + Hedgehog.LocalBounds.GetSize().Z * Scale;

			FBox WorldBounds(EForceInit::ForceInit);
			WorldBounds += FVector(Footprint.Min.X, Footprint.Min.Y, BottomZ);
			WorldBounds += FVector(Footprint.Max.X, Footprint.Max.Y, TopZ);

			FPlacedHedgehog& Placement = OutPlacements.Emplace_GetRef();
			Placement.ActorClass = Hedgehog.ActorClass;
			Placement.LocalBounds = Hedgehog.LocalBounds;
			Placement.Transform = Transform;
			Placement.WorldBounds = WorldBounds;
			Placement.CornerIndex = CornerIndex;

			InOutCluster.Bounds += WorldBounds;
			++InOutCluster.Count;
		}
	}

	/**
	 * @brief Generates the four matching corner patterns for one rectangular volume. Each corner is
	 * inset inward by InwardsOffset (never closer to a wall than the largest hedgehog radius, so the
	 * footprint stays inside), then an L of two arms runs inward along the two meeting edges.
	 */
	void GenerateVolumeCorners(
		const UPCGCreateHedgehogCornersSettings& Settings,
		const TArray<FResolvedHedgehog>& Hedgehogs,
		const FBox& VolumeBounds,
		FRandomStream& Random,
		int32& InOutCornerBaseIndex,
		TArray<FPlacedHedgehog>& OutPlacements,
		TArray<FCornerCluster>& OutClusters)
	{
		double Margin = 0.0;
		for (const FResolvedHedgehog& Hedgehog : Hedgehogs)
		{
			Margin = FMath::Max(Margin, Hedgehog.MaxCircumRadius);
		}

		const FVector2D VolumeMin(VolumeBounds.Min.X, VolumeBounds.Min.Y);
		const FVector2D VolumeMax(VolumeBounds.Max.X, VolumeBounds.Max.Y);
		const FBox2D InteriorBox(VolumeMin + FVector2D(Margin), VolumeMax - FVector2D(Margin));
		if (InteriorBox.Min.X > InteriorBox.Max.X || InteriorBox.Min.Y > InteriorBox.Max.Y)
		{
			// The volume is smaller than a single hedgehog once the safety margin is reserved on both sides.
			return;
		}

		const double FloorZ = VolumeBounds.Min.Z;
		const double InwardsOffset = FMath::Max(0.0, static_cast<double>(Settings.InwardsOffset));
		const double RequestedArmLength = FMath::Max(0.0, static_cast<double>(Settings.TotalLongitudeSize));
		const double HedgeOffset = FMath::Max(1.0, static_cast<double>(Settings.HedgeOffset));

		const FCornerDefinition CornerDefinitions[HedgehogCornersConstants::CornerCount] = {
			{ FVector2D(VolumeMin.X, VolumeMin.Y), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0) },
			{ FVector2D(VolumeMax.X, VolumeMin.Y), FVector2D(-1.0, 0.0), FVector2D(0.0, 1.0) },
			{ FVector2D(VolumeMax.X, VolumeMax.Y), FVector2D(-1.0, 0.0), FVector2D(0.0, -1.0) },
			{ FVector2D(VolumeMin.X, VolumeMax.Y), FVector2D(1.0, 0.0), FVector2D(0.0, -1.0) }
		};

		for (int32 CornerSlot = 0; CornerSlot < HedgehogCornersConstants::CornerCount; ++CornerSlot)
		{
			const FCornerDefinition& Definition = CornerDefinitions[CornerSlot];
			FVector2D Origin = Definition.Corner + (Definition.InwardDirX + Definition.InwardDirY) * InwardsOffset;
			// Never let the inset origin sit closer to a wall than the safety margin (keeps footprints inside).
			Origin.X = FMath::Clamp(Origin.X, InteriorBox.Min.X, InteriorBox.Max.X);
			Origin.Y = FMath::Clamp(Origin.Y, InteriorBox.Min.Y, InteriorBox.Max.Y);

			const int32 CornerIndex = InOutCornerBaseIndex + CornerSlot;
			FCornerCluster Cluster;
			Cluster.CornerIndex = CornerIndex;

			// Arm along the X edge includes the shared corner hedgehog; the Y arm starts one step in.
			PlaceArm(Hedgehogs, Origin, Definition.InwardDirX, InteriorBox, FloorZ, RequestedArmLength,
				HedgeOffset, true, CornerIndex, Random, OutPlacements, Cluster);
			PlaceArm(Hedgehogs, Origin, Definition.InwardDirY, InteriorBox, FloorZ, RequestedArmLength,
				HedgeOffset, false, CornerIndex, Random, OutPlacements, Cluster);

			if (Cluster.Count > 0)
			{
				OutClusters.Add(Cluster);
			}
		}

		InOutCornerBaseIndex += HedgehogCornersConstants::CornerCount;
	}

	// -----------------------------------------------------------------------
	// Inputs
	// -----------------------------------------------------------------------

	TArray<FBox> CollectVolumeBounds(FPCGContext* Context)
	{
		TArray<FBox> VolumeBounds;
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (SpatialData == nullptr)
			{
				continue;
			}
			const FBox Bounds = SpatialData->GetBounds();
			if (Bounds.IsValid)
			{
				VolumeBounds.Add(Bounds);
			}
		}
		return VolumeBounds;
	}

	// -----------------------------------------------------------------------
	// Spawning and output
	// -----------------------------------------------------------------------

	AActor* SpawnHedgehogActor(UWorld& World, const FPlacedHedgehog& Placement, const int32 PlacementIndex)
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
		Actor->SetActorLabel(FString::Printf(TEXT("PCG_HedgehogCorner_%d_%d"), Placement.CornerIndex, PlacementIndex));
#endif
		return Actor;
	}

	void SpawnAndRegisterPlacements(
		UPCGComponent& SourceComponent,
		UWorld& World,
		const TArray<FPlacedHedgehog>& Placements)
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(&SourceComponent);
		for (int32 PlacementIndex = 0; PlacementIndex < Placements.Num(); ++PlacementIndex)
		{
			if (AActor* Actor = SpawnHedgehogActor(World, Placements[PlacementIndex], PlacementIndex))
			{
				ManagedActors->GeneratedActors.Add(Actor);
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

	/** @brief One point-with-bounds per corner cluster, enclosing every hedgehog placed at that corner. */
	void EmitCollectiveVolume(
		FPCGContext* Context,
		const TArray<FCornerCluster>& Clusters,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateOutputPointData(Context, HedgehogCornersConstants::CollectiveVolumePinLabel);
		FPCGMetadataAttribute<int32>* CornerIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			HedgehogCornersConstants::CornerIndexAttributeName, INDEX_NONE, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Clusters.Num());
		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
		{
			const FCornerCluster& Cluster = Clusters[ClusterIndex];
			const FVector Center = Cluster.Bounds.GetCenter();
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(Center);
			Point.BoundsMin = Cluster.Bounds.Min - Center;
			Point.BoundsMax = Cluster.Bounds.Max - Center;
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, Cluster.CornerIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			CornerIndexAttribute->SetValue(Point.MetadataEntry, Cluster.CornerIndex);
		}
	}

	/** @brief One oriented point-with-bounds per placed hedgehog, its bounds the measured local bounds. */
	void EmitPlacements(
		FPCGContext* Context,
		const TArray<FPlacedHedgehog>& Placements,
		const int32 RandomSeed)
	{
		UPCGPointData* PointData = CreateOutputPointData(Context, HedgehogCornersConstants::PlacementsPinLabel);
		FPCGMetadataAttribute<int32>* CornerIndexAttribute = PointData->Metadata->CreateAttribute<int32>(
			HedgehogCornersConstants::CornerIndexAttributeName, INDEX_NONE, false, false);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Placements.Num());
		for (int32 PlacementIndex = 0; PlacementIndex < Placements.Num(); ++PlacementIndex)
		{
			const FPlacedHedgehog& Placement = Placements[PlacementIndex];
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = Placement.Transform;
			Point.BoundsMin = Placement.LocalBounds.Min;
			Point.BoundsMax = Placement.LocalBounds.Max;
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, PlacementIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			CornerIndexAttribute->SetValue(Point.MetadataEntry, Placement.CornerIndex);
		}
	}
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FName UPCGCreateHedgehogCornersSettings::GetDefaultNodeName() const
{
	return FName(TEXT("CreateHedgehogCorners"));
}

FText UPCGCreateHedgehogCornersSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Hedgehog Corners");
}

FText UPCGCreateHedgehogCornersSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Places anti-tank hedgehogs in a matching L-shaped pattern at each of the four corners of the input "
		"rectangular volume. From each corner the pattern is inset inward by Inwards Offset, then two arms run "
		"inward along the two edges; each arm is Total Longitude Size long with one hedgehog every Hedge Offset. "
		"Each hedgehog is weighted-picked from the Hedgehogs list and given a random yaw, and every placement is "
		"clamped to stay inside the volume. Placements outputs one point per hedgehog; CollectiveVolume outputs "
		"one point-with-bounds per corner. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGCreateHedgehogCornersSettings::CreateElement() const
{
	return MakeShared<FPCGCreateHedgehogCornersElement>();
}

TArray<FPCGPinProperties> UPCGCreateHedgehogCornersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// The rectangular volume (or any spatial data) whose four corners receive a hedgehog pattern.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreateHedgehogCornersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(HedgehogCornersConstants::CollectiveVolumePinLabel, EPCGDataType::Point);
	Pins.Emplace(HedgehogCornersConstants::PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

// ---------------------------------------------------------------------------
// Element
// ---------------------------------------------------------------------------

bool FPCGCreateHedgehogCornersElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	using namespace HedgehogCornersInternal;

	const UPCGCreateHedgehogCornersSettings* Settings = Context->GetInputSettings<UPCGCreateHedgehogCornersSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	const TArray<FBox> VolumeBounds = CollectVolumeBounds(Context);
	if (VolumeBounds.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("NoVolume", "CreateHedgehogCorners: the In pin contains no valid spatial (volume) data."));
		EmitCollectiveVolume(Context, {}, Settings->RandomSeed);
		EmitPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	TArray<FResolvedHedgehog> Hedgehogs;
	ResolveHedgehogEntries(Context, World, Settings->Hedgehogs, Hedgehogs);
	if (Hedgehogs.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoHedgehogs",
			"CreateHedgehogCorners: no valid hedgehog Blueprints are configured; nothing to place."));
		EmitCollectiveVolume(Context, {}, Settings->RandomSeed);
		EmitPlacements(Context, {}, Settings->RandomSeed);
		return true;
	}

	FRandomStream Random(Settings->RandomSeed);
	int32 CornerBaseIndex = 0;
	TArray<FPlacedHedgehog> Placements;
	TArray<FCornerCluster> Clusters;
	for (const FBox& Bounds : VolumeBounds)
	{
		GenerateVolumeCorners(*Settings, Hedgehogs, Bounds, Random, CornerBaseIndex, Placements, Clusters);
	}

	SpawnAndRegisterPlacements(*SourceComponent, World, Placements);
	EmitCollectiveVolume(Context, Clusters, Settings->RandomSeed);
	EmitPlacements(Context, Placements, Settings->RandomSeed);
	return true;
}

#undef LOCTEXT_NAMESPACE
