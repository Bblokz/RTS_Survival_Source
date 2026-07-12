// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGScorchedCity.h"

#include "ScorchedCityGenerator.h"

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
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/NavCollision/NavCollisionEnvObstacle/RTSNavCollisionEnvObstacle.h"

#define LOCTEXT_NAMESPACE "PCGScorchedCity"

namespace ScorchedCityPCGConstants
{
	const FName ExclusionPinLabel = TEXT("Exclusion");
	const FName ScatterPinLabel = TEXT("Scatter");
	const FName OccupiedBoundsPinLabel = TEXT("OccupiedBounds");
	const FName LotsPinLabel = TEXT("Lots");
	const FName OuterOrphanRoadsPinLabel = TEXT("OuterOrphanRoads");

	const FName ScatterMeshAttributeName = TEXT("Mesh");
	const FName OccupancyTypeAttributeName = TEXT("Occupancy");
	const FName LotUsedAttributeName = TEXT("Used");

	// Fallbacks when meshes/classes cannot be measured.
	constexpr double DefaultRoadMeshLength = 400.0;
	constexpr double DefaultBuildingHalfExtent = 400.0;
	constexpr double DefaultBuildingHeight = 800.0;
	constexpr double DefaultScatterMeshRadius = 50.0;

	// Ground alignment trace range around the city plane; generous so tall landscapes still hit.
	constexpr double GroundTraceUp = 10000.0;
	constexpr double GroundTraceDown = 30000.0;
	constexpr double RoadGroundSampleSpacing = 1200.0;
	constexpr double DefaultIntersectionRoadWidthMultiplier = 1.5;

	// Z half-thickness given to exported occupancy bounds points.
	constexpr double OccupancyBoundsHalfHeight = 50.0;
	constexpr double DecalProjectionDepth = 800.0;
	constexpr double DecalSurfaceOffset = 5.0;

	constexpr double MeasureSpawnDepth = -100000.0;
}

namespace
{
	/** @brief Signed city yaw in radians; FQuat::GetAngle() would lose the sign for negative yaws. */
	double CityYawRadians(const FTransform& CityToWorld)
	{
		return FMath::DegreesToRadians(CityToWorld.GetRotation().Rotator().Yaw);
	}
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

UPCGScorchedCitySettings::UPCGScorchedCitySettings()
{
	FScorchedDecalEntry BuildingFootprintEntry;
	BuildingFootprintEntry.MinScale = 1200.0f;
	BuildingFootprintEntry.MaxScale = 2400.0f;
	BuildingFootprintDecals.Decals.Add(BuildingFootprintEntry);
}

#if WITH_EDITOR
FName UPCGScorchedCitySettings::GetDefaultNodeName() const
{
	return FName(TEXT("ScorchedCity"));
}

FText UPCGScorchedCitySettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Scorched City");
}

FText UPCGScorchedCitySettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Generates a scorched city inside the input spatial area (typically a closed spline loop): grid and "
		"curved roads, bounds-aware buildings on generated lots, power lines and scatter debris. The city size "
		"derives from the area's bounds and the layout is trimmed to its shape. Roads, intersections, "
		"buildings and poles are spawned as managed actors; Scatter outputs points with a 'Mesh' attribute "
		"for a Static Mesh Spawner. OccupiedBounds outputs every reserved footprint for downstream exclusion. "
		"Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGScorchedCitySettings::CreateElement() const
{
	return MakeShared<FPCGScorchedCityElement>();
}

TArray<FPCGPinProperties> UPCGScorchedCitySettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// The city area (closed spline loop or other spatial data); only the first data is used.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	// Optional exclusion areas: buildings and scatter are never placed where these sample > 0.
	Pins.Emplace(ScorchedCityPCGConstants::ExclusionPinLabel, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGScorchedCitySettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(ScorchedCityPCGConstants::ScatterPinLabel, EPCGDataType::Point);
	Pins.Emplace(ScorchedCityPCGConstants::OccupiedBoundsPinLabel, EPCGDataType::Point);
	Pins.Emplace(ScorchedCityPCGConstants::LotsPinLabel, EPCGDataType::Point);
	// Every unconnected road ending (dead stops, failed pairings, open 4-way arms); point
	// rotation = outward yaw, so other PCG logic can continue those roads.
	Pins.Emplace(ScorchedCityPCGConstants::OuterOrphanRoadsPinLabel, EPCGDataType::Point);
	return Pins;
}

// ---------------------------------------------------------------------------
// Asset resolving
// ---------------------------------------------------------------------------

namespace
{
	/** @brief One static mesh of a scorch building, relative to the building actor's pivot. */
	struct FScorchedBuildingMeshInstance
	{
		UStaticMesh* Mesh = nullptr;
		FTransform RelativeTransform = FTransform::Identity;
	};

	/** @brief Runtime-resolved assets, parallel to the generator's resolved parameter arrays. */
	struct FScorchedCityAssets
	{
		// Parallel to FScorchedCityGenParams::Buildings.
		TArray<UClass*> BuildingClasses;
		TArray<EScorchedBuildingCategory> BuildingCategories;
		// Extracted meshes per entry; only filled for ScorchBuilding entries (HISM batching).
		TArray<TArray<FScorchedBuildingMeshInstance>> BuildingMeshInstances;
		// Measured local 3D bounds per entry, used by Scorch building nav and trace volumes.
		TArray<FBox> BuildingLocalBounds;

		UStaticMesh* RoadMesh = nullptr;
		UStaticMesh* IntersectionMesh = nullptr;
		UClass* TwoPoleClass = nullptr;
		UClass* SinglePoleClass = nullptr;

		// Parallel to FScorchedCityGenParams::AuxiliaryBlueprints.
		TArray<UClass*> AuxiliaryClasses;

		// Parallel to FScorchedCityGenParams::RoadLanterns / RoadBlockTypes.
		TArray<UClass*> RoadLanternClasses;
		TArray<UClass*> RoadBlockClasses;

		// Parallel to FScorchedCityGenParams::ScatterProfiles / their Meshes arrays.
		TArray<TArray<FSoftObjectPath>> ScatterMeshPaths;

		TArray<UMaterialInterface*> LotDecalMaterials;
		TArray<UMaterialInterface*> BuildingFootprintDecalMaterials;
		TArray<UMaterialInterface*> RoadDecalMaterials;
		TArray<UMaterialInterface*> DestroyedPowerLineDecalMaterials;
		TArray<UMaterialInterface*> PowerLineDecalMaterials;
	};

	/**
	 * @brief Spawns a transient instance of a class far below the world so its real bounds and
	 * static mesh composition can be inspected; exact for Blueprints incl. construction scripts.
	 * The caller must Destroy() the returned actor.
	 */
	AActor* SpawnTransientInspectionActor(UWorld& World, UClass& ActorClass)
	{
		const FVector SpawnLocation(0.0, 0.0, ScorchedCityPCGConstants::MeasureSpawnDepth);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;

		return World.SpawnActor<AActor>(&ActorClass, FTransform(SpawnLocation), SpawnParams);
	}

	FBox ComputeInspectionActorLocalBounds(const AActor& InspectionActor)
	{
		const FBox WorldBounds = InspectionActor.GetComponentsBoundingBox(true, true);
		if (not WorldBounds.IsValid)
		{
			return FBox(EForceInit::ForceInit);
		}
		return WorldBounds.ShiftBy(-InspectionActor.GetActorLocation());
	}

	/**
	 * @brief Collects every static mesh of the actor (plain components and ISM/HISM instances)
	 * with its transform relative to the actor pivot, so scorch buildings can be re-rendered
	 * as shared Hierarchical ISM instances instead of per-building actors.
	 */
	void ExtractStaticMeshInstances(const AActor& InspectionActor, TArray<FScorchedBuildingMeshInstance>& OutInstances)
	{
		const FTransform WorldToActor = InspectionActor.GetActorTransform().Inverse();

		TArray<UStaticMeshComponent*> MeshComponents;
		InspectionActor.GetComponents<UStaticMeshComponent>(MeshComponents);

		for (const UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
			if (Mesh == nullptr)
			{
				continue;
			}

			if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(MeshComponent))
			{
				for (int32 InstanceIndex = 0; InstanceIndex < Instanced->GetInstanceCount(); ++InstanceIndex)
				{
					FTransform InstanceTransform;
					if (Instanced->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
					{
						OutInstances.Add({Mesh, InstanceTransform * WorldToActor});
					}
				}
				continue;
			}

			OutInstances.Add({Mesh, MeshComponent->GetComponentTransform() * WorldToActor});
		}
	}

	void ResolveBuildingAssets(
		FPCGContext* Context,
		const UPCGScorchedCitySettings* Settings,
		UWorld& World,
		FScorchedCityGenParams& Params,
		FScorchedCityAssets& Assets)
	{
		using namespace ScorchedCityPCGConstants;

		for (int32 SettingsIndex = 0; SettingsIndex < Settings->Buildings.Num(); ++SettingsIndex)
		{
			const FScorchedBuildingAssetSettings& Entry = Settings->Buildings[SettingsIndex];
			UClass* BuildingClass = Entry.BuildingClass.LoadSynchronous();
			if (BuildingClass == nullptr)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("NullBuildingClass", "ScorchedCity: a building entry has no valid class and was skipped."));
				continue;
			}

			FScorchedResolvedBuilding Resolved;
			Resolved.SettingsIndex = SettingsIndex;
			Resolved.Weight = Entry.Weight;
			Resolved.MinSpacing = Entry.MinSpacing;
			Resolved.MaxSpacing = FMath::Max(Entry.MinSpacing, Entry.MaxSpacing);
			Resolved.RotationMode = Entry.RotationMode;
			Resolved.SizeCategory = Entry.SizeCategory;
			Resolved.ZonePreference = Entry.ZonePreference;

			// One transient instance serves both bounds measurement and, for scorch buildings,
			// extraction of the static meshes that get batched into shared HISM components.
			EScorchedBuildingCategory Category = Entry.Category;
			TArray<FScorchedBuildingMeshInstance> MeshInstances;
			const bool bNeedsInspection =
				Category == EScorchedBuildingCategory::ScorchBuilding || not Entry.bUseFootprintOverride;
			AActor* InspectionActor = bNeedsInspection
				? SpawnTransientInspectionActor(World, *BuildingClass)
				: nullptr;

			const FBox MeasuredLocalBounds = IsValid(InspectionActor)
				? ComputeInspectionActorLocalBounds(*InspectionActor)
				: FBox(EForceInit::ForceInit);

			if (Entry.bUseFootprintOverride)
			{
				Resolved.FootprintHalfExtents = Entry.FootprintOverride * 0.5;
				Resolved.PivotToFootprintCenter = FVector2D::ZeroVector;
			}
			else
			{
				if (MeasuredLocalBounds.IsValid)
				{
					const FVector Extent = MeasuredLocalBounds.GetExtent();
					const FVector Center = MeasuredLocalBounds.GetCenter();
					Resolved.FootprintHalfExtents = FVector2D(Extent.X, Extent.Y);
					Resolved.PivotToFootprintCenter = FVector2D(Center.X, Center.Y);
				}
				else
				{
					PCGE_LOG_C(Warning, GraphAndLog, Context,
						LOCTEXT("UnmeasurableBuilding", "ScorchedCity: could not measure a building's bounds; using a default footprint. Consider a footprint override."));
					Resolved.FootprintHalfExtents = FVector2D(DefaultBuildingHalfExtent, DefaultBuildingHalfExtent);
				}
			}

			if (Category == EScorchedBuildingCategory::ScorchBuilding)
			{
				if (IsValid(InspectionActor))
				{
					ExtractStaticMeshInstances(*InspectionActor, MeshInstances);
				}
				if (MeshInstances.IsEmpty())
				{
					PCGE_LOG_C(Warning, GraphAndLog, Context,
						LOCTEXT("ScorchBuildingNoMeshes",
							"ScorchedCity: a ScorchBuilding entry has no static meshes to batch; spawning it as a Blueprint actor instead."));
					Category = EScorchedBuildingCategory::BlueprintBuilding;
				}
			}

			if (IsValid(InspectionActor))
			{
				InspectionActor->Destroy();
			}

			Params.Buildings.Add(Resolved);
			Assets.BuildingClasses.Add(BuildingClass);
			Assets.BuildingCategories.Add(Category);
			Assets.BuildingMeshInstances.Add(MoveTemp(MeshInstances));
			Assets.BuildingLocalBounds.Add(MeasuredLocalBounds.IsValid
				? MeasuredLocalBounds
				: FBox(
					FVector(-Resolved.FootprintHalfExtents.X, -Resolved.FootprintHalfExtents.Y, 0.0),
					FVector(Resolved.FootprintHalfExtents.X, Resolved.FootprintHalfExtents.Y, DefaultBuildingHeight)));
		}
	}

	void ResolveRoadAndPowerAssets(
		FPCGContext* Context,
		const UPCGScorchedCitySettings* Settings,
		UWorld& World,
		FScorchedCityGenParams& Params,
		FScorchedCityAssets& Assets)
	{
		using namespace ScorchedCityPCGConstants;

		Assets.RoadMesh = Settings->RoadMesh.LoadSynchronous();
		Assets.IntersectionMesh = Settings->IntersectionMesh.LoadSynchronous();
		Assets.TwoPoleClass = Settings->PowerLineSettings.TwoPoleAsset.LoadSynchronous();
		Assets.SinglePoleClass = Settings->PowerLineSettings.SinglePoleAsset.LoadSynchronous();

		Params.RoadMeshLength = Settings->RoadMeshLengthOverride > 0.0f
			? Settings->RoadMeshLengthOverride
			: (Assets.RoadMesh != nullptr
				? FMath::Max(100.0, Assets.RoadMesh->GetBoundingBox().GetSize().X)
				: DefaultRoadMeshLength);

		// The 4-way asset may be non-square; keep its real X and Y footprint so road splines
		// trim flush against the matching axis instead of overlapping the intersection piece.
		// Overrides are authoritative. Derived (mesh-bounds) values are capped at one grid
		// block: scorched meshes often carry overhanging props that inflate their bounds far
		// beyond the walkable footprint, which would otherwise starve lots and power lines.
		const double FallbackIntersectionSize = Settings->RoadWidth * DefaultIntersectionRoadWidthMultiplier;
		const FVector IntersectionMeshSize = Assets.IntersectionMesh != nullptr
			? Assets.IntersectionMesh->GetBoundingBox().GetSize()
			: FVector(FallbackIntersectionSize, FallbackIntersectionSize, 0.0);

		const double MaxDerivedSize = Settings->GridBlockSize;
		Params.IntersectionSizeX = Settings->IntersectionSizeOverrideX > 0.0f
			? Settings->IntersectionSizeOverrideX
			: FMath::Clamp(IntersectionMeshSize.X, static_cast<double>(Settings->RoadWidth), MaxDerivedSize);
		Params.IntersectionSizeY = Settings->IntersectionSizeOverrideY > 0.0f
			? Settings->IntersectionSizeOverrideY
			: FMath::Clamp(IntersectionMeshSize.Y, static_cast<double>(Settings->RoadWidth), MaxDerivedSize);

		const bool bDerivedClamped =
			(Settings->IntersectionSizeOverrideX <= 0.0f && IntersectionMeshSize.X > MaxDerivedSize)
			|| (Settings->IntersectionSizeOverrideY <= 0.0f && IntersectionMeshSize.Y > MaxDerivedSize);
		if (Context != nullptr && bDerivedClamped)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context,
				LOCTEXT("IntersectionBoundsClamped",
					"ScorchedCity: the intersection mesh bounds exceed one grid block and were clamped. "
					"Set IntersectionSizeOverrideX/Y to the asset's real walkable footprint for exact spline trimming."));
		}
	}

	void ResolveScatterAssets(
		const UPCGScorchedCitySettings* Settings,
		FScorchedCityGenParams& Params,
		FScorchedCityAssets& Assets)
	{
		using namespace ScorchedCityPCGConstants;

		for (const FScorchedScatterProfile& Profile : Settings->ScatterProfiles)
		{
			// Sanitized copy: null meshes removed so generator indices always map to real meshes.
			FScorchedScatterProfile Sanitized = Profile;
			Sanitized.Meshes.Reset();

			TArray<double> Radii;
			TArray<FSoftObjectPath> Paths;

			for (const FScorchedScatterMeshEntry& Entry : Profile.Meshes)
			{
				const UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();
				if (Mesh == nullptr)
				{
					continue;
				}

				Sanitized.Meshes.Add(Entry);
				const FVector Extent = Mesh->GetBoundingBox().GetExtent();
				Radii.Add(FMath::Max(DefaultScatterMeshRadius * 0.1, FMath::Max(Extent.X, Extent.Y)));
				Paths.Add(Entry.Mesh.ToSoftObjectPath());
			}

			Params.ScatterProfiles.Add(MoveTemp(Sanitized));
			Params.ScatterMeshRadii.Add(MoveTemp(Radii));
			Assets.ScatterMeshPaths.Add(MoveTemp(Paths));
		}
	}

	void ResolveDecalSettings(
		const FScorchedDecalPlacementSettings& Source,
		FScorchedDecalPlacementSettings& Target,
		TArray<UMaterialInterface*>& TargetMaterials)
	{
		// The per-category multiplier is folded into the resolved density; the generator
		// must never apply it a second time.
		Target.Density = Source.Density * FMath::Clamp(Source.CountMultiplier, 1.0f, 20.0f);
		Target.CountMultiplier = 1.0f;
		Target.Scatter = Source.Scatter;
		Target.Decals.Reset();
		TargetMaterials.Reset();

		for (const FScorchedDecalEntry& Entry : Source.Decals)
		{
			UMaterialInterface* Material = Entry.Material.LoadSynchronous();
			if (Material == nullptr)
			{
				continue;
			}

			Target.Decals.Add(Entry);
			TargetMaterials.Add(Material);
		}
	}

	void ResolveAuxiliaryAssets(
		FPCGContext* Context,
		const UPCGScorchedCitySettings* Settings,
		UWorld& World,
		FScorchedCityGenParams& Params,
		FScorchedCityAssets& Assets)
	{
		using namespace ScorchedCityPCGConstants;

		for (int32 SettingsIndex = 0; SettingsIndex < Settings->AuxiliaryBlueprints.Num(); ++SettingsIndex)
		{
			const FScorchedAuxiliaryBlueprintSettings& Entry = Settings->AuxiliaryBlueprints[SettingsIndex];
			UClass* AuxiliaryClass = Entry.BlueprintClass.LoadSynchronous();
			if (AuxiliaryClass == nullptr)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("NullAuxiliaryClass", "ScorchedCity: an auxiliary blueprint entry has no valid class and was skipped."));
				continue;
			}

			FScorchedResolvedAuxiliary Resolved;
			Resolved.SettingsIndex = SettingsIndex;
			Resolved.CountPerBuilding = Entry.CountPerBuilding;
			Resolved.CountPerLot = Entry.CountPerLot;
			Resolved.bCheckBuildingCollision = Entry.bCheckBuildingCollision;
			Resolved.bCheckPoleCollision = Entry.bCheckPoleCollision;
			Resolved.bOverrideScale = Entry.bOverrideScale;
			Resolved.MinScale = Entry.MinScale;
			Resolved.MaxScale = FMath::Max(Entry.MinScale, Entry.MaxScale);
			Resolved.MinDistanceFromBuilding = Entry.MinDistanceFromBuilding;
			Resolved.MaxDistanceFromBuilding =
				FMath::Max(Entry.MinDistanceFromBuilding, Entry.MaxDistanceFromBuilding);

			// Real bounds so collision checks use the blueprint's true footprint.
			AActor* InspectionActor = SpawnTransientInspectionActor(World, *AuxiliaryClass);
			if (IsValid(InspectionActor))
			{
				const FBox LocalBounds = ComputeInspectionActorLocalBounds(*InspectionActor);
				if (LocalBounds.IsValid)
				{
					const FVector Extent = LocalBounds.GetExtent();
					const FVector Center = LocalBounds.GetCenter();
					Resolved.FootprintHalfExtents = FVector2D(Extent.X, Extent.Y);
					Resolved.PivotToFootprintCenter = FVector2D(Center.X, Center.Y);
				}
				InspectionActor->Destroy();
			}

			Params.AuxiliaryBlueprints.Add(Resolved);
			Assets.AuxiliaryClasses.Add(AuxiliaryClass);
		}
	}

	/** @brief Resolves one road-side blueprint list (lanterns or road block types). */
	void ResolveRoadSideEntries(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FScorchedRoadSideBlueprintEntry>& Entries,
		TArray<FScorchedResolvedRoadSideEntry>& OutResolved,
		TArray<UClass*>& OutClasses)
	{
		for (int32 SettingsIndex = 0; SettingsIndex < Entries.Num(); ++SettingsIndex)
		{
			UClass* EntryClass = Entries[SettingsIndex].BlueprintClass.LoadSynchronous();
			if (EntryClass == nullptr)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("NullRoadSideClass", "ScorchedCity: a road-side blueprint entry has no valid class and was skipped."));
				continue;
			}

			FScorchedResolvedRoadSideEntry Resolved;
			Resolved.SettingsIndex = SettingsIndex;
			Resolved.Weight = Entries[SettingsIndex].Weight;

			AActor* InspectionActor = SpawnTransientInspectionActor(World, *EntryClass);
			if (IsValid(InspectionActor))
			{
				const FBox LocalBounds = ComputeInspectionActorLocalBounds(*InspectionActor);
				if (LocalBounds.IsValid)
				{
					const FVector Extent = LocalBounds.GetExtent();
					const FVector Center = LocalBounds.GetCenter();
					Resolved.FootprintHalfExtents = FVector2D(Extent.X, Extent.Y);
					Resolved.PivotToFootprintCenter = FVector2D(Center.X, Center.Y);
				}
				InspectionActor->Destroy();
			}

			OutResolved.Add(Resolved);
			OutClasses.Add(EntryClass);
		}
	}

	void ResolveRoadSideAssets(
		FPCGContext* Context,
		const UPCGScorchedCitySettings* Settings,
		UWorld& World,
		FScorchedCityGenParams& Params,
		FScorchedCityAssets& Assets)
	{
		ResolveRoadSideEntries(Context, World, Settings->RoadLanterns.Blueprints,
			Params.RoadLanterns, Assets.RoadLanternClasses);
		Params.RoadLanternSpacing = Settings->RoadLanterns.Spacing;
		Params.RoadLanternOffsetFromRoadEdge = Settings->RoadLanterns.OffsetFromRoadEdge;

		ResolveRoadSideEntries(Context, World, Settings->RoadBlocks.Blueprints,
			Params.RoadBlockTypes, Assets.RoadBlockClasses);
		Params.RoadBlockInterval = Settings->RoadBlocks.Interval;
		Params.RoadBlockChance = Settings->RoadBlocks.Chance;
		Params.RoadBlockMinYawSpanDegrees = Settings->RoadBlocks.MinYawSpanDegrees;
		Params.RoadBlockMaxYawSpanDegrees =
			FMath::Max(Settings->RoadBlocks.MinYawSpanDegrees, Settings->RoadBlocks.MaxYawSpanDegrees);
	}

	void ResolveDecalAssets(
		const UPCGScorchedCitySettings* Settings,
		FScorchedCityGenParams& Params,
		FScorchedCityAssets& Assets)
	{
		ResolveDecalSettings(Settings->LotDecals, Params.LotDecals, Assets.LotDecalMaterials);
		ResolveDecalSettings(
			Settings->BuildingFootprintDecals,
			Params.BuildingFootprintDecals,
			Assets.BuildingFootprintDecalMaterials);
		ResolveDecalSettings(Settings->RoadDecals, Params.RoadDecals, Assets.RoadDecalMaterials);
		ResolveDecalSettings(
			Settings->DestroyedPowerLineDecals,
			Params.DestroyedPowerLineDecals,
			Assets.DestroyedPowerLineDecalMaterials);
		ResolveDecalSettings(Settings->PowerLineDecals, Params.PowerLineDecals, Assets.PowerLineDecalMaterials);
	}

	TFunction<bool(const FVector2D&)> BuildExclusionFunction(FPCGContext* Context, const FTransform& CityToWorld)
	{
		TArray<const UPCGSpatialData*> ExclusionData;
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetInputsByPin(ScorchedCityPCGConstants::ExclusionPinLabel);
		for (const FPCGTaggedData& Input : Inputs)
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

		return [ExclusionData = MoveTemp(ExclusionData), CityToWorld](const FVector2D& LocalPosition) -> bool
		{
			const FVector WorldPosition = CityToWorld.TransformPosition(FVector(LocalPosition, 0.0));
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
}

// ---------------------------------------------------------------------------
// Actor spawning
// ---------------------------------------------------------------------------

namespace
{
	AActor* SpawnManagedEmptyActor(UWorld& World, const FTransform& Transform, const FString& Label)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), Transform, SpawnParams);
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

	FCollisionQueryParams MakeGroundTraceParams(const TArray<AActor*>& ActorsToIgnore)
	{
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActors(ActorsToIgnore);
		return TraceParams;
	}

	FVector ProjectWorldPositionToGround(
		UWorld& World,
		const FVector& WorldPosition,
		const FCollisionQueryParams& TraceParams,
		const double ZOffset)
	{
		FHitResult GroundHit;
		const FVector TraceStart = WorldPosition + FVector::UpVector * ScorchedCityPCGConstants::GroundTraceUp;
		const FVector TraceEnd = WorldPosition - FVector::UpVector * ScorchedCityPCGConstants::GroundTraceDown;
		if (World.LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
		{
			return GroundHit.ImpactPoint + FVector::UpVector * ZOffset;
		}

		return WorldPosition + FVector::UpVector * ZOffset;
	}

	FVector ProjectCityPositionToGround(
		UWorld& World,
		const FVector2D& LocalPosition,
		const FTransform& CityToWorld,
		const FCollisionQueryParams& TraceParams,
		const double ZOffset)
	{
		return ProjectWorldPositionToGround(
			World,
			CityToWorld.TransformPosition(FVector(LocalPosition, 0.0)),
			TraceParams,
			ZOffset);
	}

	void AddGroundedRoadSegmentPoints(
		UWorld& World,
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd,
		const FTransform& CityToWorld,
		const FCollisionQueryParams& TraceParams,
		const double RoadZOffset,
		const bool bIncludeStart,
		TArray<FVector>& OutWorldPoints)
	{
		using namespace ScorchedCityPCGConstants;

		const double SegmentLength = FVector2D::Distance(SegmentStart, SegmentEnd);
		const int32 NumSamples = FMath::Max(1, FMath::CeilToInt32(SegmentLength / RoadGroundSampleSpacing));
		const int32 FirstSample = bIncludeStart ? 0 : 1;
		for (int32 SampleIndex = FirstSample; SampleIndex <= NumSamples; ++SampleIndex)
		{
			const double Alpha = static_cast<double>(SampleIndex) / static_cast<double>(NumSamples);
			const FVector2D LocalPosition = FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
			OutWorldPoints.Add(ProjectCityPositionToGround(
				World, LocalPosition, CityToWorld, TraceParams, RoadZOffset));
		}
	}

	TArray<FVector> BuildGroundedRoadWorldPoints(
		UWorld& World,
		const FScorchedRoadSplineResult& Road,
		const FTransform& CityToWorld,
		const FCollisionQueryParams& TraceParams,
		const double RoadZOffset)
	{
		TArray<FVector> WorldPoints;
		if (Road.Points.Num() < 2)
		{
			return WorldPoints;
		}

		double RoadLength = 0.0;
		for (int32 PointIndex = 1; PointIndex < Road.Points.Num(); ++PointIndex)
		{
			RoadLength += FVector2D::Distance(Road.Points[PointIndex - 1], Road.Points[PointIndex]);
		}

		WorldPoints.Reserve(FMath::Max(
			Road.Points.Num(),
			FMath::CeilToInt32(RoadLength / ScorchedCityPCGConstants::RoadGroundSampleSpacing) + 1));

		for (int32 PointIndex = 1; PointIndex < Road.Points.Num(); ++PointIndex)
		{
			AddGroundedRoadSegmentPoints(
				World,
				Road.Points[PointIndex - 1],
				Road.Points[PointIndex],
				CityToWorld,
				TraceParams,
				RoadZOffset,
				PointIndex == 1,
				WorldPoints);
		}
		return WorldPoints;
	}

	/** @brief One actor per road: a spline (linear for grid streets) with chained spline meshes. */
	void SpawnRoadSplineActor(
		UWorld& World,
		const FScorchedRoadSplineResult& Road,
		const FTransform& CityToWorld,
		UStaticMesh* RoadMesh,
		double RoadMeshLength,
		double RoadZOffset,
		TArray<AActor*>& OutSpawnedActors)
	{
		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(OutSpawnedActors);
		const TArray<FVector> WorldPoints = BuildGroundedRoadWorldPoints(
			World, Road, CityToWorld, TraceParams, RoadZOffset);
		if (WorldPoints.Num() < 2)
		{
			return;
		}

		const FVector FirstWorldPoint = WorldPoints[0];
		AActor* RoadActor = SpawnManagedEmptyActor(World, FTransform(FirstWorldPoint), TEXT("PCG_ScorchedRoad"));
		if (RoadActor == nullptr)
		{
			return;
		}
		OutSpawnedActors.Add(RoadActor);

		USplineComponent* Spline = NewObject<USplineComponent>(RoadActor);
		RoadActor->SetRootComponent(Spline);
		RoadActor->AddInstanceComponent(Spline);
		Spline->RegisterComponent();

		Spline->ClearSplinePoints(false);
		for (const FVector& WorldPoint : WorldPoints)
		{
			Spline->AddSplinePoint(WorldPoint, ESplineCoordinateSpace::World, false);
		}
		if (not Road.bCurved)
		{
			for (int32 PointIndex = 0; PointIndex < Spline->GetNumberOfSplinePoints(); ++PointIndex)
			{
				Spline->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
			}
		}
		Spline->UpdateSpline();

		if (RoadMesh == nullptr)
		{
			return;
		}

		const double SplineLength = Spline->GetSplineLength();
		const int32 NumChunks = FMath::Max(1, FMath::RoundToInt32(SplineLength / RoadMeshLength));
		const double ChunkLength = SplineLength / NumChunks;

		for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)
		{
			const double StartDistance = ChunkIndex * ChunkLength;
			const double EndDistance = (ChunkIndex + 1) * ChunkLength;

			USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(RoadActor);
			SplineMesh->SetMobility(EComponentMobility::Static);
			SplineMesh->SetStaticMesh(RoadMesh);
			SplineMesh->SetForwardAxis(ESplineMeshAxis::X, false);
			SplineMesh->AttachToComponent(Spline, FAttachmentTransformRules::KeepRelativeTransform);
			RoadActor->AddInstanceComponent(SplineMesh);

			const FVector StartPosition = Spline->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
			const FVector EndPosition = Spline->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);
			const FVector StartTangent = Spline->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local)
				.GetSafeNormal() * ChunkLength;
			const FVector EndTangent = Spline->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local)
				.GetSafeNormal() * ChunkLength;

			SplineMesh->SetStartAndEnd(StartPosition, StartTangent, EndPosition, EndTangent, false);
			SplineMesh->RegisterComponent();
		}
	}

	/** @brief All 4-way intersection pieces share one ISM component for cheap rendering. */
	void SpawnIntersectionsActor(
		UWorld& World,
		const TArray<FScorchedIntersectionSpawn>& Intersections,
		const FTransform& CityToWorld,
		UStaticMesh* IntersectionMesh,
		double RoadZOffset,
		TArray<AActor*>& OutSpawnedActors)
	{
		if (Intersections.IsEmpty() || IntersectionMesh == nullptr)
		{
			return;
		}

		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(OutSpawnedActors);
		AActor* Anchor = SpawnManagedEmptyActor(
			World, FTransform(CityToWorld.GetLocation()), TEXT("PCG_ScorchedIntersections"));
		if (Anchor == nullptr)
		{
			return;
		}
		OutSpawnedActors.Add(Anchor);

		UInstancedStaticMeshComponent* InstancedMeshes = NewObject<UInstancedStaticMeshComponent>(Anchor);
		Anchor->SetRootComponent(InstancedMeshes);
		Anchor->AddInstanceComponent(InstancedMeshes);
		InstancedMeshes->SetMobility(EComponentMobility::Static);
		InstancedMeshes->SetStaticMesh(IntersectionMesh);
		InstancedMeshes->RegisterComponent();

		const double CityYaw = CityYawRadians(CityToWorld);
		for (const FScorchedIntersectionSpawn& Intersection : Intersections)
		{
			const FVector WorldPosition = ProjectCityPositionToGround(
				World, Intersection.Position, CityToWorld, TraceParams, RoadZOffset);
			const FQuat WorldRotation(FVector::UpVector, CityYaw + Intersection.YawRadians);
			InstancedMeshes->AddInstance(FTransform(WorldRotation, WorldPosition), true);
		}
	}

	/**
	 * @brief All scorch buildings of the city are rendered by ONE actor holding one
	 * Hierarchical Instanced Static Mesh component per unique mesh: the same batching a
	 * designer would do by hand, done automatically. Pivots are ground-projected like actors.
	 */
	void AddScorchBuildingNavigationAndCollision(
		AActor& Anchor,
		USceneComponent& Root,
		const FTransform& BuildingTransform,
		const FBox& LocalBounds,
		const double CollisionBoundsScale)
	{
		const FVector BoundsCenter = BuildingTransform.TransformPosition(LocalBounds.GetCenter());
		const FQuat BoundsRotation = BuildingTransform.GetRotation();

		UBoxComponent* TraceCollision = NewObject<UBoxComponent>(&Anchor);
		TraceCollision->SetMobility(EComponentMobility::Static);
		TraceCollision->SetBoxExtent(LocalBounds.GetExtent() * CollisionBoundsScale, false);
		TraceCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TraceCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
		TraceCollision->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
		TraceCollision->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
		TraceCollision->SetGenerateOverlapEvents(false);
		TraceCollision->SetSimulatePhysics(false);
		TraceCollision->SetCanEverAffectNavigation(false);
		TraceCollision->AttachToComponent(&Root, FAttachmentTransformRules::KeepWorldTransform);
		Anchor.AddInstanceComponent(TraceCollision);
		TraceCollision->SetWorldLocationAndRotation(BoundsCenter, BoundsRotation);
		TraceCollision->RegisterComponent();

		URTSNavCollisionEnvObstacle* NavigationModifier =
			NewObject<URTSNavCollisionEnvObstacle>(&Anchor);
		Anchor.AddInstanceComponent(NavigationModifier);
		NavigationModifier->SetAreaClass(UNavArea_Obstacle::StaticClass());
		NavigationModifier->RegisterComponent();
		NavigationModifier->SetUpNavModifierVolume(BoundsCenter, LocalBounds.GetExtent(), BoundsRotation);
	}

	void SpawnScorchBuildingsActor(
		UWorld& World,
		const FScorchedCityGenResult& Result,
		const FScorchedCityAssets& Assets,
		const FTransform& CityToWorld,
		const float CollisionBoundsPercent,
		TArray<AActor*>& OutSpawnedActors)
	{
		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(OutSpawnedActors);
		const double CityYaw = CityYawRadians(CityToWorld);

		// Gather world transforms per unique mesh (TMap keeps insertion order: deterministic).
		TMap<UStaticMesh*, TArray<FTransform>> MeshToTransforms;
		TArray<TPair<FTransform, FBox>> BuildingPlacements;
		for (const FScorchedBuildingSpawn& Building : Result.Buildings)
		{
			if (not Assets.BuildingCategories.IsValidIndex(Building.AssetIndex)
				|| not Assets.BuildingMeshInstances.IsValidIndex(Building.AssetIndex)
				|| not Assets.BuildingLocalBounds.IsValidIndex(Building.AssetIndex)
				|| Assets.BuildingCategories[Building.AssetIndex] != EScorchedBuildingCategory::ScorchBuilding)
			{
				continue;
			}

			// Same placement rules as actor buildings: pivot projected onto the landscape.
			const FVector WorldPosition = ProjectCityPositionToGround(
				World, Building.ActorPosition, CityToWorld, TraceParams, 0.0);
			const FTransform BuildingTransform(
				FQuat(FVector::UpVector, CityYaw + Building.YawRadians), WorldPosition);
			BuildingPlacements.Emplace(BuildingTransform, Assets.BuildingLocalBounds[Building.AssetIndex]);

			for (const FScorchedBuildingMeshInstance& MeshInstance : Assets.BuildingMeshInstances[Building.AssetIndex])
			{
				MeshToTransforms.FindOrAdd(MeshInstance.Mesh).Add(MeshInstance.RelativeTransform * BuildingTransform);
			}
		}

		if (MeshToTransforms.IsEmpty())
		{
			return;
		}

		AActor* Anchor = SpawnManagedEmptyActor(
			World, FTransform(CityToWorld.GetLocation()), TEXT("PCG_ScorchedBuildings"));
		if (Anchor == nullptr)
		{
			return;
		}
		OutSpawnedActors.Add(Anchor);

		USceneComponent* Root = NewObject<USceneComponent>(Anchor);
		Anchor->SetRootComponent(Root);
		Anchor->AddInstanceComponent(Root);
		Root->RegisterComponent();

		for (TPair<UStaticMesh*, TArray<FTransform>>& MeshEntry : MeshToTransforms)
		{
			UHierarchicalInstancedStaticMeshComponent* InstancedMeshes =
				NewObject<UHierarchicalInstancedStaticMeshComponent>(Anchor);
			InstancedMeshes->SetMobility(EComponentMobility::Static);
			InstancedMeshes->SetStaticMesh(MeshEntry.Key);
			InstancedMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			InstancedMeshes->SetGenerateOverlapEvents(false);
			InstancedMeshes->SetCanEverAffectNavigation(false);
			InstancedMeshes->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			Anchor->AddInstanceComponent(InstancedMeshes);
			InstancedMeshes->RegisterComponent();
			// One bulk add per mesh so the HISM cluster tree builds once.
			InstancedMeshes->AddInstances(MeshEntry.Value, false, true);
		}

		const double CollisionBoundsScale = FMath::Clamp(CollisionBoundsPercent, 1.0f, 100.0f) / 100.0;
		for (const TPair<FTransform, FBox>& BuildingPlacement : BuildingPlacements)
		{
			AddScorchBuildingNavigationAndCollision(
				*Anchor, *Root, BuildingPlacement.Key, BuildingPlacement.Value, CollisionBoundsScale);
		}
	}

	void SpawnActorAtCityPosition(
		UWorld& World,
		UClass* ActorClass,
		const FVector2D& LocalPosition,
		const double LocalYawRadians,
		const FTransform& CityToWorld,
		const double GroundZOffset,
		const double UniformScale,
		TArray<AActor*>& OutSpawnedActors)
	{
		if (ActorClass == nullptr)
		{
			return;
		}

		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(OutSpawnedActors);
		const FVector WorldPosition = ProjectCityPositionToGround(
			World, LocalPosition, CityToWorld, TraceParams, GroundZOffset);
		const FQuat WorldRotation(FVector::UpVector, CityYawRadians(CityToWorld) + LocalYawRadians);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Actor = World.SpawnActor<AActor>(
			ActorClass, FTransform(WorldRotation, WorldPosition, FVector(UniformScale)), SpawnParams);
		if (not IsValid(Actor))
		{
			return;
		}
		Actor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
		OutSpawnedActors.Add(Actor);
	}

	UMaterialInterface* GetDecalMaterialForSpawn(
		const FScorchedCityAssets& Assets,
		const FScorchedDecalSpawn& Decal)
	{
		const TArray<UMaterialInterface*>* Materials = nullptr;
		switch (Decal.Set)
		{
			case EScorchedDecalSet::Lot:
				Materials = &Assets.LotDecalMaterials;
				break;
			case EScorchedDecalSet::BuildingFootprint:
				Materials = &Assets.BuildingFootprintDecalMaterials;
				break;
			case EScorchedDecalSet::Road:
				Materials = &Assets.RoadDecalMaterials;
				break;
			case EScorchedDecalSet::DestroyedPowerLine:
				Materials = &Assets.DestroyedPowerLineDecalMaterials;
				break;
			case EScorchedDecalSet::PowerLine:
				Materials = &Assets.PowerLineDecalMaterials;
				break;
			default:
				break;
		}

		if (Materials == nullptr || not Materials->IsValidIndex(Decal.EntryIndex))
		{
			return nullptr;
		}
		return (*Materials)[Decal.EntryIndex];
	}

	void SpawnDecalsActor(
		UWorld& World,
		const FScorchedCityGenResult& Result,
		const FScorchedCityAssets& Assets,
		const FTransform& CityToWorld,
		TArray<AActor*>& OutSpawnedActors)
	{
		using namespace ScorchedCityPCGConstants;

		if (Result.Decals.IsEmpty())
		{
			return;
		}

		AActor* Anchor = SpawnManagedEmptyActor(World, FTransform(CityToWorld.GetLocation()), TEXT("PCG_ScorchedDecals"));
		if (Anchor == nullptr)
		{
			return;
		}
		OutSpawnedActors.Add(Anchor);

		USceneComponent* Root = NewObject<USceneComponent>(Anchor);
		Anchor->SetRootComponent(Root);
		Anchor->AddInstanceComponent(Root);
		Root->RegisterComponent();

		const double CityYaw = CityYawRadians(CityToWorld);
		const FCollisionQueryParams TraceParams;
		for (const FScorchedDecalSpawn& Decal : Result.Decals)
		{
			UMaterialInterface* Material = GetDecalMaterialForSpawn(Assets, Decal);
			if (Material == nullptr)
			{
				continue;
			}

			const FVector WorldPosition = ProjectCityPositionToGround(
				World, Decal.Position, CityToWorld, TraceParams, DecalSurfaceOffset);
			const FRotator WorldRotation(
				-90.0,
				FMath::RadiansToDegrees(CityYaw + Decal.YawRadians),
				0.0);

			UDecalComponent* DecalComponent = NewObject<UDecalComponent>(Anchor);
			DecalComponent->SetMobility(EComponentMobility::Static);
			DecalComponent->SetDecalMaterial(Material);
			DecalComponent->DecalSize = FVector(DecalProjectionDepth, Decal.Size, Decal.Size);
			DecalComponent->SetWorldTransform(FTransform(WorldRotation, WorldPosition));
			DecalComponent->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
			Anchor->AddInstanceComponent(DecalComponent);
			DecalComponent->RegisterComponent();
		}
	}
}

// ---------------------------------------------------------------------------
// Point data outputs
// ---------------------------------------------------------------------------

namespace
{
	UPCGPointData* CreateOutputPointData(FPCGContext* Context, const FName PinLabel)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		PointData->InitializeFromData(nullptr);

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = PinLabel;
		return PointData;
	}

	/** @brief Emits scatter points (with 'Mesh' attribute), tracing to the ground when requested. */
	void EmitScatterOutput(
		FPCGContext* Context,
		UWorld& World,
		const FScorchedCityGenResult& Result,
		const FScorchedCityAssets& Assets,
		const FTransform& CityToWorld,
		const int32 RandomSeed,
		const TArray<AActor*>& ActorsToIgnoreInTraces)
	{
		using namespace ScorchedCityPCGConstants;

		UPCGPointData* ScatterData = CreateOutputPointData(Context, ScatterPinLabel);
		FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute =
			ScatterData->Metadata->CreateAttribute<FSoftObjectPath>(
				ScatterMeshAttributeName, FSoftObjectPath(), false, false);

		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActors(ActorsToIgnoreInTraces);

		TArray<FPCGPoint>& Points = ScatterData->GetMutablePoints();
		Points.Reserve(Result.Scatter.Num());
		const double CityYaw = CityYawRadians(CityToWorld);

		for (int32 CandidateIndex = 0; CandidateIndex < Result.Scatter.Num(); ++CandidateIndex)
		{
			const FScorchedScatterCandidate& Candidate = Result.Scatter[CandidateIndex];
			FVector WorldPosition = CityToWorld.TransformPosition(FVector(Candidate.Position, 0.0));
			FQuat WorldRotation(FVector::UpVector, CityYaw + Candidate.YawRadians);

			if (Candidate.bAlignToGround)
			{
				FHitResult GroundHit;
				const FVector TraceStart = WorldPosition + FVector::UpVector * GroundTraceUp;
				const FVector TraceEnd = WorldPosition - FVector::UpVector * GroundTraceDown;
				if (World.LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
				{
					WorldPosition.Z = GroundHit.ImpactPoint.Z;
					WorldRotation = FQuat::FindBetweenNormals(FVector::UpVector, GroundHit.ImpactNormal) * WorldRotation;
				}
			}

			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(WorldRotation, WorldPosition, FVector(Candidate.UniformScale));
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, CandidateIndex);

			ScatterData->Metadata->InitializeOnSet(Point.MetadataEntry);
			MeshAttribute->SetValue(
				Point.MetadataEntry, Assets.ScatterMeshPaths[Candidate.ProfileIndex][Candidate.MeshIndex]);
		}
	}

	/** @brief Emits every reserved oriented footprint so downstream nodes can exclude points. */
	void EmitOccupiedBoundsOutput(
		FPCGContext* Context,
		const FScorchedCityGenResult& Result,
		const FTransform& CityToWorld,
		const int32 RandomSeed)
	{
		using namespace ScorchedCityPCGConstants;

		UPCGPointData* BoundsData = CreateOutputPointData(Context, OccupiedBoundsPinLabel);
		FPCGMetadataAttribute<int32>* TypeAttribute =
			BoundsData->Metadata->CreateAttribute<int32>(OccupancyTypeAttributeName, 0, false, false);

		TArray<FPCGPoint>& Points = BoundsData->GetMutablePoints();
		Points.Reserve(Result.OccupiedFootprints.Num());
		const double CityYaw = CityYawRadians(CityToWorld);

		for (int32 EntryIndex = 0; EntryIndex < Result.OccupiedFootprints.Num(); ++EntryIndex)
		{
			const FScorchedSpatialHashGrid::FEntry& Entry = Result.OccupiedFootprints[EntryIndex];

			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(
				FQuat(FVector::UpVector, CityYaw + Entry.Footprint.YawRadians),
				CityToWorld.TransformPosition(FVector(Entry.Footprint.Center, 0.0)));
			Point.BoundsMin = FVector(-Entry.Footprint.HalfExtents.X, -Entry.Footprint.HalfExtents.Y, -OccupancyBoundsHalfHeight);
			Point.BoundsMax = FVector(Entry.Footprint.HalfExtents.X, Entry.Footprint.HalfExtents.Y, OccupancyBoundsHalfHeight);
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, EntryIndex);

			BoundsData->Metadata->InitializeOnSet(Point.MetadataEntry);
			TypeAttribute->SetValue(Point.MetadataEntry, static_cast<int32>(Entry.Type));
		}
	}

	/**
	 * @brief Emits every unconnected road ending (including open intersection arms); each
	 * point sits on the road's final outer position (ground-projected) and its rotation yaws
	 * outward, continuing the road.
	 */
	void EmitOuterOrphanRoadsOutput(
		FPCGContext* Context,
		UWorld& World,
		const FScorchedCityGenResult& Result,
		const FTransform& CityToWorld,
		const int32 RandomSeed,
		const double RoadZOffset,
		const TArray<AActor*>& ActorsToIgnoreInTraces)
	{
		using namespace ScorchedCityPCGConstants;

		UPCGPointData* OrphanData = CreateOutputPointData(Context, OuterOrphanRoadsPinLabel);
		TArray<FPCGPoint>& Points = OrphanData->GetMutablePoints();
		Points.Reserve(Result.OuterOrphanRoads.Num());

		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(ActorsToIgnoreInTraces);
		const double CityYaw = CityYawRadians(CityToWorld);

		for (int32 OrphanIndex = 0; OrphanIndex < Result.OuterOrphanRoads.Num(); ++OrphanIndex)
		{
			const FScorchedOrphanRoadEnd& Orphan = Result.OuterOrphanRoads[OrphanIndex];

			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(
				FQuat(FVector::UpVector, CityYaw + Orphan.YawRadians),
				ProjectCityPositionToGround(World, Orphan.Position, CityToWorld, TraceParams, RoadZOffset));
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, OrphanIndex);
		}
	}

	void EmitLotsOutput(
		FPCGContext* Context,
		const FScorchedCityGenResult& Result,
		const FTransform& CityToWorld,
		const int32 RandomSeed)
	{
		using namespace ScorchedCityPCGConstants;

		UPCGPointData* LotsData = CreateOutputPointData(Context, LotsPinLabel);
		FPCGMetadataAttribute<int32>* UsedAttribute =
			LotsData->Metadata->CreateAttribute<int32>(LotUsedAttributeName, 0, false, false);

		TArray<FPCGPoint>& Points = LotsData->GetMutablePoints();
		Points.Reserve(Result.Lots.Num());
		const double CityYaw = CityYawRadians(CityToWorld);

		for (int32 LotIndex = 0; LotIndex < Result.Lots.Num(); ++LotIndex)
		{
			const FScorchedLot& Lot = Result.Lots[LotIndex];

			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(
				FQuat(FVector::UpVector, CityYaw + Lot.YawRadians),
				CityToWorld.TransformPosition(FVector(Lot.Center, 0.0)));
			Point.BoundsMin = FVector(-Lot.HalfExtents.X, -Lot.HalfExtents.Y, 0.0);
			Point.BoundsMax = FVector(Lot.HalfExtents.X, Lot.HalfExtents.Y, 0.0);
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, LotIndex);

			LotsData->Metadata->InitializeOnSet(Point.MetadataEntry);
			UsedAttribute->SetValue(Point.MetadataEntry, Lot.bUsed ? 1 : 0);
		}
	}
}

// ---------------------------------------------------------------------------
// Element
// ---------------------------------------------------------------------------

namespace
{
	/**
	 * @brief Extracts the city frame from the first spatial input: the area's bounds center
	 * becomes the city origin and the bounds size becomes the city rectangle.
	 */
	bool TryGetCityArea(
		FPCGContext* Context,
		FTransform& OutCityToWorld,
		FVector2D& OutCityLengths,
		const UPCGSpatialData*& OutAreaData)
	{
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (SpatialData == nullptr)
			{
				continue;
			}

			const FBox Bounds = SpatialData->GetBounds();
			if (not Bounds.IsValid)
			{
				continue;
			}

			OutCityToWorld = FTransform(Bounds.GetCenter());
			OutCityLengths = FVector2D(Bounds.GetSize().X, Bounds.GetSize().Y);
			OutAreaData = SpatialData;

			if (Inputs.Num() > 1)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("MultipleAreas", "ScorchedCity: multiple input data; only the first spatial data is used as the city area."));
			}
			return true;
		}
		return false;
	}

	/** @brief Even-odd ray-crossing test; the polygon is implicitly closed. */
	bool IsPointInPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Position)
	{
		bool bInside = false;
		for (int32 CurrentIndex = 0, PreviousIndex = Polygon.Num() - 1;
			CurrentIndex < Polygon.Num();
			PreviousIndex = CurrentIndex++)
		{
			const FVector2D& VertexA = Polygon[CurrentIndex];
			const FVector2D& VertexB = Polygon[PreviousIndex];

			const bool bCrossesRay = (VertexA.Y > Position.Y) != (VertexB.Y > Position.Y);
			if (bCrossesRay
				&& Position.X < (VertexB.X - VertexA.X) * (Position.Y - VertexA.Y) / (VertexB.Y - VertexA.Y) + VertexA.X)
			{
				bInside = not bInside;
			}
		}
		return bInside;
	}

	/**
	 * @brief Builds the city-local area test. Poly lines (spline loops) are sampled into a 2D
	 * polygon for fast point-in-polygon checks; other spatial data falls back to density sampling.
	 */
	TFunction<bool(const FVector2D&)> BuildAreaFunction(
		const UPCGSpatialData* AreaData,
		const FTransform& CityToWorld,
		const double GridBlockSize)
	{
		if (const UPCGPolyLineData* PolyLine = Cast<UPCGPolyLineData>(AreaData))
		{
			const double SampleStep = FMath::Clamp(GridBlockSize * 0.25, 200.0, 2000.0);
			TArray<FVector2D> Polygon;

			for (int32 SegmentIndex = 0; SegmentIndex < PolyLine->GetNumSegments(); ++SegmentIndex)
			{
				const double SegmentLength = PolyLine->GetSegmentLength(SegmentIndex);
				const int32 NumSamples = FMath::Clamp(FMath::CeilToInt32(SegmentLength / SampleStep), 2, 128);
				for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
				{
					const double Distance = SegmentLength * SampleIndex / NumSamples;
					const FTransform SampleTransform = PolyLine->GetTransformAtDistance(SegmentIndex, Distance, true);
					const FVector LocalPosition = CityToWorld.InverseTransformPosition(SampleTransform.GetLocation());
					Polygon.Add(FVector2D(LocalPosition.X, LocalPosition.Y));
				}
			}

			if (Polygon.Num() >= 3)
			{
				return [Polygon = MoveTemp(Polygon)](const FVector2D& LocalPosition) -> bool
				{
					return IsPointInPolygon(Polygon, LocalPosition);
				};
			}
		}

		// Generic spatial data (volumes, surfaces, unions...): sample the density field.
		return [AreaData, CityToWorld](const FVector2D& LocalPosition) -> bool
		{
			const FVector WorldPosition = CityToWorld.TransformPosition(FVector(LocalPosition, 0.0));
			const FBox SampleBounds(FVector(-50.0), FVector(50.0));
			FPCGPoint SampledPoint;
			return AreaData->SamplePoint(FTransform(WorldPosition), SampleBounds, SampledPoint, nullptr)
				&& SampledPoint.Density > 0.0f;
		};
	}

	void FillScalarParams(const UPCGScorchedCitySettings* Settings, FScorchedCityGenParams& Params)
	{
		Params.RandomSeed = Settings->RandomSeed;
		Params.OverallDensity = Settings->OverallDensity;
		Params.GridLayoutAmount = Settings->GridLayoutAmount;
		Params.CurvedRoadAmount = Settings->CurvedRoadAmount;
		Params.BuildingSpacingExtra = Settings->BuildingSpacingExtra;
		Params.RoadWidth = Settings->RoadWidth;
		Params.RoadZOffset = Settings->RoadZOffset;
		Params.RoadSetback = Settings->RoadSetback;
		Params.MinRoadBuildingDistance = Settings->MinRoadBuildingDistance;
		Params.GridBlockSize = Settings->GridBlockSize;
		Params.MajorRoadInterval = Settings->MajorRoadInterval;
		Params.CurvedRoadCurvature = Settings->CurvedRoadCurvature;
		Params.CurvedRoadVariation = Settings->CurvedRoadVariation;
		Params.CurvedRoadBranchChance = Settings->CurvedRoadBranchChance;
		Params.CurvedRoadSegmentLength = Settings->CurvedRoadSegmentLength;
		Params.LotWidth = Settings->LotWidth;
		Params.LotDepth = Settings->LotDepth;
		Params.PowerLines = Settings->PowerLineSettings;
		Params.LotDecals = Settings->LotDecals;
		Params.BuildingFootprintDecals = Settings->BuildingFootprintDecals;
		Params.RoadDecals = Settings->RoadDecals;
		Params.DestroyedPowerLineDecals = Settings->DestroyedPowerLineDecals;
		Params.PowerLineDecals = Settings->PowerLineDecals;
	}

	void SpawnCityActors(
		UWorld& World,
		const UPCGScorchedCitySettings& Settings,
		const FScorchedCityGenResult& Result,
		const FScorchedCityAssets& Assets,
		const FScorchedCityGenParams& Params,
		const FTransform& CityToWorld,
		TArray<AActor*>& OutSpawnedActors)
	{
		for (const FScorchedRoadSplineResult& Road : Result.RoadSplines)
		{
			SpawnRoadSplineActor(
				World,
				Road,
				CityToWorld,
				Assets.RoadMesh,
				Params.RoadMeshLength,
				Params.RoadZOffset,
				OutSpawnedActors);
		}

		SpawnIntersectionsActor(
			World, Result.Intersections, CityToWorld, Assets.IntersectionMesh, Params.RoadZOffset, OutSpawnedActors);

		// Scorch buildings are batched into one shared HISM actor; Blueprint buildings spawn
		// as their actors, unchanged.
		SpawnScorchBuildingsActor(
			World,
			Result,
			Assets,
			CityToWorld,
			Settings.ScorchBuildingCollisionBoundsPercent,
			OutSpawnedActors);

		for (const FScorchedBuildingSpawn& Building : Result.Buildings)
		{
			if (not Assets.BuildingClasses.IsValidIndex(Building.AssetIndex)
				|| Assets.BuildingCategories[Building.AssetIndex] != EScorchedBuildingCategory::BlueprintBuilding)
			{
				continue;
			}

			// Buildings are authored with their pivot at ground level: project the pivot
			// straight onto the landscape (no bounds-based lift, which made them float).
			SpawnActorAtCityPosition(World, Assets.BuildingClasses[Building.AssetIndex],
				Building.ActorPosition, Building.YawRadians, CityToWorld,
				0.0, 1.0, OutSpawnedActors);
		}

		for (const FScorchedAuxiliarySpawn& Auxiliary : Result.Auxiliaries)
		{
			if (not Assets.AuxiliaryClasses.IsValidIndex(Auxiliary.AssetIndex))
			{
				continue;
			}
			SpawnActorAtCityPosition(World, Assets.AuxiliaryClasses[Auxiliary.AssetIndex],
				Auxiliary.Position, Auxiliary.YawRadians, CityToWorld,
				0.0, Auxiliary.UniformScale, OutSpawnedActors);
		}

		for (const FScorchedAuxiliarySpawn& Lantern : Result.RoadLanterns)
		{
			if (not Assets.RoadLanternClasses.IsValidIndex(Lantern.AssetIndex))
			{
				continue;
			}
			SpawnActorAtCityPosition(World, Assets.RoadLanternClasses[Lantern.AssetIndex],
				Lantern.Position, Lantern.YawRadians, CityToWorld,
				0.0, Lantern.UniformScale, OutSpawnedActors);
		}

		for (const FScorchedAuxiliarySpawn& RoadBlockItem : Result.RoadBlockItems)
		{
			if (not Assets.RoadBlockClasses.IsValidIndex(RoadBlockItem.AssetIndex))
			{
				continue;
			}
			SpawnActorAtCityPosition(World, Assets.RoadBlockClasses[RoadBlockItem.AssetIndex],
				RoadBlockItem.Position, RoadBlockItem.YawRadians, CityToWorld,
				0.0, RoadBlockItem.UniformScale, OutSpawnedActors);
		}

		for (const FScorchedPoleSpawn& Pole : Result.Poles)
		{
			// Fall back to whichever pole asset exists so broken settings still produce a city.
			UClass* PoleClass = Pole.bTwoPole ? Assets.TwoPoleClass : Assets.SinglePoleClass;
			if (PoleClass == nullptr)
			{
				PoleClass = Pole.bTwoPole ? Assets.SinglePoleClass : Assets.TwoPoleClass;
			}

			// Pole pivots are authored at their base: project them straight onto the ground.
			SpawnActorAtCityPosition(
				World,
				PoleClass,
				Pole.Position,
				Pole.YawRadians,
				CityToWorld,
				0.0,
				1.0,
				OutSpawnedActors);
		}

		SpawnDecalsActor(World, Result, Assets, CityToWorld, OutSpawnedActors);
	}
}

bool FPCGScorchedCityElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	const UPCGScorchedCitySettings* Settings = Context->GetInputSettings<UPCGScorchedCitySettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	FTransform CityToWorld;
	FVector2D CityLengths = FVector2D::ZeroVector;
	const UPCGSpatialData* AreaData = nullptr;
	if (not TryGetCityArea(Context, CityToWorld, CityLengths, AreaData))
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoArea",
			"ScorchedCity: no valid spatial input area (expected e.g. a closed spline loop)."));
		return true;
	}

	FScorchedCityGenParams Params;
	FScorchedCityAssets Assets;
	FillScalarParams(Settings, Params);
	Params.CityLengthX = CityLengths.X;
	Params.CityLengthY = CityLengths.Y;
	ResolveRoadAndPowerAssets(Context, Settings, World, Params, Assets);
	ResolveBuildingAssets(Context, Settings, World, Params, Assets);
	ResolveAuxiliaryAssets(Context, Settings, World, Params, Assets);
	ResolveRoadSideAssets(Context, Settings, World, Params, Assets);
	ResolveScatterAssets(Settings, Params, Assets);
	ResolveDecalAssets(Settings, Params, Assets);
	Params.IsExcluded = BuildExclusionFunction(Context, CityToWorld);
	Params.IsInsideArea = BuildAreaFunction(AreaData, CityToWorld, Params.GridBlockSize);

	FScorchedCityGenResult Result;
	FScorchedCityGenerator Generator(Params);
	Generator.Generate(Result);

	if (Result.RoadSplines.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoRoads",
			"ScorchedCity: no roads were generated; check GridLayoutAmount/CurvedRoadAmount and city size vs GridBlockSize."));
	}

	TArray<AActor*> SpawnedActors;
	SpawnCityActors(World, *Settings, Result, Assets, Params, CityToWorld, SpawnedActors);

	if (not SpawnedActors.IsEmpty())
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
		for (AActor* SpawnedActor : SpawnedActors)
		{
			ManagedActors->GeneratedActors.Add(SpawnedActor);
		}
		SourceComponent->AddToManagedResources(ManagedActors);
	}

	EmitScatterOutput(Context, World, Result, Assets, CityToWorld, Settings->RandomSeed, SpawnedActors);
	EmitOccupiedBoundsOutput(Context, Result, CityToWorld, Settings->RandomSeed);
	EmitLotsOutput(Context, Result, CityToWorld, Settings->RandomSeed);
	EmitOuterOrphanRoadsOutput(
		Context, World, Result, CityToWorld, Settings->RandomSeed, Settings->RoadZOffset, SpawnedActors);
	return true;
}

#undef LOCTEXT_NAMESPACE
