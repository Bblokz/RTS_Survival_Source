// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/TimerHandle.h"
#include "UObject/ObjectKey.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseDressing.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseLayout.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "SurvivalSovietBaseComponent.generated.h"

class ASurvivalModeDirector;
struct FAsyncDetailedUnitState;
struct FFindEnemyBaseClusters;
class ASurvivalStartLocation;
class ASurvivalSovietBasePad;
class USurvivalSovietBaseSettings;
class USplineComponent;
struct FSurvivalSovietGroundDecalShape;
struct FSurvivalSovietSiteKit;
struct FStreamableHandle;

/** @brief Broadcast once when all production buildings of a base are dead (BaseIndex, base centre). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSurvivalSovietBaseDestroyed, int32 /*BaseIndex*/, const FVector& /*BaseCenter*/);

#if RTS_WITH_MAP_TESTS
/** @brief What the headless preview forces on one base instead of the match rules. */
struct FSurvivalSovietBasePreviewRequest
{
	FName PresetName = NAME_None;
	int32 Strength = 0;
	int32 Seed = 0;
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	FVector2D ThreatLocation = FVector2D::ZeroVector;
	int32 DistanceRank = 0;
	int32 BaseCount = 4;
};

/** @brief What one garrison unit really came out as, for the spawned-units manifest. */
struct FSurvivalSovietSpawnedUnitRecord
{
	FTrainingOption Option;
	ESurvivalSovietStepKind Kind = ESurvivalSovietStepKind::HoldGarrison;
	FString ClassName;
};
#endif

/**
 * @brief Runtime state of one generated Soviet base.
 */
struct FSurvivalSovietBaseState
{
	int32 BaseIndex = INDEX_NONE;
	TWeakObjectPtr<ASurvivalStartLocation> StartLocation;
	int32 PresetIndex = INDEX_NONE;
	int32 Strength = 0;
	int32 Seed = 0;
	int32 DistanceRank = 0;
	int32 ThreatBudget = 0;
	int32 ReserveBudget = 0;
	bool bIsFarthestBase = false;
	FSurvivalSovietBaseDifficultyScaling Scaling;

	FVector Center = FVector::ZeroVector;
	FVector LaunchPoint = FVector::ZeroVector;
	FSurvivalSovietBasePlan Plan;
	FSurvivalSovietSiteMap Site;
	int32 NextSiteSample = 0;
	// Classes of this base's preset and kit still to be footprint-measured.
	TArray<TSoftClassPtr<AActor>> ClassesToMeasure;
	// Where the base faces when no start location was chosen (preview runs only).
	TOptional<FVector2D> ThreatOverride;

	TArray<TWeakObjectPtr<AActor>> SpawnedStructures;
	TArray<TWeakObjectPtr<AActor>> SpawnedRoads;
	TArray<TWeakObjectPtr<AActor>> GarrisonUnits;
#if RTS_WITH_MAP_TESTS
	TArray<FSurvivalSovietSpawnedUnitRecord> SpawnedUnitRecords;
#endif
	// Ground traces ignore the structures this base already spawned.
	FCollisionQueryParams GroundTraceParams;

	// Structures whose death counts towards destroying the base (production, or every building when the base
	// has no production building).
	int32 TrackedStructuresAlive = 0;
	int32 TrackedStructuresTotal = 0;
	// Threat points of the structures that count as this base's defences; destroying them earns the defence payout.
	int32 DefensePointsTotal = 0;
	int32 DefensePointsDestroyed = 0;
	float PayoutFractionPaid = 0.f;
	int32 PendingStructureSpawns = 0;
	int32 PendingGarrisonGroups = 0;
	// Footprint dressing (round 5): the base's one dressing actor, what each pad laid, and the cost counts.
	TWeakObjectPtr<ASurvivalSovietBaseDressing> Dressing;
	TArray<FSurvivalSovietFootprintRecord> FootprintRecords;
	int32 DressedPads = 0;
	int32 DressingInstances = 0;
	int32 DressingComponents = 0;

	TSharedPtr<FStreamableHandle> LoadHandle;
	// The PCG volume running this base's graph, the layout its stage nodes advance, and what wakes the sleeping node.
	TWeakObjectPtr<ASurvivalSovietBasePCGVolume> Volume;
	TSharedPtr<FSurvivalSovietBaseLayoutRun> LayoutRun;
	TFunction<void()> OnGraphStepDone;
	double LayoutMilliseconds = 0.0;
	bool bPrepared = false;
	bool bSpawnRequested = false;
	bool bPlanBuilt = false;
	bool bStructuresSpawned = false;
	bool bDestroyed = false;
};

/**
 * @brief Generates a Soviet base on every start location the player did not pick.
 *
 * Flow per base: pick strength (distance rank from the player + difficulty) and a preset (seeded) -> async load
 * the preset's classes -> a per-base PCG volume runs the BaseGraph: its site node measures footprints and traces the
 * terrain over frames, its stage nodes run the preset's master plan (streets, plots, perimeter, guns, garrison posts)
 * through the shared EnemyBaseSetup placement builder, and its spawn node builds what their points describe in small
 * batches -> garrison spawns through ARTSAsyncSpawner.
 * Spawned Rus building expansions register with the game unit manager as enemy (player 2) in their own BeginPlay,
 * which unlocks enemy tech levels and makes the strategic AI see the base as an AI base cluster.
 * @note Owned by the Soviet-base design; the public signatures of InitSovietBases and GenerateSovietBaseAt are
 * part of the contract.
 */
UCLASS(ClassGroup=(Survival), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USurvivalSovietBaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USurvivalSovietBaseComponent();

	/** @brief Called by the director at BeginPlay. */
	void InitSovietBases(ASurvivalModeDirector* Director);

	/** @brief Builds a base on the start location; BaseIndex is 0..N-1 over the unpicked locations. */
	void GenerateSovietBaseAt(ASurvivalStartLocation* StartLocation, int32 BaseIndex);

	// ---- Called by the base PCG graph's nodes ----

	bool GetIsBasePrepared(int32 BaseArrayIndex) const;
	bool GetIsBaseSpawnRequested(int32 BaseArrayIndex) const;
	const FSurvivalSovietSiteMap* GetBaseSite(int32 BaseArrayIndex) const;
	const FSurvivalSovietBasePlan* GetBasePlan(int32 BaseArrayIndex) const;

	/**
	 * @brief Measures the base's Blueprints and traces its terrain, spread over frames.
	 * @param OnPrepared Wakes the graph's site node.
	 */
	void BeginBasePreparation(int32 BaseArrayIndex, TFunction<void()> OnPrepared);

	/** @return Index of the first structure this stage appended to the base's plan. */
	int32 RunBasePlanStage(int32 BaseArrayIndex, ESurvivalSovietPlanStage Stage);

	/**
	 * @brief Spawns what the graph's points describe, in the usual batches over frames.
	 * @param OnSpawned Wakes the graph's spawn node once every structure stands.
	 */
	void SpawnBaseFromGraph(int32 BaseArrayIndex, TArray<FSurvivalSovietPlannedStructure>&& Structures,
	                        TArray<FSurvivalSovietPlannedRoad>&& Roads, TFunction<void()> OnSpawned);

	/** @brief Fired when a base is destroyed. Nothing outside this system listens to it yet. */
	FOnSurvivalSovietBaseDestroyed OnSovietBaseDestroyed;

	/** @return Bases requested so far (destroyed ones included). */
	UFUNCTION(BlueprintPure, Category = "Survival|Soviet Bases")
	int32 GetSovietBaseCount() const { return M_Bases.Num(); }

	/** @return Bases that still have a production building alive. */
	UFUNCTION(BlueprintPure, Category = "Survival|Soviet Bases")
	int32 GetAliveSovietBaseCount() const;

	/** @return Centres of the bases that are still alive. */
	TArray<FVector> GetAliveBaseCenters() const;

	/**
	 * @return Per alive base a ground point on its perimeter facing the player's start location; a natural
	 * spawn point for attacks that come out of a base.
	 */
	TArray<FVector> GetAliveBaseLaunchPoints() const;

	/** @return 1 + alive bases * WavePressurePerAliveBase; the waves director may scale wave budgets with it. */
	UFUNCTION(BlueprintPure, Category = "Survival|Soviet Bases")
	float GetWavePressureMultiplier() const;

	/** @return Strength (0 = weakest) of the base, -1 when the index is unknown. */
	int32 GetBaseStrength(int32 BaseIndex) const;

	bool GetIsBaseDestroyed(int32 BaseIndex) const;

	/** @return The radius a base at this start is planned in: the settings' BuildAreaRadius, else the start location's own. */
	float GetBaseBuildRadius(const ASurvivalStartLocation& StartLocation) const;

#if RTS_WITH_MAP_TESTS
	/** @brief Builds one base with forced preset, strength, seed and threat for FSurvivalSovietBasePreview. */
	void GenerateBaseForPreview(ASurvivalStartLocation* StartLocation, int32 BaseIndex,
	                            const FSurvivalSovietBasePreviewRequest& Request);

	/** @brief Destroys every previewed base without paying rewards or firing base destroyed events. */
	void ClearBasesForPreview();

	const TArray<FSurvivalSovietBaseState>& GetBaseStatesForPreview() const { return M_Bases; }

	/** @return What really stands in the world for this base, against what was planned, as JSON. */
	FString BuildSpawnManifestJson(const FSurvivalSovietBaseState& Base, const FString& Label) const;

	/** @return The base's ground pads with the class of the building on each, as a JSON array body. */
	FString BuildPadsJson(const FSurvivalSovietBaseState& Base) const;

	/** @return The base's storage yards (items, rows, fence, nets) as a JSON array body. */
	FString BuildYardsJson(const FSurvivalSovietBaseState& Base) const;

	/** @return The base's lanterns and what each lights, as a JSON array body. */
	FString BuildLanternsJson(const FSurvivalSovietBaseState& Base) const;

	FString GetClassNameForPreview(int32 ClassIndex) const;
	FString JoinClassNames(const TArray<int32>& ClassIndices) const;

	/** @return Per family (wall, building, defence, prop) the largest gap between visible mesh bottom and ground, as JSON. */
	FString BuildGroundGapsJson(const FSurvivalSovietBaseState& Base) const;

	/**
	 * @brief Rolls whole matches through the real strength and preset picks and plans every base, nothing spawned.
	 * @param MatchSeeds Match seeds to roll per difficulty and player start.
	 * @return One JSON object per line, one per base.
	 */
	FString BuildMatchTableJson(const TArray<int32>& MatchSeeds);

	int32 GetPendingGarrisonGroupsForPreview() const { return M_PendingGarrisonGroups.Num(); }

	/** @return The enemy controller's live base cluster request. */
	const FFindEnemyBaseClusters* GetClusterRequestForPreview() const;

	/** @return Every spawned enemy building as the snapshot the strategic AI's cluster search reads. */
	TArray<FAsyncDetailedUnitState> BuildBuildingSnapshotForPreview() const;

	/** @return The nearest defence still standing, or production once no defence stands. */
	FVector GetNearestStandingStructureForPreview(const FSurvivalSovietBaseState& Base, const FVector& From) const;

	int32 CountLiveGarrisonForPreview(const FSurvivalSovietBaseState& Base) const;

	/** @return "asset" or "code defaults": where the tables of this run come from. */
	FString GetTuningSourceForPreview() const;

	/** @return Ground points inside the base units must be able to path to. */
	TArray<FVector> GetPathTestPointsForPreview(const FSurvivalSovietBaseState& Base) const;
#endif

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	TArray<FSurvivalSovietBaseState> M_Bases;

	// Per-match seed every base seed is derived from.
	int32 M_MatchSeed = 0;
	bool bM_HasMatchSeed = false;

	// Resolved class table shared by every base; the layout refers to classes by index.
	UPROPERTY()
	TArray<TObjectPtr<UClass>> M_ResolvedClasses;

	// Footprint dressing assets, loaded on the first dressed pad and shared by every base.
	FSurvivalSovietFootprintAssets M_FootprintAssets;
	bool bM_FootprintAssetsLoaded = false;
	UPROPERTY()
	TArray<TObjectPtr<UObject>> M_FootprintAssetRefs;

	TArray<FBox> M_MeasuredLocalBounds;
	TMap<FSoftObjectPath, int32> M_ClassIndexByPath;
	// Classes already reported for not being enemy owned (one error per class).
	TSet<FName> M_ReportedOwnerClassNames;

	/** @brief One structure or road (RoadIndex set) waiting in the spawn queue. */
	struct FPendingStructureSpawn
	{
		int32 BaseArrayIndex = INDEX_NONE;
		int32 StructureIndex = INDEX_NONE;
		int32 RoadIndex = INDEX_NONE;
	};

	TArray<FPendingStructureSpawn> M_SpawnQueue;
	int32 M_SpawnQueueHead = 0;
	FTimerHandle M_SpawnTimerHandle;

	// Bases (array index) measuring classes and tracing terrain; the head one advances every preparation step.
	TArray<int32> M_BasesInPreparation;
	FTimerHandle M_PreparationTimerHandle;

	/** @brief A garrison group whose units are still spawning. */
	struct FPendingGarrisonGroup
	{
		int32 BaseArrayIndex = INDEX_NONE;
		int32 PlannedGroupIndex = INDEX_NONE;
		int32 PendingUnits = 0;
		TArray<TWeakObjectPtr<AActor>> SpawnedUnits;
	};

	TMap<int32, FPendingGarrisonGroup> M_PendingGarrisonGroups;
	int32 M_NextGarrisonGroupId = 0;

	/** @brief What losing a tracked structure means for its base. */
	struct FTrackedStructure
	{
		int32 BaseArrayIndex = INDEX_NONE;
		int32 ThreatPoints = 0;
		bool bCountsTowardsDestruction = false;
	};

	TMap<TObjectKey<AActor>, FTrackedStructure> M_TrackedStructureToBase;

	bool bM_HasConfiguredStrategicAI = false;

	// ---- Generation ----
	const USurvivalSovietBaseSettings* GetSovietBaseSettings() const;
	bool GetIsValidDirector() const;
	void EnsureMatchSeed(const USurvivalSovietBaseSettings& Settings);
	TArray<ASurvivalStartLocation*> GetUnpickedStartLocations(const ASurvivalStartLocation* Chosen) const;
	/** @return 0 for the base nearest the player, BaseCount - 1 for the farthest. */
	int32 ComputeDistanceRank(const ASurvivalStartLocation& StartLocation, const ASurvivalStartLocation* Chosen,
	                          int32 BaseIndex) const;
	static int32 ReadRankTable(const TArray<int32>& Table, int32 DistanceRank, int32 BaseCount);
	/** @brief Reads strength, threat budget and reserve budget from the base's difficulty row. */
	void ApplyDistanceRank(FSurvivalSovietBaseState& Base, int32 DistanceRank, int32 BaseCount,
	                       const USurvivalSovietBaseSettings& Settings) const;
	/**
	 * @brief Seeded, weighted pick among the presets this base's strength, tech cap and rank allow.
	 * @param UsedPresets Presets the other bases of this match already took.
	 * @return The preset index; 0 with an error when no preset allows the strength.
	 */
	int32 PickPresetIndex(const USurvivalSovietBaseSettings& Settings, const FSurvivalSovietBaseState& Base,
	                      const TArray<int32>& UsedPresets) const;
	int32 ComputeBaseSeed(const ASurvivalStartLocation& StartLocation, int32 BaseIndex, int32 MatchSeed) const;
	TArray<FSoftObjectPath> CollectPresetClassPaths(const FSurvivalSovietBasePreset& Preset,
	                                                const FSurvivalSovietSiteKit& Kit) const;

	/** @brief Starts the async load of the base's preset; the plan is built when it arrives. */
	void BeginBaseGeneration(int32 BaseArrayIndex, const USurvivalSovietBaseSettings& Settings);
	void OnBaseAssetsLoaded(int32 BaseArrayIndex);

	/** @brief Places the base's PCG volume and starts the graph; everything else runs from the graph's nodes. */
	void StartBaseGraph(int32 BaseArrayIndex, const USurvivalSovietBaseSettings& Settings);
	void WakeBaseGraph(FSurvivalSovietBaseState& Base);
	void ProcessPreparation();
	/** @return True when this base has no measuring or tracing left. */
	bool AdvanceBasePreparation(FSurvivalSovietBaseState& Base, const USurvivalSovietBaseSettings& Settings);
	void TraceSiteSamples(FSurvivalSovietBaseState& Base, int32 SampleBudget) const;
	void OnBasePrepared(int32 BaseArrayIndex);
	FSurvivalSovietLayoutInput BuildLayoutInput(const FSurvivalSovietBaseState& Base,
	                                            const USurvivalSovietBaseSettings& Settings);
	void FinishBasePlan(FSurvivalSovietBaseState& Base);

	// ---- Footprint resolution ----
	bool ResolveEntry(const FEnemyBlueprintEntry& Entry, FEnemyResolvedEntry& OutResolved);
	int32 FindOrResolveClass(const TSoftClassPtr<AActor>& SoftClass);
	FBox MeasureClassLocalBounds(UClass* ActorClass) const;

	// ---- Structure spawning ----
	void EnqueueBaseStructures(int32 BaseArrayIndex);
	void ProcessSpawnQueue();
	AActor* SpawnStructure(FSurvivalSovietBaseState& Base, const FSurvivalSovietPlannedStructure& Structure);
	UMaterialInterface* GetPadMaterial(ESurvivalSovietPadKind Kind) const;
	/** @brief Applies a wall piece's stretch to an actor whose Blueprint dropped the spawn scale. */
	void EnforceSpawnScale(AActor& Actor, const FVector& Scale) const;
	static void LimitPipeConnections(AActor& Actor);
	/** @brief Dresses a building pad's footprint for its class through the base's dressing actor; records seating and counts. */
	void DressPad(FSurvivalSovietBaseState& Base, const FSurvivalSovietPlannedStructure& Structure, ASurvivalSovietBasePad& Pad);
	static TArray<const TArray<TSoftObjectPtr<UStaticMesh>>*> GetFootprintMeshLists(const FSurvivalSovietSiteKit& Kit);
	static void LoadFootprintAssets(const FSurvivalSovietSiteKit& Kit, FSurvivalSovietFootprintAssets& OutAssets);
	/** @brief Keeps the loaded footprint assets referenced (the dressing holds raw pointers). */
	void KeepFootprintAssets();
#if RTS_WITH_MAP_TESTS
	/** @brief -SovietBaseDesertNets: the desert camouflage variant on the base kit's net meshes (an owner choice, not the default). */
	void ApplyDesertNetsForPreview(AActor& Actor);
#endif
	/** @brief Pit meshes paint earth with a virtual texture material; on a map without that texture it reads black. */
	void ReplaceVirtualTextureEarth(AActor& Actor) const;
	void SpawnRoad(FSurvivalSovietBaseState& Base, int32 RoadIndex, const USurvivalSovietBaseSettings& Settings);
	UMaterialInterface* GetRoadSurface(ESurvivalSovietRoadKind Kind, const FSurvivalSovietSiteKit& Kit) const;
	/** @return Points without those that lie within Tolerance cm of the line through their neighbours. */
	static TArray<FVector2D> SimplifyRoadPoints(const TArray<FVector2D>& Points, double Tolerance);
public:
	static double GetRoadPaintHalfWidth(const FSurvivalSovietPlannedRoad& Road, const FSurvivalSovietSiteKit& Kit);
private:
	/**
	 * @brief Paints a road as decal pieces: soft verges, ruts along it, ends that run into the street they meet or fade out.
	 * @param RoadIndex Index in Base.Plan.Roads.
	 * @param Surface The road kind's instance of M_SovietGroundDecal.
	 */
	void SpawnRoadDecals(FSurvivalSovietBaseState& Base, int32 RoadIndex, UMaterialInterface& Surface, const FSurvivalSovietSiteKit& Kit);
	/** @brief Fills where a street meets another, and lays hard standing through every gate; runs once, after the last road. */
	void SpawnJunctionsAndAprons(FSurvivalSovietBaseState& Base, const USurvivalSovietBaseSettings& Settings);
	/**
	 * @brief Finds the street, other than ExcludeIndex, that Point lies on.
	 * @return Index of that road or INDEX_NONE; OutOnCentre is Point moved onto its centre line, OutDirection its direction there.
	 */
	int32 FindRoadUnder(const FSurvivalSovietBaseState& Base, const FVector2D& Point, int32 ExcludeIndex, const FSurvivalSovietSiteKit& Kit,
	                    FVector2D& OutOnCentre, FVector2D& OutDirection) const;
	void SpawnGroundDecal(FSurvivalSovietBaseState& Base, const FSurvivalSovietGroundDecalShape& Shape, UMaterialInterface* Material);
	/** @brief Lays a ground path Blueprint along the road; its construction script only knows its default spline. */
	void ApplyRoadToSpline(AActor& RoadActor, const FSurvivalSovietBaseState& Base, const FSurvivalSovietPlannedRoad& Road) const;
	void RebuildRoadTiles(AActor& RoadActor, const USplineComponent& Spline, const FSurvivalSovietBaseState& Base,
	                      const FSurvivalSovietPlannedRoad& Road) const;
	void OnPendingSpawnDone(int32 BaseArrayIndex);
	FVector ProjectToGround(const FSurvivalSovietBaseState& Base, const FVector2D& Position) const;
	void OnStructureSpawned(int32 BaseArrayIndex, const FSurvivalSovietPlannedStructure& Structure, AActor* Actor);
	void OnBaseStructuresSpawned(int32 BaseArrayIndex);
	/** @brief Reports (once per class) a spawned RTS unit that is not owned by the enemy player. */
	void ValidateEnemyOwnership(AActor* Actor);

	// ---- Destruction tracking ----
	/**
	 * @brief Listens for the structure's loss.
	 * @param bCountsTowardsDestruction The base is destroyed once all such structures are gone.
	 * @param ThreatPoints What its loss adds to the base's broken defences.
	 */
	void TrackStructureForDestruction(int32 BaseArrayIndex, AActor* Actor, bool bCountsTowardsDestruction, int32 ThreatPoints);

	UFUNCTION()
	void OnTrackedStructureDestroyed(AActor* DestroyedActor);

	void OnTrackedStructureLost(const TObjectKey<AActor>& StructureKey);
	void OnBaseDestroyed(int32 BaseArrayIndex);
	/** @return 0..1 of the base's payout earned so far: production share once it is destroyed, plus broken defences. */
	static float GetEarnedPayoutFraction(const FSurvivalSovietBaseState& Base, const USurvivalSovietBaseSettings& Settings);
	/** @brief Pays what was earned since the last call. */
	void PayEarnedRewards(FSurvivalSovietBaseState& Base);

	// ---- Garrison ----
	void SpawnBaseGarrison(int32 BaseArrayIndex);
	void OnGarrisonUnitSpawned(int32 GroupId, AActor* SpawnedActor, const FTrainingOption& Option);
	void OnGarrisonGroupComplete(const FPendingGarrisonGroup& Group);
	void RegisterGarrisonAsStrategicReserve(const TArray<TWeakObjectPtr<AActor>>& Units) const;
	void StartGarrisonPatrol(const FSurvivalSovietBaseState& Base, const FSurvivalSovietPlannedGarrison& Planned,
	                         const TArray<TWeakObjectPtr<AActor>>& Units) const;

	// ---- Strategic AI ----
	void ConfigureStrategicAIBaseDetection(const USurvivalSovietBaseSettings& Settings,
	                                       const FSurvivalSovietBasePlan& Plan);
};
