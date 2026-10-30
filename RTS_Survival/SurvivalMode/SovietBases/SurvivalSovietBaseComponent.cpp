// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/HitResult.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseSettings.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurvivalSovietBaseConstants
{
	constexpr uint8 EnemyPlayer = 2;
	constexpr double MeasureSpawnDepth = -100000.0;
	constexpr double DefaultFootprintHalfExtent = 300.0;
	constexpr double MinFootprintHalfExtent = 0.5;
	constexpr double GroundTraceUp = 10000.0;
	constexpr double GroundTraceDown = 30000.0;
	constexpr double UnitSpawnZOffset = 50.0;
	constexpr double ClusterDistanceMargin = 250.0;
	constexpr int32 PatrolMaxFormationWidth = 2;
	constexpr float PatrolFormationOffsetMultiplier = 1.f;
	const FName SovietBaseActorTag = TEXT("SurvivalSovietBase");
}

USurvivalSovietBaseComponent::USurvivalSovietBaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USurvivalSovietBaseComponent::InitSovietBases(ASurvivalModeDirector* Director)
{
	M_Director = Director;
}

void USurvivalSovietBaseComponent::GenerateSovietBaseAt(ASurvivalStartLocation* StartLocation, const int32 BaseIndex)
{
	if (not IsValid(StartLocation))
	{
		RTSFunctionLibrary::ReportError("USurvivalSovietBaseComponent::GenerateSovietBaseAt: invalid start location.");
		return;
	}
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr)
	{
		return;
	}
	if (Settings->Presets.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Soviet base settings have no presets; no base generated at "
			+ StartLocation->GetName());
		return;
	}

	EnsureMatchSeed(*Settings);

	FSurvivalSovietBaseState& Base = M_Bases.AddDefaulted_GetRef();
	const int32 BaseArrayIndex = M_Bases.Num() - 1;
	Base.BaseIndex = BaseIndex;
	Base.StartLocation = StartLocation;
	Base.Center = StartLocation->GetDeployCenter();
	Base.Scaling = Settings->GetScalingForDifficulty(M_Director->GetDifficulty().DifficultyLevel);
	Base.Seed = ComputeBaseSeed(*StartLocation, BaseIndex);
	Base.Strength = ComputeBaseStrength(*StartLocation, BaseIndex, *Settings, Base.Scaling);
	Base.PresetIndex = PickPresetIndex(*Settings, Base.Strength, Base.Seed);
	Base.GroundTraceParams = FCollisionQueryParams(TEXT("SurvivalSovietBaseGround"), false);

	const FSurvivalSovietBasePreset& Preset = Settings->Presets[Base.PresetIndex];
	const TArray<FSoftObjectPath> ClassPaths = CollectPresetClassPaths(Preset);
	UE_LOG(LogTemp, Log, TEXT("Survival Soviet base %d at %s: preset %d, strength %d, %d classes to load."), BaseIndex,
	       *StartLocation->GetName(), Base.PresetIndex, Base.Strength, ClassPaths.Num());
	if (ClassPaths.IsEmpty())
	{
		OnBaseAssetsLoaded(BaseArrayIndex);
		return;
	}

	TWeakObjectPtr<USurvivalSovietBaseComponent> WeakThis(this);
	Base.LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ClassPaths,
		FStreamableDelegate::CreateLambda([WeakThis, BaseArrayIndex]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnBaseAssetsLoaded(BaseArrayIndex);
			}
		}));
	if (not Base.LoadHandle.IsValid())
	{
		// Nothing to stream (all loaded) or the request failed; resolution falls back to synchronous loads.
		OnBaseAssetsLoaded(BaseArrayIndex);
	}
}

int32 USurvivalSovietBaseComponent::GetAliveSovietBaseCount() const
{
	int32 AliveCount = 0;
	for (const FSurvivalSovietBaseState& Base : M_Bases)
	{
		AliveCount += Base.bDestroyed ? 0 : 1;
	}
	return AliveCount;
}

TArray<FVector> USurvivalSovietBaseComponent::GetAliveBaseCenters() const
{
	TArray<FVector> Centers;
	for (const FSurvivalSovietBaseState& Base : M_Bases)
	{
		if (not Base.bDestroyed)
		{
			Centers.Add(Base.Center);
		}
	}
	return Centers;
}

TArray<FVector> USurvivalSovietBaseComponent::GetAliveBaseLaunchPoints() const
{
	TArray<FVector> LaunchPoints;
	for (const FSurvivalSovietBaseState& Base : M_Bases)
	{
		if (not Base.bDestroyed and Base.bStructuresSpawned)
		{
			LaunchPoints.Add(Base.LaunchPoint);
		}
	}
	return LaunchPoints;
}

float USurvivalSovietBaseComponent::GetWavePressureMultiplier() const
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr)
	{
		return 1.f;
	}
	return 1.f + GetAliveSovietBaseCount() * Settings->WavePressurePerAliveBase;
}

int32 USurvivalSovietBaseComponent::GetBaseStrength(const int32 BaseIndex) const
{
	for (const FSurvivalSovietBaseState& Base : M_Bases)
	{
		if (Base.BaseIndex == BaseIndex)
		{
			return Base.Strength;
		}
	}
	return -1;
}

bool USurvivalSovietBaseComponent::GetIsBaseDestroyed(const int32 BaseIndex) const
{
	for (const FSurvivalSovietBaseState& Base : M_Bases)
	{
		if (Base.BaseIndex == BaseIndex)
		{
			return Base.bDestroyed;
		}
	}
	return false;
}

void USurvivalSovietBaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SpawnTimerHandle);
	}
	for (FSurvivalSovietBaseState& Base : M_Bases)
	{
		if (Base.LoadHandle.IsValid())
		{
			Base.LoadHandle->CancelHandle();
			Base.LoadHandle.Reset();
		}
	}
	M_SpawnQueue.Reset();
	M_PendingGarrisonGroups.Reset();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

bool USurvivalSovietBaseComponent::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Director", "GetIsValidDirector", this);
	return false;
}

const USurvivalSovietBaseSettings* USurvivalSovietBaseComponent::GetSovietBaseSettings() const
{
	if (not GetIsValidDirector())
	{
		return nullptr;
	}
	const USurvivalModeSettings* ModeSettings = M_Director->GetSettings();
	if (not IsValid(ModeSettings) or not IsValid(ModeSettings->SovietBaseSettings))
	{
		RTSFunctionLibrary::ReportError("Survival mode settings have no SovietBaseSettings asset set.");
		return nullptr;
	}
	return ModeSettings->SovietBaseSettings;
}

void USurvivalSovietBaseComponent::EnsureMatchSeed(const USurvivalSovietBaseSettings& Settings)
{
	if (bM_HasMatchSeed)
	{
		return;
	}
	bM_HasMatchSeed = true;
	if (Settings.SeedMode == ESurvivalSovietSeedMode::Fixed)
	{
		M_MatchSeed = Settings.FixedSeed;
		return;
	}
	M_MatchSeed = static_cast<int32>(HashCombine(
		GetTypeHash(FDateTime::UtcNow().GetTicks()), GetTypeHash(FPlatformTime::Cycles64())));
}

int32 USurvivalSovietBaseComponent::ComputeBaseSeed(const ASurvivalStartLocation& StartLocation,
                                                    const int32 BaseIndex) const
{
	// Location-based so the same map + match seed always gives the same base on the same spot.
	const FVector Center = StartLocation.GetDeployCenter();
	const FIntVector RoundedCenter(
		FMath::RoundToInt(Center.X / 100.0), FMath::RoundToInt(Center.Y / 100.0), 0);
	uint32 Seed = HashCombine(GetTypeHash(M_MatchSeed), GetTypeHash(RoundedCenter));
	Seed = HashCombine(Seed, GetTypeHash(BaseIndex));
	return static_cast<int32>(Seed);
}

TArray<ASurvivalStartLocation*> USurvivalSovietBaseComponent::GetUnpickedStartLocations() const
{
	TArray<ASurvivalStartLocation*> Unpicked;
	if (not GetIsValidDirector())
	{
		return Unpicked;
	}
	const ASurvivalStartLocation* Chosen = M_Director->GetChosenStartLocation();
	for (const TWeakObjectPtr<ASurvivalStartLocation>& WeakLocation : M_Director->GetStartLocations())
	{
		ASurvivalStartLocation* Location = WeakLocation.Get();
		if (IsValid(Location) and Location != Chosen)
		{
			Unpicked.Add(Location);
		}
	}
	return Unpicked;
}

int32 USurvivalSovietBaseComponent::ComputeBaseStrength(
	const ASurvivalStartLocation& StartLocation,
	const int32 BaseIndex,
	const USurvivalSovietBaseSettings& Settings,
	const FSurvivalSovietBaseDifficultyScaling& Scaling) const
{
	const TArray<ASurvivalStartLocation*> Unpicked = GetUnpickedStartLocations();
	const int32 BaseCount = FMath::Max(1, Unpicked.Num());
	int32 DistanceRank = BaseIndex;

	const ASurvivalStartLocation* Chosen = GetIsValidDirector() ? M_Director->GetChosenStartLocation() : nullptr;
	if (IsValid(Chosen))
	{
		const FVector PlayerCenter = Chosen->GetDeployCenter();
		const double OwnDistance = FVector::DistSquared2D(StartLocation.GetDeployCenter(), PlayerCenter);
		const int32 OwnArrayIndex = Unpicked.IndexOfByKey(&StartLocation);
		DistanceRank = 0;
		for (int32 Index = 0; Index < Unpicked.Num(); ++Index)
		{
			const double OtherDistance = FVector::DistSquared2D(Unpicked[Index]->GetDeployCenter(), PlayerCenter);
			const bool bCloser = OtherDistance < OwnDistance
				or (OtherDistance == OwnDistance and Index < OwnArrayIndex);
			DistanceRank += bCloser ? 1 : 0;
		}
	}
	DistanceRank = FMath::Clamp(DistanceRank, 0, BaseCount - 1);

	// Nearest base = 0, farthest = MaxStrength, spread evenly over the bases.
	const int32 StrengthLevels = Settings.MaxStrength + 1;
	const int32 DistanceStrength = BaseCount <= 1 ? 0 : (DistanceRank * StrengthLevels) / BaseCount;
	return FMath::Clamp(DistanceStrength + Scaling.StrengthBonus, 0, Settings.MaxStrength);
}

int32 USurvivalSovietBaseComponent::PickPresetIndex(
	const USurvivalSovietBaseSettings& Settings,
	const int32 Strength,
	const int32 Seed) const
{
	TArray<int32> Candidates;
	TArray<float> Weights;
	for (int32 PresetIndex = 0; PresetIndex < Settings.Presets.Num(); ++PresetIndex)
	{
		const FSurvivalSovietBasePreset& Preset = Settings.Presets[PresetIndex];
		if (Preset.Weight <= 0.f or Strength < Preset.MinStrength or Strength > Preset.MaxStrength)
		{
			continue;
		}
		const bool bAlreadyUsed = M_Bases.ContainsByPredicate([PresetIndex](const FSurvivalSovietBaseState& Base)
		{
			return Base.PresetIndex == PresetIndex;
		});
		Candidates.Add(PresetIndex);
		Weights.Add(Preset.Weight * (bAlreadyUsed ? Settings.RepeatedPresetWeightFactor : 1.f));
	}

	if (Candidates.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("No Soviet base preset allows strength " + FString::FromInt(Strength)
			+ "; using the first preset.");
		return 0;
	}

	float TotalWeight = 0.f;
	for (const float Weight : Weights)
	{
		TotalWeight += Weight;
	}
	const FRandomStream Random(Seed);
	float Roll = Random.FRand() * TotalWeight;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.f)
		{
			return Candidates[Index];
		}
	}
	return Candidates.Last();
}

TArray<FSoftObjectPath> USurvivalSovietBaseComponent::CollectPresetClassPaths(
	const FSurvivalSovietBasePreset& Preset) const
{
	TArray<FSoftObjectPath> Paths;
	auto AddEntry = [&Paths](const FEnemyBlueprintEntry& Entry)
	{
		const FSoftObjectPath Path = Entry.BlueprintClass.ToSoftObjectPath();
		if (Path.IsValid())
		{
			Paths.AddUnique(Path);
		}
	};
	auto AddEntries = [&AddEntry](const TArray<FEnemyBlueprintEntry>& Entries)
	{
		for (const FEnemyBlueprintEntry& Entry : Entries)
		{
			AddEntry(Entry);
		}
	};
	auto AddSlots = [&AddEntry](const TArray<FSurvivalSovietStructureSlot>& Slots)
	{
		for (const FSurvivalSovietStructureSlot& Slot : Slots)
		{
			AddEntry(Slot.Building);
		}
	};

	AddSlots(Preset.ProductionBuildings);
	AddSlots(Preset.SupportBuildings);
	AddSlots(Preset.GunPits);
	AddEntries(Preset.Perimeter.Bunkers);
	AddEntries(Preset.Perimeter.ObstacleBelt.Obstacles);
	AddEntries(Preset.Perimeter.BarbedWire.WireSegments);
	AddEntries(Preset.Perimeter.Sandbags.SandbagUnits);
	for (const FEnemyDecoratorProfile& Profile : Preset.DecoratorProfiles)
	{
		AddEntries(Profile.Decorators);
	}
	return Paths;
}

void USurvivalSovietBaseComponent::OnBaseAssetsLoaded(const int32 BaseArrayIndex)
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or not M_Bases.IsValidIndex(BaseArrayIndex))
	{
		return;
	}
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	if (Base.bPlanBuilt)
	{
		// The streamable delegate can fire synchronously when everything was already loaded.
		return;
	}
	Base.bPlanBuilt = true;
	if (not Base.StartLocation.IsValid())
	{
		RTSFunctionLibrary::ReportError("Soviet base start location vanished before its base was generated.");
		return;
	}

	BuildBasePlan(Base, *Settings);
	UE_LOG(LogTemp, Log, TEXT("Survival Soviet base %d planned: %d structures, %d production."), Base.BaseIndex,
	       Base.Plan.Structures.Num(), Base.Plan.ProductionCount);
	if (Settings->bConfigureStrategicAIBaseDetection)
	{
		ConfigureStrategicAIBaseDetection(*Settings, Base.Plan);
	}
	EnqueueBaseStructures(BaseArrayIndex);
}

void USurvivalSovietBaseComponent::BuildBasePlan(FSurvivalSovietBaseState& Base,
                                                 const USurvivalSovietBaseSettings& Settings)
{
	const ASurvivalStartLocation* StartLocation = Base.StartLocation.Get();
	const ASurvivalStartLocation* Chosen = GetIsValidDirector() ? M_Director->GetChosenStartLocation() : nullptr;

	FSurvivalSovietLayoutInput Input;
	Input.Preset = &Settings.Presets[Base.PresetIndex];
	Input.Scaling = Base.Scaling;
	Input.Strength = Base.Strength;
	Input.Seed = Base.Seed;
	Input.Center = FVector2D(Base.Center);
	Input.BuildAreaRadius = StartLocation->GetBuildAreaRadius();
	// Without a chosen location (debug), face the map origin.
	Input.ThreatLocation = IsValid(Chosen) ? FVector2D(Chosen->GetDeployCenter()) : FVector2D::ZeroVector;
	for (const FVector& DefensePoint : StartLocation->GetDefensePoints())
	{
		Input.DefensePoints.Add(FVector2D(DefensePoint));
	}
	Input.ResolveEntry = [this](const FEnemyBlueprintEntry& Entry, FEnemyResolvedEntry& OutResolved)
	{
		return ResolveEntry(Entry, OutResolved);
	};

	Base.Plan = FSurvivalSovietBaseLayout::BuildPlan(Input);
	Base.LaunchPoint = ProjectToGround(Base, Base.Plan.LaunchPoint);
	if (Base.Plan.ProductionCount == 0)
	{
		RTSFunctionLibrary::ReportError("Soviet base " + FString::FromInt(Base.BaseIndex)
			+ " has no production building; it will count as destroyed once all its buildings are gone.");
	}
}

// ---------------------------------------------------------------------------
// Footprint resolution
// ---------------------------------------------------------------------------

bool USurvivalSovietBaseComponent::ResolveEntry(const FEnemyBlueprintEntry& Entry, FEnemyResolvedEntry& OutResolved)
{
	using namespace SurvivalSovietBaseConstants;
	const int32 ClassIndex = FindOrResolveClass(Entry.BlueprintClass);
	if (ClassIndex == INDEX_NONE)
	{
		return false;
	}

	OutResolved.OutputAssetIndex = ClassIndex;
	OutResolved.Weight = Entry.Weight;
	OutResolved.bOverrideScale = Entry.bOverrideScale;
	OutResolved.MinScale = Entry.MinScale;
	OutResolved.MaxScale = FMath::Max(Entry.MinScale, Entry.MaxScale);
	OutResolved.SandbagFrontAxis = Entry.SandbagFrontAxis;

	if (Entry.bUseFootprintOverride)
	{
		OutResolved.FootprintHalfExtents = FVector2D(
			FMath::Max(MinFootprintHalfExtent, Entry.FootprintOverride.X * 0.5),
			FMath::Max(MinFootprintHalfExtent, Entry.FootprintOverride.Y * 0.5));
		OutResolved.PivotToFootprintCenter = FVector2D::ZeroVector;
		return true;
	}

	const FBox& LocalBounds = M_MeasuredLocalBounds[ClassIndex];
	if (LocalBounds.IsValid)
	{
		const FVector Extent = LocalBounds.GetExtent();
		const FVector Center = LocalBounds.GetCenter();
		OutResolved.FootprintHalfExtents = FVector2D(
			FMath::Max(MinFootprintHalfExtent, Extent.X), FMath::Max(MinFootprintHalfExtent, Extent.Y));
		OutResolved.PivotToFootprintCenter = FVector2D(Center.X, Center.Y);
		return true;
	}

	OutResolved.FootprintHalfExtents = FVector2D(DefaultFootprintHalfExtent, DefaultFootprintHalfExtent);
	OutResolved.PivotToFootprintCenter = FVector2D::ZeroVector;
	return true;
}

int32 USurvivalSovietBaseComponent::FindOrResolveClass(const TSoftClassPtr<AActor>& SoftClass)
{
	const FSoftObjectPath Path = SoftClass.ToSoftObjectPath();
	if (not Path.IsValid())
	{
		return INDEX_NONE;
	}
	if (const int32* ExistingIndex = M_ClassIndexByPath.Find(Path))
	{
		return *ExistingIndex;
	}

	UClass* ActorClass = SoftClass.Get();
	if (ActorClass == nullptr)
	{
		// The async load should have brought it in; a synchronous load keeps generation working regardless.
		ActorClass = SoftClass.LoadSynchronous();
	}
	if (ActorClass == nullptr)
	{
		RTSFunctionLibrary::ReportError("Soviet base could not load class " + Path.ToString() + "; entry skipped.");
		M_ClassIndexByPath.Add(Path, INDEX_NONE);
		return INDEX_NONE;
	}

	const int32 ClassIndex = M_ResolvedClasses.Add(ActorClass);
	M_MeasuredLocalBounds.Add(MeasureClassLocalBounds(ActorClass));
	M_ClassIndexByPath.Add(Path, ClassIndex);
	return ClassIndex;
}

FBox USurvivalSovietBaseComponent::MeasureClassLocalBounds(UClass* ActorClass) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) or ActorClass == nullptr)
	{
		return FBox(ForceInit);
	}

	// Deferred so the unit can be kept out of the game state (no enemy registration, no tech unlock flicker)
	// before its BeginPlay runs.
	const FTransform MeasureTransform(FVector(0.0, 0.0, SurvivalSovietBaseConstants::MeasureSpawnDepth));
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bDeferConstruction = true;
	AActor* InspectionActor = World->SpawnActor<AActor>(ActorClass, MeasureTransform, SpawnParams);
	if (not IsValid(InspectionActor))
	{
		return FBox(ForceInit);
	}
	if (URTSComponent* RTSComponent = InspectionActor->FindComponentByClass<URTSComponent>())
	{
		RTSComponent->SetAddToGameState(false);
	}
	InspectionActor->FinishSpawning(MeasureTransform);

	const FBox WorldBounds = InspectionActor->GetComponentsBoundingBox(true, true);
	const FVector ActorLocation = InspectionActor->GetActorLocation();
	InspectionActor->Destroy();
	if (not WorldBounds.IsValid)
	{
		return FBox(ForceInit);
	}
	return WorldBounds.ShiftBy(-ActorLocation);
}

// ---------------------------------------------------------------------------
// Structure spawning
// ---------------------------------------------------------------------------

void USurvivalSovietBaseComponent::EnqueueBaseStructures(const int32 BaseArrayIndex)
{
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.PendingStructureSpawns = Base.Plan.Structures.Num();
	if (Base.PendingStructureSpawns == 0)
	{
		OnBaseStructuresSpawned(BaseArrayIndex);
		return;
	}

	for (int32 StructureIndex = 0; StructureIndex < Base.Plan.Structures.Num(); ++StructureIndex)
	{
		M_SpawnQueue.Add({BaseArrayIndex, StructureIndex});
	}

	UWorld* World = GetWorld();
	if (not IsValid(World) or World->GetTimerManager().IsTimerActive(M_SpawnTimerHandle))
	{
		return;
	}
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	const float StepSeconds = Settings != nullptr ? FMath::Max(0.01f, Settings->SpawnStepSeconds) : 0.05f;
	World->GetTimerManager().SetTimer(
		M_SpawnTimerHandle, this, &USurvivalSovietBaseComponent::ProcessSpawnQueue, StepSeconds, true, 0.f);
}

void USurvivalSovietBaseComponent::ProcessSpawnQueue()
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	const int32 PerStep = Settings != nullptr ? FMath::Max(1, Settings->StructuresPerSpawnStep) : 6;

	for (int32 Spawned = 0; Spawned < PerStep and M_SpawnQueueHead < M_SpawnQueue.Num(); ++Spawned)
	{
		const FPendingStructureSpawn Pending = M_SpawnQueue[M_SpawnQueueHead++];
		if (not M_Bases.IsValidIndex(Pending.BaseArrayIndex))
		{
			continue;
		}
		FSurvivalSovietBaseState& Base = M_Bases[Pending.BaseArrayIndex];
		if (not Base.Plan.Structures.IsValidIndex(Pending.StructureIndex))
		{
			continue;
		}
		const FSurvivalSovietPlannedStructure Structure = Base.Plan.Structures[Pending.StructureIndex];
		AActor* Actor = SpawnStructure(Base, Structure);
		OnStructureSpawned(Pending.BaseArrayIndex, Structure, Actor);
	}

	if (M_SpawnQueueHead < M_SpawnQueue.Num())
	{
		return;
	}
	M_SpawnQueue.Reset();
	M_SpawnQueueHead = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SpawnTimerHandle);
	}
}

FVector USurvivalSovietBaseComponent::ProjectToGround(const FSurvivalSovietBaseState& Base,
                                                      const FVector2D& Position) const
{
	using namespace SurvivalSovietBaseConstants;
	const FVector Fallback(Position, Base.Center.Z);
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return Fallback;
	}
	FHitResult Hit;
	const FVector TraceStart(Position, Base.Center.Z + GroundTraceUp);
	const FVector TraceEnd(Position, Base.Center.Z - GroundTraceDown);
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Base.GroundTraceParams))
	{
		return Hit.ImpactPoint;
	}
	return Fallback;
}

AActor* USurvivalSovietBaseComponent::SpawnStructure(FSurvivalSovietBaseState& Base,
                                                     const FSurvivalSovietPlannedStructure& Structure)
{
	UWorld* World = GetWorld();
	if (not IsValid(World) or not M_ResolvedClasses.IsValidIndex(Structure.ClassIndex))
	{
		return nullptr;
	}
	UClass* ActorClass = M_ResolvedClasses[Structure.ClassIndex];
	if (ActorClass == nullptr)
	{
		return nullptr;
	}

	const FVector Location = ProjectToGround(Base, Structure.Position);
	const FQuat Rotation(FVector::UpVector, Structure.YawRadians);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World->SpawnActor<AActor>(
		ActorClass, FTransform(Rotation, Location, FVector(Structure.UniformScale)), SpawnParams);
	if (not IsValid(Actor))
	{
		return nullptr;
	}
	Actor->Tags.Add(SurvivalSovietBaseConstants::SovietBaseActorTag);
	Base.GroundTraceParams.AddIgnoredActor(Actor);
	return Actor;
}

void USurvivalSovietBaseComponent::OnStructureSpawned(
	const int32 BaseArrayIndex,
	const FSurvivalSovietPlannedStructure& Structure,
	AActor* Actor)
{
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	if (IsValid(Actor))
	{
		Base.SpawnedStructures.Add(Actor);
		if (Structure.Role != ESurvivalSovietStructureRole::Fortification)
		{
			ValidateEnemyOwnership(Actor);
		}
		const bool bTracksProduction = Base.Plan.ProductionCount > 0;
		const bool bCountsTowardsDestruction = bTracksProduction
			                                       ? Structure.Role == ESurvivalSovietStructureRole::Production
			                                       : Structure.Role != ESurvivalSovietStructureRole::Fortification;
		if (bCountsTowardsDestruction)
		{
			TrackStructureForDestruction(BaseArrayIndex, Actor);
		}
	}

	Base.PendingStructureSpawns--;
	if (Base.PendingStructureSpawns <= 0)
	{
		OnBaseStructuresSpawned(BaseArrayIndex);
	}
}

void USurvivalSovietBaseComponent::OnBaseStructuresSpawned(const int32 BaseArrayIndex)
{
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.bStructuresSpawned = true;
	Base.LoadHandle.Reset();
	if (Base.TrackedStructuresTotal == 0)
	{
		RTSFunctionLibrary::ReportError("Soviet base " + FString::FromInt(Base.BaseIndex)
			+ " spawned no structure that counts towards its destruction; it can never be destroyed or pay out.");
	}
	if (Base.TrackedStructuresTotal > 0 and Base.TrackedStructuresAlive <= 0 and not Base.bDestroyed)
	{
		// Every tracked structure died while the rest of the base was still spawning.
		OnBaseDestroyed(BaseArrayIndex);
		return;
	}
	SpawnBaseGarrison(BaseArrayIndex);
}

void USurvivalSovietBaseComponent::ValidateEnemyOwnership(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}
	const URTSComponent* RTSComponent = Actor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		// Not an RTS unit (props, obstacles); ownership does not apply.
		return;
	}
	if (RTSComponent->GetOwningPlayer() == SurvivalSovietBaseConstants::EnemyPlayer)
	{
		return;
	}
	const FName ClassName = Actor->GetClass()->GetFName();
	if (M_ReportedOwnerClassNames.Contains(ClassName))
	{
		return;
	}
	M_ReportedOwnerClassNames.Add(ClassName);
	RTSFunctionLibrary::ReportError("Soviet base spawned " + ClassName.ToString()
		+ " but it is not owned by the enemy (player 2); set OwningPlayer = 2 in its Blueprint, check the async"
		" spawner mapping (player variants) or remove it from the Soviet base presets.");
}

// ---------------------------------------------------------------------------
// Destruction tracking
// ---------------------------------------------------------------------------

void USurvivalSovietBaseComponent::TrackStructureForDestruction(const int32 BaseArrayIndex, AActor* Actor)
{
	const TObjectKey<AActor> StructureKey(Actor);
	if (M_TrackedStructureToBase.Contains(StructureKey))
	{
		return;
	}
	M_TrackedStructureToBase.Add(StructureKey, BaseArrayIndex);
	M_Bases[BaseArrayIndex].TrackedStructuresAlive++;
	M_Bases[BaseArrayIndex].TrackedStructuresTotal++;

	// Units die (collapse, wreck) before they are destroyed; listen to both and count whichever comes first.
	if (IHealthBarOwner* HealthOwner = Cast<IHealthBarOwner>(Actor))
	{
		if (FOnUnitDies* OnUnitDies = HealthOwner->GetOnUnitDiesDelegate())
		{
			OnUnitDies->AddWeakLambda(this, [this, StructureKey]()
			{
				OnTrackedStructureLost(StructureKey);
			});
		}
	}
	Actor->OnDestroyed.AddDynamic(this, &USurvivalSovietBaseComponent::OnTrackedStructureDestroyed);
}

void USurvivalSovietBaseComponent::OnTrackedStructureDestroyed(AActor* DestroyedActor)
{
	OnTrackedStructureLost(TObjectKey<AActor>(DestroyedActor));
}

void USurvivalSovietBaseComponent::OnTrackedStructureLost(const TObjectKey<AActor>& StructureKey)
{
	int32 BaseArrayIndex = INDEX_NONE;
	if (not M_TrackedStructureToBase.RemoveAndCopyValue(StructureKey, BaseArrayIndex))
	{
		// Already counted (death followed by destroy).
		return;
	}
	if (not M_Bases.IsValidIndex(BaseArrayIndex))
	{
		return;
	}
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.TrackedStructuresAlive--;
	if (Base.TrackedStructuresAlive <= 0 and Base.bStructuresSpawned and not Base.bDestroyed)
	{
		OnBaseDestroyed(BaseArrayIndex);
	}
}

void USurvivalSovietBaseComponent::OnBaseDestroyed(const int32 BaseArrayIndex)
{
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.bDestroyed = true;

	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings != nullptr and Settings->Presets.IsValidIndex(Base.PresetIndex))
	{
		const FSurvivalSovietBasePreset& Preset = Settings->Presets[Base.PresetIndex];
		GrantBaseRewards(Base, Preset);
		if (not Settings->BaseDestroyedNotification.IsEmpty())
		{
			RTSFunctionLibrary::DisplayNotification(
				FText::Format(Settings->BaseDestroyedNotification, Preset.DisplayName));
		}
	}
	OnSovietBaseDestroyed.Broadcast(Base.BaseIndex, Base.Center);
}

void USurvivalSovietBaseComponent::GrantBaseRewards(const FSurvivalSovietBaseState& Base,
                                                    const FSurvivalSovietBasePreset& Preset) const
{
	if (Preset.DestroyRewards.IsEmpty())
	{
		return;
	}
	UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(ResourceManager))
	{
		RTSFunctionLibrary::ReportError("Soviet base destroyed but the player resource manager is invalid; no reward.");
		return;
	}
	for (const FSurvivalSovietBaseReward& Reward : Preset.DestroyRewards)
	{
		const float StrengthFactor = 1.f + Reward.BonusFractionPerStrength * Base.Strength;
		const int32 Amount = FMath::RoundToInt(Reward.Amount * StrengthFactor * Base.Scaling.RewardMultiplier);
		if (Amount > 0)
		{
			ResourceManager->AddResource(Reward.Resource, Amount);
		}
	}
}

// ---------------------------------------------------------------------------
// Garrison
// ---------------------------------------------------------------------------

void USurvivalSovietBaseComponent::SpawnBaseGarrison(const int32 BaseArrayIndex)
{
	using namespace SurvivalSovietBaseConstants;
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	if (Base.Plan.Garrison.IsEmpty())
	{
		return;
	}
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Soviet base garrison cannot spawn: no valid ARTSAsyncSpawner in the level.");
		return;
	}

	const TWeakObjectPtr<USurvivalSovietBaseComponent> WeakThis(this);
	for (int32 GroupIndex = 0; GroupIndex < Base.Plan.Garrison.Num(); ++GroupIndex)
	{
		const FSurvivalSovietPlannedGarrison& Planned = Base.Plan.Garrison[GroupIndex];
		if (Planned.Units.IsEmpty())
		{
			continue;
		}
		const int32 GroupId = M_NextGarrisonGroupId++;
		FPendingGarrisonGroup& Pending = M_PendingGarrisonGroups.Add(GroupId);
		Pending.BaseArrayIndex = BaseArrayIndex;
		Pending.PlannedGroupIndex = GroupIndex;
		Pending.PendingUnits = Planned.Units.Num();
		Base.PendingGarrisonGroups++;

		const FRotator Rotation(0.f, FMath::RadiansToDegrees(Planned.FacingYawRadians), 0.f);
		// Copy: spawn callbacks can run synchronously and touch M_Bases / the pending map.
		const TArray<FTrainingOption> Units = Planned.Units;
		const TArray<FVector2D> Positions = Planned.SpawnPositions;
		for (int32 UnitIndex = 0; UnitIndex < Units.Num(); ++UnitIndex)
		{
			const FVector2D Position2D = Positions.IsValidIndex(UnitIndex) ? Positions[UnitIndex] : FVector2D(Base.Center);
			const FVector Location = ProjectToGround(M_Bases[BaseArrayIndex], Position2D)
				+ FVector::UpVector * UnitSpawnZOffset;

			const TSharedRef<bool> bResolved = MakeShared<bool>(false);
			auto OnSpawned = [WeakThis, bResolved](const FTrainingOption&, AActor* SpawnedActor, const int32 Id)
			{
				if (*bResolved)
				{
					return;
				}
				*bResolved = true;
				if (WeakThis.IsValid())
				{
					WeakThis->OnGarrisonUnitSpawned(Id, SpawnedActor);
				}
			};
			const bool bRequested = AsyncSpawner->AsyncSpawnOptionAtLocation(
				Units[UnitIndex], Location, this, GroupId, OnSpawned, Rotation);
			if (not bRequested and not *bResolved)
			{
				*bResolved = true;
				OnGarrisonUnitSpawned(GroupId, nullptr);
			}
		}
	}
}

void USurvivalSovietBaseComponent::OnGarrisonUnitSpawned(const int32 GroupId, AActor* SpawnedActor)
{
	FPendingGarrisonGroup* Pending = M_PendingGarrisonGroups.Find(GroupId);
	if (Pending == nullptr or not M_Bases.IsValidIndex(Pending->BaseArrayIndex))
	{
		return;
	}
	FSurvivalSovietBaseState& Base = M_Bases[Pending->BaseArrayIndex];
	if (IsValid(SpawnedActor))
	{
		SpawnedActor->Tags.AddUnique(SurvivalSovietBaseConstants::SovietBaseActorTag);
		ValidateEnemyOwnership(SpawnedActor);
		Pending->SpawnedUnits.Add(SpawnedActor);
		Base.GarrisonUnits.Add(SpawnedActor);
	}
	else
	{
		RTSFunctionLibrary::ReportError("A Soviet base garrison unit failed to spawn (base "
			+ FString::FromInt(Base.BaseIndex) + ").");
	}

	Pending->PendingUnits--;
	if (Pending->PendingUnits > 0)
	{
		return;
	}
	const FPendingGarrisonGroup CompletedGroup = *Pending;
	M_PendingGarrisonGroups.Remove(GroupId);
	Base.PendingGarrisonGroups--;
	OnGarrisonGroupComplete(CompletedGroup);
}

void USurvivalSovietBaseComponent::OnGarrisonGroupComplete(const FPendingGarrisonGroup& Group)
{
	if (not M_Bases.IsValidIndex(Group.BaseArrayIndex))
	{
		return;
	}
	const FSurvivalSovietBaseState& Base = M_Bases[Group.BaseArrayIndex];
	if (not Base.Plan.Garrison.IsValidIndex(Group.PlannedGroupIndex))
	{
		return;
	}
	const FSurvivalSovietPlannedGarrison& Planned = Base.Plan.Garrison[Group.PlannedGroupIndex];

	switch (Planned.Behaviour)
	{
	case ESurvivalSovietGarrisonBehaviour::Patrol:
		StartGarrisonPatrol(Base, Planned, Group.SpawnedUnits);
		break;
	case ESurvivalSovietGarrisonBehaviour::StrategicAIReserve:
		RegisterGarrisonAsStrategicReserve(Group.SpawnedUnits);
		break;
	case ESurvivalSovietGarrisonBehaviour::HoldPosition:
	default:
		// Spawned facing the player's start location; units auto-engage what enters their range.
		break;
	}
}

void USurvivalSovietBaseComponent::RegisterGarrisonAsStrategicReserve(
	const TArray<TWeakObjectPtr<AActor>>& Units) const
{
	// Same path the strategic AI uses for its own freshly trained units: idle on the blackboard.
	for (const TWeakObjectPtr<AActor>& WeakUnit : Units)
	{
		ICommands* Commands = Cast<ICommands>(WeakUnit.Get());
		if (Commands == nullptr)
		{
			continue;
		}
		constexpr bool bSetUnitToIdle = true;
		Commands->RegisterAsBlackboardIdle(bSetUnitToIdle);
	}
}

void USurvivalSovietBaseComponent::StartGarrisonPatrol(
	const FSurvivalSovietBaseState& Base,
	const FSurvivalSovietPlannedGarrison& Planned,
	const TArray<TWeakObjectPtr<AActor>>& Units) const
{
	using namespace SurvivalSovietBaseConstants;
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	if (not IsValid(EnemyController))
	{
		RTSFunctionLibrary::ReportError("Soviet base patrol cannot start: no valid AEnemyController in the level."
			" The patrol units stay where they spawned.");
		return;
	}

	TArray<ASquadController*> Squads;
	TArray<ATankMaster*> Tanks;
	FVector AverageLocation = FVector::ZeroVector;
	for (const TWeakObjectPtr<AActor>& WeakUnit : Units)
	{
		AActor* Unit = WeakUnit.Get();
		if (not IsValid(Unit))
		{
			continue;
		}
		if (ASquadController* Squad = Cast<ASquadController>(Unit))
		{
			Squads.Add(Squad);
		}
		else if (ATankMaster* Tank = Cast<ATankMaster>(Unit))
		{
			Tanks.Add(Tank);
		}
		else
		{
			continue;
		}
		AverageLocation += Unit->GetActorLocation();
	}
	const int32 UnitCount = Squads.Num() + Tanks.Num();
	if (UnitCount == 0 or Planned.PatrolPoints.IsEmpty())
	{
		return;
	}
	AverageLocation /= UnitCount;

	TArray<FVector> PatrolPoints;
	for (const FVector2D& Point : Planned.PatrolPoints)
	{
		PatrolPoints.Add(ProjectToGround(Base, Point));
	}

	EnemyController->MoveRandomPatrolWithAttackMoveFormation(
		Squads,
		Tanks,
		PatrolPoints,
		INDEX_NONE,
		Planned.IterationsAtPatrolPoint,
		Planned.GuardSecondsPerIteration,
		Planned.GuardSphereRadius,
		PatrolMaxFormationWidth,
		PatrolFormationOffsetMultiplier,
		FAttackMoveWaveSettings(),
		AverageLocation);
}

// ---------------------------------------------------------------------------
// Strategic AI
// ---------------------------------------------------------------------------

void USurvivalSovietBaseComponent::ConfigureStrategicAIBaseDetection(
	const USurvivalSovietBaseSettings& Settings,
	const FSurvivalSovietBasePlan& Plan)
{
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	if (not IsValid(EnemyController))
	{
		if (not bM_HasConfiguredStrategicAI)
		{
			bM_HasConfiguredStrategicAI = true;
			RTSFunctionLibrary::ReportError("Soviet bases: no AEnemyController in the level; the strategic AI will"
				" not use the bases (place RTS_BP_EnemyController in the survival map).");
		}
		return;
	}
	UEnemyStrategicAIComponent* StrategicAI = EnemyController->GetEnemyStrategicAIComponent();
	if (not IsValid(StrategicAI))
	{
		return;
	}

	FFindEnemyBaseClusters& Request = StrategicAI->FindEnemyBase_TimerRequest;
	for (const EBuildingExpansionType CoreType : Settings.StrategicAICoreBuildingTypes)
	{
		Request.CoreBuildingTypes.AddUnique(CoreType);
	}
	for (const EBuildingExpansionType SatelliteType : Settings.StrategicAISatelliteBuildingTypes)
	{
		Request.SatelliteBuildingTypes.AddUnique(SatelliteType);
	}
	const int32 ExpectedBases = FMath::Max(1, GetUnpickedStartLocations().Num());
	Request.MaxBasesToReturn = FMath::Max(Request.MaxBasesToReturn, ExpectedBases);
	const float RequiredDistance = static_cast<float>(
		Plan.RequiredCoreClusterDistance + SurvivalSovietBaseConstants::ClusterDistanceMargin);
	Request.CoreClusterDistanceXY = FMath::Max3(
		Request.CoreClusterDistanceXY, Settings.MinStrategicAICoreClusterDistance, RequiredDistance);
	// A base's score is its building count (core + attached satellites); the request default of 100 would reject
	// every Soviet base, so cap the thresholds (never raise them).
	Request.MinBaseScoreToReturn = FMath::Min(Request.MinBaseScoreToReturn, Settings.StrategicAIMaxMinBaseScore);
	Request.MinTotalBuildingsPerBase = FMath::Min(
		Request.MinTotalBuildingsPerBase, Settings.StrategicAIMaxMinBuildingsPerBase);
	bM_HasConfiguredStrategicAI = true;
}
