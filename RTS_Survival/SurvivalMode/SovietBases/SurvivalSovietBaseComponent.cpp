// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseComponent.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/AssetManager.h"
#include "PCGGraph.h"
#include "EngineUtils.h"
#include "Engine/HitResult.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/AsyncUnitDetailedState/AsyncUnitDetailedState.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseLantern.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBasePad.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBasePreview.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseSettings.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Environment/Splines/ProceduralDestructibleSpline/ProceduralDestructibleSplineSpawner.h"

namespace SurvivalSovietBaseConstants
{
	constexpr uint8 EnemyPlayer = 2;
	// Pipes between fuel tanks only to a neighbour standing beside it.
	constexpr float MaxBasePipeRange = 1200.f;
	constexpr double MeasureSpawnDepth = -100000.0;
	constexpr double DefaultFootprintHalfExtent = 300.0;
	constexpr double MinFootprintHalfExtent = 0.5;
	constexpr double GroundTraceUp = 10000.0;
	constexpr double GroundTraceDown = 30000.0;
	constexpr double SiteReachPastBuildArea = 5000.0;
	constexpr int32 SiteTracesPerStep = 500;
	constexpr int32 ClassesMeasuredPerStep = 2;
	constexpr float PreparationStepSeconds = 0.02f;
	constexpr double MaxFootprintComponentDistance = 15000.0;
	constexpr double RoadGroundOffset = 6.0;
	// PathWidth the tiles of a freshly spawned ground path Blueprint are scaled for.
	constexpr double RoadDefaultWidth = 520.0;
	// Road decals: painted in pieces that follow the ground, with soft sides and faded ends.
	constexpr double RoadPieceLength = 1600.0;
	constexpr double RoadSimplifyTolerance = 140.0;
	constexpr double RoadPieceOverlap = 1.0;
	constexpr double RoadEdgeShare = 0.38;
	constexpr double RoadEdgeNoiseShare = 0.17;
	constexpr double RoadFadeLength = 1200.0;
	constexpr double RoadFadeShareOfSegment = 0.45;
	constexpr double RoadReachIntoJunction = 0.8;
	constexpr int32 RoadSortOrder = -6;
	constexpr int32 JunctionSortOrder = -5;
	constexpr int32 ApronSortOrder = -4;
	constexpr double JunctionFillet = 280.0;
	constexpr double JunctionVergeStrength = 0.15;
	constexpr double ApronVergeStrength = 0.5;
	constexpr double JunctionTolerance = 60.0;
	constexpr double ApronAlongShare = 0.7;
	constexpr double ApronAcrossShare = 0.5;
	constexpr double ApronAcrossMargin = 160.0;
	constexpr double ApronOutwardShift = 0.15;
	constexpr double ApronCornerRadius = 420.0;
	constexpr double ApronEdgeWidth = 300.0;
	constexpr double ApronEdgeNoise = 130.0;
	constexpr double UnitSpawnZOffset = 50.0;
	constexpr double ClusterDistanceMargin = 250.0;
	constexpr int32 MaxBases = 32;
	constexpr int32 PatrolIterationsAtPoint = 2;
	constexpr float PatrolGuardSeconds = 8.f;
	constexpr float PatrolGuardRadius = 600.f;
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
	// A layout in progress points at its base's site map, so the array must not move while a graph runs.
	M_Bases.Reserve(SurvivalSovietBaseConstants::MaxBases);
#if RTS_WITH_MAP_TESTS
	FSurvivalSovietBasePreview::StartIfRequested(Director, this);
#endif
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
	Base.Seed = ComputeBaseSeed(*StartLocation, BaseIndex, M_MatchSeed);
	const ASurvivalStartLocation* Chosen = M_Director->GetChosenStartLocation();
	const int32 BaseCount = FMath::Max(1, GetUnpickedStartLocations(Chosen).Num());
	ApplyDistanceRank(Base, ComputeDistanceRank(*StartLocation, Chosen, BaseIndex), BaseCount, *Settings);
	TArray<int32> UsedPresets;
	for (int32 Index = 0; Index + 1 < M_Bases.Num(); ++Index)
	{
		UsedPresets.Add(M_Bases[Index].PresetIndex);
	}
	Base.PresetIndex = PickPresetIndex(*Settings, Base, UsedPresets);
	Base.GroundTraceParams = FCollisionQueryParams(TEXT("SurvivalSovietBaseGround"), false);

	BeginBaseGeneration(BaseArrayIndex, *Settings);
}

void USurvivalSovietBaseComponent::BeginBaseGeneration(const int32 BaseArrayIndex,
                                                       const USurvivalSovietBaseSettings& Settings)
{
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	const FSurvivalSovietBasePreset& Preset = Settings.Presets[Base.PresetIndex];
	const TArray<FSoftObjectPath> ClassPaths = CollectPresetClassPaths(Preset, Settings.Kit);
	UE_LOG(LogTemp, Log, TEXT("Survival Soviet base %d: preset %s, strength %d, %d classes to load."), Base.BaseIndex,
	       *Preset.PresetName.ToString(), Base.Strength, ClassPaths.Num());
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
		World->GetTimerManager().ClearTimer(M_PreparationTimerHandle);
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
	M_BasesInPreparation.Reset();
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
                                                    const int32 BaseIndex,
                                                    const int32 MatchSeed) const
{
	// Location-based so the same map + match seed always gives the same base on the same spot.
	const FVector Center = StartLocation.GetDeployCenter();
	const FIntVector RoundedCenter(
		FMath::RoundToInt(Center.X / 100.0), FMath::RoundToInt(Center.Y / 100.0), 0);
	uint32 Seed = HashCombine(GetTypeHash(MatchSeed), GetTypeHash(RoundedCenter));
	Seed = HashCombine(Seed, GetTypeHash(BaseIndex));
	return static_cast<int32>(Seed);
}

TArray<ASurvivalStartLocation*> USurvivalSovietBaseComponent::GetUnpickedStartLocations(
	const ASurvivalStartLocation* Chosen) const
{
	TArray<ASurvivalStartLocation*> Unpicked;
	if (not GetIsValidDirector())
	{
		return Unpicked;
	}
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

int32 USurvivalSovietBaseComponent::ComputeDistanceRank(
	const ASurvivalStartLocation& StartLocation,
	const ASurvivalStartLocation* Chosen,
	const int32 BaseIndex) const
{
	const TArray<ASurvivalStartLocation*> Unpicked = GetUnpickedStartLocations(Chosen);
	const int32 BaseCount = FMath::Max(1, Unpicked.Num());
	int32 DistanceRank = BaseIndex;

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
	return FMath::Clamp(DistanceRank, 0, BaseCount - 1);
}

int32 USurvivalSovietBaseComponent::ReadRankTable(const TArray<int32>& Table, const int32 DistanceRank, const int32 BaseCount)
{
	// The tables are written for any number of bases: nearest = first entry, farthest = last, stretched in between.
	if (Table.IsEmpty())
	{
		return 0;
	}
	const int32 TableIndex = BaseCount <= 1 ? 0 : FMath::RoundToInt32(
		static_cast<double>(DistanceRank) * (Table.Num() - 1) / (BaseCount - 1));
	return Table[FMath::Clamp(TableIndex, 0, Table.Num() - 1)];
}

void USurvivalSovietBaseComponent::ApplyDistanceRank(FSurvivalSovietBaseState& Base, const int32 DistanceRank,
                                                     const int32 BaseCount, const USurvivalSovietBaseSettings& Settings) const
{
	Base.DistanceRank = DistanceRank;
	Base.bIsFarthestBase = DistanceRank == BaseCount - 1 and BaseCount > 1;
	Base.Strength = FMath::Clamp(ReadRankTable(Base.Scaling.StrengthByDistanceRank, DistanceRank, BaseCount), 0, Settings.MaxStrength);
	Base.ThreatBudget = ReadRankTable(Base.Scaling.ThreatBudgetByDistanceRank, DistanceRank, BaseCount);
	Base.ReserveBudget = ReadRankTable(Base.Scaling.ReservePointsByDistanceRank, DistanceRank, BaseCount);
}

int32 USurvivalSovietBaseComponent::PickPresetIndex(
	const USurvivalSovietBaseSettings& Settings,
	const FSurvivalSovietBaseState& Base,
	const TArray<int32>& UsedPresets) const
{
	const int32 Strength = Base.Strength;
	TArray<int32> Candidates;
	TArray<float> Weights;
	TArray<bool> bCandidateUsed;
	bool bHasUnusedCandidate = false;
	for (int32 PresetIndex = 0; PresetIndex < Settings.Presets.Num(); ++PresetIndex)
	{
		const FSurvivalSovietBasePreset& Preset = Settings.Presets[PresetIndex];
		if (Preset.Weight <= 0.f or Strength < Preset.MinStrength or Strength > Preset.MaxStrength
			or Base.Scaling.MaxProductionTier < Preset.MinProductionTier)
		{
			continue;
		}
		const float* FarthestWeight = Base.bIsFarthestBase ? Base.Scaling.FarthestBasePresetWeights.Find(Preset.PresetName) : nullptr;
		if (FarthestWeight != nullptr and *FarthestWeight <= 0.f)
		{
			continue;
		}
		const bool bAlreadyUsed = UsedPresets.ContainsByPredicate([&Settings, PresetIndex](const int32 UsedPreset)
		{
			return Settings.Presets.IsValidIndex(UsedPreset)
				and Settings.Presets[UsedPreset].Archetype == Settings.Presets[PresetIndex].Archetype;
		});
		Candidates.Add(PresetIndex);
		Weights.Add(Preset.Weight * (FarthestWeight != nullptr ? *FarthestWeight : 1.f)
			* (bAlreadyUsed ? Settings.RepeatedPresetWeightFactor : 1.f));
		bHasUnusedCandidate = bHasUnusedCandidate or not bAlreadyUsed;
		bCandidateUsed.Add(bAlreadyUsed);
	}
	// Two bases of one match never share a master plan while another plan is free for that strength.
	for (int32 Index = Candidates.Num() - 1; Index >= 0 and bHasUnusedCandidate; --Index)
	{
		if (bCandidateUsed[Index])
		{
			Candidates.RemoveAt(Index);
			Weights.RemoveAt(Index);
		}
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
	const FRandomStream Random(Base.Seed);
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
	const FSurvivalSovietBasePreset& Preset,
	const FSurvivalSovietSiteKit& Kit) const
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
	for (const FSurvivalSovietThreatStep& Step : Preset.ThreatPath)
	{
		AddEntry(Step.Building);
	}

	AddSlots(Preset.ProductionBuildings);
	AddSlots(Preset.SupportBuildings);
	AddEntries(Preset.ObstacleBelt.Obstacles);
	for (const TArray<FEnemyBlueprintEntry>* KitEntries : {
		     &Kit.SandbagRuns, &Kit.BarbedWire, &Kit.GateBlocks, &Kit.Revetments, &Kit.CamoNets, &Kit.Wrecks, &Kit.Huts, &Kit.Hangars, &Kit.Flags, &Kit.Crates,
		     &Kit.Barrels, &Kit.Munitions, &Kit.Materials
	     })
	{
		AddEntries(*KitEntries);
	}
	for (const FSurvivalSovietWallFamily& Family : Kit.WallFamilies)
	{
		for (const FSurvivalSovietWallPiece* Piece : {
			     &Family.Straight, &Family.FlagStraight, &Family.StartCap, &Family.EndCap, &Family.FlagStartCap, &Family.FlagEndCap,
			     &Family.Corner, &Family.Gate, &Family.GateLeaf
		     })
		{
			AddEntry(Piece->Entry);
		}
	}
	for (const TSoftObjectPtr<UMaterialInterface>* PadMaterial : {
		     &Kit.ConcretePad, &Kit.GravelPad, &Kit.PackedEarthPad, &Kit.EarthStandIn, &Kit.MainRoadSurface, &Kit.LaneRoadSurface,
		     &Kit.TrackRoadSurface, &Kit.StripRoadSurface, &Kit.GateApronSurface
	     })
	{
		if (not PadMaterial->IsNull())
		{
			Paths.AddUnique(PadMaterial->ToSoftObjectPath());
		}
	}
	AddEntries(Kit.YardNets);
	AddEntry(Kit.Lantern);
	if (not Kit.LanternMesh.IsNull())
	{
		Paths.AddUnique(Kit.LanternMesh.ToSoftObjectPath());
	}
	for (const TSoftClassPtr<AActor>* RoadClass : {&Kit.MainRoadClass, &Kit.LaneRoadClass, &Kit.StripRoadClass})
	{
		if (not RoadClass->IsNull())
		{
			Paths.AddUnique(RoadClass->ToSoftObjectPath());
		}
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

	StartBaseGraph(BaseArrayIndex, *Settings);
}

void USurvivalSovietBaseComponent::StartBaseGraph(const int32 BaseArrayIndex, const USurvivalSovietBaseSettings& Settings)
{
	UWorld* World = GetWorld();
	UPCGGraphInterface* Graph = Settings.BaseGraph.LoadSynchronous();
	if (not IsValid(World) or not IsValid(Graph))
	{
		RTSFunctionLibrary::ReportError("Soviet bases have no PCG graph set in USurvivalSovietBaseSettings::BaseGraph; "
			"no base generated.");
		return;
	}
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASurvivalSovietBasePCGVolume* Volume = World->SpawnActor<ASurvivalSovietBasePCGVolume>(
		ASurvivalSovietBasePCGVolume::StaticClass(), FTransform(Base.Center), SpawnParams);
	if (not IsValid(Volume))
	{
		RTSFunctionLibrary::ReportError("Soviet base could not spawn its PCG volume.");
		return;
	}
	Base.Volume = Volume;
	Volume->StartGeneration(this, BaseArrayIndex, Graph, Base.Seed, Base.StartLocation->GetBuildAreaRadius());
}

bool USurvivalSovietBaseComponent::GetIsBasePrepared(const int32 BaseArrayIndex) const
{
	return M_Bases.IsValidIndex(BaseArrayIndex) and M_Bases[BaseArrayIndex].bPrepared;
}

bool USurvivalSovietBaseComponent::GetIsBaseSpawnRequested(const int32 BaseArrayIndex) const
{
	return M_Bases.IsValidIndex(BaseArrayIndex) and M_Bases[BaseArrayIndex].bSpawnRequested;
}

const FSurvivalSovietSiteMap* USurvivalSovietBaseComponent::GetBaseSite(const int32 BaseArrayIndex) const
{
	return M_Bases.IsValidIndex(BaseArrayIndex) ? &M_Bases[BaseArrayIndex].Site : nullptr;
}

const FSurvivalSovietBasePlan* USurvivalSovietBaseComponent::GetBasePlan(const int32 BaseArrayIndex) const
{
	return M_Bases.IsValidIndex(BaseArrayIndex) ? &M_Bases[BaseArrayIndex].Plan : nullptr;
}

void USurvivalSovietBaseComponent::BeginBasePreparation(const int32 BaseArrayIndex, TFunction<void()> OnPrepared)
{
	using namespace SurvivalSovietBaseConstants;
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or not M_Bases.IsValidIndex(BaseArrayIndex))
	{
		if (OnPrepared)
		{
			OnPrepared();
		}
		return;
	}
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.OnGraphStepDone = MoveTemp(OnPrepared);
	const ASurvivalStartLocation* StartLocation = Base.StartLocation.Get();
	Base.Site.Init(FVector2D(Base.Center), Base.Center.Z, StartLocation->GetBuildAreaRadius(), SiteReachPastBuildArea);
	// The load list also holds the kit's decal materials and the lantern mesh; only Blueprint classes are measured.
	const FSurvivalSovietSiteKit& Kit = Settings->Kit;
	TSet<FSoftObjectPath> NotClasses = {Kit.LanternMesh.ToSoftObjectPath()};
	for (const TSoftObjectPtr<UMaterialInterface>* Material : {
		     &Kit.ConcretePad, &Kit.GravelPad, &Kit.PackedEarthPad, &Kit.EarthStandIn, &Kit.MainRoadSurface, &Kit.LaneRoadSurface,
		     &Kit.TrackRoadSurface, &Kit.StripRoadSurface, &Kit.GateApronSurface
	     })
	{
		NotClasses.Add(Material->ToSoftObjectPath());
	}
	for (const FSoftObjectPath& Path : CollectPresetClassPaths(Settings->Presets[Base.PresetIndex], Kit))
	{
		if (not NotClasses.Contains(Path))
		{
			Base.ClassesToMeasure.Add(TSoftClassPtr<AActor>(Path));
		}
	}
	M_BasesInPreparation.Add(BaseArrayIndex);

	UWorld* World = GetWorld();
	if (IsValid(World) and not World->GetTimerManager().IsTimerActive(M_PreparationTimerHandle))
	{
		World->GetTimerManager().SetTimer(M_PreparationTimerHandle, this,
		                                  &USurvivalSovietBaseComponent::ProcessPreparation,
		                                  PreparationStepSeconds, true, 0.f);
	}
}

void USurvivalSovietBaseComponent::ProcessPreparation()
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or M_BasesInPreparation.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_PreparationTimerHandle);
		}
		return;
	}
	const int32 BaseArrayIndex = M_BasesInPreparation[0];
	if (not M_Bases.IsValidIndex(BaseArrayIndex))
	{
		M_BasesInPreparation.RemoveAt(0);
		return;
	}
	if (AdvanceBasePreparation(M_Bases[BaseArrayIndex], *Settings))
	{
		M_BasesInPreparation.RemoveAt(0);
		OnBasePrepared(BaseArrayIndex);
	}
}

bool USurvivalSovietBaseComponent::AdvanceBasePreparation(FSurvivalSovietBaseState& Base,
                                                          const USurvivalSovietBaseSettings& Settings)
{
	using namespace SurvivalSovietBaseConstants;
	for (int32 Measured = 0; Measured < ClassesMeasuredPerStep and not Base.ClassesToMeasure.IsEmpty(); ++Measured)
	{
		FindOrResolveClass(Base.ClassesToMeasure.Pop());
	}
	if (not Base.ClassesToMeasure.IsEmpty())
	{
		return false;
	}
	TraceSiteSamples(Base, SiteTracesPerStep);
	return Base.NextSiteSample >= Base.Site.GetSampleCount();
}

void USurvivalSovietBaseComponent::TraceSiteSamples(FSurvivalSovietBaseState& Base, const int32 SampleBudget) const
{
	using namespace SurvivalSovietBaseConstants;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const int32 LastSample = FMath::Min(Base.NextSiteSample + SampleBudget, Base.Site.GetSampleCount());
	for (; Base.NextSiteSample < LastSample; ++Base.NextSiteSample)
	{
		const FVector2D Position = Base.Site.GetSamplePosition(Base.NextSiteSample);
		FHitResult Hit;
		const FVector TraceStart(Position, Base.Center.Z + GroundTraceUp);
		const FVector TraceEnd(Position, Base.Center.Z - GroundTraceDown);
		const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Base.GroundTraceParams);
		Base.Site.SetSample(Base.NextSiteSample, bHit ? Hit.ImpactPoint.Z : Base.Center.Z, bHit);
	}
	if (Base.NextSiteSample >= Base.Site.GetSampleCount())
	{
		Base.Site.Finish();
	}
}

void USurvivalSovietBaseComponent::OnBasePrepared(const int32 BaseArrayIndex)
{
	if (not M_Bases.IsValidIndex(BaseArrayIndex))
	{
		return;
	}
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.bPrepared = true;
	WakeBaseGraph(Base);
}

void USurvivalSovietBaseComponent::WakeBaseGraph(FSurvivalSovietBaseState& Base)
{
	if (Base.OnGraphStepDone)
	{
		const TFunction<void()> WakeUp = MoveTemp(Base.OnGraphStepDone);
		Base.OnGraphStepDone = nullptr;
		WakeUp();
	}
}

int32 USurvivalSovietBaseComponent::RunBasePlanStage(const int32 BaseArrayIndex, const ESurvivalSovietPlanStage Stage)
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or not M_Bases.IsValidIndex(BaseArrayIndex))
	{
		return 0;
	}
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	if (not Base.LayoutRun.IsValid())
	{
		Base.LayoutRun = MakeShared<FSurvivalSovietBaseLayoutRun>(BuildLayoutInput(Base, *Settings));
	}
	const int32 FirstStructure = Base.Plan.Structures.Num();
	const double StageStartSeconds = FPlatformTime::Seconds();
	Base.LayoutRun->RunStage(Stage, Base.Plan);
	Base.LayoutMilliseconds += (FPlatformTime::Seconds() - StageStartSeconds) * 1000.0;
	return FirstStructure;
}

void USurvivalSovietBaseComponent::SpawnBaseFromGraph(const int32 BaseArrayIndex,
                                                      TArray<FSurvivalSovietPlannedStructure>&& Structures,
                                                      TArray<FSurvivalSovietPlannedRoad>&& Roads,
                                                      TFunction<void()> OnSpawned)
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or not M_Bases.IsValidIndex(BaseArrayIndex))
	{
		if (OnSpawned)
		{
			OnSpawned();
		}
		return;
	}
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.bSpawnRequested = true;
	Base.OnGraphStepDone = MoveTemp(OnSpawned);
	// What the graph hands over is what gets built: a node between the stages and here may have filtered it.
	Base.Plan.Structures = MoveTemp(Structures);
	Base.Plan.Roads = MoveTemp(Roads);
	FinishBasePlan(Base);
	UE_LOG(LogTemp, Log, TEXT("Survival Soviet base %d planned by PCG: %d structures, %d roads, %d production, %.2f ms."),
	       Base.BaseIndex, Base.Plan.Structures.Num(), Base.Plan.Roads.Num(), Base.Plan.ProductionCount,
	       Base.LayoutMilliseconds);
	if (Settings->bConfigureStrategicAIBaseDetection)
	{
		ConfigureStrategicAIBaseDetection(*Settings, Base.Plan);
	}
	EnqueueBaseStructures(BaseArrayIndex);
}

FSurvivalSovietLayoutInput USurvivalSovietBaseComponent::BuildLayoutInput(const FSurvivalSovietBaseState& Base,
                                                                          const USurvivalSovietBaseSettings& Settings)
{
	const ASurvivalStartLocation* StartLocation = Base.StartLocation.Get();
	const ASurvivalStartLocation* Chosen = GetIsValidDirector() ? M_Director->GetChosenStartLocation() : nullptr;

	FSurvivalSovietLayoutInput Input;
	Input.Preset = &Settings.Presets[Base.PresetIndex];
	Input.Kit = &Settings.Kit;
	Input.Site = &Base.Site;
	Input.Scaling = Base.Scaling;
	Input.Strength = Base.Strength;
	Input.Seed = Base.Seed;
	Input.ThreatBudget = Base.ThreatBudget;
	Input.ReserveBudget = Base.ReserveBudget;
	Input.Center = FVector2D(Base.Center);
	Input.BuildAreaRadius = StartLocation->GetBuildAreaRadius();
	// Without a chosen location (debug), face the map origin.
	Input.ThreatLocation = Base.ThreatOverride.IsSet()
		                       ? Base.ThreatOverride.GetValue()
		                       : (IsValid(Chosen) ? FVector2D(Chosen->GetDeployCenter()) : FVector2D::ZeroVector);
	for (const FVector& DefensePoint : StartLocation->GetDefensePoints())
	{
		Input.DefensePoints.Add(FVector2D(DefensePoint));
	}
	Input.ResolveEntry = [this](const FEnemyBlueprintEntry& Entry, FEnemyResolvedEntry& OutResolved)
	{
		return ResolveEntry(Entry, OutResolved);
	};

	return Input;
}

void USurvivalSovietBaseComponent::FinishBasePlan(FSurvivalSovietBaseState& Base)
{
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

	const FVector ActorLocation = InspectionActor->GetActorLocation();
	// Colliders are what stands on the ground: most Rus Blueprints also carry a large non-colliding selection sphere,
	// and some leave a hidden component at the world origin.
	FBox CollidingBounds(ForceInit);
	FBox AllBounds(ForceInit);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(InspectionActor);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (FVector::Dist2D(Primitive->Bounds.Origin, ActorLocation) > SurvivalSovietBaseConstants::MaxFootprintComponentDistance)
		{
			continue;
		}
		AllBounds += Primitive->Bounds.GetBox();
		if (Primitive->IsCollisionEnabled())
		{
			CollidingBounds += Primitive->Bounds.GetBox();
		}
	}
	InspectionActor->Destroy();
	const FBox WorldBounds = CollidingBounds.IsValid ? CollidingBounds : AllBounds;
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
	Base.PendingStructureSpawns = Base.Plan.Structures.Num() + Base.Plan.Roads.Num();
	if (Base.PendingStructureSpawns == 0)
	{
		OnBaseStructuresSpawned(BaseArrayIndex);
		return;
	}

	// Roads first: buildings must not be in the way of the traces that snap a road to the ground.
	for (int32 RoadIndex = 0; RoadIndex < Base.Plan.Roads.Num(); ++RoadIndex)
	{
		M_SpawnQueue.Add({BaseArrayIndex, INDEX_NONE, RoadIndex});
	}
	for (int32 StructureIndex = 0; StructureIndex < Base.Plan.Structures.Num(); ++StructureIndex)
	{
		M_SpawnQueue.Add({BaseArrayIndex, StructureIndex, INDEX_NONE});
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
		if (Base.Plan.Roads.IsValidIndex(Pending.RoadIndex))
		{
			if (Settings != nullptr)
			{
				SpawnRoad(Base, Pending.RoadIndex, *Settings);
			}
			if (Settings != nullptr and Pending.RoadIndex == Base.Plan.Roads.Num() - 1)
			{
				SpawnJunctionsAndAprons(Base, *Settings);
			}
			OnPendingSpawnDone(Pending.BaseArrayIndex);
			continue;
		}
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

void USurvivalSovietBaseComponent::SpawnRoad(FSurvivalSovietBaseState& Base,
                                             const int32 RoadIndex,
                                             const USurvivalSovietBaseSettings& Settings)
{
	const FSurvivalSovietPlannedRoad& Road = Base.Plan.Roads[RoadIndex];
	UMaterialInterface* Surface = GetRoadSurface(Road.Kind, Settings.Kit);
	if (Surface != nullptr)
	{
		SpawnRoadDecals(Base, RoadIndex, *Surface, Settings.Kit);
		return;
	}
	UWorld* World = GetWorld();
	const TSoftClassPtr<AActor>& RoadClass = Road.Kind == ESurvivalSovietRoadKind::Main
		                                         ? Settings.Kit.MainRoadClass
		                                         : (Road.Kind == ESurvivalSovietRoadKind::Strip
			                                            ? Settings.Kit.StripRoadClass
			                                            : Settings.Kit.LaneRoadClass);
	UClass* ActorClass = RoadClass.Get();
	if (not IsValid(World) or ActorClass == nullptr or Road.Points.Num() < 2)
	{
		return;
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location = ProjectToGround(Base, Road.Points[0]);
	AActor* RoadActor = World->SpawnActor<AActor>(ActorClass, FTransform(Location), SpawnParams);
	if (not IsValid(RoadActor))
	{
		return;
	}
	RoadActor->Tags.Add(SurvivalSovietBaseConstants::SovietBaseActorTag);
	ApplyRoadToSpline(*RoadActor, Base, Road);
	Base.SpawnedRoads.Add(RoadActor);
}

TArray<FVector2D> USurvivalSovietBaseComponent::SimplifyRoadPoints(const TArray<FVector2D>& Points, const double Tolerance)
{
	if (Points.Num() < 3)
	{
		return Points;
	}
	// Douglas-Peucker without recursion: keep the point farthest from each chord while it is off by more than Tolerance.
	TArray<bool> bKeep;
	bKeep.Init(false, Points.Num());
	bKeep[0] = true;
	bKeep.Last() = true;
	TArray<TPair<int32, int32>> Stack = {TPair<int32, int32>(0, Points.Num() - 1)};
	while (not Stack.IsEmpty())
	{
		const TPair<int32, int32> Chord = Stack.Pop();
		const FVector2D A = Points[Chord.Key];
		const FVector2D Along = (Points[Chord.Value] - A).GetSafeNormal();
		int32 Farthest = INDEX_NONE;
		double FarthestDistance = Tolerance;
		for (int32 Index = Chord.Key + 1; Index < Chord.Value; ++Index)
		{
			const double Distance = FMath::Abs(FVector2D::CrossProduct(Along, Points[Index] - A));
			if (Distance > FarthestDistance)
			{
				FarthestDistance = Distance;
				Farthest = Index;
			}
		}
		if (Farthest != INDEX_NONE)
		{
			bKeep[Farthest] = true;
			Stack.Add(TPair<int32, int32>(Chord.Key, Farthest));
			Stack.Add(TPair<int32, int32>(Farthest, Chord.Value));
		}
	}
	TArray<FVector2D> Kept;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		if (bKeep[Index])
		{
			Kept.Add(Points[Index]);
		}
	}
	return Kept;
}

UMaterialInterface* USurvivalSovietBaseComponent::GetRoadSurface(const ESurvivalSovietRoadKind Kind,
                                                                 const FSurvivalSovietSiteKit& Kit) const
{
	switch (Kind)
	{
	case ESurvivalSovietRoadKind::Main:
		return Kit.MainRoadSurface.Get();
	case ESurvivalSovietRoadKind::Strip:
		return Kit.StripRoadSurface.Get();
	case ESurvivalSovietRoadKind::Track:
		return Kit.TrackRoadSurface.Get();
	case ESurvivalSovietRoadKind::Lane:
	case ESurvivalSovietRoadKind::None:
	default:
		return Kit.LaneRoadSurface.Get();
	}
}

double USurvivalSovietBaseComponent::GetRoadPaintHalfWidth(const FSurvivalSovietPlannedRoad& Road, const FSurvivalSovietSiteKit& Kit)
{
	using namespace SurvivalSovietBaseConstants;
	switch (Road.Kind)
	{
	case ESurvivalSovietRoadKind::Main:
		return Road.Width * 0.5 * Kit.MainRoadPaintShare;
	case ESurvivalSovietRoadKind::Track:
		return Road.Width * 0.5 * Kit.TrackRoadPaintShare;
	case ESurvivalSovietRoadKind::Strip:
		return Road.Width * 0.5;
	case ESurvivalSovietRoadKind::Lane:
	case ESurvivalSovietRoadKind::None:
	default:
		return Road.Width * 0.5 * Kit.LaneRoadPaintShare;
	}
}

int32 USurvivalSovietBaseComponent::FindRoadUnder(const FSurvivalSovietBaseState& Base, const FVector2D& Point, const int32 ExcludeIndex,
                                                  const FSurvivalSovietSiteKit& Kit, FVector2D& OutOnCentre,
                                                  FVector2D& OutDirection) const
{
	using namespace SurvivalSovietBaseConstants;
	for (int32 RoadIndex = 0; RoadIndex < Base.Plan.Roads.Num(); ++RoadIndex)
	{
		const FSurvivalSovietPlannedRoad& Road = Base.Plan.Roads[RoadIndex];
		const double Reach = GetRoadPaintHalfWidth(Road, Kit) + JunctionTolerance;
		for (int32 Index = 0; RoadIndex != ExcludeIndex and Index + 1 < Road.Points.Num(); ++Index)
		{
			const FVector2D& A = Road.Points[Index];
			const FVector2D& B = Road.Points[Index + 1];
			const FVector2D Along = (B - A).GetSafeNormal();
			const FVector2D OnCentre = A + Along * FMath::Clamp(FVector2D::DotProduct(Point - A, Along), 0.0, FVector2D::Distance(A, B));
			if (FVector2D::Distance(Point, OnCentre) < Reach)
			{
				OutOnCentre = OnCentre;
				OutDirection = Along;
				return RoadIndex;
			}
		}
	}
	return INDEX_NONE;
}

void USurvivalSovietBaseComponent::SpawnGroundDecal(FSurvivalSovietBaseState& Base, const FSurvivalSovietGroundDecalShape& Shape,
                                                    UMaterialInterface* Material)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FVector2D Where = Shape.BoxHalfAlong > 0.0 ? Shape.BoxCenter : Shape.Center;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASurvivalSovietBasePad* Decal = World->SpawnActor<ASurvivalSovietBasePad>(
		ASurvivalSovietBasePad::StaticClass(), FTransform(ProjectToGround(Base, Where)), SpawnParams);
	if (not IsValid(Decal))
	{
		return;
	}
	Decal->Tags.Add(SurvivalSovietBaseConstants::SovietBaseActorTag);
	Decal->InitShape(Shape, Material);
	Base.SpawnedRoads.Add(Decal);
}

void USurvivalSovietBaseComponent::SpawnRoadDecals(FSurvivalSovietBaseState& Base, const int32 RoadIndex, UMaterialInterface& Surface,
                                                   const FSurvivalSovietSiteKit& Kit)
{
	using namespace SurvivalSovietBaseConstants;
	FSurvivalSovietPlannedRoad Road = Base.Plan.Roads[RoadIndex];
	// Paths found over the terrain grid have a point every few metres; one decal per straight stretch reads better.
	Road.Points = SimplifyRoadPoints(Road.Points, RoadSimplifyTolerance);
	const double Half = GetRoadPaintHalfWidth(Road, Kit);
	const int32 LastSegment = Road.Points.Num() - 2;
	for (int32 Segment = 0; Segment <= LastSegment; ++Segment)
	{
		const FVector2D A = Road.Points[Segment];
		const FVector2D B = Road.Points[Segment + 1];
		const FVector2D Along = (B - A).GetSafeNormal();
		FSurvivalSovietGroundDecalShape Shape;
		Shape.Mode = ESurvivalSovietGroundDecalMode::Road;
		Shape.SortOrder = RoadSortOrder;
		Shape.EdgeWidth = Half * RoadEdgeShare;
		Shape.EdgeNoise = Half * RoadEdgeNoiseShare;
		// Inner bends: both segments run on past the bend point by a half width, so the outside of the bend is covered.
		double ReachBack = Segment > 0 ? Half : 0.0;
		double ReachOn = Segment < LastSegment ? Half : 0.0;
		FVector2D OnCentre;
		FVector2D Direction;
		const auto OtherHalf = [this, &Base, &Kit](const FVector2D& Point, const int32 Exclude, FVector2D& OutCentre, FVector2D& OutDirection)
		{
			const int32 Other = FindRoadUnder(Base, Point, Exclude, Kit, OutCentre, OutDirection);
			return Other == INDEX_NONE ? -1.0 : GetRoadPaintHalfWidth(Base.Plan.Roads[Other], Kit);
		};
		if (Segment == 0)
		{
			// A road end either runs into the street it meets or fades out into the sand.
			const double Other = OtherHalf(A, RoadIndex, OnCentre, Direction);
			Shape.bFadeStart = Other < 0.0;
			ReachBack = Other < 0.0 ? 0.0 : Other * RoadReachIntoJunction;
		}
		if (Segment == LastSegment)
		{
			const double Other = OtherHalf(B, RoadIndex, OnCentre, Direction);
			Shape.bFadeEnd = Other < 0.0;
			ReachOn = Other < 0.0 ? 0.0 : Other * RoadReachIntoJunction;
		}
		const FVector2D From = A - Along * ReachBack;
		const FVector2D To = B + Along * ReachOn;
		const double Length = FVector2D::Distance(From, To);
		Shape.Center = (From + To) * 0.5;
		Shape.YawRadians = FMath::Atan2(Along.Y, Along.X);
		Shape.HalfSize = FVector2D(Length * 0.5, Half);
		Shape.FadeLength = FMath::Min(RoadFadeLength, Length * RoadFadeShareOfSegment);
		// Pieces that each sit on their own ground; together they cover the shape and its edge noise along the road.
		const double Covered = Length + 2.0 * Shape.EdgeNoise;
		const int32 Pieces = FMath::Max(1, FMath::CeilToInt32(Covered / RoadPieceLength));
		const double PieceLength = Covered / Pieces;
		for (int32 Piece = 0; Piece < Pieces; ++Piece)
		{
			Shape.BoxCenter = From - Along * Shape.EdgeNoise + Along * (PieceLength * (Piece + 0.5));
			Shape.BoxHalfAlong = PieceLength * 0.5 + RoadPieceOverlap;
			SpawnGroundDecal(Base, Shape, &Surface);
		}
	}
}

void USurvivalSovietBaseComponent::SpawnJunctionsAndAprons(FSurvivalSovietBaseState& Base, const USurvivalSovietBaseSettings& Settings)
{
	using namespace SurvivalSovietBaseConstants;
	const FSurvivalSovietSiteKit& Kit = Settings.Kit;
	for (int32 RoadIndex = 0; RoadIndex < Base.Plan.Roads.Num(); ++RoadIndex)
	{
		const FSurvivalSovietPlannedRoad Road = Base.Plan.Roads[RoadIndex];
		if (Road.Points.Num() < 2)
		{
			continue;
		}
		for (const FVector2D& End : {Road.Points[0], Road.Points.Last()})
		{
			FVector2D OnCentre;
			FVector2D Direction;
			const int32 Other = FindRoadUnder(Base, End, RoadIndex, Kit, OnCentre, Direction);
			if (Other == INDEX_NONE)
			{
				continue;
			}
			// The wider street's surface fills the mouth, with rounded corners where the narrower one joins.
			const FSurvivalSovietPlannedRoad& Wider = GetRoadPaintHalfWidth(Base.Plan.Roads[Other], Kit) >= GetRoadPaintHalfWidth(Road, Kit)
				                                          ? Base.Plan.Roads[Other]
				                                          : Road;
			UMaterialInterface* Surface = GetRoadSurface(Wider.Kind, Kit);
			if (Surface == nullptr)
			{
				continue;
			}
			const double OwnHalf = GetRoadPaintHalfWidth(Road, Kit);
			const double OtherHalfWidth = GetRoadPaintHalfWidth(Base.Plan.Roads[Other], Kit);
			FSurvivalSovietGroundDecalShape Shape;
			Shape.Center = OnCentre;
			Shape.YawRadians = FMath::Atan2(Direction.Y, Direction.X);
			Shape.HalfSize = FVector2D(OwnHalf + JunctionFillet, OtherHalfWidth + JunctionFillet * 0.5);
			Shape.CornerRadius = JunctionFillet;
			Shape.EdgeWidth = OtherHalfWidth * RoadEdgeShare;
			Shape.EdgeNoise = OtherHalfWidth * RoadEdgeNoiseShare;
			Shape.SortOrder = JunctionSortOrder;
			Shape.VergeStrength = JunctionVergeStrength;
			SpawnGroundDecal(Base, Shape, Surface);
		}
	}
	UMaterialInterface* Apron = Kit.GateApronSurface.Get();
	for (const FSurvivalSovietPlannedGate& Gate : Base.Plan.Gates)
	{
		if (Apron == nullptr or Gate.Width <= 0.0)
		{
			continue;
		}
		// Hard standing through the gate, reaching a little further out than in, fading into the ground on every side.
		FSurvivalSovietGroundDecalShape Shape;
		Shape.Center = Gate.Center + Gate.Outward * (Gate.Width * ApronOutwardShift);
		Shape.YawRadians = FMath::Atan2(Gate.Outward.Y, Gate.Outward.X);
		Shape.HalfSize = FVector2D(Gate.Width * ApronAlongShare, Gate.Width * ApronAcrossShare + ApronAcrossMargin);
		Shape.CornerRadius = ApronCornerRadius;
		Shape.EdgeWidth = ApronEdgeWidth;
		Shape.EdgeNoise = ApronEdgeNoise;
		Shape.SortOrder = ApronSortOrder;
		Shape.VergeStrength = ApronVergeStrength;
		SpawnGroundDecal(Base, Shape, Apron);
	}
}

void USurvivalSovietBaseComponent::ApplyRoadToSpline(AActor& RoadActor, const FSurvivalSovietBaseState& Base,
                                                     const FSurvivalSovietPlannedRoad& Road) const
{
	USplineComponent* Spline = RoadActor.FindComponentByClass<USplineComponent>();
	if (not IsValid(Spline))
	{
		return;
	}
	TArray<FVector> WorldPoints;
	for (const FVector2D& Point : Road.Points)
	{
		WorldPoints.Add(ProjectToGround(Base, Point)
			+ FVector::UpVector * SurvivalSovietBaseConstants::RoadGroundOffset);
	}
	Spline->SetSplinePoints(WorldPoints, ESplineCoordinateSpace::World, false);
	for (int32 PointIndex = 0; PointIndex < WorldPoints.Num(); ++PointIndex)
	{
		Spline->SetSplinePointType(PointIndex, ESplinePointType::CurveClamped, false);
	}
	Spline->UpdateSpline();
	RebuildRoadTiles(RoadActor, *Spline, Base, Road);
}

void USurvivalSovietBaseComponent::RebuildRoadTiles(AActor& RoadActor, const USplineComponent& Spline,
                                                    const FSurvivalSovietBaseState& Base,
                                                    const FSurvivalSovietPlannedRoad& Road) const
{
	using namespace SurvivalSovietBaseConstants;
	// The construction script laid its tiles along the Blueprint's default spline when the actor spawned. They carry
	// the surface: mesh, material and how tile scale relates to width. New ones along the road replace them.
	TInlineComponentArray<USplineMeshComponent*> DefaultTiles(&RoadActor);
	if (DefaultTiles.IsEmpty())
	{
		return;
	}
	const USplineMeshComponent* Sample = DefaultTiles[0];
	TArray<UStaticMesh*> Meshes;
	for (const USplineMeshComponent* Tile : DefaultTiles)
	{
		Meshes.AddUnique(Tile->GetStaticMesh());
	}
	// The ribbon is one texture cut into Q0..Q3 along its length: out of order, the edges of two tiles do not meet.
	Meshes.Sort([](const UStaticMesh& Left, const UStaticMesh& Right)
	{
		return Left.GetName() < Right.GetName();
	});
	UMaterialInterface* Material = Sample->GetMaterial(0);
	const double RoadWidthAtDefaultScale = FMath::Max(1.0, RoadDefaultWidth);
	// A tile's length belongs to its width, or the edge pattern of a wide road is squashed into steps.
	const double SampleLength = FVector::Dist(Sample->GetStartPosition(), Sample->GetEndPosition()) * (Road.Width / RoadWidthAtDefaultScale);
	const FVector2D Scale = Sample->GetStartScale() * (Road.Width / RoadWidthAtDefaultScale);
	const ESplineMeshAxis::Type ForwardAxis = Sample->GetForwardAxis();
	const FVector2D SampleScale = Sample->GetStartScale();
	for (USplineMeshComponent* Tile : DefaultTiles)
	{
		Tile->DestroyComponent();
	}

	const double SplineLength = Spline.GetSplineLength();
	const int32 TileCount = FMath::Max(1, FMath::RoundToInt32(SplineLength / FMath::Max(1.0, SampleLength)));
	const double TileLength = SplineLength / TileCount;
	const FTransform ActorTransform = RoadActor.GetActorTransform();
	for (int32 TileIndex = 0; TileIndex < TileCount; ++TileIndex)
	{
		const double StartDistance = TileIndex * TileLength;
		const double EndDistance = StartDistance + TileLength;
		const FVector2D StartXY(Spline.GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::World));
		const FVector2D EndXY(Spline.GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::World));
		const FVector Start = ActorTransform.InverseTransformPosition(
			ProjectToGround(Base, StartXY) + FVector::UpVector * RoadGroundOffset);
		const FVector End = ActorTransform.InverseTransformPosition(
			ProjectToGround(Base, EndXY) + FVector::UpVector * RoadGroundOffset);
		USplineMeshComponent* Tile = NewObject<USplineMeshComponent>(&RoadActor);
		Tile->SetMobility(EComponentMobility::Movable);
		Tile->SetForwardAxis(ForwardAxis, false);
		Tile->SetStaticMesh(Meshes[TileIndex % Meshes.Num()]);
		Tile->SetMaterial(0, Material);
		Tile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tile->SetCastShadow(false);
		Tile->SetCanEverAffectNavigation(false);
		const FVector Tangent = End - Start;
		Tile->SetStartAndEnd(Start, Tangent, End, Tangent, false);
		Tile->SetStartScale(SampleScale.X > 0.0 ? Scale : SampleScale, false);
		Tile->SetEndScale(SampleScale.X > 0.0 ? Scale : SampleScale, false);
		Tile->AttachToComponent(RoadActor.GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Tile->RegisterComponent();
		Tile->UpdateMesh();
	}
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
		ActorClass,
		FTransform(Rotation, Location, FVector(Structure.StretchXY * Structure.UniformScale, Structure.UniformScale)),
		SpawnParams);
	if (not IsValid(Actor))
	{
		return nullptr;
	}
	Actor->Tags.Add(SurvivalSovietBaseConstants::SovietBaseActorTag);
	LimitPipeConnections(*Actor);
	if (not Structure.StretchXY.Equals(FVector2D(1.0, 1.0), KINDA_SMALL_NUMBER) and not Actor->IsA<ASurvivalSovietBasePad>())
	{
		EnforceSpawnScale(*Actor, FVector(Structure.StretchXY * Structure.UniformScale, Structure.UniformScale));
	}
	if (ASurvivalSovietBasePad* Pad = Cast<ASurvivalSovietBasePad>(Actor))
	{
		Pad->InitPad(Structure.StretchXY, GetPadMaterial(Structure.PadKind));
	}
	else if (ASurvivalSovietBaseLantern* Lantern = Cast<ASurvivalSovietBaseLantern>(Actor))
	{
		const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
		Lantern->InitLantern(Settings != nullptr ? Settings->Kit.LanternMesh.LoadSynchronous() : nullptr);
	}
	else
	{
		ReplaceVirtualTextureEarth(*Actor);
	}
	Base.GroundTraceParams.AddIgnoredActor(Actor);
	return Actor;
}

void USurvivalSovietBaseComponent::EnforceSpawnScale(AActor& Actor, const FVector& Scale) const
{
	// Some environment Blueprints (sandbags, precast fences) drop the spawn scale, so a stretched wall piece kept its
	// own length and left sand at the joints. Set it again once the actor stands, also on meshes with absolute scale.
	Actor.SetActorScale3D(Scale);
	TInlineComponentArray<USceneComponent*> Components(&Actor);
	for (USceneComponent* Component : Components)
	{
		if (Component != nullptr and Component->IsUsingAbsoluteScale())
		{
			Component->SetWorldScale3D(Component->GetComponentScale() * Scale);
		}
	}
}

UMaterialInterface* USurvivalSovietBaseComponent::GetPadMaterial(const ESurvivalSovietPadKind Kind) const
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr)
	{
		return nullptr;
	}
	switch (Kind)
	{
	case ESurvivalSovietPadKind::Concrete:
		return Settings->Kit.ConcretePad.LoadSynchronous();
	case ESurvivalSovietPadKind::PackedEarth:
		return Settings->Kit.PackedEarthPad.LoadSynchronous();
	case ESurvivalSovietPadKind::Gravel:
	case ESurvivalSovietPadKind::None:
	default:
		return Settings->Kit.GravelPad.LoadSynchronous();
	}
}

void USurvivalSovietBaseComponent::ReplaceVirtualTextureEarth(AActor& Actor) const
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or Settings->Kit.VirtualTextureEarth.IsNull())
	{
		return;
	}
	const FSoftObjectPath BrokenPath = Settings->Kit.VirtualTextureEarth.ToSoftObjectPath();
	TInlineComponentArray<UMeshComponent*> Meshes(&Actor);
	for (UMeshComponent* Mesh : Meshes)
	{
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
			if (Material != nullptr and FSoftObjectPath(Material) == BrokenPath)
			{
				Mesh->SetMaterial(Slot, Settings->Kit.EarthStandIn.LoadSynchronous());
			}
		}
	}
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
		const bool bIsBuilding = Structure.Role != ESurvivalSovietStructureRole::Fortification
			and Structure.Role != ESurvivalSovietStructureRole::Foundation;
		if (bIsBuilding)
		{
			ValidateEnemyOwnership(Actor);
		}
		const bool bTracksProduction = Base.Plan.ProductionCount > 0;
		const bool bCountsTowardsDestruction = bTracksProduction
			                                       ? Structure.Role == ESurvivalSovietStructureRole::Production
			                                       : bIsBuilding;
		if (bCountsTowardsDestruction or Structure.ThreatPoints > 0)
		{
			TrackStructureForDestruction(BaseArrayIndex, Actor, bCountsTowardsDestruction, Structure.ThreatPoints);
		}
	}

	OnPendingSpawnDone(BaseArrayIndex);
}

void USurvivalSovietBaseComponent::OnPendingSpawnDone(const int32 BaseArrayIndex)
{
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
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
	WakeBaseGraph(Base);
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

void USurvivalSovietBaseComponent::TrackStructureForDestruction(const int32 BaseArrayIndex, AActor* Actor,
                                                                const bool bCountsTowardsDestruction, const int32 ThreatPoints)
{
	const TObjectKey<AActor> StructureKey(Actor);
	if (M_TrackedStructureToBase.Contains(StructureKey))
	{
		return;
	}
	FTrackedStructure& Tracked = M_TrackedStructureToBase.Add(StructureKey);
	Tracked.BaseArrayIndex = BaseArrayIndex;
	Tracked.ThreatPoints = ThreatPoints;
	Tracked.bCountsTowardsDestruction = bCountsTowardsDestruction;
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.DefensePointsTotal += ThreatPoints;
	if (bCountsTowardsDestruction)
	{
		Base.TrackedStructuresAlive++;
		Base.TrackedStructuresTotal++;
	}

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
	FTrackedStructure Tracked;
	if (not M_TrackedStructureToBase.RemoveAndCopyValue(StructureKey, Tracked))
	{
		// Already counted (death followed by destroy).
		return;
	}
	if (not M_Bases.IsValidIndex(Tracked.BaseArrayIndex))
	{
		return;
	}
	FSurvivalSovietBaseState& Base = M_Bases[Tracked.BaseArrayIndex];
	Base.DefensePointsDestroyed += Tracked.ThreatPoints;
	if (Tracked.bCountsTowardsDestruction)
	{
		Base.TrackedStructuresAlive--;
	}
	if (Base.TrackedStructuresAlive <= 0 and Base.bStructuresSpawned and not Base.bDestroyed)
	{
		OnBaseDestroyed(Tracked.BaseArrayIndex);
	}
	PayEarnedRewards(Base);
}

void USurvivalSovietBaseComponent::OnBaseDestroyed(const int32 BaseArrayIndex)
{
	FSurvivalSovietBaseState& Base = M_Bases[BaseArrayIndex];
	Base.bDestroyed = true;

	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings != nullptr and Settings->Presets.IsValidIndex(Base.PresetIndex))
	{
		const FSurvivalSovietBasePreset& Preset = Settings->Presets[Base.PresetIndex];
		if (not Settings->BaseDestroyedNotification.IsEmpty())
		{
			RTSFunctionLibrary::DisplayNotification(
				FText::Format(Settings->BaseDestroyedNotification, Preset.DisplayName));
		}
	}
	OnSovietBaseDestroyed.Broadcast(Base.BaseIndex, Base.Center);
}

float USurvivalSovietBaseComponent::GetEarnedPayoutFraction(const FSurvivalSovietBaseState& Base,
                                                             const USurvivalSovietBaseSettings& Settings)
{
	const float ProductionShare = Base.bDestroyed ? Settings.ProductionPayoutShare : 0.f;
	if (Base.DefensePointsTotal <= 0)
	{
		return ProductionShare + (Base.bDestroyed ? 1.f - Settings.ProductionPayoutShare : 0.f);
	}
	const float BrokenFraction = static_cast<float>(Base.DefensePointsDestroyed) / Base.DefensePointsTotal;
	const float DefenseShare = 1.f - Settings.ProductionPayoutShare;
	return ProductionShare + DefenseShare * FMath::Min(1.f, BrokenFraction / Settings.MinDefenseBrokenFraction);
}

void USurvivalSovietBaseComponent::PayEarnedRewards(FSurvivalSovietBaseState& Base)
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or not Settings->Presets.IsValidIndex(Base.PresetIndex))
	{
		return;
	}
	const float EarnedFraction = GetEarnedPayoutFraction(Base, *Settings);
	const float NewFraction = EarnedFraction - Base.PayoutFractionPaid;
	if (NewFraction <= 0.f)
	{
		return;
	}
	UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(ResourceManager))
	{
		RTSFunctionLibrary::ReportError("Soviet base reward earned but the player resource manager is invalid.");
		return;
	}
	Base.PayoutFractionPaid = EarnedFraction;
	const float Payout = Base.Plan.ThreatPoints * Base.Scaling.RewardPerThreatPoint * NewFraction;
	for (const FSurvivalSovietBaseReward& Reward : Settings->Presets[Base.PresetIndex].DestroyRewards)
	{
		const int32 Amount = FMath::RoundToInt(Payout * Reward.Share);
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

		// Copy: spawn callbacks can run synchronously and touch M_Bases / the pending map.
		const TArray<FTrainingOption> Units = Planned.Units;
		const TArray<FVector2D> Positions = Planned.SpawnPositions;
		const TArray<double> Yaws = Planned.SpawnYaws;
		for (int32 UnitIndex = 0; UnitIndex < Units.Num(); ++UnitIndex)
		{
			const FVector2D Position2D = Positions.IsValidIndex(UnitIndex) ? Positions[UnitIndex] : FVector2D(Base.Center);
			const FVector Location = ProjectToGround(M_Bases[BaseArrayIndex], Position2D)
				+ FVector::UpVector * UnitSpawnZOffset;
			const double Yaw = Yaws.IsValidIndex(UnitIndex) ? Yaws[UnitIndex] : 0.0;
			const FRotator Rotation(0.f, FMath::RadiansToDegrees(Yaw), 0.f);

			const TSharedRef<bool> bResolved = MakeShared<bool>(false);
			auto OnSpawned = [WeakThis, bResolved](const FTrainingOption& Option, AActor* SpawnedActor, const int32 Id)
			{
				if (*bResolved)
				{
					return;
				}
				*bResolved = true;
				if (WeakThis.IsValid())
				{
					WeakThis->OnGarrisonUnitSpawned(Id, SpawnedActor, Option);
				}
			};
			const bool bRequested = AsyncSpawner->AsyncSpawnOptionAtLocation(
				Units[UnitIndex], Location, this, GroupId, OnSpawned, Rotation);
			if (not bRequested and not *bResolved)
			{
				*bResolved = true;
				OnGarrisonUnitSpawned(GroupId, nullptr, Units[UnitIndex]);
			}
		}
	}
}

void USurvivalSovietBaseComponent::OnGarrisonUnitSpawned(const int32 GroupId, AActor* SpawnedActor,
                                                         const FTrainingOption& Option)
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
#if RTS_WITH_MAP_TESTS
		FSurvivalSovietSpawnedUnitRecord& Record = Base.SpawnedUnitRecords.AddDefaulted_GetRef();
		Record.Option = Option;
		Record.Kind = Base.Plan.Garrison.IsValidIndex(Pending->PlannedGroupIndex)
			              ? Base.Plan.Garrison[Pending->PlannedGroupIndex].Kind
			              : ESurvivalSovietStepKind::HoldGarrison;
		Record.ClassName = SpawnedActor->GetClass()->GetName();
#endif
	}
	else
	{
		RTSFunctionLibrary::ReportError("A Soviet base garrison unit failed to spawn (base "
			+ FString::FromInt(Base.BaseIndex) + ", option type " + FString::FromInt(static_cast<int32>(Option.UnitType))
			+ " subtype " + FString::FromInt(Option.SubtypeValue) + ").");
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

	switch (Planned.Kind)
	{
	case ESurvivalSovietStepKind::PatrolGarrison:
		StartGarrisonPatrol(Base, Planned, Group.SpawnedUnits);
		break;
	case ESurvivalSovietStepKind::Reserve:
		RegisterGarrisonAsStrategicReserve(Group.SpawnedUnits);
		break;
	default:
		// Spawned at their post; units auto-engage what enters their range.
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
		// Vehicles hold the post they spawned on: a patrolling tank ploughs tyre loops over the whole base.
		ASquadController* Squad = Cast<ASquadController>(Unit);
		if (Squad == nullptr)
		{
			continue;
		}
		Squads.Add(Squad);
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
		PatrolIterationsAtPoint,
		PatrolGuardSeconds,
		PatrolGuardRadius,
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
	const int32 ExpectedBases = FMath::Max(1, GetUnpickedStartLocations(M_Director->GetChosenStartLocation()).Num());
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

// ---------------------------------------------------------------------------
// Preview (test builds only)
// ---------------------------------------------------------------------------

#if RTS_WITH_MAP_TESTS
void USurvivalSovietBaseComponent::GenerateBaseForPreview(
	ASurvivalStartLocation* StartLocation,
	const int32 BaseIndex,
	const FSurvivalSovietBasePreviewRequest& Request)
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or not IsValid(StartLocation))
	{
		return;
	}
	const int32 PresetIndex = Settings->Presets.IndexOfByPredicate(
		[&Request](const FSurvivalSovietBasePreset& Preset) { return Preset.PresetName == Request.PresetName; });
	if (PresetIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("Soviet base preview: unknown preset %s"), *Request.PresetName.ToString());
		return;
	}

	FSurvivalSovietBaseState& Base = M_Bases.AddDefaulted_GetRef();
	const int32 BaseArrayIndex = M_Bases.Num() - 1;
	Base.BaseIndex = BaseIndex;
	Base.StartLocation = StartLocation;
	Base.Center = StartLocation->GetDeployCenter();
	Base.Scaling = Settings->GetScalingForDifficulty(Request.Difficulty);
	Base.Scaling.Difficulty = Request.Difficulty;
	Base.Seed = Request.Seed;
	ApplyDistanceRank(Base, Request.DistanceRank, Request.BaseCount, *Settings);
	Base.Strength = Request.Strength;
	Base.PresetIndex = PresetIndex;
	Base.ThreatOverride = Request.ThreatLocation;
	Base.GroundTraceParams = FCollisionQueryParams(TEXT("SurvivalSovietBaseGround"), false);
	BeginBaseGeneration(BaseArrayIndex, *Settings);
}

void USurvivalSovietBaseComponent::ClearBasesForPreview()
{
	M_TrackedStructureToBase.Reset();
	// Squad members are separate actors that outlive a destroyed squad controller and report it every tick.
	for (TActorIterator<ASquadUnit> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	for (FSurvivalSovietBaseState& Base : M_Bases)
	{
		Base.bDestroyed = true;
		for (const TWeakObjectPtr<AActor>& Structure : Base.SpawnedStructures)
		{
			if (Structure.IsValid())
			{
				Structure->Destroy();
			}
		}
		for (const TWeakObjectPtr<AActor>& Unit : Base.GarrisonUnits)
		{
			if (Unit.IsValid())
			{
				Unit->Destroy();
			}
		}
		for (const TWeakObjectPtr<AActor>& Road : Base.SpawnedRoads)
		{
			if (Road.IsValid())
			{
				Road->Destroy();
			}
		}
		if (Base.Volume.IsValid())
		{
			Base.Volume->Destroy();
		}
	}
	M_Bases.Reset();
	M_SpawnQueue.Reset();
	M_SpawnQueueHead = 0;
	M_BasesInPreparation.Reset();
}

namespace
{
	constexpr double GroundGapTolerance = 15.0;

	FString SovietPreview_UnitName(const FTrainingOption& Option)
	{
		return Option.UnitType == EAllUnitType::UNType_Squad
			       ? UEnum::GetValueAsString(static_cast<ESquadSubtype>(Option.SubtypeValue))
			       : UEnum::GetValueAsString(static_cast<ETankSubtype>(Option.SubtypeValue));
	}

	FString SovietPreview_JoinStrings(const TArray<FString>& Strings)
	{
		TArray<FString> Parts;
		for (const FString& Text : Strings)
		{
			Parts.Add(TEXT("\"") + Text + TEXT("\""));
		}
		return FString::Join(Parts, TEXT(","));
	}

	FString SovietPreview_JoinCounts(const TMap<FString, int32>& Counts)
	{
		TArray<FString> Parts;
		for (const TPair<FString, int32>& Count : Counts)
		{
			Parts.Add(FString::Printf(TEXT("\"%s\":%d"), *Count.Key, Count.Value));
		}
		Parts.Sort();
		return TEXT("{") + FString::Join(Parts, TEXT(",")) + TEXT("}");
	}

	/** @brief Where an actor's visible meshes reach, in a wall frame: along the walk, across (outward) and up. */
	struct FSovietVisualExtent
	{
		bool bFound = false;
		double AlongMin = TNumericLimits<double>::Max();
		double AlongMax = -TNumericLimits<double>::Max();
		double OutMin = TNumericLimits<double>::Max();
		double OutMax = -TNumericLimits<double>::Max();
		double ZMin = TNumericLimits<double>::Max();
		double ZMax = -TNumericLimits<double>::Max();

		void Add(const FVector& World, const FVector2D& Origin, const FVector2D& Walk, const FVector2D& Out)
		{
			const FVector2D Offset = FVector2D(World) - Origin;
			const double Along = FVector2D::DotProduct(Offset, Walk);
			const double Across = FVector2D::DotProduct(Offset, Out);
			AlongMin = FMath::Min(AlongMin, Along);
			AlongMax = FMath::Max(AlongMax, Along);
			OutMin = FMath::Min(OutMin, Across);
			OutMax = FMath::Max(OutMax, Across);
			ZMin = FMath::Min(ZMin, World.Z);
			ZMax = FMath::Max(ZMax, World.Z);
			bFound = true;
		}
	};

	/**
	 * @brief Measures the visible static meshes of a spawned wall piece from their mesh bounds and component transforms, so
	 * gaps, overlaps and faces that stand proud are numbers in the manifest, not only pixels.
	 */
	FSovietVisualExtent SovietPreview_MeasureVisual(const AActor& Actor, const FVector2D& Origin, const FVector2D& Walk,
	                                                const FVector2D& Out)
	{
		FSovietVisualExtent Extent;
		TInlineComponentArray<UStaticMeshComponent*> Meshes(&Actor);
		for (const UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh == nullptr or not Mesh->IsVisible() or Mesh->bHiddenInGame or Mesh->GetStaticMesh() == nullptr)
			{
				continue;
			}
			const FBox Local = Mesh->GetStaticMesh()->GetBoundingBox();
			const FTransform& Transform = Mesh->GetComponentTransform();
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector Point((Corner & 1) ? Local.Max.X : Local.Min.X, (Corner & 2) ? Local.Max.Y : Local.Min.Y,
				                    (Corner & 4) ? Local.Max.Z : Local.Min.Z);
				Extent.Add(Transform.TransformPosition(Point), Origin, Walk, Out);
			}
		}
		return Extent;
	}

	const TCHAR* SovietPreview_PieceKindName(const ESurvivalSovietWallPieceKind Kind)
	{
		switch (Kind)
		{
		case ESurvivalSovietWallPieceKind::End:
			return TEXT("end");
		case ESurvivalSovietWallPieceKind::Gate:
			return TEXT("gate");
		case ESurvivalSovietWallPieceKind::Corner:
			return TEXT("corner");
		case ESurvivalSovietWallPieceKind::Straight:
		default:
			return TEXT("straight");
		}
	}

	const AActor* SovietPreview_FindActorAt(const TArray<TWeakObjectPtr<AActor>>& Actors, const FVector2D& Pivot, const FString& ClassName)
	{
		constexpr double PivotTolerance = 10.0;
		for (const TWeakObjectPtr<AActor>& Actor : Actors)
		{
			if (Actor.IsValid() and Actor->GetClass()->GetName() == ClassName
				and FVector2D::Distance(FVector2D(Actor->GetActorLocation()), Pivot) < PivotTolerance)
			{
				return Actor.Get();
			}
		}
		return nullptr;
	}

	FString SovietPreview_PieceJson(const FSurvivalSovietPlannedWallPiece& Piece, const FVector& BaseCenter,
	                                const TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		const FVector2D Walk = (Piece.JointEnd - Piece.JointStart).GetSafeNormal();
		FVector2D Out(-Walk.Y, Walk.X);
		const FVector2D Middle = (Piece.JointStart + Piece.JointEnd) * 0.5;
		Out = FVector2D::DotProduct(Out, Middle - FVector2D(BaseCenter)) < 0.0 ? -Out : Out;
		const AActor* Actor = SovietPreview_FindActorAt(Actors, Piece.Pivot, Piece.ClassName);
		FString Visual = TEXT("null");
		if (Actor != nullptr)
		{
			const FSovietVisualExtent Extent = SovietPreview_MeasureVisual(*Actor, Piece.JointStart, Walk, Out);
			Visual = Extent.bFound
				         ? FString::Printf(TEXT("[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f]"), Extent.AlongMin, Extent.AlongMax, Extent.OutMin,
				                           Extent.OutMax, Extent.ZMin - BaseCenter.Z, Extent.ZMax - BaseCenter.Z)
				         : Visual;
		}
		return FString::Printf(
			TEXT("{\"class\":\"%s\",\"kind\":\"%s\",\"span\":%d,\"center\":[%.1f,%.1f],\"yaw\":%.2f,\"nominal\":%.1f,\"length\":%.1f,")
			TEXT("\"stretch\":%.3f,\"flag\":%s,\"a\":[%.1f,%.1f],\"b\":[%.1f,%.1f],\"vis\":%s}"),
			*Piece.ClassName, SovietPreview_PieceKindName(Piece.Kind), Piece.Span, Middle.X, Middle.Y,
			FMath::RadiansToDegrees(Piece.YawRadians), Piece.NominalLength, FVector2D::Distance(Piece.JointStart, Piece.JointEnd),
			Piece.Stretch, Piece.bFlag ? TEXT("true") : TEXT("false"), Piece.JointStart.X, Piece.JointStart.Y, Piece.JointEnd.X,
			Piece.JointEnd.Y, *Visual);
	}

	FString SovietPreview_PieceListJson(const FSurvivalSovietBaseState& Base)
	{
		TArray<FString> Parts;
		for (const FSurvivalSovietPlannedWallPiece& Piece : Base.Plan.WallPieces)
		{
			Parts.Add(SovietPreview_PieceJson(Piece, Base.Center, Base.SpawnedStructures));
		}
		return FString::Join(Parts, TEXT(","));
	}

	FString SovietPreview_RoadsJson(const TArray<FSurvivalSovietPlannedRoad>& Roads, const FSurvivalSovietSiteKit* Kit)
	{
		TArray<FString> Parts;
		for (const FSurvivalSovietPlannedRoad& Road : Roads)
		{
			TArray<FString> Points;
			for (const FVector2D& Point : Road.Points)
			{
				Points.Add(FString::Printf(TEXT("[%.0f,%.0f]"), Point.X, Point.Y));
			}
			const TCHAR* Kind = Road.Kind == ESurvivalSovietRoadKind::Main
				                    ? TEXT("Main")
				                    : Road.Kind == ESurvivalSovietRoadKind::Strip
				                    ? TEXT("Strip")
				                    : Road.Kind == ESurvivalSovietRoadKind::Track ? TEXT("Track") : TEXT("Lane");
			const double Painted = Kit != nullptr ? 2.0 * USurvivalSovietBaseComponent::GetRoadPaintHalfWidth(Road, *Kit) : Road.Width;
			Parts.Add(FString::Printf(TEXT("{\"kind\":\"%s\",\"width\":%.0f,\"painted_width\":%.0f,\"decal\":\"MI_SovietRoad_%s\",\"points\":[%s]}"),
			                          Kind, Road.Width, Painted, Road.Kind == ESurvivalSovietRoadKind::Strip ? TEXT("Main") : Kind,
			                          *FString::Join(Points, TEXT(","))));
		}
		return FString::Join(Parts, TEXT(","));
	}

	/**
	 * @brief How far a gate's middle lies off the centre line of the nearest street, and how square that street crosses it.
	 * @return Distance in cm; OutAlignment is |cos| between the street and the gate's outward direction (1 = square).
	 */
	double SovietPreview_GateRoadOffset(const FSurvivalSovietPlannedGate& Gate, const TArray<FSurvivalSovietPlannedRoad>& Roads,
	                                    double& OutAlignment)
	{
		double Best = TNumericLimits<double>::Max();
		OutAlignment = 0.0;
		for (const FSurvivalSovietPlannedRoad& Road : Roads)
		{
			for (int32 Index = 0; Index + 1 < Road.Points.Num(); ++Index)
			{
				const FVector2D& A = Road.Points[Index];
				const FVector2D& B = Road.Points[Index + 1];
				const FVector2D Along = (B - A).GetSafeNormal();
				const double Reach = FMath::Clamp(FVector2D::DotProduct(Gate.Center - A, Along), 0.0, FVector2D::Distance(A, B));
				const double Distance = FVector2D::Distance(Gate.Center, A + Along * Reach);
				if (Distance < Best)
				{
					Best = Distance;
					OutAlignment = FMath::Abs(FVector2D::DotProduct(Along, Gate.Outward));
				}
			}
		}
		return Best;
	}

	FString SovietPreview_OpenSlotsJson(const FSurvivalSovietBasePlan& Plan)
	{
		TArray<FString> Parts;
		for (const FSurvivalSovietPlannedGate& Slot : Plan.OpenWallSlots)
		{
			Parts.Add(FString::Printf(TEXT("{\"center\":[%.0f,%.0f],\"outward\":[%.3f,%.3f],\"length\":%.0f}"), Slot.Center.X, Slot.Center.Y,
			                          Slot.Outward.X, Slot.Outward.Y, Slot.Width));
		}
		return FString::Join(Parts, TEXT(","));
	}

	FString SovietPreview_GatesJson(const FSurvivalSovietBasePlan& Plan)
	{
		TArray<FString> Parts;
		for (const FSurvivalSovietPlannedGate& Gate : Plan.Gates)
		{
			double Alignment = 0.0;
			const double Offset = SovietPreview_GateRoadOffset(Gate, Plan.Roads, Alignment);
			Parts.Add(FString::Printf(TEXT("{\"center\":[%.1f,%.1f],\"outward\":[%.3f,%.3f],\"width\":%.0f,\"road_offset\":%.1f,\"road_alignment\":%.3f}"),
			                          Gate.Center.X, Gate.Center.Y, Gate.Outward.X, Gate.Outward.Y, Gate.Width, Offset, Alignment));
		}
		return FString::Join(Parts, TEXT(","));
	}
}

const FFindEnemyBaseClusters* USurvivalSovietBaseComponent::GetClusterRequestForPreview() const
{
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	UEnemyStrategicAIComponent* StrategicAI = IsValid(EnemyController) ? EnemyController->GetEnemyStrategicAIComponent() : nullptr;
	return IsValid(StrategicAI) ? &StrategicAI->FindEnemyBase_TimerRequest : nullptr;
}

TArray<FAsyncDetailedUnitState> USurvivalSovietBaseComponent::BuildBuildingSnapshotForPreview() const
{
	TArray<FAsyncDetailedUnitState> Snapshot;
	for (const FSurvivalSovietBaseState& Base : M_Bases)
	{
		for (const TWeakObjectPtr<AActor>& Structure : Base.SpawnedStructures)
		{
			const URTSComponent* RTSComponent = Structure.IsValid() ? Structure->FindComponentByClass<URTSComponent>() : nullptr;
			const ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(Structure.Get());
			if (not IsValid(RTSComponent) or not IsValid(Expansion))
			{
				continue;
			}
			FAsyncDetailedUnitState& State = Snapshot.AddDefaulted_GetRef();
			State.OwningPlayer = RTSComponent->GetOwningPlayer();
			State.UnitType = EAllUnitType::UNType_BuildingExpansion;
			State.UnitSubtypeRaw = static_cast<uint8>(Expansion->GetBuildingExpansionType());
			State.UnitLocation = Structure->GetActorLocation();
			State.UnitActorPtr = Structure;
		}
	}
	return Snapshot;
}

FVector USurvivalSovietBaseComponent::GetNearestStandingStructureForPreview(const FSurvivalSovietBaseState& Base,
                                                                            const FVector& From) const
{
	FVector Nearest = Base.Center;
	double NearestDistance = TNumericLimits<double>::Max();
	bool bAnyDefence = false;
	for (const TPair<TObjectKey<AActor>, FTrackedStructure>& Tracked : M_TrackedStructureToBase)
	{
		const AActor* Actor = Tracked.Key.ResolveObjectPtr();
		if (Actor == nullptr or &M_Bases[Tracked.Value.BaseArrayIndex] != &Base)
		{
			continue;
		}
		const bool bDefence = Tracked.Value.ThreatPoints > 0;
		if (bAnyDefence and not bDefence)
		{
			continue;
		}
		const double Distance = FVector::Dist2D(Actor->GetActorLocation(), From);
		if (Distance < NearestDistance or (bDefence and not bAnyDefence))
		{
			NearestDistance = Distance;
			Nearest = Actor->GetActorLocation();
			bAnyDefence = bDefence;
		}
	}
	return Nearest;
}

int32 USurvivalSovietBaseComponent::CountLiveGarrisonForPreview(const FSurvivalSovietBaseState& Base) const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<AActor>& Unit : Base.GarrisonUnits)
	{
		Alive += Unit.IsValid() ? 1 : 0;
	}
	return Alive;
}

FString USurvivalSovietBaseComponent::GetTuningSourceForPreview() const
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr)
	{
		return TEXT("none");
	}
	return Settings->GetIsUsingCodeDefaults() ? TEXT("code defaults") : TEXT("asset");
}

FString USurvivalSovietBaseComponent::BuildSpawnManifestJson(
	const FSurvivalSovietBaseState& Base,
	const FString& Label) const
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	const FSurvivalSovietBasePreset* Preset = Settings != nullptr and Settings->Presets.IsValidIndex(Base.PresetIndex)
		                                          ? &Settings->Presets[Base.PresetIndex]
		                                          : nullptr;
	// Planned, by class and role.
	TMap<FString, int32> Planned;
	TMap<FString, FString> RoleByClass;
	for (const FSurvivalSovietPlannedStructure& Structure : Base.Plan.Structures)
	{
		const FString ClassName = GetNameSafe(M_ResolvedClasses.IsValidIndex(Structure.ClassIndex)
			                                      ? M_ResolvedClasses[Structure.ClassIndex].Get()
			                                      : nullptr);
		Planned.FindOrAdd(ClassName)++;
		RoleByClass.Add(ClassName, UEnum::GetValueAsString(Structure.Role));
	}
	// Spawned: what stands in the world right now, read from the actors.
	TMap<FString, int32> Spawned;
	FBox2D LocalBounds(ForceInit);
	double MaxGroundGap = 0.0;
	FString MaxGapClass;
	int32 ActorsAboveGround = 0;
	const FVector2D Forward(FMath::Cos(Base.Plan.AxisYawRadians), FMath::Sin(Base.Plan.AxisYawRadians));
	for (const TWeakObjectPtr<AActor>& Structure : Base.SpawnedStructures)
	{
		if (not Structure.IsValid())
		{
			continue;
		}
		Spawned.FindOrAdd(Structure->GetClass()->GetName())++;
		// Height over the ground under the actor, read from the world: proof that the base stands on this terrain.
		const double Gap = Structure->GetActorLocation().Z - ProjectToGround(Base, FVector2D(Structure->GetActorLocation())).Z;
		if (FMath::Abs(Gap) > MaxGroundGap)
		{
			MaxGroundGap = FMath::Abs(Gap);
			MaxGapClass = Structure->GetClass()->GetName();
		}
		ActorsAboveGround += FMath::Abs(Gap) <= GroundGapTolerance ? 1 : 0;
		const FVector2D Offset = FVector2D(Structure->GetActorLocation() - Base.Center);
		LocalBounds += FVector2D(FVector2D::DotProduct(Offset, Forward), FVector2D(-Forward.Y, Forward.X) | Offset);
	}
	// Every bunker and the line it must stand beyond, for the independent check of the matrix.
	FString Bunkers;
	for (const FSurvivalSovietPlannedStructure& Structure : Base.Plan.Structures)
	{
		if (Structure.Role == ESurvivalSovietStructureRole::PerimeterBunker)
		{
			Bunkers += FString::Printf(TEXT("%s[%.0f,%.0f]"), Bunkers.IsEmpty() ? TEXT("") : TEXT(","), Structure.Position.X, Structure.Position.Y);
		}
	}
	FString Perimeter;
	for (const FVector2D& Point : Base.Plan.PerimeterLine)
	{
		Perimeter += FString::Printf(TEXT("%s[%.0f,%.0f]"), Perimeter.IsEmpty() ? TEXT("") : TEXT(","), Point.X, Point.Y);
	}
	const FSurvivalSovietWallReport& Wall = Base.Plan.Wall;
	const FString Rules = FString::Printf(
		TEXT("\"wall\":{\"family\":\"%s\",\"pieces\":%d,\"flags\":%d,\"gates\":%d,\"corners\":%d,\"blocked_slots\":%d,\"max_stretch\":%.3f,\"piece_list\":[%s]},")
		TEXT("\"buildings\":%d,\"buildings_on_pad\":%d,\"perimeter\":[%s],\"bunkers\":[%s],\"rule_violations\":[%s],")
		TEXT("\"gates\":[%s],\"roads\":[%s],\"pads\":[%s],\"yards\":[%s],\"lanterns\":[%s],\"ground_gaps\":%s,\"open_wall_slots\":[%s],"),
		*UEnum::GetValueAsString(Wall.Family), Wall.Pieces, Wall.FlagPieces, Wall.Gates, Wall.Corners, Wall.BlockedSlots, Wall.MaxStretch,
		*SovietPreview_PieceListJson(Base), Base.Plan.BuildingsTotal, Base.Plan.BuildingsOnPad, *Perimeter, *Bunkers, *SovietPreview_JoinStrings(Base.Plan.RuleViolations),
		*SovietPreview_GatesJson(Base.Plan), *SovietPreview_RoadsJson(Base.Plan.Roads, Settings != nullptr ? &Settings->Kit : nullptr),
		*BuildPadsJson(Base), *BuildYardsJson(Base), *BuildLanternsJson(Base), *BuildGroundGapsJson(Base), *SovietPreview_OpenSlotsJson(Base.Plan));
	FString Structures;
	FString Missing;
	for (const TPair<FString, int32>& Plan : Planned)
	{
		const int32 Real = Spawned.FindRef(Plan.Key);
		Structures += FString::Printf(TEXT("%s{\"class\":\"%s\",\"role\":\"%s\",\"planned\":%d,\"spawned\":%d}"),
		                              Structures.IsEmpty() ? TEXT("") : TEXT(","), *Plan.Key, *RoleByClass[Plan.Key],
		                              Plan.Value, Real);
		if (Real < Plan.Value)
		{
			Missing += FString::Printf(TEXT("%s\"%s x%d\""), Missing.IsEmpty() ? TEXT("") : TEXT(","), *Plan.Key,
			                           Plan.Value - Real);
		}
	}
	FString Units;
	for (const FSurvivalSovietSpawnedUnitRecord& Record : Base.SpawnedUnitRecords)
	{
		Units += FString::Printf(TEXT("%s{\"type\":\"%s\",\"class\":\"%s\",\"kind\":\"%s\",\"points\":%d}"),
		                         Units.IsEmpty() ? TEXT("") : TEXT(","), *SovietPreview_UnitName(Record.Option),
		                         *Record.ClassName, *UEnum::GetValueAsString(Record.Kind),
		                         FSurvivalSovietBaseLayout::GetUnitThreatPoints(Record.Option));
	}
	int32 PlannedUnits = 0;
	for (const FSurvivalSovietPlannedGarrison& Group : Base.Plan.Garrison)
	{
		PlannedUnits += Group.Units.Num();
	}
	return FString::Printf(
		TEXT("{\"label\":\"%s\",\"preset\":\"%s\",\"archetype\":\"%s\",\"difficulty\":\"%s\",\"strength\":%d,\"seed\":%d,")
		TEXT("\"tuning_source\":\"%s\",\"rank\":%d,\"budget\":%d,\"unspent\":%d,\"spots_used\":%d,\"spots_available\":%d,\"points_off_main_line\":%d,\"planned_threat\":%d,\"planned_reserve\":%d,\"production\":%d,")
		TEXT("\"core_chain\":%.0f,\"road_count\":%d,\"footprint\":[%.0f,%.0f,%.0f,%.0f],\"planned_units\":%d,\"spawned_units\":%d,")
		TEXT("\"start_location\":\"%s\",\"center_z\":%.1f,\"on_ground\":%d,\"max_ground_gap\":%.1f,\"max_gap_class\":\"%s\",")
		TEXT("%s\"dropped_slots\":[%s],\"structures\":[%s],\"units\":[%s],\"missing\":[%s]}"),
		*Label, Preset != nullptr ? *Preset->PresetName.ToString() : TEXT("?"),
		Preset != nullptr ? *UEnum::GetValueAsString(Preset->Archetype) : TEXT("?"),
		*UEnum::GetValueAsString(Base.Scaling.Difficulty), Base.Strength, Base.Seed, *GetTuningSourceForPreview(),
		Base.DistanceRank, Base.Plan.ThreatBudget, Base.Plan.UnspentPoints, Base.Plan.EmplacementSpotsUsed,
		Base.Plan.EmplacementSpotsAvailable, Base.Plan.PointsOutsideMainLine, Base.Plan.ThreatPoints, Base.Plan.ReservePoints,
		Base.Plan.ProductionCount, Base.Plan.RequiredCoreClusterDistance, Base.Plan.Roads.Num(), LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Max.X,
		LocalBounds.Max.Y, PlannedUnits, Base.SpawnedUnitRecords.Num(), *GetNameSafe(Base.StartLocation.Get()), Base.Center.Z,
		ActorsAboveGround, MaxGroundGap, *MaxGapClass, *Rules, *SovietPreview_JoinStrings(Base.Plan.DroppedSlots), *Structures, *Units, *Missing);
}

void USurvivalSovietBaseComponent::LimitPipeConnections(AActor& Actor)
{
	// Fuel tanks lay pipes to any socket within 50 m; in a base that arcs a pipe over yard fences. The spline manager
	// connects on the next tick, so shortening the range right after spawning keeps only pipes between neighbouring tanks.
	TInlineComponentArray<UProceduralDestructibleSplineSpawner*> Spawners(&Actor);
	for (UProceduralDestructibleSplineSpawner* Spawner : Spawners)
	{
		if (IsValid(Spawner))
		{
			Spawner->Settings.Range = FMath::Min(Spawner->Settings.Range, SurvivalSovietBaseConstants::MaxBasePipeRange);
		}
	}
}

FString USurvivalSovietBaseComponent::GetClassNameForPreview(const int32 ClassIndex) const
{
	return GetNameSafe(M_ResolvedClasses.IsValidIndex(ClassIndex) ? M_ResolvedClasses[ClassIndex].Get() : nullptr);
}

FString USurvivalSovietBaseComponent::BuildYardsJson(const FSurvivalSovietBaseState& Base) const
{
	const auto CountsJson = [this](const TMap<FString, int32>& Counts)
	{
		TArray<FString> Parts;
		for (const TPair<FString, int32>& Count : Counts)
		{
			Parts.Add(FString::Printf(TEXT("\"%s\":%d"), *GetClassNameForPreview(FCString::Atoi(*Count.Key)), Count.Value));
		}
		return TEXT("{") + FString::Join(Parts, TEXT(",")) + TEXT("}");
	};
	const auto NamedCountsJson = [](const TMap<FString, int32>& Counts)
	{
		TArray<FString> Parts;
		for (const TPair<FString, int32>& Count : Counts)
		{
			Parts.Add(FString::Printf(TEXT("\"%s\":%d"), *Count.Key, Count.Value));
		}
		return TEXT("{") + FString::Join(Parts, TEXT(",")) + TEXT("}");
	};
	TArray<FString> Parts;
	for (const FSurvivalSovietPlannedYard& Yard : Base.Plan.Yards)
	{
		TArray<FString> Items;
		for (const FVector2D& Position : Yard.ItemPositions)
		{
			Items.Add(FString::Printf(TEXT("[%.0f,%.0f]"), Position.X, Position.Y));
		}
		TArray<FString> GapPositions;
		for (const FVector2D& Position : Yard.FenceGapPositions)
		{
			GapPositions.Add(FString::Printf(TEXT("[%.0f,%.0f]"), Position.X, Position.Y));
		}
		Parts.Add(FString::Printf(
			TEXT("{\"kind\":\"%s\",\"center\":[%.0f,%.0f],\"half_extents\":[%.0f,%.0f],\"yaw\":%.2f,\"items\":%s,\"item_positions\":[%s],")
			TEXT("\"rows\":%d,\"aisle\":%.0f,\"stack_area\":%.0f,\"fence_class\":\"%s\",\"fence_pieces\":%d,\"fence_gaps\":%d,")
			TEXT("\"entrance\":[%.0f,%.0f],\"nets\":%s,\"net_area\":%.0f,\"stores\":[%s],\"buildings_inside\":[%s],")
			TEXT("\"fence_gap_causes\":%s,\"fence_gap_positions\":[%s],\"fence_tighten_steps\":%d,\"stack_list\":\"%s\",\"stack_types\":%d,\"stack_estimate\":%d}"),
			Yard.Kind == ESurvivalSovietYardKind::Ammo ? TEXT("ammo") : TEXT("fuel"), Yard.Center.X, Yard.Center.Y, Yard.HalfExtents.X,
			Yard.HalfExtents.Y, FMath::RadiansToDegrees(Yard.YawRadians), *CountsJson(Yard.ItemCounts), *FString::Join(Items, TEXT(",")),
			Yard.Rows, Yard.AisleWidth, Yard.StackArea, *Yard.FenceClass, Yard.FencePieces, Yard.FenceGaps, Yard.Entrance.X, Yard.Entrance.Y,
			*CountsJson(Yard.NetCounts), Yard.NetArea, *SovietPreview_JoinStrings(Yard.StoreClasses), *JoinClassNames(Yard.BuildingsInside),
			*NamedCountsJson(Yard.FenceGapCauses), *FString::Join(GapPositions, TEXT(",")), Yard.FenceTightenSteps, *Yard.StackList, Yard.StackTypes, Yard.StackEstimate));
	}
	return FString::Join(Parts, TEXT(","));
}

FString USurvivalSovietBaseComponent::JoinClassNames(const TArray<int32>& ClassIndices) const
{
	TArray<FString> Parts;
	for (const int32 ClassIndex : ClassIndices)
	{
		Parts.Add(TEXT("\"") + GetClassNameForPreview(ClassIndex) + TEXT("\""));
	}
	return FString::Join(Parts, TEXT(","));
}

FString USurvivalSovietBaseComponent::BuildGroundGapsJson(const FSurvivalSovietBaseState& Base) const
{
	// Lowest visible mesh point against the ground under the actor, per family, so a floating wall cannot hide behind a tent.
	// Read from the meshes, not the actor bounds: many Blueprints carry a root billboard or a pivot off the ground.
	TSet<FString> WallClasses;
	for (const FSurvivalSovietPlannedWallPiece& Piece : Base.Plan.WallPieces)
	{
		WallClasses.Add(Piece.ClassName);
	}
	TMap<FString, TPair<double, FString>> Worst;
	TMap<FString, TPair<double, FString>> Sunk;
	for (const FSurvivalSovietPlannedStructure& Structure : Base.Plan.Structures)
	{
		if (Structure.Role == ESurvivalSovietStructureRole::Foundation)
		{
			continue;
		}
		const FString ClassName = GetClassNameForPreview(Structure.ClassIndex);
		const AActor* Actor = SovietPreview_FindActorAt(Base.SpawnedStructures, Structure.Position, ClassName);
		if (Actor == nullptr)
		{
			continue;
		}
		const FSovietVisualExtent Extent = SovietPreview_MeasureVisual(*Actor, Structure.Position, FVector2D(1.0, 0.0), FVector2D(0.0, 1.0));
		if (not Extent.bFound)
		{
			continue;
		}
		const double Gap = Extent.ZMin - ProjectToGround(Base, Structure.Position).Z;
		const TCHAR* Family = WallClasses.Contains(ClassName)
			                      ? TEXT("wall")
			                      : Structure.Role == ESurvivalSovietStructureRole::Production or Structure.Role == ESurvivalSovietStructureRole::Support
			                      ? TEXT("building")
			                      : Structure.Role == ESurvivalSovietStructureRole::GunPit or Structure.Role == ESurvivalSovietStructureRole::PerimeterBunker
			                      ? TEXT("defence")
			                      : TEXT("prop");
		// Floating (mesh bottom above the ground) is the fault; a mesh reaching below ground is a sunk foundation, reported apart.
		TPair<double, FString>& Float = Worst.FindOrAdd(Family, TPair<double, FString>(0.0, FString()));
		TPair<double, FString>& Sink = Sunk.FindOrAdd(Family, TPair<double, FString>(0.0, FString()));
		Float = Gap > Float.Key ? TPair<double, FString>(Gap, ClassName) : Float;
		Sink = Gap < Sink.Key ? TPair<double, FString>(Gap, ClassName) : Sink;
	}
	TArray<FString> Parts;
	for (const TPair<FString, TPair<double, FString>>& Entry : Worst)
	{
		const TPair<double, FString>& Sink = Sunk.FindChecked(Entry.Key);
		Parts.Add(FString::Printf(TEXT("\"%s\":{\"float\":%.1f,\"float_class\":\"%s\",\"sunk\":%.1f,\"sunk_class\":\"%s\"}"), *Entry.Key,
		                          Entry.Value.Key, *Entry.Value.Value, Sink.Key, *Sink.Value));
	}
	return TEXT("{") + FString::Join(Parts, TEXT(",")) + TEXT("}");
}

FString USurvivalSovietBaseComponent::BuildLanternsJson(const FSurvivalSovietBaseState& Base) const
{
	TArray<FString> Parts;
	for (const FSurvivalSovietPlannedLantern& Lantern : Base.Plan.Lanterns)
	{
		Parts.Add(FString::Printf(TEXT("{\"class\":\"SurvivalSovietBaseLantern\",\"position\":[%.0f,%.0f],\"lights\":\"%s\",\"target\":[%.0f,%.0f],\"distance\":%.0f}"),
		                          Lantern.Position.X, Lantern.Position.Y, *Lantern.Lights, Lantern.Target.X, Lantern.Target.Y,
		                          FVector2D::Distance(Lantern.Position, Lantern.Target)));
	}
	return FString::Join(Parts, TEXT(","));
}

FString USurvivalSovietBaseComponent::BuildPadsJson(const FSurvivalSovietBaseState& Base) const
{
	TArray<FString> Parts;
	for (const FSurvivalSovietPlannedPad& Pad : Base.Plan.Pads)
	{
		const FString ClassName = GetNameSafe(M_ResolvedClasses.IsValidIndex(Pad.BuildingClassIndex)
			                                      ? M_ResolvedClasses[Pad.BuildingClassIndex].Get()
			                                      : nullptr);
		Parts.Add(FString::Printf(
			TEXT("{\"building\":\"%s\",\"kind\":\"%s\",\"center\":[%.0f,%.0f],\"pad_half\":[%.0f,%.0f],\"footprint_half\":[%.0f,%.0f],\"yaw\":%.2f}"),
			*ClassName, *UEnum::GetValueAsString(Pad.Kind), Pad.Center.X, Pad.Center.Y, Pad.PadHalfExtents.X, Pad.PadHalfExtents.Y,
			Pad.FootprintHalfExtents.X, Pad.FootprintHalfExtents.Y, FMath::RadiansToDegrees(Pad.YawRadians)));
	}
	return FString::Join(Parts, TEXT(","));
}

FString USurvivalSovietBaseComponent::BuildMatchTableJson(const TArray<int32>& MatchSeeds)
{
	const USurvivalSovietBaseSettings* Settings = GetSovietBaseSettings();
	if (Settings == nullptr or not GetIsValidDirector())
	{
		return FString();
	}
	TArray<ASurvivalStartLocation*> Locations;
	for (const TWeakObjectPtr<ASurvivalStartLocation>& Location : M_Director->GetStartLocations())
	{
		if (Location.IsValid())
		{
			Locations.Add(Location.Get());
		}
	}
	// One terrain scan per start location, reused by every roll on it.
	TMap<ASurvivalStartLocation*, FSurvivalSovietSiteMap> Sites;
	for (ASurvivalStartLocation* Location : Locations)
	{
		FSurvivalSovietBaseState Scan;
		Scan.Center = Location->GetDeployCenter();
		Scan.GroundTraceParams = FCollisionQueryParams(TEXT("SurvivalSovietBaseGround"), false);
		Scan.Site.Init(FVector2D(Scan.Center), Scan.Center.Z, Location->GetBuildAreaRadius(),
		               SurvivalSovietBaseConstants::SiteReachPastBuildArea);
		TraceSiteSamples(Scan, Scan.Site.GetSampleCount());
		Sites.Add(Location, Scan.Site);
	}

	FString Lines;
	for (const FSurvivalSovietBaseDifficultyScaling& Row : Settings->DifficultyScaling)
	{
		for (const ASurvivalStartLocation* Chosen : Locations)
		{
			for (const int32 MatchSeed : MatchSeeds)
			{
				TArray<int32> UsedPresets;
				int32 BaseIndex = 0;
				for (ASurvivalStartLocation* Location : Locations)
				{
					if (Location == Chosen)
					{
						continue;
					}
					FSurvivalSovietBaseState Base;
					Base.BaseIndex = BaseIndex;
					Base.StartLocation = Location;
					Base.Center = Location->GetDeployCenter();
					Base.Scaling = Row;
					Base.Seed = ComputeBaseSeed(*Location, BaseIndex, MatchSeed);
					ApplyDistanceRank(Base, ComputeDistanceRank(*Location, Chosen, BaseIndex), Locations.Num() - 1, *Settings);
					Base.PresetIndex = PickPresetIndex(*Settings, Base, UsedPresets);
					Base.ThreatOverride = FVector2D(Chosen->GetDeployCenter());
					Base.Site = Sites[Location];
					UsedPresets.Add(Base.PresetIndex);
					Base.Plan = FSurvivalSovietBaseLayout::BuildPlan(BuildLayoutInput(Base, *Settings));

					TMap<FString, int32> Counts;
					for (const FSurvivalSovietPlannedStructure& Structure : Base.Plan.Structures)
					{
						if (Structure.Role != ESurvivalSovietStructureRole::Fortification)
						{
							Counts.FindOrAdd(GetNameSafe(M_ResolvedClasses[Structure.ClassIndex].Get()))++;
						}
					}
					TMap<FString, int32> UnitCounts;
					for (const FSurvivalSovietPlannedGarrison& Group : Base.Plan.Garrison)
					{
						for (const FTrainingOption& Option : Group.Units)
						{
							UnitCounts.FindOrAdd(SovietPreview_UnitName(Option) + TEXT("|") + UEnum::GetValueAsString(Group.Kind))++;
						}
					}
					Lines += FString::Printf(
						TEXT("{\"difficulty\":\"%s\",\"player_start\":\"%s\",\"match_seed\":%d,\"base\":\"%s\",\"rank\":%d,\"strength\":%d,")
						TEXT("\"preset\":\"%s\",\"archetype\":\"%s\",\"budget\":%d,\"threat\":%d,\"reserve\":%d,\"production\":%d,")
						TEXT("\"core_chain\":%.0f,\"reward_per_threat\":%.2f,\"unspent\":%d,\"spots_used\":%d,\"spots_available\":%d,\"points_off_main_line\":%d,\"dropped_slots\":[%s],\"buildings\":%s,\"units\":%s}\n"),
						*UEnum::GetValueAsString(Row.Difficulty), *Chosen->GetName(), MatchSeed, *Location->GetName(),
						Base.DistanceRank, Base.Strength, *Settings->Presets[Base.PresetIndex].PresetName.ToString(),
						*UEnum::GetValueAsString(Settings->Presets[Base.PresetIndex].Archetype), Base.Plan.ThreatBudget,
						Base.Plan.ThreatPoints, Base.Plan.ReservePoints, Base.Plan.ProductionCount,
						Base.Plan.RequiredCoreClusterDistance, Row.RewardPerThreatPoint, Base.Plan.UnspentPoints,
						Base.Plan.EmplacementSpotsUsed, Base.Plan.EmplacementSpotsAvailable, Base.Plan.PointsOutsideMainLine,
						*SovietPreview_JoinStrings(Base.Plan.DroppedSlots), *SovietPreview_JoinCounts(Counts),
						*SovietPreview_JoinCounts(UnitCounts));
					++BaseIndex;
				}
			}
		}
	}
	return Lines;
}

TArray<FVector> USurvivalSovietBaseComponent::GetPathTestPointsForPreview(const FSurvivalSovietBaseState& Base) const
{
	TArray<FVector> Points;
	Points.Add(Base.Center);
	Points.Add(Base.LaunchPoint);
	for (const FSurvivalSovietPlannedGarrison& Group : Base.Plan.Garrison)
	{
		for (const FVector2D& Position : Group.SpawnPositions)
		{
			Points.Add(ProjectToGround(Base, Position));
		}
	}
	return Points;
}
#endif
