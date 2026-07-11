// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGSpawnLevelInstance.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"

#include "Engine/World.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "PCGSpawnLevelInstance"

namespace PCGSpawnLevelInstanceConstants
{
	const FName PivotsPinLabel = TEXT("Pivots");
	const FName BoundsPinLabel = TEXT("Bounds");

	constexpr float QuarterTurnDegrees = 90.0f;
	constexpr float EighthTurnDegrees = 45.0f;
	constexpr float FullTurnDegrees = 360.0f;

	// Seconds to wait for async level streaming (runtime) before giving up on exact bounds.
	constexpr double MaxLoadWaitSeconds = 10.0;
}

#if WITH_EDITOR
FName UPCGSpawnLevelInstanceSettings::GetDefaultNodeName() const
{
	return FName(TEXT("SpawnLevelInstance"));
}

FText UPCGSpawnLevelInstanceSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Spawn Level Instance");
}

FText UPCGSpawnLevelInstanceSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Treats each input point as a pivot, picks a level instance asset and a yaw turn using the seed, "
		"and spawns the level instance there (with ZOffset). Outputs the pivot points and one bounds point "
		"per spawned instance whose oriented bounds match the chosen rotation, for excluding overlapping points.");
}
#endif

FPCGElementPtr UPCGSpawnLevelInstanceSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnLevelInstanceElement>();
}

TArray<FPCGPinProperties> UPCGSpawnLevelInstanceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Pivot points at which the level instances are spawned.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UPCGSpawnLevelInstanceSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// One point per spawned instance; point bounds are the local bounds oriented by the chosen yaw.
	Pins.Emplace_GetRef(PCGSpawnLevelInstanceConstants::BoundsPinLabel, EPCGDataType::Point);
	// The input points, promoted to pivots, passed through unchanged.
	Pins.Emplace_GetRef(PCGSpawnLevelInstanceConstants::PivotsPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGContext* FPCGSpawnLevelInstanceElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGSpawnLevelInstanceContext* Context = new FPCGSpawnLevelInstanceContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

namespace
{
	/** @brief Builds the pool of yaw angles the seed may pick from; index 0 is always "no rotation". */
	TArray<float> BuildAllowedYawAngles(const UPCGSpawnLevelInstanceSettings& Settings)
	{
		using namespace PCGSpawnLevelInstanceConstants;

		TArray<float> Angles;
		Angles.Add(0.0f);

		if (Settings.bAllow90DegTurns)
		{
			for (float Angle = QuarterTurnDegrees; Angle < FullTurnDegrees; Angle += QuarterTurnDegrees)
			{
				Angles.Add(Angle);
			}
		}

		if (Settings.bAllow45DegTurns)
		{
			for (float Angle = EighthTurnDegrees; Angle < FullTurnDegrees; Angle += QuarterTurnDegrees)
			{
				Angles.Add(Angle);
			}
		}

		return Angles;
	}

	/**
	 * @brief Sets the level asset on a spawned level instance actor. SetWorldAsset is editor-only,
	 * so cooked builds assign the WorldAsset property through reflection instead.
	 * @return True if the asset was assigned.
	 */
	bool AssignWorldAsset(ALevelInstance& LevelInstanceActor, const TSoftObjectPtr<UWorld>& LevelAsset)
	{
#if WITH_EDITOR
		return LevelInstanceActor.SetWorldAsset(LevelAsset);
#else
		FSoftObjectProperty* WorldAssetProperty =
			FindFProperty<FSoftObjectProperty>(ALevelInstance::StaticClass(), TEXT("WorldAsset"));
		if (WorldAssetProperty == nullptr)
		{
			return false;
		}

		WorldAssetProperty->SetPropertyValue_InContainer(
			&LevelInstanceActor,
			FSoftObjectPtr(LevelAsset.ToSoftObjectPath()));
		return true;
#endif
	}

	/**
	 * @brief In editor builds, reads the level's bounds straight from its package (no loading needed).
	 * With an identity transform this yields the bounds in the level instance's local space.
	 * @return True if bounds could be read; at runtime always false (resolved later, once streamed in).
	 */
	bool TryGetLocalBoundsFromAsset(const TSoftObjectPtr<UWorld>& LevelAsset, FBox& OutLocalBounds)
	{
#if WITH_EDITOR
		const FName LevelPackageName(*LevelAsset.ToSoftObjectPath().GetLongPackageName());
		return ULevelInstanceSubsystem::GetLevelInstanceBoundsFromPackage(
			FTransform::Identity, LevelPackageName, OutLocalBounds);
#else
		return false;
#endif
	}

	/**
	 * @brief Editor worlds do not pump the level instance streaming queue like game worlds do,
	 * so a programmatically spawned level instance would stay invisible; block-load it so the
	 * designer immediately previews the result of the current graph and seed.
	 */
	void EnsureLoadedForEditorPreview(UWorld& World, ALevelInstance& LevelInstanceActor)
	{
#if WITH_EDITOR
		if (World.IsGameWorld())
		{
			// PIE/runtime worlds stream level instances in automatically.
			return;
		}

		if (LevelInstanceActor.IsLoaded())
		{
			return;
		}

		ULevelInstanceSubsystem* LevelInstanceSubsystem = World.GetSubsystem<ULevelInstanceSubsystem>();
		if (LevelInstanceSubsystem == nullptr)
		{
			return;
		}

		LevelInstanceSubsystem->BlockLoadLevelInstance(&LevelInstanceActor);
#endif
	}

	/** @brief Converts a world-space AABB into the local space of the spawn transform (conservative). */
	FBox MakeLocalBounds(const FBox& WorldBounds, const FTransform& SpawnTransform)
	{
		FBox LocalBounds(EForceInit::ForceInit);

		for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
		{
			const FVector Corner(
				(CornerIndex & 1) ? WorldBounds.Max.X : WorldBounds.Min.X,
				(CornerIndex & 2) ? WorldBounds.Max.Y : WorldBounds.Min.Y,
				(CornerIndex & 4) ? WorldBounds.Max.Z : WorldBounds.Min.Z);
			LocalBounds += SpawnTransform.InverseTransformPosition(Corner);
		}

		return LocalBounds;
	}

	/** @brief Spawns one level instance at the given pivot point and records it in the context. */
	void SpawnLevelInstanceAtPoint(
		FPCGSpawnLevelInstanceContext* Context,
		const UPCGSpawnLevelInstanceSettings& Settings,
		UWorld& World,
		const FPCGPoint& PivotPoint,
		const TArray<TSoftObjectPtr<UWorld>>& ValidLevels,
		const TArray<float>& AllowedYawAngles,
		UPCGManagedActors& ManagedActors)
	{
		// Deterministic per pivot: combines the node/component seed with the point's own seed.
		const int32 PivotSeed = PCGHelpers::ComputeSeed(Context->GetSeed(), PivotPoint.Seed);
		FRandomStream RandomStream(PivotSeed);

		const TSoftObjectPtr<UWorld>& LevelAsset = ValidLevels[RandomStream.RandRange(0, ValidLevels.Num() - 1)];
		const float YawDegrees = AllowedYawAngles[RandomStream.RandRange(0, AllowedYawAngles.Num() - 1)];

		// Apply the picked yaw on top of the pivot's rotation; rotation is only ever around Z.
		const FQuat YawRotation(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));
		const FQuat FinalRotation = PivotPoint.Transform.GetRotation() * YawRotation;
		FVector SpawnLocation = PivotPoint.Transform.GetLocation();
		SpawnLocation.Z += Settings.ZOffset;
		const FTransform SpawnTransform(FinalRotation, SpawnLocation);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Deferred so the world asset is set before construction/registration triggers loading.
		SpawnParams.bDeferConstruction = true;

		ALevelInstance* LevelInstanceActor =
			World.SpawnActor<ALevelInstance>(ALevelInstance::StaticClass(), SpawnTransform, SpawnParams);
		if (not IsValid(LevelInstanceActor))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context,
				LOCTEXT("SpawnFailed", "SpawnLevelInstance: failed to spawn a level instance actor."));
			return;
		}

		if (not AssignWorldAsset(*LevelInstanceActor, LevelAsset))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context,
				LOCTEXT("AssignWorldAssetFailed", "SpawnLevelInstance: could not assign the level asset to the spawned level instance."));
			LevelInstanceActor->Destroy();
			return;
		}

		LevelInstanceActor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
		LevelInstanceActor->FinishSpawning(SpawnTransform);
		ManagedActors.GeneratedActors.Add(LevelInstanceActor);

		// Make the spawned level visible right away when generating in the editor (preview).
		EnsureLoadedForEditorPreview(World, *LevelInstanceActor);

		FPCGSpawnedLevelInstanceEntry Entry;
		Entry.Actor = LevelInstanceActor;
		Entry.SpawnTransform = SpawnTransform;
		Entry.PivotSeed = PivotSeed;
		// In editor builds this resolves immediately from the level package.
		Entry.bBoundsResolved = TryGetLocalBoundsFromAsset(LevelAsset, Entry.LocalBounds);
		Context->SpawnedEntries.Add(Entry);
	}

	/** @brief Spawns a level instance for every input point and registers all actors with PCG. */
	void SpawnAllLevelInstances(FPCGSpawnLevelInstanceContext* Context, const UPCGSpawnLevelInstanceSettings& Settings)
	{
		UPCGComponent* SourceComponent = Context->SourceComponent.Get();
		if (not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context,
				LOCTEXT("NoSourceComponent", "SpawnLevelInstance: no valid source component/world to spawn into."));
			return;
		}

		TArray<TSoftObjectPtr<UWorld>> ValidLevels;
		for (const TSoftObjectPtr<UWorld>& Level : Settings.LevelInstances)
		{
			if (not Level.IsNull())
			{
				ValidLevels.Add(Level);
			}
		}

		if (ValidLevels.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context,
				LOCTEXT("NoLevels", "SpawnLevelInstance: the LevelInstances array contains no valid level assets."));
			return;
		}

		const TArray<float> AllowedYawAngles = BuildAllowedYawAngles(Settings);

		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
		UWorld& World = *SourceComponent->GetWorld();

		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
			if (PointData == nullptr)
			{
				continue;
			}

			for (const FPCGPoint& PivotPoint : PointData->GetPoints())
			{
				SpawnLevelInstanceAtPoint(
					Context, Settings, World, PivotPoint, ValidLevels, AllowedYawAngles, *ManagedActors);
			}
		}

		if (not ManagedActors->GeneratedActors.IsEmpty())
		{
			SourceComponent->AddToManagedResources(ManagedActors);
		}
	}

	/**
	 * @brief Attempts to resolve local bounds for entries still waiting on level streaming.
	 * @return True once every entry has resolved bounds.
	 */
	bool TryResolvePendingBounds(FPCGSpawnLevelInstanceContext* Context)
	{
		const UPCGComponent* SourceComponent = Context->SourceComponent.Get();
		UWorld* World = IsValid(SourceComponent) ? SourceComponent->GetWorld() : nullptr;
		ULevelInstanceSubsystem* LevelInstanceSubsystem =
			IsValid(World) ? World->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;

		bool bAllResolved = true;
		for (FPCGSpawnedLevelInstanceEntry& Entry : Context->SpawnedEntries)
		{
			if (Entry.bBoundsResolved)
			{
				continue;
			}

			ALevelInstance* LevelInstanceActor = Entry.Actor.Get();
			if (not IsValid(LevelInstanceActor) || LevelInstanceSubsystem == nullptr)
			{
				// Actor died or the world is gone; emit a degenerate box at the pivot instead of stalling.
				Entry.LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
				Entry.bBoundsResolved = true;
				continue;
			}

			FBox WorldBounds(EForceInit::ForceInit);
			if (not LevelInstanceActor->IsLoaded()
				|| not LevelInstanceSubsystem->GetLevelInstanceBounds(LevelInstanceActor, WorldBounds))
			{
				bAllResolved = false;
				continue;
			}

			Entry.LocalBounds = MakeLocalBounds(WorldBounds, Entry.SpawnTransform);
			Entry.bBoundsResolved = true;
		}

		return bAllResolved;
	}

	/** @brief Marks all unresolved entries as resolved with degenerate bounds after a streaming timeout. */
	void ResolveTimedOutBounds(FPCGSpawnLevelInstanceContext* Context)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context,
			LOCTEXT("BoundsTimeout", "SpawnLevelInstance: timed out waiting for level instance(s) to stream in; emitting empty bounds for the unresolved instances."));

		for (FPCGSpawnedLevelInstanceEntry& Entry : Context->SpawnedEntries)
		{
			if (not Entry.bBoundsResolved)
			{
				Entry.LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
				Entry.bBoundsResolved = true;
			}
		}
	}

	/** @brief Emits the Pivots passthrough and the per-instance oriented bounds points. */
	void EmitOutputs(FPCGSpawnLevelInstanceContext* Context)
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

		// Pass the input points through as the promoted pivots.
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			if (Cast<UPCGPointData>(Input.Data) == nullptr)
			{
				continue;
			}

			FPCGTaggedData& PivotOutput = Outputs.Add_GetRef(Input);
			PivotOutput.Pin = PCGSpawnLevelInstanceConstants::PivotsPinLabel;
		}

		// One point per spawned instance; transform carries the chosen yaw so the bounds are oriented.
		UPCGPointData* BoundsPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		BoundsPointData->InitializeFromData(nullptr);
		TArray<FPCGPoint>& BoundsPoints = BoundsPointData->GetMutablePoints();
		BoundsPoints.Reserve(Context->SpawnedEntries.Num());

		for (const FPCGSpawnedLevelInstanceEntry& Entry : Context->SpawnedEntries)
		{
			FPCGPoint BoundsPoint;
			BoundsPoint.Transform = Entry.SpawnTransform;
			BoundsPoint.BoundsMin = Entry.LocalBounds.Min;
			BoundsPoint.BoundsMax = Entry.LocalBounds.Max;
			BoundsPoint.Density = 1.0f;
			BoundsPoint.Seed = Entry.PivotSeed;
			BoundsPoints.Add(BoundsPoint);
		}

		FPCGTaggedData& BoundsOutput = Outputs.Emplace_GetRef();
		BoundsOutput.Data = BoundsPointData;
		BoundsOutput.Pin = PCGSpawnLevelInstanceConstants::BoundsPinLabel;
	}
}

bool FPCGSpawnLevelInstanceElement::ExecuteInternal(FPCGContext* InContext) const
{
	check(InContext);
	FPCGSpawnLevelInstanceContext* Context = static_cast<FPCGSpawnLevelInstanceContext*>(InContext);

	const UPCGSpawnLevelInstanceSettings* Settings = Context->GetInputSettings<UPCGSpawnLevelInstanceSettings>();
	if (Settings == nullptr)
	{
		return true;
	}

	if (not Context->bSpawnPhaseComplete)
	{
		SpawnAllLevelInstances(Context, *Settings);
		Context->bSpawnPhaseComplete = true;
	}

	if (not TryResolvePendingBounds(Context))
	{
		const double CurrentTimeSeconds = FPlatformTime::Seconds();
		if (Context->LoadWaitStartTimeSeconds < 0.0)
		{
			Context->LoadWaitStartTimeSeconds = CurrentTimeSeconds;
		}

		if (CurrentTimeSeconds - Context->LoadWaitStartTimeSeconds < PCGSpawnLevelInstanceConstants::MaxLoadWaitSeconds)
		{
			// Still waiting on async level streaming; run again.
			return false;
		}

		ResolveTimedOutBounds(Context);
	}

	EmitOutputs(Context);
	return true;
}

#undef LOCTEXT_NAMESPACE
