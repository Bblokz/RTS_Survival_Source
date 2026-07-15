// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGEnemyBaseSetupShared.h"

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
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "PCGEnemyBaseSetup"

namespace EnemyBaseSetupShared
{
	namespace Pins
	{
		const FName TotalBounds = TEXT("TotalBounds");
		const FName Bunkers = TEXT("Bunkers");
		const FName Sandbags = TEXT("Sandbags");
		const FName Obstacles = TEXT("Obstacles");
		const FName BarbedWire = TEXT("BarbedWire");
		const FName Decorators = TEXT("Decorators");
		const FName Foliage = TEXT("Foliage");
		const FName OccupiedBounds = TEXT("OccupiedBounds");

		const FName AimTarget = TEXT("AimTarget");
		const FName Exclusion = TEXT("Exclusion");

		const FName AssetIndexAttr = TEXT("AssetIndex");
		const FName ClusterAttr = TEXT("Cluster");
		const FName CategoryAttr = TEXT("Category");
		const FName MeshAttr = TEXT("Mesh");
	}
}

namespace EnemyBaseSetupSharedConstants
{
	constexpr double MeasureSpawnDepth = -100000.0;
	constexpr double DefaultFootprintHalfExtent = 200.0;
	constexpr double DefaultBuildingHeight = 400.0;

	constexpr double GroundTraceUp = 10000.0;
	constexpr double GroundTraceDown = 30000.0;

	constexpr double OccupancyBoundsHalfHeight = 50.0;
	constexpr double ClusterBoundsHalfHeight = 500.0;

	constexpr double DecalProjectionDepth = 800.0;
	constexpr double DecalSurfaceOffset = 5.0;
}

// ---------------------------------------------------------------------------
// Internal helpers (bounds measurement, ground projection, spawning primitives)
// ---------------------------------------------------------------------------

namespace
{

	AActor* EBS_SpawnInspectionActor(UWorld& World, UClass& ActorClass)
	{
		const FVector SpawnLocation(0.0, 0.0, EnemyBaseSetupSharedConstants::MeasureSpawnDepth);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;

		return World.SpawnActor<AActor>(&ActorClass, FTransform(SpawnLocation), SpawnParams);
	}

	FBox EBS_MeasureInspectionBounds(const AActor& InspectionActor)
	{
		const FBox WorldBounds = InspectionActor.GetComponentsBoundingBox(true, true);
		if (not WorldBounds.IsValid)
		{
			return FBox(EForceInit::ForceInit);
		}
		return WorldBounds.ShiftBy(-InspectionActor.GetActorLocation());
	}

	FCollisionQueryParams EBS_MakeGroundTraceParams(const TArray<AActor*>& ActorsToIgnore)
	{
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActors(ActorsToIgnore);
		return TraceParams;
	}

	FVector EBS_ProjectToGround(
		UWorld& World,
		const FVector2D& Position,
		const FCollisionQueryParams& TraceParams,
		const double ZOffset)
	{
		const FVector WorldPosition(Position, 0.0);
		FHitResult GroundHit;
		const FVector TraceStart = WorldPosition + FVector::UpVector * EnemyBaseSetupSharedConstants::GroundTraceUp;
		const FVector TraceEnd = WorldPosition - FVector::UpVector * EnemyBaseSetupSharedConstants::GroundTraceDown;
		if (World.LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
		{
			return GroundHit.ImpactPoint + FVector::UpVector * ZOffset;
		}
		return WorldPosition + FVector::UpVector * ZOffset;
	}

	AActor* EBS_SpawnActorAtWorld(
		UWorld& World,
		UClass* ActorClass,
		const FVector2D& Position,
		const double YawRadians,
		const double UniformScale,
		TArray<AActor*>& OutSpawnedActors)
	{
		if (ActorClass == nullptr)
		{
			return nullptr;
		}

		const FCollisionQueryParams TraceParams = EBS_MakeGroundTraceParams(OutSpawnedActors);
		const FVector WorldPosition = EBS_ProjectToGround(World, Position, TraceParams, 0.0);
		const FQuat WorldRotation(FVector::UpVector, YawRadians);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Actor = World.SpawnActor<AActor>(
			ActorClass, FTransform(WorldRotation, WorldPosition, FVector(UniformScale)), SpawnParams);
		if (not IsValid(Actor))
		{
			return nullptr;
		}
		Actor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
		OutSpawnedActors.Add(Actor);
		return Actor;
	}

	AActor* EBS_SpawnManagedEmptyActor(UWorld& World, const FString& Label)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (not IsValid(Actor))
		{
			return nullptr;
		}
		Actor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		Actor->SetActorLabel(Label);
#endif
		return Actor;
	}

	UPCGPointData* EBS_CreateOutputPointData(FPCGContext* Context, const FName PinLabel)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		PointData->InitializeFromData(nullptr);

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = PinLabel;
		return PointData;
	}

	/** @brief Fills a resolved entry's footprint from an override or the measured local bounds. */
	void EBS_FillEntryFootprint(
		const FEnemyBlueprintEntry& Settings,
		const FBox& MeasuredLocalBounds,
		FEnemyResolvedEntry& OutEntry,
		FBox& OutLocalBounds)
	{
		if (Settings.bUseFootprintOverride)
		{
			OutEntry.FootprintHalfExtents = Settings.FootprintOverride * 0.5;
			OutEntry.PivotToFootprintCenter = FVector2D::ZeroVector;
			OutLocalBounds = FBox(
				FVector(-OutEntry.FootprintHalfExtents.X, -OutEntry.FootprintHalfExtents.Y, 0.0),
				FVector(OutEntry.FootprintHalfExtents.X, OutEntry.FootprintHalfExtents.Y, EnemyBaseSetupSharedConstants::DefaultBuildingHeight));
			return;
		}

		if (MeasuredLocalBounds.IsValid)
		{
			const FVector Extent = MeasuredLocalBounds.GetExtent();
			const FVector Center = MeasuredLocalBounds.GetCenter();
			OutEntry.FootprintHalfExtents = FVector2D(Extent.X, Extent.Y);
			OutEntry.PivotToFootprintCenter = FVector2D(Center.X, Center.Y);
			OutLocalBounds = MeasuredLocalBounds;
			return;
		}

		OutEntry.FootprintHalfExtents = FVector2D(EnemyBaseSetupSharedConstants::DefaultFootprintHalfExtent, EnemyBaseSetupSharedConstants::DefaultFootprintHalfExtent);
		OutEntry.PivotToFootprintCenter = FVector2D::ZeroVector;
		OutLocalBounds = FBox(
			FVector(-EnemyBaseSetupSharedConstants::DefaultFootprintHalfExtent, -EnemyBaseSetupSharedConstants::DefaultFootprintHalfExtent, 0.0),
			FVector(EnemyBaseSetupSharedConstants::DefaultFootprintHalfExtent, EnemyBaseSetupSharedConstants::DefaultFootprintHalfExtent, EnemyBaseSetupSharedConstants::DefaultBuildingHeight));
	}
}

// ---------------------------------------------------------------------------
// Asset resolution
// ---------------------------------------------------------------------------

void EnemyBaseSetupShared::ResolveBlueprintList(
	FPCGContext* Context,
	UWorld& World,
	const TArray<FEnemyBlueprintEntry>& List,
	FEnemyCategoryAssets& InOutAssets,
	TArray<FEnemyResolvedEntry>& OutEntries)
{
	for (int32 SettingsIndex = 0; SettingsIndex < List.Num(); ++SettingsIndex)
	{
		const FEnemyBlueprintEntry& Settings = List[SettingsIndex];
		UClass* BlueprintClass = Settings.BlueprintClass.LoadSynchronous();
		if (BlueprintClass == nullptr)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context,
				LOCTEXT("NullEntryClass", "EnemyBaseSetup: a Blueprint entry has no valid class and was skipped."));
			continue;
		}

		FEnemyResolvedEntry Resolved;
		Resolved.SettingsIndex = SettingsIndex;
		Resolved.Weight = Settings.Weight;
		Resolved.bOverrideScale = Settings.bOverrideScale;
		Resolved.MinScale = Settings.MinScale;
		Resolved.MaxScale = FMath::Max(Settings.MinScale, Settings.MaxScale);

		FBox MeasuredLocalBounds(EForceInit::ForceInit);
		if (not Settings.bUseFootprintOverride)
		{
			AActor* InspectionActor = EBS_SpawnInspectionActor(World, *BlueprintClass);
			if (IsValid(InspectionActor))
			{
				MeasuredLocalBounds = EBS_MeasureInspectionBounds(*InspectionActor);
				InspectionActor->Destroy();
			}
		}

		FBox LocalBounds(EForceInit::ForceInit);
		EBS_FillEntryFootprint(Settings, MeasuredLocalBounds, Resolved, LocalBounds);

		Resolved.OutputAssetIndex = InOutAssets.Classes.Add(BlueprintClass);
		InOutAssets.LocalBounds.Add(LocalBounds);
		OutEntries.Add(Resolved);
	}
}

void EnemyBaseSetupShared::ResolveFoliage(
	const TArray<FEnemyFoliageEntry>& List,
	TArray<FSoftObjectPath>& OutPaths)
{
	OutPaths.Reset();
	for (const FEnemyFoliageEntry& Entry : List)
	{
		OutPaths.Add(Entry.Mesh.ToSoftObjectPath());
	}
}

void EnemyBaseSetupShared::ResolveDecals(
	const TArray<FEnemyDecalEntry>& List,
	TArray<UMaterialInterface*>& OutMaterials)
{
	OutMaterials.Reset();
	for (const FEnemyDecalEntry& Entry : List)
	{
		OutMaterials.Add(Entry.Material.LoadSynchronous());
	}
}

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------

TArray<EnemyBaseSetupShared::FEnemyInputPoint> EnemyBaseSetupShared::CollectInputPoints(
	FPCGContext* Context,
	const FName Pin)
{
	TArray<FEnemyInputPoint> Points;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(Pin))
	{
		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
		if (PointData == nullptr)
		{
			continue;
		}
		for (const FPCGPoint& Point : PointData->GetPoints())
		{
			FEnemyInputPoint& Added = Points.Emplace_GetRef();
			Added.Position = FVector2D(Point.Transform.GetLocation());
			Added.YawRadians = FMath::DegreesToRadians(Point.Transform.GetRotation().Rotator().Yaw);
		}
	}
	return Points;
}

TArray<FVector2D> EnemyBaseSetupShared::CollectAimTargets(FPCGContext* Context, const FName Pin)
{
	TArray<FVector2D> Targets;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(Pin))
	{
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data))
		{
			for (const FPCGPoint& Point : PointData->GetPoints())
			{
				Targets.Add(FVector2D(Point.Transform.GetLocation()));
			}
			continue;
		}
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
		{
			const FBox Bounds = SpatialData->GetBounds();
			if (Bounds.IsValid)
			{
				Targets.Add(FVector2D(Bounds.GetCenter()));
			}
		}
	}
	return Targets;
}

TArray<FBox> EnemyBaseSetupShared::CollectSpatialBounds(FPCGContext* Context, const FName Pin)
{
	TArray<FBox> Bounds;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(Pin))
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
		if (SpatialData == nullptr)
		{
			continue;
		}

		if (const UPCGPointData* PointData = Cast<UPCGPointData>(SpatialData))
		{
			for (const FPCGPoint& Point : PointData->GetPoints())
			{
				const FBox PointBounds = FBox(Point.BoundsMin, Point.BoundsMax).TransformBy(Point.Transform);
				if (PointBounds.IsValid)
				{
					Bounds.Add(PointBounds);
				}
			}
			continue;
		}

		const FBox SpatialBounds = SpatialData->GetBounds();
		if (SpatialBounds.IsValid)
		{
			Bounds.Add(SpatialBounds);
		}
	}
	return Bounds;
}

TFunction<bool(const FVector2D&)> EnemyBaseSetupShared::BuildExclusionPredicate(
	FPCGContext* Context,
	const FName Pin)
{
	TArray<const UPCGSpatialData*> ExclusionData;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(Pin))
	{
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
		{
			ExclusionData.Add(SpatialData);
		}
	}

	if (ExclusionData.IsEmpty())
	{
		return nullptr;
	}

	return [ExclusionData = MoveTemp(ExclusionData)](const FVector2D& Position) -> bool
	{
		const FVector WorldPosition(Position, 0.0);
		const FBox SampleBounds(FVector(-50.0), FVector(50.0));
		for (const UPCGSpatialData* SpatialData : ExclusionData)
		{
			FPCGPoint SampledPoint;
			if (SpatialData->SamplePoint(FTransform(WorldPosition), SampleBounds, SampledPoint, nullptr)
				&& SampledPoint.Density > 0.0f)
			{
				return true;
			}
		}
		return false;
	};
}

double EnemyBaseSetupShared::ComputeFacingYaw(
	const EEnemyFacingMode Mode,
	const FVector2D& Origin,
	const double PointYaw,
	const double FixedYawDegrees,
	const TArray<FVector2D>& AimTargets)
{
	if (Mode == EEnemyFacingMode::UsePointRotation)
	{
		return PointYaw;
	}
	if (Mode == EEnemyFacingMode::FixedYaw)
	{
		return FMath::DegreesToRadians(FixedYawDegrees);
	}

	if (AimTargets.IsEmpty())
	{
		return PointYaw;
	}

	FVector2D NearestTarget = AimTargets[0];
	double NearestDistanceSquared = FVector2D::DistSquared(Origin, NearestTarget);
	for (const FVector2D& Target : AimTargets)
	{
		const double DistanceSquared = FVector2D::DistSquared(Origin, Target);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Target;
		}
	}

	const FVector2D Direction = NearestTarget - Origin;
	if (Direction.IsNearlyZero())
	{
		return PointYaw;
	}

	const double Yaw = FMath::Atan2(Direction.Y, Direction.X);
	return Mode == EEnemyFacingMode::AwayFromTarget ? Yaw + PI : Yaw;
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

void EnemyBaseSetupShared::SpawnSetupActors(
	UWorld& World,
	const FEnemyBaseSetupResult& Result,
	const FEnemyBaseSetupAssets& Assets,
	TArray<AActor*>& OutSpawnedActors)
{
	for (const FEnemyPlacedActor& Actor : Result.Actors)
	{
		const FEnemyCategoryAssets& CategoryAssets = Assets.Category(Actor.Category);
		if (not CategoryAssets.Classes.IsValidIndex(Actor.AssetIndex))
		{
			continue;
		}
		EBS_SpawnActorAtWorld(
			World, CategoryAssets.Classes[Actor.AssetIndex], Actor.Position, Actor.YawRadians,
			Actor.UniformScale, OutSpawnedActors);
	}
}

void EnemyBaseSetupShared::SpawnDecalsActor(
	UWorld& World,
	const FEnemyBaseSetupResult& Result,
	const FEnemyBaseSetupAssets& Assets,
	TArray<AActor*>& OutSpawnedActors)
{
	if (Result.Decals.IsEmpty())
	{
		return;
	}

	AActor* Anchor = EBS_SpawnManagedEmptyActor(World, TEXT("PCG_EnemyBaseDecals"));
	if (Anchor == nullptr)
	{
		return;
	}
	OutSpawnedActors.Add(Anchor);

	USceneComponent* Root = NewObject<USceneComponent>(Anchor);
	Anchor->SetRootComponent(Root);
	Anchor->AddInstanceComponent(Root);
	Root->RegisterComponent();

	const FCollisionQueryParams TraceParams;
	for (const FEnemyDecalSpawn& Decal : Result.Decals)
	{
		if (not Assets.DecalMaterials.IsValidIndex(Decal.EntryIndex))
		{
			continue;
		}
		UMaterialInterface* Material = Assets.DecalMaterials[Decal.EntryIndex];
		if (Material == nullptr)
		{
			continue;
		}

		const FVector WorldPosition = EBS_ProjectToGround(World, Decal.Position, TraceParams, EnemyBaseSetupSharedConstants::DecalSurfaceOffset);
		const FRotator WorldRotation(-90.0, FMath::RadiansToDegrees(Decal.YawRadians), 0.0);

		UDecalComponent* DecalComponent = NewObject<UDecalComponent>(Anchor);
		DecalComponent->SetMobility(EComponentMobility::Static);
		DecalComponent->SetDecalMaterial(Material);
		DecalComponent->DecalSize = FVector(EnemyBaseSetupSharedConstants::DecalProjectionDepth, Decal.Size, Decal.Size);
		DecalComponent->SetWorldTransform(FTransform(WorldRotation, WorldPosition));
		DecalComponent->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
		Anchor->AddInstanceComponent(DecalComponent);
		DecalComponent->RegisterComponent();
	}
}

// ---------------------------------------------------------------------------
// Outputs
// ---------------------------------------------------------------------------

void EnemyBaseSetupShared::EmitCategoryOutput(
	FPCGContext* Context,
	UWorld& World,
	const FEnemyBaseSetupResult& Result,
	const FEnemyBaseSetupAssets& Assets,
	const EEnemyPlacementCategory Category,
	const FName PinLabel,
	const int32 RandomSeed,
	const TArray<AActor*>& ActorsToIgnore)
{
	UPCGPointData* PointData = EBS_CreateOutputPointData(Context, PinLabel);
	FPCGMetadataAttribute<int32>* AssetIndexAttribute =
		PointData->Metadata->CreateAttribute<int32>(Pins::AssetIndexAttr, 0, false, false);
	FPCGMetadataAttribute<int32>* ClusterAttribute =
		PointData->Metadata->CreateAttribute<int32>(Pins::ClusterAttr, 0, false, false);

	const FEnemyCategoryAssets& CategoryAssets = Assets.Category(Category);
	const FCollisionQueryParams TraceParams = EBS_MakeGroundTraceParams(ActorsToIgnore);
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

	int32 EmittedIndex = 0;
	for (const FEnemyPlacedActor& Actor : Result.Actors)
	{
		if (Actor.Category != Category || not CategoryAssets.LocalBounds.IsValidIndex(Actor.AssetIndex))
		{
			continue;
		}
		const FBox& LocalBounds = CategoryAssets.LocalBounds[Actor.AssetIndex];

		FPCGPoint& Point = Points.Emplace_GetRef();
		Point.Transform = FTransform(
			FQuat(FVector::UpVector, Actor.YawRadians),
			EBS_ProjectToGround(World, Actor.Position, TraceParams, 0.0),
			FVector(Actor.UniformScale));
		Point.BoundsMin = LocalBounds.Min;
		Point.BoundsMax = LocalBounds.Max;
		Point.Density = 1.0f;
		Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, EmittedIndex);

		PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
		AssetIndexAttribute->SetValue(Point.MetadataEntry, Actor.AssetIndex);
		ClusterAttribute->SetValue(Point.MetadataEntry, Actor.ClusterIndex);
		++EmittedIndex;
	}
}

void EnemyBaseSetupShared::EmitTotalBoundsOutput(
	FPCGContext* Context,
	UWorld& World,
	const FEnemyBaseSetupResult& Result,
	const FName PinLabel,
	const int32 RandomSeed,
	const TArray<AActor*>& ActorsToIgnore)
{
	UPCGPointData* PointData = EBS_CreateOutputPointData(Context, PinLabel);
	FPCGMetadataAttribute<int32>* ClusterAttribute =
		PointData->Metadata->CreateAttribute<int32>(Pins::ClusterAttr, 0, false, false);

	const FCollisionQueryParams TraceParams = EBS_MakeGroundTraceParams(ActorsToIgnore);
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

	for (int32 ClusterIndex = 0; ClusterIndex < Result.NumClusters; ++ClusterIndex)
	{
		FBox2D Bounds(ForceInit);
		for (const FEnemyPlacedActor& Actor : Result.Actors)
		{
			if (Actor.ClusterIndex == ClusterIndex)
			{
				Bounds += Actor.Footprint.GetAABB();
			}
		}
		for (const FEnemyFoliageSpawn& Foliage : Result.Foliage)
		{
			if (Foliage.ClusterIndex == ClusterIndex)
			{
				Bounds += Foliage.Position;
			}
		}
		for (const FEnemyDecalSpawn& Decal : Result.Decals)
		{
			if (Decal.ClusterIndex == ClusterIndex)
			{
				Bounds += Decal.Position;
			}
		}

		if (not Bounds.bIsValid)
		{
			continue;
		}

		const FVector2D Center = Bounds.GetCenter();
		const FVector2D Extent = Bounds.GetExtent();

		FPCGPoint& Point = Points.Emplace_GetRef();
		Point.Transform = FTransform(EBS_ProjectToGround(World, Center, TraceParams, 0.0));
		Point.BoundsMin = FVector(-Extent.X, -Extent.Y, -EnemyBaseSetupSharedConstants::ClusterBoundsHalfHeight);
		Point.BoundsMax = FVector(Extent.X, Extent.Y, EnemyBaseSetupSharedConstants::ClusterBoundsHalfHeight);
		Point.Density = 1.0f;
		Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, ClusterIndex);

		PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
		ClusterAttribute->SetValue(Point.MetadataEntry, ClusterIndex);
	}
}

void EnemyBaseSetupShared::EmitOccupiedBoundsOutput(
	FPCGContext* Context,
	UWorld& World,
	const FEnemyBaseSetupResult& Result,
	const FName PinLabel,
	const int32 RandomSeed,
	const TArray<AActor*>& ActorsToIgnore)
{
	UPCGPointData* PointData = EBS_CreateOutputPointData(Context, PinLabel);
	FPCGMetadataAttribute<int32>* CategoryAttribute =
		PointData->Metadata->CreateAttribute<int32>(Pins::CategoryAttr, 0, false, false);

	const FCollisionQueryParams TraceParams = EBS_MakeGroundTraceParams(ActorsToIgnore);
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
	Points.Reserve(Result.Actors.Num());

	for (int32 ActorIndex = 0; ActorIndex < Result.Actors.Num(); ++ActorIndex)
	{
		const FEnemyPlacedActor& Actor = Result.Actors[ActorIndex];

		FPCGPoint& Point = Points.Emplace_GetRef();
		Point.Transform = FTransform(
			FQuat(FVector::UpVector, Actor.Footprint.YawRadians),
			EBS_ProjectToGround(World, Actor.Footprint.Center, TraceParams, 0.0));
		Point.BoundsMin = FVector(-Actor.Footprint.HalfExtents.X, -Actor.Footprint.HalfExtents.Y, -EnemyBaseSetupSharedConstants::OccupancyBoundsHalfHeight);
		Point.BoundsMax = FVector(Actor.Footprint.HalfExtents.X, Actor.Footprint.HalfExtents.Y, EnemyBaseSetupSharedConstants::OccupancyBoundsHalfHeight);
		Point.Density = 1.0f;
		Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, ActorIndex);

		PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
		CategoryAttribute->SetValue(Point.MetadataEntry, static_cast<int32>(Actor.Category));
	}
}

void EnemyBaseSetupShared::EmitFoliageOutput(
	FPCGContext* Context,
	UWorld& World,
	const FEnemyBaseSetupResult& Result,
	const FEnemyBaseSetupAssets& Assets,
	const FName PinLabel,
	const int32 RandomSeed,
	const TArray<AActor*>& ActorsToIgnore)
{
	UPCGPointData* PointData = EBS_CreateOutputPointData(Context, PinLabel);
	FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute =
		PointData->Metadata->CreateAttribute<FSoftObjectPath>(
			Pins::MeshAttr, FSoftObjectPath(), false, false);

	const FCollisionQueryParams TraceParams = EBS_MakeGroundTraceParams(ActorsToIgnore);
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

	int32 EmittedIndex = 0;
	for (const FEnemyFoliageSpawn& Foliage : Result.Foliage)
	{
		if (not Assets.FoliageMeshPaths.IsValidIndex(Foliage.MeshEntryIndex))
		{
			continue;
		}
		const FSoftObjectPath& MeshPath = Assets.FoliageMeshPaths[Foliage.MeshEntryIndex];
		if (MeshPath.IsNull())
		{
			continue;
		}

		FVector WorldPosition(Foliage.Position, 0.0);
		FQuat WorldRotation(FVector::UpVector, Foliage.YawRadians);

		FHitResult GroundHit;
		const FVector TraceStart = WorldPosition + FVector::UpVector * EnemyBaseSetupSharedConstants::GroundTraceUp;
		const FVector TraceEnd = WorldPosition - FVector::UpVector * EnemyBaseSetupSharedConstants::GroundTraceDown;
		if (World.LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
		{
			WorldPosition.Z = GroundHit.ImpactPoint.Z;
			if (Foliage.bAlignToGround)
			{
				WorldRotation = FQuat::FindBetweenNormals(FVector::UpVector, GroundHit.ImpactNormal) * WorldRotation;
			}
		}

		FPCGPoint& Point = Points.Emplace_GetRef();
		Point.Transform = FTransform(WorldRotation, WorldPosition, FVector(Foliage.UniformScale));
		Point.Density = 1.0f;
		Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, EmittedIndex);

		PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
		MeshAttribute->SetValue(Point.MetadataEntry, MeshPath);
		++EmittedIndex;
	}
}

void EnemyBaseSetupShared::RegisterManagedActors(FPCGContext* Context, const TArray<AActor*>& SpawnedActors)
{
	if (SpawnedActors.IsEmpty())
	{
		return;
	}

	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (not IsValid(SourceComponent))
	{
		return;
	}

	UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
	for (AActor* SpawnedActor : SpawnedActors)
	{
		ManagedActors->GeneratedActors.Add(SpawnedActor);
	}
	SourceComponent->AddToManagedResources(ManagedActors);
}

#undef LOCTEXT_NAMESPACE
