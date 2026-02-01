// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/GenerationStep/Enum_CampaignGenerationStep.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyHQPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyPlacementRules/EnemyPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/MissionPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/NeutralItemPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/PlayerHQPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignCountDifficultyTuning.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignPlacementFailurePolicy.h"
#include "GeneratorWorldCampaign.generated.h"

class AAnchorPoint;
class AConnection;

namespace WorldCampaignDebugDefaults
{
	constexpr float ConnectionDrawDurationSeconds = 10.f;
	constexpr float ConnectionLineThickness = 5.f;
	constexpr float ConnectionDrawHeightOffset = 50.f;
}

USTRUCT(BlueprintType)
struct FConnectionGenerationRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation",
		meta = (ToolTip = R"(Used in: ConnectionsCreated -> ExecuteCreateConnections/GenerateConnectionsForAnchors.
Why: Sets a hard minimum connection degree so anchors are not stranded.
Technical: Enforced in desired-connection assignment and validated during connection passes.
Notes: Hard constraint; may trigger retries/backtracking if impossible with current anchors.)"))
	int32 MinConnections = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation",
		meta = (ToolTip = R"(Used in: ConnectionsCreated -> ExecuteCreateConnections/GenerateConnectionsForAnchors.
Why: Caps graph density so the map retains readable routing and pacing.
Technical: Hard upper bound for per-anchor connection counts during generation.
Notes: MinConnections must be <= MaxConnections; if too low, generation may under-connect.)"))
	int32 MaxConnections = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation",
		meta = (ToolTip = R"(Used in: ConnectionsCreated -> GeneratePhasePreferredConnections.
Why: Defines the preferred spatial radius for first-pass connections so topology stays local.
Technical: Acts as a soft preference (distance filter) before extended phases relax it.
Notes: If too small, later phases will extend beyond it to satisfy MinConnections.)"))
	float MaxPreferredDistance = 5000.f;
};

USTRUCT(BlueprintType)
struct FWorldCampaignPlacementState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: All generation steps.
Why: Records the deterministic seed actually used so results can be reproduced.
Technical: This seed is fed into FRandomStream and combined with step attempt indices.
Notes: Changing it invalidates determinism for retries and debugging.)"))
	int32 SeedUsed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: ConnectionsCreated and all downstream steps.
Why: Caches the anchor set that the generator operated on for inspection and undo.
Technical: Filled during CacheGeneratedState; drives placement candidate lookups.
Notes: Acts as the authoritative anchor list for this generation attempt.)"))
	TArray<TObjectPtr<AAnchorPoint>> CachedAnchors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: ConnectionsCreated and all downstream steps.
Why: Caches spawned connections so later steps can derive hops and undo reliably.
Technical: Populated during connection generation and referenced for hop distance maps.
Notes: Serves as the rollback source when backtracking connections.)"))
	TArray<TObjectPtr<AConnection>> CachedConnections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: PlayerHQPlaced and all subsequent steps.
Why: Stores the selected player HQ anchor so other placement rules can reference it.
Technical: Set by ExecutePlaceHQ after filtering candidates and selecting by attempt.
Notes: Hard prerequisite for enemy/neutral/mission placement filters.)"))
	TObjectPtr<AAnchorPoint> PlayerHQAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: Cached lookup keys and undo transactions.
Why: Provides a stable identifier for the player HQ anchor across retries.
Technical: Cached from the anchor's GUID; used for hop distance maps and maps keyed by anchor.
Notes: Required for determinism even if anchor pointers change during regeneration.)"))
	FGuid PlayerHQAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: EnemyHQPlaced and all subsequent steps.
Why: Stores the selected enemy HQ anchor to drive enemy spacing rules.
Technical: Set by ExecutePlaceEnemyHQ after filtering candidates and selection.
Notes: Downstream enemy placement uses its hop distances as hard filters.)"))
	TObjectPtr<AAnchorPoint> EnemyHQAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: Cached lookup keys and undo transactions.
Why: Provides a stable identifier for the enemy HQ anchor across retries.
Technical: Cached from the anchor GUID; used for hop distance maps and anchor-keyed maps.
Notes: Keeps deterministic keying even if anchors are rebuilt.)"))
	FGuid EnemyHQAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: EnemyObjectsPlaced.
Why: Records which enemy item type was placed at each anchor for later filtering and undo.
Technical: Written during enemy placement, keyed by anchor GUID for deterministic lookup.
Notes: Acts as a hard occupancy map; a key present means the anchor is consumed.)"))
	TMap<FGuid, EMapEnemyItem> EnemyItemsByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced.
Why: Records which neutral object type was placed at each anchor for spacing checks and undo.
Technical: Filled during neutral placement; queried when validating mission placement adjacency.
Notes: Hard occupancy map; used as a filter during mission requirements.)"))
	TMap<FGuid, EMapNeutralObjectType> NeutralItemsByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: MissionsPlaced.
Why: Records the mission type placed at each anchor to prevent duplicates and support undo.
Technical: Populated during mission placement; keyed by anchor GUID for deterministic access.
Notes: Hard occupancy map for mission anchors; used for validation and backtracking.)"))
	TMap<FGuid, EMapMission> MissionsByAnchorKey;
};

USTRUCT(BlueprintType)
struct FWorldCampaignDerivedData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: PlayerHQPlaced, EnemyObjectsPlaced, NeutralObjectsPlaced, MissionsPlaced.
Why: Stores hop distances from the player HQ to every anchor as a reusable cache.
Technical: Built after connections are generated; reused for hop-based filtering.
Notes: Deterministic given seed and connection graph; treated as authoritative distances.)"))
	TMap<FGuid, int32> PlayerHQHopDistancesByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: EnemyObjectsPlaced.
Why: Caches hop distances from the enemy HQ for enemy spacing rules.
Technical: Built after EnemyHQPlaced; used for Min/Max hop filters.
Notes: Hard filter source; if missing or empty, enemy placement can fail.)"))
	TMap<FGuid, int32> EnemyHQHopDistancesByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: PlayerHQPlaced, EnemyHQPlaced, MissionsPlaced.
Why: Stores connection degree per anchor for degree-based filters.
Technical: Cached via CacheAnchorConnectionDegrees and reused by selection logic.
Notes: Hard constraint when Min/Max connection degree rules are enabled.)"))
	TMap<FGuid, int32> AnchorConnectionDegreesByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: ConnectionsCreated debug/analysis.
Why: Preserves chokepoint scoring so designers can inspect connectivity bottlenecks.
Technical: Derived data keyed by anchor GUID, typically computed after connections exist.
Notes: Informational/diagnostic; does not directly gate placement.)"))
	TMap<FGuid, float> ChokepointScoresByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: EnemyObjectsPlaced.
Why: Tracks how many enemy items of each type have been placed.
Technical: Updated as items are spawned; compared against required counts.
Notes: Hard constraint; when counts are met, additional placement is skipped.)"))
	TMap<EMapEnemyItem, int32> EnemyItemPlacedCounts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced.
Why: Tracks how many neutral items of each type have been placed.
Technical: Updated during neutral placement and used to stop once required counts are reached.
Notes: Hard constraint; affects retry logic if counts cannot be satisfied.)"))
	TMap<EMapNeutralObjectType, int32> NeutralItemPlacedCounts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (ToolTip = R"(Used in: MissionsPlaced.
Why: Tracks how many missions of each type have been placed.
Technical: Updated during mission placement and checked against required mission counts.
Notes: Hard constraint; unmet counts can trigger backtracking.)"))
	TMap<EMapMission, int32> MissionPlacedCounts;
};

USTRUCT(BlueprintType)
struct FCampaignGenerationStepTransaction
{
	GENERATED_BODY()

	UPROPERTY(meta = (ToolTip = R"(Used in: Backtracking/Undo for every generation step.
Why: Identifies which step completed to decide how to roll back state.
Technical: Set in ExecuteStepWithTransaction when a step succeeds.
Notes: Determinism relies on the same step order when replayed.)"))
	ECampaignGenerationStep CompletedStep = ECampaignGenerationStep::NotStarted;

	UPROPERTY(meta = (ToolTip = R"(Used in: Undo of placement steps.
Why: Records spawned actors so a failed step can be fully rolled back.
Technical: Populated by each step's Execute* function; destroyed in UndoLastTransaction.
Notes: Hard rollback data; includes any spawn that must be removed.)"))
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY(meta = (ToolTip = R"(Used in: Undo for ConnectionsCreated.
Why: Tracks connection actors spawned during a step for deterministic rollback.
Technical: Stored in the transaction and consumed by UndoConnections.
Notes: Hard rollback data; must include all connections created in the step.)"))
	TArray<TObjectPtr<AConnection>> SpawnedConnections;

	UPROPERTY(meta = (ToolTip = R"(Used in: Undo for any step.
Why: Snapshot of placement state before the step so rollback restores maps and anchors.
Technical: Copied in ExecuteStepWithTransaction prior to mutation.
Notes: Restores deterministic state for retries.)"))
	FWorldCampaignPlacementState PreviousPlacementState;

	UPROPERTY(meta = (ToolTip = R"(Used in: Undo for any step.
Why: Snapshot of derived data before the step for consistent rollback.
Technical: Copied in ExecuteStepWithTransaction prior to mutation.
Notes: Keeps hop/degree caches aligned with placement state during backtracking.)"))
	FWorldCampaignDerivedData PreviousDerivedData;
};

// Must be in Ustruct for GC tracking.
USTRUCT()
struct FConnectionSegment
{
	GENERATED_BODY()

	FVector2D StartPoint;
	FVector2D EndPoint;
	UPROPERTY(meta = (ToolTip = R"(Used in: Connection intersection checks during ConnectionsCreated.
Why: Ties this segment back to the owning anchor for shared-endpoint logic.
Technical: Weak pointer to avoid GC issues; validated before use.
Notes: Internal debug/validation data only; not a placement rule.)"))
	TWeakObjectPtr<AAnchorPoint> StartAnchor;
	UPROPERTY(meta = (ToolTip = R"(Used in: Connection intersection checks during ConnectionsCreated.
Why: Ties this segment back to the second endpoint anchor for intersection filters.
Technical: Weak pointer to avoid GC ownership; validated before use.
Notes: Internal data; ignored if invalid.)"))
	TWeakObjectPtr<AAnchorPoint> EndAnchor;
	UPROPERTY(meta = (ToolTip = R"(Used in: Connection intersection checks during ConnectionsCreated.
Why: Allows ignoring segments that belong to a specific connection during checks.
Technical: Weak pointer so destroyed connections do not keep references alive.
Notes: Internal data for generation safety; not exposed as tuning.)"))
	TWeakObjectPtr<AConnection> OwningConnection;
};

/**
 * @brief Generator actor that can be placed on a campaign map to spawn and configure world connections.
 */
UCLASS()
class RTS_SURVIVAL_API AGeneratorWorldCampaign : public AActor
{
	GENERATED_BODY()

public:
	AGeneratorWorldCampaign();

	UFUNCTION(CallInEditor, Category = "World Campaign|Connection Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::NotStarted"))
	void CreateConnectionsStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|HQ",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::ConnectionsCreated"))
	void PlaceHQStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|HQ",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::PlayerHQPlaced"))
	void PlaceEnemyHQStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|Enemy",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::EnemyHQPlaced"))
	void PlaceEnemyObjectsStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|Neutral",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::EnemyObjectsPlaced"))
	void PlaceNeutralObjectsStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|Missions",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::NeutralObjectsPlaced"))
	void PlaceMissionsStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Generation")
	void ExecuteAllSteps();

	UFUNCTION(CallInEditor, Category = "World Campaign|Generation")
	void EraseAllGeneration();

	UFUNCTION(CallInEditor, Category = "World Campaign|Debug")
	void DebugDrawAllConnections() const;

private:
	bool GetIsValidPlayerHQAnchor() const;
	bool GetIsValidEnemyHQAnchor() const;

	/**
	 * @brief Wraps step execution so undo data is recorded for backtracking.
	 * @param CompletedStep Step state that will be marked as completed on success.
	 * @param StepFunction Function that performs the step work.
	 * @return true if the step executed successfully.
	 */
	bool ExecuteStepWithTransaction(ECampaignGenerationStep CompletedStep,
	                                bool (AGeneratorWorldCampaign::*StepFunction)(FCampaignGenerationStepTransaction&));
	bool ExecuteCreateConnections(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceHQ(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyHQ(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyObjects(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceNeutralObjects(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceMissions(FCampaignGenerationStepTransaction& OutTransaction);

	bool ExecuteAllStepsWithBacktracking();
	bool CanExecuteStep(ECampaignGenerationStep CompletedStep) const;
	ECampaignGenerationStep GetPrerequisiteStep(ECampaignGenerationStep CompletedStep) const;
	ECampaignGenerationStep GetNextStep(ECampaignGenerationStep CurrentStep) const;
	int32 GetStepAttemptIndex(ECampaignGenerationStep CompletedStep) const;
	void IncrementStepAttempt(ECampaignGenerationStep CompletedStep);
	void ResetStepAttemptsFrom(ECampaignGenerationStep CompletedStep);
	EPlacementFailurePolicy GetFailurePolicyForStep(ECampaignGenerationStep FailedStep) const;
	/**
	 * @brief Handles failed steps by undoing transactions and advancing the retry index.
	 * @param FailedStep Step that failed to execute.
	 * @param InOutStepIndex Step index to resume from after backtracking.
	 * @param StepOrder Ordered list of completed step states.
	 * @return true if backtracking can continue.
	 */
	bool HandleStepFailure(ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
	                       const TArray<ECampaignGenerationStep>& StepOrder);
	void UndoLastTransaction();
	void UndoConnections(const FCampaignGenerationStepTransaction& Transaction);
	void ClearPlacementState();
	void ClearDerivedData();
	void CacheAnchorConnectionDegrees();
	void DebugNotifyAnchorPicked(const AAnchorPoint* AnchorPoint, const FString& Label, const FColor& Color) const;

	/**
	 * @brief Prepares anchors for a deterministic connection pass so retries start cleanly.
	 * @param OutAnchorPoints Output list of sorted, valid anchors.
	 * @return true when there are enough valid anchors to continue.
	 */
	bool PrepareAnchorsForConnectionGeneration(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints);

	/**
	 * @brief Runs the connection generation phases with deterministic shuffle per attempt.
	 * @param AnchorPoints Sorted anchors to process.
	 * @param RandomStream Stream seeded for deterministic attempts.
	 * @param OutDesiredConnections Desired connection counts per anchor.
	 */
	void GenerateConnectionsForAnchors(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	                                   FRandomStream& RandomStream,
	                                   TMap<TObjectPtr<AAnchorPoint>, int32>& OutDesiredConnections);

	/**
	 * @brief Stores derived cached data so designers can inspect intermediate results.
	 * @param AnchorPoints Anchors used for keying cached state.
	 */
	void CacheGeneratedState(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints);

	bool ValidateGenerationRules() const;
	int32 GetAnchorConnectionDegree(const AAnchorPoint* AnchorPoint) const;
	bool IsAnchorCached(const AAnchorPoint* AnchorPoint) const;
	/**
	 * @brief Filters and sorts HQ anchor candidates so retries stay deterministic.
	 * @param CandidateSource Input anchors to evaluate.
	 * @param MinDegree Minimum required connection degree.
	 * @param MaxDegree Maximum allowed connection degree.
	 * @param AnchorToExclude Anchor that cannot be selected.
	 * @param OutCandidates Filtered, sorted candidates.
	 * @return true if at least one candidate remains.
	 */
	bool BuildHQAnchorCandidates(const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource, int32 MinDegree,
	                             int32 MaxDegree,
	                             const AAnchorPoint* AnchorToExclude,
	                             TArray<TObjectPtr<AAnchorPoint>>& OutCandidates) const;
	/**
	 * @brief Picks a candidate deterministically based on the attempt index.
	 * @param Candidates Sorted candidate list.
	 * @param AttemptIndex Attempt index for selection.
	 * @return The chosen anchor or nullptr if none are available.
	 */
	AAnchorPoint* SelectAnchorCandidateByAttempt(const TArray<TObjectPtr<AAnchorPoint>>& Candidates,
	                                             int32 AttemptIndex) const;
	bool HasSharedEndpoint(const FConnectionSegment& Segment, const AAnchorPoint* AnchorA,
	                       const AAnchorPoint* AnchorB) const;
	/**
	 * @brief Avoids creating crossing segments so the connection graph stays readable.
	 * @param StartPoint Segment start.
	 * @param EndPoint Segment end.
	 * @param StartAnchor Start anchor for endpoint sharing checks.
	 * @param EndAnchor End anchor for endpoint sharing checks.
	 * @param ExistingSegments Segments that should remain non-intersecting.
	 * @param ConnectionToIgnore Optional connection ignored during intersection tests.
	 * @return true if the new segment would intersect existing segments.
	 */
	bool IsSegmentIntersectingExisting(const FVector2D& StartPoint, const FVector2D& EndPoint,
	                                   const AAnchorPoint* StartAnchor, const AAnchorPoint* EndAnchor,
	                                   const TArray<FConnectionSegment>& ExistingSegments,
	                                   const AConnection* ConnectionToIgnore) const;
	void ClearExistingConnections();
	void GatherAnchorPoints(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints) const;

	/**
	 * @brief Assigns stable desired connection counts so the generator can target a per-node degree.
	 * @param AnchorPoints Anchors to receive desired connection counts.
	 * @param RandomStream Stream used to keep the assignment deterministic.
	 * @param OutDesiredConnections Output mapping of anchors to desired counts.
	 */
	void AssignDesiredConnections(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	                              FRandomStream& RandomStream,
	                              TMap<TObjectPtr<AAnchorPoint>, int32>& OutDesiredConnections) const;

	/**
	 * @brief Attempts to create preferred-distance connections for a single anchor.
	 * @param AnchorPoint Anchor currently being processed.
	 * @param AnchorPoints Sorted list of all anchors for deterministic ordering.
	 * @param DesiredConnections Desired connection counts per anchor.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void GeneratePhasePreferredConnections(AAnchorPoint* AnchorPoint,
	                                       const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	                                       const TMap<TObjectPtr<AAnchorPoint>, int32>& DesiredConnections,
	                                       TArray<FConnectionSegment>& ExistingSegments);

	/**
	 * @brief Extends the search beyond distance limits to satisfy minimum connections.
	 * @param AnchorPoint Anchor currently being processed.
	 * @param AnchorPoints Sorted list of all anchors for deterministic ordering.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void GeneratePhaseExtendedConnections(AAnchorPoint* AnchorPoint,
	                                      const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	                                      TArray<FConnectionSegment>& ExistingSegments);

	/**
	 * @brief Attempts to attach the anchor to an existing connection segment as a junction.
	 * @param AnchorPoint Anchor currently being processed.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void GeneratePhaseThreeWayConnections(AAnchorPoint* AnchorPoint, TArray<FConnectionSegment>& ExistingSegments);

	/**
	 * @brief Spawns and registers a new connection between two anchors if allowed.
	 * @param AnchorPoint First endpoint anchor.
	 * @param CandidateAnchor Second endpoint anchor.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 * @param DebugColor Debug draw color if enabled at compile time.
	 * @return true if the connection was created and registered.
	 */
	bool TryCreateConnection(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor,
	                         TArray<FConnectionSegment>& ExistingSegments, const FColor& DebugColor);

	/**
	 * @brief Validates a potential anchor-to-anchor segment against max counts and intersections.
	 * @param AnchorPoint First endpoint anchor.
	 * @param CandidateAnchor Second endpoint anchor.
	 * @param StartPoint Segment start in XY.
	 * @param EndPoint Segment end in XY.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 * @param ConnectionToIgnore Optional connection to ignore for intersection tests.
	 * @return true if the connection is allowed.
	 */
	bool IsConnectionAllowed(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor,
	                         const FVector2D& StartPoint, const FVector2D& EndPoint,
	                         const TArray<FConnectionSegment>& ExistingSegments,
	                         const AConnection* ConnectionToIgnore) const;

	/**
	 * @brief Finds and adds a valid three-way junction connection to satisfy minimum connections.
	 * @param AnchorPoint Anchor currently being processed.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 * @return true if a three-way connection was created.
	 */
	bool TryAddThreeWayConnection(AAnchorPoint* AnchorPoint, TArray<FConnectionSegment>& ExistingSegments);

	/**
	 * @brief Registers the base connection relationship on both anchors.
	 * @param Connection Connection actor to register.
	 * @param AnchorA First endpoint anchor.
	 * @param AnchorB Second endpoint anchor.
	 */
	void RegisterConnectionOnAnchors(AConnection* Connection, AAnchorPoint* AnchorA, AAnchorPoint* AnchorB) const;

	/**
	 * @brief Registers a third anchor on an existing connection without duplicating the actor.
	 * @param Connection Connection actor to update.
	 * @param ThirdAnchor The new third anchor to register.
	 */
	void RegisterThirdAnchorOnConnection(AConnection* Connection, AAnchorPoint* ThirdAnchor) const;

	/**
	 * @brief Adds a 2D segment entry to the list for intersection checks.
	 * @param Connection Connection actor owning the segment.
	 * @param AnchorA First endpoint anchor.
	 * @param AnchorB Second endpoint anchor.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void AddConnectionSegment(AConnection* Connection, AAnchorPoint* AnchorA, AAnchorPoint* AnchorB,
	                          TArray<FConnectionSegment>& ExistingSegments) const;

	/**
	 * @brief Adds the third-branch segment entry for intersection checks.
	 * @param Connection Connection actor owning the segment.
	 * @param ThirdAnchor Anchor that is attached to the junction.
	 * @param JunctionLocation World-space location on the base segment.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void AddThirdConnectionSegment(AConnection* Connection, AAnchorPoint* ThirdAnchor, const FVector& JunctionLocation,
	                               TArray<FConnectionSegment>& ExistingSegments) const;

	void DebugNotifyAnchorProcessing(const AAnchorPoint* AnchorPoint, const FString& Label, const FColor& Color) const;
	void DebugDrawConnection(const AConnection* Connection, const FColor& Color) const;
	void DebugDrawThreeWay(const AConnection* Connection, const FColor& Color) const;
	void DrawDebugConnectionLine(UWorld* World, const FVector& Start, const FVector& End, const FColor& Color) const;
	void DrawDebugConnectionForActor(UWorld* World, const AConnection* Connection, const FVector& HeightOffset,
	                                 const FColor& BaseConnectionColor, const FColor& ThreeWayConnectionColor) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: Editor button gating for step execution.
Why: Enforces the ordered generation pipeline (ConnectionsCreated -> ... -> MissionsPlaced).
Technical: Each CallInEditor step checks this enum via EditCondition to enable buttons.
Notes: Changing it manually can desync cached data; intended for controlled progression.)"))
	ECampaignGenerationStep M_GenerationStep = ECampaignGenerationStep::NotStarted;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: All placement steps and undo.
Why: Exposes the current placement state for debugging and verification.
Technical: Mutated by Execute* functions; restored from transactions on backtrack.
Notes: Visible-only snapshot; editing is not supported.)"))
	FWorldCampaignPlacementState M_PlacementState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: Debugging and placement filters.
Why: Shows derived caches like hop distances and connection degrees.
Technical: Updated during cache steps; read by placement logic for filtering.
Notes: Derived data; regenerated when connections change.)"))
	FWorldCampaignDerivedData M_DerivedData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: Backtracking system.
Why: Stores a stack of step transactions for undo and retries.
Technical: Each successful step pushes a transaction; failures pop and restore.
Notes: Determinism depends on consistent transaction order.)"))
	TArray<FCampaignGenerationStepTransaction> M_StepTransactions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: Backtracking and retry determinism.
Why: Tracks how many times each step has been attempted to vary the random stream.
Technical: Incremented per step failure; used to seed deterministic retries.
Notes: Changing this breaks reproducibility of attempt-based selection.)"))
	TMap<ECampaignGenerationStep, int32> M_StepAttemptIndices;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: Backtracking guardrails.
Why: Prevents infinite retry loops across all steps.
Technical: Incremented every attempt; compared against MaxTotalAttempts.
Notes: Hard fail-safe; exceeding it aborts generation.)"))
	int32 M_TotalAttemptCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: ConnectionsCreated.
Why: Centralizes tuning for connection creation phases.
Technical: Read by ExecuteCreateConnections and helper functions when building the graph.
Notes: Changing these requires regenerating connections to take effect.)"))
	FConnectionGenerationRules ConnectionGenerationRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: ConnectionsCreated.
Why: Defines which connection actor class is spawned so visuals/behavior can be customized.
Technical: Spawned per segment; must be a valid AConnection subclass.
Notes: If null or invalid, connection generation will fail.)"))
	TSubclassOf<AConnection> M_ConnectionClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: Debugging and backtracking.
Why: Tracks the live connection actors generated in the current run.
Technical: Populated by ExecuteCreateConnections; used for later debug draws.
Notes: Cleared when generation is erased or rolled back.)"))
	TArray<TObjectPtr<AConnection>> M_GeneratedConnections;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Debug",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: DebugDrawAllConnections.
Why: Keeps debug lines visible long enough for inspection without pausing the editor.
Technical: Passed into DrawDebugLine as Duration for every segment.
Notes: Does not affect generation; purely debug visualization.)"))
	float M_DebugConnectionDrawDurationSeconds = WorldCampaignDebugDefaults::ConnectionDrawDurationSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Debug",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: DebugDrawAllConnections.
Why: Makes debug lines readable over dense map art or UI overlays.
Technical: Passed into DrawDebugLine as line thickness.
Notes: This is visual-only; does not affect collision or selection.)"))
	float M_DebugConnectionLineThickness = WorldCampaignDebugDefaults::ConnectionLineThickness;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Debug",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: DebugDrawAllConnections.
Why: Offsets debug lines above the map so they are not Z-fighting with geometry.
Technical: Added to the Z of anchor and junction locations before drawing.
Notes: Visual-only; does not affect any placement logic.)"))
	float M_DebugConnectionDrawHeightOffset = WorldCampaignDebugDefaults::ConnectionDrawHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Player HQ",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: PlayerHQPlaced.
Why: Defines player HQ eligibility and safety constraints for the start of the campaign.
Technical: Read by ExecutePlaceHQ and BuildHQAnchorCandidates.
Notes: Hard filters; if too strict, HQ placement will fail and trigger backtracking.)"))
	FPlayerHQPlacementRules M_PlayerHQPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Enemy HQ",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: EnemyHQPlaced.
Why: Defines where the enemy HQ may appear relative to the player HQ and graph topology.
Technical: Read by ExecutePlaceEnemyHQ when building candidate lists and selecting by attempt.
Notes: Hard filters; overly strict rules trigger backtracking.)"))
	FEnemyHQPlacementRules M_EnemyHQPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Enemy Items",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: EnemyObjectsPlaced.
Why: Controls spacing and variant selection for enemy item placement.
Technical: Read by ExecutePlaceEnemyObjects and per-item placement logic.
Notes: Mix of hard filters (spacing) and soft variant selection (ordering).)"))
	FEnemyPlacementRules M_EnemyPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Neutral Items",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: NeutralObjectsPlaced.
Why: Controls spacing and selection of neutral items relative to the player HQ and other neutrals.
Technical: Read by ExecutePlaceNeutralObjects for hop-distance filtering and preference biasing.
Notes: Min/Max values are hard bounds; Preference is a soft bias within those bounds.)"))
	FNeutralItemPlacementRules M_NeutralItemPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Missions",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: MissionsPlaced.
Why: Defines mission placement tiers, spacing, and adjacency requirements.
Technical: Read by ExecutePlaceMissions and per-mission selection logic.
Notes: Mix of hard constraints (filters) and preferences (ordering/bias).)"))
	FMissionPlacement M_MissionPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Counts & Difficulty",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: EnemyObjectsPlaced and NeutralObjectsPlaced setup.
Why: Scales base counts by difficulty to tune content density.
Technical: Read before placement to compute required item counts and per-type quotas.
Notes: Determinism depends on Seed and difficulty settings staying fixed.)"))
	FWorldCampaignCountDifficultyTuning M_CountAndDifficultyTuning;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy",
		meta = (AllowPrivateAccess = "true",
		        ToolTip = R"(Used in: Backtracking when a step fails.
Why: Lets designers decide whether to relax rules, retry, or abort per step.
Technical: Read by HandleStepFailure and GetFailurePolicyForStep.
Notes: Affects determinism because different policies change which steps retry.)"))
	FWorldCampaignPlacementFailurePolicy M_PlacementFailurePolicy;
};
