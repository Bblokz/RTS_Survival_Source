// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreatePowerLineInVolume.h"

#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "CollisionQueryParams.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableWireComponent/DestructableWireComp.h"

#define LOCTEXT_NAMESPACE "PCGCreatePowerLineInVolume"

// Node-unique constants. Kept in a named namespace so the sibling unity-build blob cannot collide with them.
namespace PowerLineVolumeConstants
{
	const FName BoundsVolumePin = TEXT("BoundsVolume");
	const FName ExcludedVolumesPin = TEXT("ExcludedVolumes");
	const FName PolePointsPin = TEXT("PolePoints");

	const FName LineIndexAttribute = TEXT("LineIndex");
	const FName PoleIndexAttribute = TEXT("PoleIndex");

	constexpr double GroundTraceUp = 15000.0;
	constexpr double GroundTraceDown = 40000.0;
	constexpr double MinimumPoleSpacing = 100.0;
	constexpr double MinimumLineLength = 100.0;
	constexpr double MaximumEdgeCornerInset = 0.5;
	constexpr int32 AnchorDirectionCount = 4;
	// The forced endpoint pole is dropped when it would sit closer than this fraction of the minimum
	// spacing to the previous pole, avoiding an ugly near-duplicate at the end of the line.
	constexpr double EndPoleMergeFraction = 0.5;
}

// Node-unique helper namespace. A named namespace (rather than anonymous) prevents the generic helper
// names below from redefining a sibling node's helpers when files merge into one unity translation unit.
namespace PowerLineVolumeInternal
{
	using namespace PowerLineVolumeConstants;

	/** One placed pole, retained so the whole line can be emitted as points after generation. */
	struct FPowerLineVolumePolePlacement
	{
		FVector Position = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		int32 LineIndex = INDEX_NONE;
		int32 PoleIndex = INDEX_NONE;
	};

	/** Invariant per-line inputs, grouped to keep the generation helpers' parameter lists readable. */
	struct FPowerLineGenerationContext
	{
		UWorld* World = nullptr;
		const UPCGCreatePowerLineInVolumeSettings* Settings = nullptr;
		UClass* PoleClass = nullptr;
		UStaticMesh* WireMesh = nullptr;
		const class FPoleExclusionTester* Exclusions = nullptr;
		int32 EntryIndex = INDEX_NONE;
		int32 LineIndex = INDEX_NONE;
	};

	/** Running state of a single pole chain as it walks from the start endpoint to the end endpoint. */
	struct FPowerLineChainState
	{
		UDestructibleWire* PreviousWireComponent = nullptr;
		double PreviousPoleDistance = 0.0;
		int32 PlacedPoleCount = 0;
	};

	/** Density-samples excluded volumes so poles are never placed inside them. */
	class FPoleExclusionTester
	{
	public:
		explicit FPoleExclusionTester(TArray<const UPCGSpatialData*>&& InSpatialData)
			: M_SpatialData(MoveTemp(InSpatialData))
		{
		}

		bool IsPositionExcluded(const FVector2D& Position, const double Clearance) const
		{
			const FVector SampleExtent(Clearance, Clearance, Clearance);
			const FBox SampleBounds(-SampleExtent, SampleExtent);
			for (const UPCGSpatialData* SpatialData : M_SpatialData)
			{
				if (IsPositionExcludedBySpatialData(SpatialData, Position, Clearance, SampleBounds))
				{
					return true;
				}
			}
			return false;
		}

	private:
		static bool IsPositionExcludedBySpatialData(
			const UPCGSpatialData* SpatialData,
			const FVector2D& Position,
			const double Clearance,
			const FBox& SampleBounds)
		{
			if (SpatialData == nullptr)
			{
				return false;
			}
			const FBox SpatialBounds = SpatialData->GetBounds();
			if (Position.X < SpatialBounds.Min.X - Clearance || Position.X > SpatialBounds.Max.X + Clearance
				|| Position.Y < SpatialBounds.Min.Y - Clearance || Position.Y > SpatialBounds.Max.Y + Clearance)
			{
				return false;
			}
			FPCGPoint SampledPoint;
			const FVector SamplePosition(Position, SpatialBounds.GetCenter().Z);
			return SpatialData->SamplePoint(FTransform(SamplePosition), SampleBounds, SampledPoint, nullptr)
				&& SampledPoint.Density > 0.0f;
		}

		TArray<const UPCGSpatialData*> M_SpatialData;
	};

	TArray<FBox> CollectBoundsVolumes(FPCGContext& Context)
	{
		TArray<FBox> BoundsVolumes;
		for (const FPCGTaggedData& Input : Context.InputData.GetInputsByPin(BoundsVolumePin))
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (SpatialData == nullptr)
			{
				continue;
			}
			const FBox Bounds = SpatialData->GetBounds();
			if (Bounds.IsValid)
			{
				BoundsVolumes.Add(Bounds);
			}
		}
		return BoundsVolumes;
	}

	FPoleExclusionTester BuildPoleExclusionTester(FPCGContext& Context)
	{
		TArray<const UPCGSpatialData*> SpatialInputs;
		for (const FPCGTaggedData& Input : Context.InputData.GetInputsByPin(ExcludedVolumesPin))
		{
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
			{
				SpatialInputs.Add(SpatialData);
			}
		}
		return FPoleExclusionTester(MoveTemp(SpatialInputs));
	}

	EPowerLineVolumeAnchorDirection GetOppositeAnchorDirection(const EPowerLineVolumeAnchorDirection Direction)
	{
		switch (Direction)
		{
		case EPowerLineVolumeAnchorDirection::NegativeX: return EPowerLineVolumeAnchorDirection::PositiveX;
		case EPowerLineVolumeAnchorDirection::PositiveX: return EPowerLineVolumeAnchorDirection::NegativeX;
		case EPowerLineVolumeAnchorDirection::NegativeY: return EPowerLineVolumeAnchorDirection::PositiveY;
		case EPowerLineVolumeAnchorDirection::PositiveY: return EPowerLineVolumeAnchorDirection::NegativeY;
		default: return EPowerLineVolumeAnchorDirection::Automatic;
		}
	}

	EPowerLineVolumeAnchorDirection PickRandomAnchorDirection(FRandomStream& Random)
	{
		const EPowerLineVolumeAnchorDirection Directions[AnchorDirectionCount] = {
			EPowerLineVolumeAnchorDirection::NegativeX,
			EPowerLineVolumeAnchorDirection::PositiveX,
			EPowerLineVolumeAnchorDirection::NegativeY,
			EPowerLineVolumeAnchorDirection::PositiveY
		};
		return Directions[Random.RandRange(0, AnchorDirectionCount - 1)];
	}

	/**
	 * @brief Turns the optional designer directions into two concrete faces.
	 * @param Settings Node settings holding the requested start and end directions.
	 * @param Random Seeded stream used only when a face must be chosen automatically.
	 * @param OutStartDirection Resolved face for the start endpoint.
	 * @param OutEndDirection Resolved face for the end endpoint.
	 */
	void ResolvePowerLineAnchorDirections(
		const UPCGCreatePowerLineInVolumeSettings& Settings,
		FRandomStream& Random,
		EPowerLineVolumeAnchorDirection& OutStartDirection,
		EPowerLineVolumeAnchorDirection& OutEndDirection)
	{
		OutStartDirection = Settings.StartDirection;
		OutEndDirection = Settings.EndDirection;
		if (OutStartDirection == EPowerLineVolumeAnchorDirection::Automatic)
		{
			OutStartDirection = OutEndDirection != EPowerLineVolumeAnchorDirection::Automatic
				? GetOppositeAnchorDirection(OutEndDirection)
				: PickRandomAnchorDirection(Random);
		}
		if (OutEndDirection == EPowerLineVolumeAnchorDirection::Automatic)
		{
			OutEndDirection = GetOppositeAnchorDirection(OutStartDirection);
		}
	}

	/**
	 * @brief Picks a point on the requested bounds face, inset from the corners and pushed inward.
	 * @param Bounds Axis-aligned bounds of the input volume.
	 * @param Direction Face the endpoint is anchored to.
	 * @param Settings Node settings supplying the corner and face insets.
	 * @param Random Seeded stream used to slide the point along the face.
	 * @return The planar (X,Y) endpoint on the chosen face.
	 */
	FVector2D GetBoundsAnchorPoint(
		const FBox& Bounds,
		const EPowerLineVolumeAnchorDirection Direction,
		const UPCGCreatePowerLineInVolumeSettings& Settings,
		FRandomStream& Random)
	{
		const double CornerInset = FMath::Clamp(
			static_cast<double>(Settings.EdgeCornerInset), 0.0, MaximumEdgeCornerInset);
		const double FaceInset = FMath::Max(0.0, static_cast<double>(Settings.EdgeInset));
		const double Alpha = Random.FRandRange(CornerInset, 1.0 - CornerInset);
		const FVector Minimum = Bounds.Min;
		const FVector Maximum = Bounds.Max;
		switch (Direction)
		{
		case EPowerLineVolumeAnchorDirection::NegativeX:
			return FVector2D(Minimum.X + FaceInset, FMath::Lerp(Minimum.Y, Maximum.Y, Alpha));
		case EPowerLineVolumeAnchorDirection::PositiveX:
			return FVector2D(Maximum.X - FaceInset, FMath::Lerp(Minimum.Y, Maximum.Y, Alpha));
		case EPowerLineVolumeAnchorDirection::NegativeY:
			return FVector2D(FMath::Lerp(Minimum.X, Maximum.X, Alpha), Minimum.Y + FaceInset);
		case EPowerLineVolumeAnchorDirection::PositiveY:
			return FVector2D(FMath::Lerp(Minimum.X, Maximum.X, Alpha), Maximum.Y - FaceInset);
		default:
			return FVector2D(Bounds.GetCenter());
		}
	}

	TArray<int32> GetConfiguredPoleIndices(const UPCGCreatePowerLineInVolumeSettings& Settings)
	{
		TArray<int32> ConfiguredIndices;
		for (int32 PoleIndex = 0; PoleIndex < Settings.Poles.Num(); ++PoleIndex)
		{
			if (not Settings.Poles[PoleIndex].PoleActor.IsNull())
			{
				ConfiguredIndices.Add(PoleIndex);
			}
		}
		return ConfiguredIndices;
	}

	/**
	 * @brief Chooses a single configured pole entry for one line, weighted by each entry's Weight.
	 * @param Settings Node settings holding the pole list.
	 * @param ConfiguredPoleIndices Indices of poles with an assigned actor class.
	 * @param Random Seeded stream driving the weighted pick.
	 * @return Index into Settings.Poles of the chosen entry, or INDEX_NONE when none are configured.
	 */
	int32 SelectPoleEntryIndex(
		const UPCGCreatePowerLineInVolumeSettings& Settings,
		const TArray<int32>& ConfiguredPoleIndices,
		FRandomStream& Random)
	{
		if (ConfiguredPoleIndices.IsEmpty())
		{
			return INDEX_NONE;
		}
		double TotalWeight = 0.0;
		for (const int32 PoleIndex : ConfiguredPoleIndices)
		{
			TotalWeight += FMath::Max(0.0, static_cast<double>(Settings.Poles[PoleIndex].Weight));
		}
		if (TotalWeight <= 0.0)
		{
			return ConfiguredPoleIndices[Random.RandRange(0, ConfiguredPoleIndices.Num() - 1)];
		}
		double Pick = Random.FRand() * TotalWeight;
		for (const int32 PoleIndex : ConfiguredPoleIndices)
		{
			Pick -= FMath::Max(0.0, static_cast<double>(Settings.Poles[PoleIndex].Weight));
			if (Pick <= 0.0)
			{
				return PoleIndex;
			}
		}
		return ConfiguredPoleIndices.Last();
	}

	FVector ProjectPowerLinePoleToGround(
		UWorld& World,
		const FVector& Position,
		const TArray<AActor*>& ActorsToIgnore)
	{
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActors(ActorsToIgnore);
		FHitResult Hit;
		const FVector TraceStart = Position + FVector::UpVector * GroundTraceUp;
		const FVector TraceEnd = Position - FVector::UpVector * GroundTraceDown;
		return World.LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams)
			? Hit.ImpactPoint
			: Position;
	}

	FRotator GetHorizontalAxisAlignmentRotation(const FVector& DesiredDirection, const FVector& LocalOrientation)
	{
		FVector2D LocalOrientation2D(LocalOrientation.X, LocalOrientation.Y);
		if (LocalOrientation2D.IsNearlyZero())
		{
			LocalOrientation2D = FVector2D::UnitX();
		}
		const double DesiredYaw = FMath::Atan2(DesiredDirection.Y, DesiredDirection.X);
		const double LocalYaw = FMath::Atan2(LocalOrientation2D.Y, LocalOrientation2D.X);
		return FRotator(0.0, FMath::RadiansToDegrees(DesiredYaw - LocalYaw), 0.0);
	}

	/**
	 * @brief Spawns one PCG-managed pole aligned to the line and grounded onto the terrain.
	 * @param GenerationContext Invariant per-line inputs (world, pole class, settings, indices).
	 * @param PoleEntry The chosen pole entry supplying orientation.
	 * @param LinePosition Ungrounded pole position at the sampled distance.
	 * @param LineDirection Horizontal line direction the pole aligns to.
	 * @param ActorsToIgnore Actors excluded from the grounding trace.
	 * @param PoleIndex Sequential index of this pole within its line.
	 * @return The spawned actor, or nullptr when spawning failed.
	 */
	AActor* SpawnPowerLinePoleActor(
		const FPowerLineGenerationContext& GenerationContext,
		const FPowerLineVolumePoleEntry& PoleEntry,
		const FVector& LinePosition,
		const FVector& LineDirection,
		const TArray<AActor*>& ActorsToIgnore,
		const int32 PoleIndex)
	{
		UWorld& World = *GenerationContext.World;
		FVector GroundPosition = ProjectPowerLinePoleToGround(World, LinePosition, ActorsToIgnore);
		GroundPosition.Z += GenerationContext.Settings->GroundOffset;
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* PoleActor = World.SpawnActor<AActor>(
			GenerationContext.PoleClass, GroundPosition,
			GetHorizontalAxisAlignmentRotation(LineDirection, PoleEntry.Orientation), SpawnParams);
		if (not IsValid(PoleActor))
		{
			return nullptr;
		}
		PoleActor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		PoleActor->SetActorLabel(FString::Printf(
			TEXT("PCG_PowerLinePole_%d_%d"), GenerationContext.LineIndex, PoleIndex));
#endif
		return PoleActor;
	}

	void ConnectAdjacentPowerLinePoles(
		UDestructibleWire* PreviousWireComponent,
		UDestructibleWire* CurrentWireComponent,
		const int32 PreferredAmountWires,
		UStaticMesh* WireMesh,
		const FVector& WireMeshScale)
	{
		if (not IsValid(PreviousWireComponent) || not IsValid(CurrentWireComponent) || not IsValid(WireMesh))
		{
			return;
		}
		const int32 WireCount = FMath::Max(1, PreferredAmountWires);
		PreviousWireComponent->SetupWireConnection(CurrentWireComponent, WireCount, WireMesh, WireMeshScale);
	}

	/**
	 * @brief Places one pole at an arc distance, skipping positions inside excluded volumes.
	 * @param GenerationContext Invariant per-line inputs.
	 * @param StartPoint Planar start endpoint of the line.
	 * @param LineDirection2D Unit planar direction from start to end.
	 * @param Distance Arc distance along the line for this pole.
	 * @param BoundsCenterZ Height used to seed the grounding trace.
	 * @param PoleIndex Sequential index of this pole within its line.
	 * @param InOutSpawnedActors Accumulates spawned actors for managed-resource cleanup and trace ignores.
	 * @param OutPlacements Accumulates placed poles for point output.
	 * @return The placed pole's wire component, or nullptr when the position was skipped or spawning failed.
	 */
	UDestructibleWire* TrySpawnPowerLinePoleAtDistance(
		const FPowerLineGenerationContext& GenerationContext,
		const FVector2D& StartPoint,
		const FVector2D& LineDirection2D,
		const double Distance,
		const double BoundsCenterZ,
		const int32 PoleIndex,
		TArray<AActor*>& InOutSpawnedActors,
		TArray<FPowerLineVolumePolePlacement>& OutPlacements)
	{
		const UPCGCreatePowerLineInVolumeSettings& Settings = *GenerationContext.Settings;
		const FVector2D Position2D = StartPoint + LineDirection2D * Distance;
		if (GenerationContext.Exclusions->IsPositionExcluded(Position2D, Settings.ExclusionClearance))
		{
			return nullptr;
		}
		const FVector LineDirection(LineDirection2D, 0.0);
		AActor* PoleActor = SpawnPowerLinePoleActor(
			GenerationContext, Settings.Poles[GenerationContext.EntryIndex],
			FVector(Position2D, BoundsCenterZ), LineDirection, InOutSpawnedActors, PoleIndex);
		if (not IsValid(PoleActor))
		{
			return nullptr;
		}
		InOutSpawnedActors.Add(PoleActor);
		FPowerLineVolumePolePlacement& Placement = OutPlacements.Emplace_GetRef();
		Placement.Position = PoleActor->GetActorLocation();
		Placement.Direction = LineDirection;
		Placement.LineIndex = GenerationContext.LineIndex;
		Placement.PoleIndex = PoleIndex;
		return PoleActor->FindComponentByClass<UDestructibleWire>();
	}

	/**
	 * @brief Wires a freshly placed pole to the previous one and advances the chain bookkeeping.
	 * @param GenerationContext Invariant per-line inputs.
	 * @param CurrentWireComponent Wire component of the pole just placed.
	 * @param CurrentDistance Arc distance of the pole just placed.
	 * @param MaximumWireSpan Largest gap that may still be wired; larger gaps break the run.
	 * @param InOutChainState Chain bookkeeping updated in place.
	 */
	void AdvancePowerLineChain(
		const FPowerLineGenerationContext& GenerationContext,
		UDestructibleWire* CurrentWireComponent,
		const double CurrentDistance,
		const double MaximumWireSpan,
		FPowerLineChainState& InOutChainState)
	{
		const UPCGCreatePowerLineInVolumeSettings& Settings = *GenerationContext.Settings;
		const bool bWithinWireSpan =
			CurrentDistance - InOutChainState.PreviousPoleDistance <= MaximumWireSpan;
		if (bWithinWireSpan)
		{
			ConnectAdjacentPowerLinePoles(
				InOutChainState.PreviousWireComponent, CurrentWireComponent,
				Settings.Poles[GenerationContext.EntryIndex].PreferredAmountWires,
				GenerationContext.WireMesh, Settings.WireMeshScale);
		}
		InOutChainState.PreviousWireComponent = CurrentWireComponent;
		InOutChainState.PreviousPoleDistance = CurrentDistance;
		++InOutChainState.PlacedPoleCount;
	}

	double ResolveMinimumPoleSpacing(const UPCGCreatePowerLineInVolumeSettings& Settings)
	{
		return FMath::Max(MinimumPoleSpacing,
			FMath::Min(Settings.MinimumDistanceBetweenPoles, Settings.MaximumDistanceBetweenPoles));
	}

	double ResolveMaximumPoleSpacing(const UPCGCreatePowerLineInVolumeSettings& Settings, const double MinimumSpacing)
	{
		return FMath::Max(MinimumSpacing,
			FMath::Max(Settings.MinimumDistanceBetweenPoles, Settings.MaximumDistanceBetweenPoles));
	}

	/** Whether the forced endpoint pole would crowd the previous pole and should therefore be skipped. */
	bool IsRedundantEndPole(
		const bool bIsEndPole,
		const double EndDistance,
		const double MinimumSpacing,
		const FPowerLineChainState& ChainState)
	{
		return bIsEndPole
			&& ChainState.PlacedPoleCount > 0
			&& EndDistance - ChainState.PreviousPoleDistance < MinimumSpacing * EndPoleMergeFraction;
	}

	/** Places one pole for the current step (if not excluded) and chains it to the previous pole. */
	void PlacePowerLinePoleStep(
		const FPowerLineGenerationContext& GenerationContext,
		const FVector2D& StartPoint,
		const FVector2D& LineDirection2D,
		const double Distance,
		const double BoundsCenterZ,
		const double MaximumWireSpan,
		FPowerLineChainState& InOutChainState,
		TArray<AActor*>& InOutSpawnedActors,
		TArray<FPowerLineVolumePolePlacement>& OutPlacements)
	{
		UDestructibleWire* CurrentWireComponent = TrySpawnPowerLinePoleAtDistance(
			GenerationContext, StartPoint, LineDirection2D, Distance, BoundsCenterZ,
			InOutChainState.PlacedPoleCount, InOutSpawnedActors, OutPlacements);
		if (IsValid(CurrentWireComponent))
		{
			AdvancePowerLineChain(
				GenerationContext, CurrentWireComponent, Distance, MaximumWireSpan, InOutChainState);
		}
	}

	/**
	 * @brief Generates one power line: resolves endpoints, walks the route, and wires the pole chain.
	 * @param GenerationContext Invariant per-line inputs (world, pole class, wire mesh, settings, indices).
	 * @param Bounds Axis-aligned bounds of the volume the line lives in.
	 * @param Random Seeded stream for endpoint and spacing variation.
	 * @param InOutSpawnedActors Accumulates spawned actors for managed-resource cleanup.
	 * @param OutPlacements Accumulates placed poles for point output.
	 */
	void GeneratePowerLineForBounds(
		const FPowerLineGenerationContext& GenerationContext,
		const FBox& Bounds,
		FRandomStream& Random,
		TArray<AActor*>& InOutSpawnedActors,
		TArray<FPowerLineVolumePolePlacement>& OutPlacements)
	{
		const UPCGCreatePowerLineInVolumeSettings& Settings = *GenerationContext.Settings;
		EPowerLineVolumeAnchorDirection StartDirection;
		EPowerLineVolumeAnchorDirection EndDirection;
		ResolvePowerLineAnchorDirections(Settings, Random, StartDirection, EndDirection);
		const FVector2D StartPoint = GetBoundsAnchorPoint(Bounds, StartDirection, Settings, Random);
		const FVector2D EndPoint = GetBoundsAnchorPoint(Bounds, EndDirection, Settings, Random);
		const double LineLength = FVector2D::Distance(StartPoint, EndPoint);
		if (LineLength < MinimumLineLength)
		{
			return;
		}

		const FVector2D LineDirection2D = (EndPoint - StartPoint) / LineLength;
		const double MinimumSpacing = ResolveMinimumPoleSpacing(Settings);
		const double MaximumSpacing = ResolveMaximumPoleSpacing(Settings, MinimumSpacing);
		const double MaximumWireSpan = MaximumSpacing
			* FMath::Max(1.0, static_cast<double>(Settings.MaximumWireSpanSpacingMultiplier));
		const double BoundsCenterZ = Bounds.GetCenter().Z;

		FPowerLineChainState ChainState;
		double PoleDistance = 0.0;
		bool bReachedEnd = false;
		while (not bReachedEnd)
		{
			bReachedEnd = PoleDistance >= LineLength;
			const double ClampedDistance = bReachedEnd ? LineLength : PoleDistance;
			if (not IsRedundantEndPole(bReachedEnd, ClampedDistance, MinimumSpacing, ChainState))
			{
				PlacePowerLinePoleStep(
					GenerationContext, StartPoint, LineDirection2D, ClampedDistance, BoundsCenterZ,
					MaximumWireSpan, ChainState, InOutSpawnedActors, OutPlacements);
			}
			PoleDistance += Random.FRandRange(MinimumSpacing, MaximumSpacing);
		}
	}

	/**
	 * @brief Resolves the pole class for one volume and generates its power line.
	 * @param Context PCG context providing the deterministic seed.
	 * @param GenerationContext Invariant inputs with the per-line fields still to be filled.
	 * @param ConfiguredPoleIndices Indices of poles with an assigned actor class.
	 * @param Bounds Axis-aligned bounds of the volume the line lives in.
	 * @param LineIndex Sequential index of this line across all input volumes.
	 * @param InOutSpawnedActors Accumulates spawned actors for managed-resource cleanup.
	 * @param InOutPlacements Accumulates placed poles for point output.
	 */
	void TryGeneratePowerLineForBounds(
		FPCGContext& Context,
		FPowerLineGenerationContext GenerationContext,
		const TArray<int32>& ConfiguredPoleIndices,
		const FBox& Bounds,
		const int32 LineIndex,
		TArray<AActor*>& InOutSpawnedActors,
		TArray<FPowerLineVolumePolePlacement>& InOutPlacements)
	{
		const UPCGCreatePowerLineInVolumeSettings& Settings = *GenerationContext.Settings;
		FRandomStream Random(PCGHelpers::ComputeSeed(Context.GetSeed(), LineIndex));
		const int32 EntryIndex = SelectPoleEntryIndex(Settings, ConfiguredPoleIndices, Random);
		if (EntryIndex == INDEX_NONE)
		{
			return;
		}
		UClass* PoleClass = Settings.Poles[EntryIndex].PoleActor.LoadSynchronous();
		if (not IsValid(PoleClass) || PoleClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return;
		}
		GenerationContext.PoleClass = PoleClass;
		GenerationContext.EntryIndex = EntryIndex;
		GenerationContext.LineIndex = LineIndex;
		GeneratePowerLineForBounds(GenerationContext, Bounds, Random, InOutSpawnedActors, InOutPlacements);
	}

	void RegisterManagedActors(UPCGComponent& SourceComponent, const TArray<AActor*>& SpawnedActors)
	{
		if (SpawnedActors.IsEmpty())
		{
			return;
		}
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(&SourceComponent);
		for (AActor* SpawnedActor : SpawnedActors)
		{
			ManagedActors->GeneratedActors.Add(SpawnedActor);
		}
		SourceComponent.AddToManagedResources(ManagedActors);
	}

	void EmitPowerLinePolePoints(
		FPCGContext& Context,
		const TArray<FPowerLineVolumePolePlacement>& Placements)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(&Context);
		PointData->InitializeFromData(nullptr);
		FPCGMetadataAttribute<int32>* LineIndex = PointData->Metadata->CreateAttribute<int32>(
			LineIndexAttribute, INDEX_NONE, false, false);
		FPCGMetadataAttribute<int32>* PoleIndex = PointData->Metadata->CreateAttribute<int32>(
			PoleIndexAttribute, INDEX_NONE, false, false);

		TArray<FPCGPoint>& OutputPoints = PointData->GetMutablePoints();
		for (const FPowerLineVolumePolePlacement& Placement : Placements)
		{
			FPCGPoint& Point = OutputPoints.Emplace_GetRef();
			Point.Transform = FTransform(Placement.Direction.Rotation(), Placement.Position);
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(Placement.LineIndex, Placement.PoleIndex);
			PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
			LineIndex->SetValue(Point.MetadataEntry, Placement.LineIndex);
			PoleIndex->SetValue(Point.MetadataEntry, Placement.PoleIndex);
		}

		FPCGTaggedData& Output = Context.OutputData.TaggedData.Emplace_GetRef();
		Output.Pin = PolePointsPin;
		Output.Data = PointData;
	}
}

#if WITH_EDITOR
FName UPCGCreatePowerLineInVolumeSettings::GetDefaultNodeName() const
{
	return TEXT("CreatePowerLineInVolume");
}

FText UPCGCreatePowerLineInVolumeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Power Line In Volume");
}

FText UPCGCreatePowerLineInVolumeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Creates one power line per input bounds volume. Two endpoints are chosen on the volume's faces - optionally "
		"steered by start/end anchor directions - and a single pole type from the weighted list is placed at varying "
		"spacing between them, wired to its neighbours. Poles inside excluded volumes are skipped; short gaps stay "
		"wired while long gaps break the run. Placed poles are emitted as points.");
}
#endif

TArray<FPCGPinProperties> UPCGCreatePowerLineInVolumeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PowerLineVolumeConstants::BoundsVolumePin, EPCGDataType::Spatial).SetRequiredPin();
	Pins.Emplace(PowerLineVolumeConstants::ExcludedVolumesPin, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreatePowerLineInVolumeSettings::OutputPinProperties() const
{
	return {FPCGPinProperties(PowerLineVolumeConstants::PolePointsPin, EPCGDataType::Point)};
}

FPCGElementPtr UPCGCreatePowerLineInVolumeSettings::CreateElement() const
{
	return MakeShared<FPCGCreatePowerLineInVolumeElement>();
}

bool FPCGCreatePowerLineInVolumeElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace PowerLineVolumeInternal;
	check(Context);
	const UPCGCreatePowerLineInVolumeSettings* Settings =
		Context->GetInputSettings<UPCGCreatePowerLineInVolumeSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}

	const TArray<FBox> BoundsVolumes = CollectBoundsVolumes(*Context);
	const TArray<int32> ConfiguredPoleIndices = GetConfiguredPoleIndices(*Settings);
	if (BoundsVolumes.IsEmpty() || ConfiguredPoleIndices.IsEmpty())
	{
		EmitPowerLinePolePoints(*Context, {});
		return true;
	}

	FPowerLineGenerationContext BaseContext;
	BaseContext.World = SourceComponent->GetWorld();
	BaseContext.Settings = Settings;
	BaseContext.WireMesh = Settings->WireMesh.LoadSynchronous();
	const FPoleExclusionTester Exclusions = BuildPoleExclusionTester(*Context);
	BaseContext.Exclusions = &Exclusions;

	TArray<AActor*> SpawnedActors;
	TArray<FPowerLineVolumePolePlacement> Placements;
	for (int32 LineIndex = 0; LineIndex < BoundsVolumes.Num(); ++LineIndex)
	{
		TryGeneratePowerLineForBounds(
			*Context, BaseContext, ConfiguredPoleIndices, BoundsVolumes[LineIndex],
			LineIndex, SpawnedActors, Placements);
	}

	RegisterManagedActors(*SourceComponent, SpawnedActors);
	EmitPowerLinePolePoints(*Context, Placements);
	return true;
}

#undef LOCTEXT_NAMESPACE
