// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/GenerationStep/Enum_CampaignGenerationStep.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyHQPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyWallPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyPlacementRules/EnemyPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/MissionPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/NeutralItemPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/PlayerHQPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignCountDifficultyTuning.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignPlacementFailurePolicy.h"
#include "GeneratorWorldCampaign.generated.h"

class AAnchorPoint;
class AConnection;
class AWorldSplineBoundary;
class UWorldCampaignDebugger;

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

	/**
	 * @note Used in: ConnectionsCreated -> ExecuteCreateConnections/GenerateConnectionsForAnchors.
	 * @note Why: Sets a hard minimum connection degree so anchors are not stranded.
	 * @note Technical: Enforced in desired-connection assignment and validated during connection passes.
	 * @note Notes: Hard constraint; may trigger retries/backtracking if impossible with current anchors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation")
	int32 MinConnections = 1;

	/**
	 * @note Used in: ConnectionsCreated -> ExecuteCreateConnections/GenerateConnectionsForAnchors.
	 * @note Why: Caps graph density so the map retains readable routing and pacing.
	 * @note Technical: Hard upper bound for per-anchor connection counts during generation.
	 * @note Notes: MinConnections must be <= MaxConnections; if too low, generation may under-connect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation")
	int32 MaxConnections = 3;

	/**
	 * @note Used in: ConnectionsCreated -> GeneratePhasePreferredConnections.
	 * @note Why: Defines the preferred spatial radius for first-pass connections so topology stays local.
	 * @note Technical: Acts as a soft preference (distance filter) before extended phases relax it.
	 * @note Notes: If too small, later phases will extend beyond it to satisfy MinConnections.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation")
	float MaxPreferredDistance = 5000.f;
};

USTRUCT(BlueprintType)
struct FAnchorPointGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	bool bM_EnableGeneratedAnchorPoints = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	int32 M_MinNewAnchorPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	int32 M_MaxNewAnchorPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	float M_MinDistanceBetweenAnchorPoints = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	float M_GridCellSize = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	float M_SplineSampleSpacing = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	TSubclassOf<AAnchorPoint> M_GeneratedAnchorPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	bool bM_DebugDrawSplineBoundary = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	bool bM_DebugDrawGeneratedGrid = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	float M_DebugDrawBoundaryDuration = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation")
	float M_DebugDrawBoundaryZOffset = 50.f;
};

USTRUCT(BlueprintType)
struct FWorldCampaignPlacementState
{
	GENERATED_BODY()

	/**
	 * @note Used in: All generation steps.
	 * @note Why: Records the deterministic seed actually used so results can be reproduced.
	 * @note Technical: This seed is fed into FRandomStream and combined with step attempt indices.
	 * @note Notes: Changing it invalidates determinism for retries and debugging.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	int32 SeedUsed = 0;

	/**
	 * @note Used in: ConnectionsCreated and all downstream steps.
	 * @note Why: Caches the anchor set that the generator operated on for inspection and undo.
	 * @note Technical: Filled during CacheGeneratedState; drives placement candidate lookups.
	 * @note Notes: Acts as the authoritative anchor list for this generation attempt.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TArray<TObjectPtr<AAnchorPoint>> CachedAnchors;

	/**
	 * @note Used in: ConnectionsCreated and all downstream steps.
	 * @note Why: Caches spawned connections so later steps can derive hops and undo reliably.
	 * @note Technical: Populated during connection generation and referenced for hop distance maps.
	 * @note Notes: Serves as the rollback source when backtracking connections.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TArray<TObjectPtr<AConnection>> CachedConnections;

	/**
	 * @note Used in: PlayerHQPlaced and all subsequent steps.
	 * @note Why: Stores the selected player HQ anchor so other placement rules can reference it.
	 * @note Technical: Set by ExecutePlaceHQ after filtering candidates and selecting by attempt.
	 * @note Notes: Hard prerequisite for enemy/neutral/mission placement filters.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TObjectPtr<AAnchorPoint> PlayerHQAnchor;

	/**
	 * @note Used in: Cached lookup keys and undo transactions.
	 * @note Why: Provides a stable identifier for the player HQ anchor across retries.
	 * @note Technical: Cached from the anchor's GUID; used for hop distance maps and maps keyed by anchor.
	 * @note Notes: Required for determinism even if anchor pointers change during regeneration.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	FGuid PlayerHQAnchorKey;

	/**
	 * @note Used in: EnemyHQPlaced and all subsequent steps.
	 * @note Why: Stores the selected enemy HQ anchor to drive enemy spacing rules.
	 * @note Technical: Set by ExecutePlaceEnemyHQ after filtering candidates and selection.
	 * @note Notes: Downstream enemy placement uses its hop distances as hard filters.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TObjectPtr<AAnchorPoint> EnemyHQAnchor;

	/**
	 * @note Used in: Cached lookup keys and undo transactions.
	 * @note Why: Provides a stable identifier for the enemy HQ anchor across retries.
	 * @note Technical: Cached from the anchor GUID; used for hop distance maps and anchor-keyed maps.
	 * @note Notes: Keeps deterministic keying even if anchors are rebuilt.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	FGuid EnemyHQAnchorKey;

	/**
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Records which enemy item type was placed at each anchor for later filtering and undo.
	 * @note Technical: Written during enemy placement, keyed by anchor GUID for deterministic lookup.
	 * @note Notes: Acts as a hard occupancy map; a key present means the anchor is consumed.
	 * @note Used in: EnemyWallPlaced.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, EMapEnemyItem> EnemyItemsByAnchorKey;

	/**
	 * @note Used in: NeutralObjectsPlaced.
	 * @note Why: Records which neutral object type was placed at each anchor for spacing checks and undo.
	 * @note Technical: Filled during neutral placement; queried when validating mission placement adjacency.
	 * @note Notes: Hard occupancy map; used as a filter during mission requirements.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, EMapNeutralObjectType> NeutralItemsByAnchorKey;

	/**
	 * @note Used in: MissionsPlaced.
	 * @note Why: Records the mission type placed at each anchor to prevent duplicates and support undo.
	 * @note Technical: Populated during mission placement; keyed by anchor GUID for deterministic access.
	 * @note Notes: Hard occupancy map for mission anchors; used for validation and backtracking.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, EMapMission> MissionsByAnchorKey;
};

USTRUCT(BlueprintType)
struct FWorldCampaignDerivedData
{
	GENERATED_BODY()

	/**
	 * @note Used in: PlayerHQPlaced, EnemyObjectsPlaced, NeutralObjectsPlaced, MissionsPlaced.
	 * @note Why: Stores hop distances from the player HQ to every anchor as a reusable cache.
	 * @note Technical: Built after connections are generated; reused for hop-based filtering.
	 * @note Notes: Deterministic given seed and connection graph; treated as authoritative distances.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> PlayerHQHopDistancesByAnchorKey;

	/**
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Caches hop distances from the enemy HQ for enemy spacing rules.
	 * @note Technical: Built after EnemyHQPlaced; used for Min/Max hop filters.
	 * @note Notes: Hard filter source; if missing or empty, enemy placement can fail.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> EnemyHQHopDistancesByAnchorKey;

	/**
	 * @note Used in: PlayerHQPlaced, EnemyHQPlaced, MissionsPlaced.
	 * @note Why: Stores connection degree per anchor for degree-based filters.
	 * @note Technical: Cached via CacheAnchorConnectionDegrees and reused by selection logic.
	 * @note Notes: Hard constraint when Min/Max connection degree rules are enabled.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> AnchorConnectionDegreesByAnchorKey;

	/**
	 * @note Used in: ConnectionsCreated debug/analysis.
	 * @note Why: Preserves chokepoint scoring so designers can inspect connectivity bottlenecks.
	 * @note Technical: Derived data keyed by anchor GUID, typically computed after connections exist.
	 * @note Notes: Informational/diagnostic; does not directly gate placement.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, float> ChokepointScoresByAnchorKey;

	/**
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Tracks how many enemy items of each type have been placed.
	 * @note Technical: Updated as items are spawned; compared against required counts.
	 * @note Notes: Hard constraint; when counts are met, additional placement is skipped.
	 * @note Used in: EnemyWallPlaced.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapEnemyItem, int32> EnemyItemPlacedCounts;

	/**
	 * @note Used in: NeutralObjectsPlaced.
	 * @note Why: Tracks how many neutral items of each type have been placed.
	 * @note Technical: Updated during neutral placement and used to stop once required counts are reached.
	 * @note Notes: Hard constraint; affects retry logic if counts cannot be satisfied.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapNeutralObjectType, int32> NeutralItemPlacedCounts;

	/**
	 * @note Used in: MissionsPlaced.
	 * @note Why: Tracks how many missions of each type have been placed.
	 * @note Technical: Updated during mission placement and checked against required mission counts.
	 * @note Notes: Hard constraint; unmet counts can trigger backtracking.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapMission, int32> MissionPlacedCounts;
};

USTRUCT(BlueprintType)
struct FCampaignGenerationStepTransaction
{
	GENERATED_BODY()

	/**
	 * @note Used in: Backtracking/Undo for every generation step.
	 * @note Why: Identifies which step completed to decide how to roll back state.
	 * @note Technical: Set in ExecuteStepWithTransaction when a step succeeds.
	 * @note Notes: Determinism relies on the same step order when replayed.
	 */
	UPROPERTY()
	ECampaignGenerationStep CompletedStep = ECampaignGenerationStep::NotStarted;

	/**
	 * @note Used in: Undo of placement steps.
	 * @note Why: Records spawned actors so a failed step can be fully rolled back.
	 * @note Technical: Populated by each step's Execute* function; destroyed in UndoLastTransaction.
	 * @note Notes: Hard rollback data; includes any spawn that must be removed.
	 */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	/**
	 * @note Used in: Undo for ConnectionsCreated.
	 * @note Why: Tracks connection actors spawned during a step for deterministic rollback.
	 * @note Technical: Stored in the transaction and consumed by UndoConnections.
	 * @note Notes: Hard rollback data; must include all connections created in the step.
	 */
	UPROPERTY()
	TArray<TObjectPtr<AConnection>> SpawnedConnections;

	/**
	 * @note Used in: Undo for any step.
	 * @note Why: Snapshot of placement state before the step so rollback restores maps and anchors.
	 * @note Technical: Copied in ExecuteStepWithTransaction prior to mutation.
	 * @note Notes: Restores deterministic state for retries.
	 */
	UPROPERTY()
	FWorldCampaignPlacementState PreviousPlacementState;

	/**
	 * @note Used in: Undo for any step.
	 * @note Why: Snapshot of derived data before the step for consistent rollback.
	 * @note Technical: Copied in ExecuteStepWithTransaction prior to mutation.
	 * @note Notes: Keeps hop/degree caches aligned with placement state during backtracking.
	 */
	UPROPERTY()
	FWorldCampaignDerivedData PreviousDerivedData;

	/**
	 * @note Used in: EnemyObjectsPlaced and MissionsPlaced micro transactions.
	 * @note Why: Indicates this transaction represents a single item placement.
	 * @note Technical: Checked by backtracking to undo per-item placements.
	 * @note Notes: Macro steps keep this false.
	 */
	UPROPERTY()
	bool bIsMicroTransaction = false;

	/**
	 * @note Used in: Micro transaction debugging and rollback.
	 * @note Why: Tracks which macro step owns this micro placement.
	 * @note Technical: Set for micro transactions only.
	 * @note Notes: Defaults to NotStarted when unused.
	 */
	UPROPERTY()
	ECampaignGenerationStep MicroParentStep = ECampaignGenerationStep::NotStarted;

	/**
	 * @note Used in: Undo for micro placements.
	 * @note Why: Identifies the anchor that was promoted in this micro step.
	 * @note Technical: Used to avoid clearing promotions on unrelated anchors.
	 * @note Notes: Invalid for non-micro transactions.
	 */
	UPROPERTY()
	FGuid MicroAnchorKey;

	/**
	 * @note Used in: Debugging micro placements.
	 * @note Why: Labels the type of item placed during this micro transaction.
	 * @note Technical: Set for micro transactions only.
	 * @note Notes: Defaults to None when unused.
	 */
	UPROPERTY()
	EMapItemType MicroItemType = EMapItemType::None;

	/**
	 * @note Used in: Debug placement reporting for EnemyObjectsPlaced micro transactions.
	 * @note Why: Tracks which enemy type was placed so undo can attribute removal pressure.
	 * @note Technical: Set only when MicroItemType is EnemyItem.
	 * @note Notes: Defaults to None when unused.
	 */
	UPROPERTY()
	EMapEnemyItem MicroEnemyItemType = EMapEnemyItem::None;

	/**
	 * @note Used in: Debug placement reporting for MissionsPlaced micro transactions.
	 * @note Why: Tracks which mission type was placed so undo can attribute removal pressure.
	 * @note Technical: Set only when MicroItemType is Mission.
	 * @note Notes: Defaults to None when unused.
	 */
	UPROPERTY()
	EMapMission MicroMissionType = EMapMission::None;

	/**
	 * @note Used in: Debugging and deterministic replay.
	 * @note Why: Records the zero-based placement index within the macro plan.
	 * @note Technical: Set for micro transactions only.
	 * @note Notes: INDEX_NONE when unused.
	 */
	UPROPERTY()
	int32 MicroIndexWithinParent = INDEX_NONE;

	/**
	 * @note Used in: Mission micro placements with companion neutrals.
	 * @note Why: Ensures companion anchor promotions are rolled back together.
	 * @note Technical: Filled only when companion anchors differ from the primary.
	 * @note Notes: Empty for most transactions.
	 */
	UPROPERTY()
	TArray<FGuid> MicroAdditionalAnchorKeys;
};

// Must be in Ustruct for GC tracking.
USTRUCT()
struct FConnectionSegment
{
	GENERATED_BODY()

	FVector2D StartPoint;
	FVector2D EndPoint;
	/**
	 * @note Used in: Connection intersection checks during ConnectionsCreated.
	 * @note Why: Ties this segment back to the owning anchor for shared-endpoint logic.
	 * @note Technical: Weak pointer to avoid GC issues; validated before use.
	 * @note Notes: Internal debug/validation data only; not a placement rule.
	 */
	UPROPERTY()
	TWeakObjectPtr<AAnchorPoint> StartAnchor;
	/**
	 * @note Used in: Connection intersection checks during ConnectionsCreated.
	 * @note Why: Ties this segment back to the second endpoint anchor for intersection filters.
	 * @note Technical: Weak pointer to avoid GC ownership; validated before use.
	 * @note Notes: Internal data; ignored if invalid.
	 */
	UPROPERTY()
	TWeakObjectPtr<AAnchorPoint> EndAnchor;
	/**
	 * @note Used in: Connection intersection checks during ConnectionsCreated.
	 * @note Why: Allows ignoring segments that belong to a specific connection during checks.
	 * @note Technical: Weak pointer so destroyed connections do not keep references alive.
	 * @note Notes: Internal data for generation safety; not exposed as tuning.
	 */
	UPROPERTY()
	TWeakObjectPtr<AConnection> OwningConnection;
};

/**
 * @brief Generator actor that can be placed on a campaign map to spawn and configure world connections.
 */
UCLASS()
class RTS_SURVIVAL_API AGeneratorWorldCampaign : public AActor
{
	GENERATED_BODY()

	friend class UWorldCampaignDebugger;

public:
	AGeneratorWorldCampaign();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, show connection degree details where relevant. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Debug Anchor Degree"))
	bool bM_DebugAnchorDegree = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, include player HQ hop distances in debug strings where applicable. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Debug Player HQ Hops"))
	bool bM_DebugPlayerHQHops = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, include enemy HQ hop distances in enemy placement debug strings. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Debug Enemy HQ Hops"))
	bool bM_DebugEnemyHQHops = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, show variant selection info for enemy placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Variation Enemy Object Placement"))
	bool bM_DisplayVariationEnemyObjectPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, show hop distance to nearest same enemy type after placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Hops From Same Enemy Items"))
	bool bM_DisplayHopsFromSameEnemyItems = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, show hop distance to other neutral items. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Hops From Other Neutral Items"))
	bool bM_DisplayHopsFromOtherNeutralItems = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, display mission placement failure reasons. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Debug Failed Mission Placement"))
	bool bM_DebugFailedMissionPlacement = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, display mission candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Debug Mission Candidate Rejections"))
	bool bM_DebugMissionCandidateRejections = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, display enemy candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Debug Enemy Candidate Rejections"))
	bool bM_DebugEnemyCandidateRejections = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, display neutral candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Debug Neutral Candidate Rejections"))
	bool bM_DebugNeutralCandidateRejections = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, include hop distance from HQ in mission placement debug. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Hops From HQ For Missions"))
	bool bM_DisplayHopsFromHQForMissions = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, include mission spacing hop distances when spacing rules are active. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Debug Mission Spacing Hops"))
	bool bM_DebugMissionSpacingHops = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, show min/max connection requirements for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Min Max Connections For Mission Placement"))
	bool bM_DisplayMinMaxConnectionsForMissionPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, show adjacency requirement summaries for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Mission Adjacency Requirements"))
	bool bM_DisplayMissionAdjacencyRequirements = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "When enabled, show required neutral item type for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Neutral Item Requirement For Mission"))
	bool bM_DisplayNeutralItemRequirementForMission = true;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// NOTE: EditCondition disables CallInEditor clicks; avoid relying on it alone if you want runtime error logs.
	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (DisplayPriority = 1))
	void EraseAllGeneration();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::NotStarted", DisplayPriority = 2))
	void GenerateAnchorPointsStep();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::AnchorPointsGenerated",
		        DisplayPriority = 3))
	void CreateConnectionsStep();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::ConnectionsCreated",
		        DisplayPriority = 4))
	void PlaceHQStep();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::PlayerHQPlaced", DisplayPriority = 5))
	void PlaceEnemyHQStep();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::EnemyHQPlaced", DisplayPriority = 6))
	void PlaceEnemyWallStep();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::EnemyWallPlaced", DisplayPriority = 7))
	void PlaceEnemyObjectsStep();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::EnemyObjectsPlaced", DisplayPriority = 8))
	void PlaceNeutralObjectsStep();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::NeutralObjectsPlaced",
		        DisplayPriority = 9))
	void PlaceMissionsStep();

	UFUNCTION(CallInEditor, Category = "00 - World Campaign|Generation",
		meta = (DisplayPriority = 10))
	void ExecuteAllSteps();

	UFUNCTION(CallInEditor, Category = "01 - World Campaign|Debugging",
		meta = (DisplayPriority = 1))
	void DebugDrawAllConnections() const;

	UFUNCTION(CallInEditor, Category = "01 - World Campaign|Debugging",
		meta = (DisplayPriority = 2))
	void DebugDrawPlayerHQHops() const;

	UFUNCTION(CallInEditor, Category = "01 - World Campaign|Debugging",
		meta = (DisplayPriority = 3))
	void DebugDrawEnemyHQHops() const;

	UFUNCTION(CallInEditor, Category = "01 - World Campaign|Debugging",
		meta = (DisplayPriority = 4))
	void DebugDrawSplineBoundaryArea();

	UFUNCTION(CallInEditor, Category = "01 - World Campaign|Debugging",
		meta = (DisplayPriority = 5))
	void ShowPlacementReport();

	int32 GetAnchorConnectionDegree(const AAnchorPoint* AnchorPoint) const;
	bool GetIsValidCampaignDebugger() const;
	UWorldCampaignDebugger* GetCampaignDebugger() const;
	bool IsAnchorCached(const AAnchorPoint* AnchorPoint) const;
	
	/**
	 * @brief Builds a deterministic key so generated anchors stay stable across retries.
	 * @param StepAttemptIndex Attempt index for the generated-anchor step.
	 * @param CellIndex Grid cell index used for placement.
	 * @param SpawnOrdinal Ordinal of the spawn within the step.
	 * @return Deterministic anchor key for generated anchors.
	 */
	FGuid BuildGeneratedAnchorKey_Deterministic(int32 StepAttemptIndex, int32 CellIndex, int32 SpawnOrdinal) const;

private:
	void ApplyDebuggerSettingsToComponent();
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
	/**
	 * @brief Executes steps that push per-item micro transactions without adding a macro transaction.
	 * @param CompletedStep Step state that will be marked as completed on success.
	 * @param StepFunction Function that performs micro-placement and pushes its own transactions.
	 * @return true if the step executed successfully.
	 */
	bool ExecuteStepWithMicroTransactions(ECampaignGenerationStep CompletedStep,
	                                      bool (AGeneratorWorldCampaign::*StepFunction)(
		                                      FCampaignGenerationStepTransaction&));
	bool ExecuteGenerateAnchorPoints(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecuteCreateConnections(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceHQ(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyHQ(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyWall(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyObjects(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceNeutralObjects(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceMissions(FCampaignGenerationStepTransaction& OutTransaction);
	/**
	 * @brief Places a single enemy item so backtracking can undo one object at a time.
	 * @param EnemyTypeToPlace Type of enemy item to place.
	 * @param PlacementOrdinalWithinType Index of this type among already placed items.
	 * @param MicroIndexWithinParent Index within the overall placement plan.
	 * @param OutMicroTransaction Transaction storing pre-placement state and spawned actor.
	 * @return true if the item placed and spawned successfully.
	 */
	bool ExecutePlaceSingleEnemyObject(EMapEnemyItem EnemyTypeToPlace, int32 PlacementOrdinalWithinType,
	                                   int32 MicroIndexWithinParent,
	                                   FCampaignGenerationStepTransaction& OutMicroTransaction);
	/**
	 * @brief Places a single mission so backtracking can undo one mission at a time.
	 * @param MissionTypeToPlace Mission type to place.
	 * @param MicroIndexWithinParent Index within the overall placement plan.
	 * @param OutMicroTransaction Transaction storing pre-placement state and spawned actors.
	 * @return true if the mission and any companion placement succeeded.
	 */
	bool ExecutePlaceSingleMission(EMapMission MissionTypeToPlace, int32 MicroIndexWithinParent,
	                               FCampaignGenerationStepTransaction& OutMicroTransaction);
	bool ValidateEnemyObjectPlacementPrerequisites() const;
	bool ValidateMissionPlacementPrerequisites() const;
	bool GetIsMissionExcludedFromPlacement(EMapMission MissionType) const;
	TArray<EMapMission> BuildMissionPlacementPlanFiltered() const;
	void RollbackMicroPlacementAndDestroyActors(FCampaignGenerationStepTransaction& InOutTransaction);
	/**
	 * @brief Selects one enemy placement while preserving deterministic ordering for micro steps.
	 * @param EnemyTypeToPlace Enemy item type to place.
	 * @param WorkingPlacementState Working placement state to mutate on success.
	 * @param WorkingDerivedData Working derived data to mutate on success.
	 * @param OutPromotion Selected anchor and enemy type pair.
	 * @return true if a placement candidate was selected.
	 */
	bool TrySelectSingleEnemyPlacement(EMapEnemyItem EnemyTypeToPlace,
	                                   FWorldCampaignPlacementState& WorkingPlacementState,
	                                   FWorldCampaignDerivedData& WorkingDerivedData,
	                                   TPair<TObjectPtr<AAnchorPoint>, EMapEnemyItem>& OutPromotion);
	/**
	 * @brief Selects one mission placement while preserving deterministic ordering for micro steps.
	 * @param MissionTypeToPlace Mission type to place.
	 * @param MicroIndexWithinParent Index within the mission placement plan.
	 * @param WorkingPlacementState Working placement state to mutate on success.
	 * @param WorkingDerivedData Working derived data to mutate on success.
	 * @param OutPromotion Selected anchor and mission type pair.
	 * @param OutCompanionPromotions Companion neutral promotions when required.
	 * @return true if a placement candidate was selected.
	 */
	bool TrySelectSingleMissionPlacement(EMapMission MissionTypeToPlace, int32 MicroIndexWithinParent,
	                                     FWorldCampaignPlacementState& WorkingPlacementState,
	                                     FWorldCampaignDerivedData& WorkingDerivedData,
	                                     TPair<TObjectPtr<AAnchorPoint>, EMapMission>& OutPromotion,
	                                     TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>&
	                                     OutCompanionPromotions);
	/**
	 * @brief Finalizes mission micro placement by spawning objects and recording anchors.
	 * @param Promotion Selected mission anchor and type.
	 * @param CompanionPromotions Companion neutral placements tied to the mission.
	 * @param OutMicroTransaction Transaction to populate with spawned actors and anchor keys.
	 * @return true if all mission-related spawns succeeded.
	 */
	bool TryFinalizeMissionMicroPlacement(const TPair<TObjectPtr<AAnchorPoint>, EMapMission>& Promotion,
	                                      const TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>&
	                                      CompanionPromotions,
	                                      FCampaignGenerationStepTransaction& OutMicroTransaction);

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
	int32 CountTrailingMicroTransactionsForStep(ECampaignGenerationStep Step) const;
	bool IsStepEarlierThan(ECampaignGenerationStep StepToCheck, ECampaignGenerationStep ReferenceStep) const;
	int32 CountMicroTransactionsForStepSinceLastInvalidation(ECampaignGenerationStep MacroStep) const;
	void UpdateMicroPlacementProgressFromTransactions();
	void ClearPlacementState();
	void ClearDerivedData();
	void CacheAnchorConnectionDegrees();
	void BuildChokepointScoresCache(const AAnchorPoint* OptionalHQAnchor);
	void DebugNotifyAnchorPicked(AAnchorPoint* AnchorPoint, const FString& Label, const FColor& Color) const;

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

	void DebugNotifyAnchorProcessing(AAnchorPoint* AnchorPoint, const FString& Label, const FColor& Color) const;
	void DebugDrawConnection(const AConnection* Connection, const FColor& Color) const;
	void DebugDrawThreeWay(const AConnection* Connection, const FColor& Color) const;
	void DrawDebugConnectionLine(UWorld* World, const FVector& Start, const FVector& End, const FColor& Color) const;
	void DrawDebugConnectionForActor(UWorld* World, const AConnection* Connection, const FVector& HeightOffset,
	                                 const FColor& BaseConnectionColor, const FColor& ThreeWayConnectionColor) const;

	/**
	 * @note Used in: Editor button gating for step execution.
	 * @note Why: Enforces the ordered generation pipeline (AnchorPointsGenerated -> ... -> MissionsPlaced).
	 * @note Technical: Each CallInEditor step checks this enum via EditCondition to enable buttons.
	 * @note Notes: Changing it manually can desync cached data; intended for controlled progression.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	ECampaignGenerationStep M_GenerationStep = ECampaignGenerationStep::NotStarted;

	/**
	 * @note Used in: All placement steps and undo.
	 * @note Why: Exposes the current placement state for debugging and verification.
	 * @note Technical: Mutated by Execute* functions; restored from transactions on backtrack.
	 * @note Notes: Visible-only snapshot; editing is not supported.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	FWorldCampaignPlacementState M_PlacementState;

	/**
	 * @note Used in: Debugging and placement filters.
	 * @note Why: Shows derived caches like hop distances and connection degrees.
	 * @note Technical: Updated during cache steps; read by placement logic for filtering.
	 * @note Notes: Derived data; regenerated when connections change.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	FWorldCampaignDerivedData M_DerivedData;

	/**
	 * @note Used in: Backtracking system.
	 * @note Why: Stores a stack of step transactions for undo and retries.
	 * @note Technical: Each successful step pushes a transaction; failures pop and restore.
	 * @note Notes: Determinism depends on consistent transaction order.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	TArray<FCampaignGenerationStepTransaction> M_StepTransactions;

	/**
	 * @note Used in: Backtracking and retry determinism.
	 * @note Why: Tracks how many times each step has been attempted to vary the random stream.
	 * @note Technical: Incremented per step failure; used to seed deterministic retries.
	 * @note Notes: Changing this breaks reproducibility of attempt-based selection.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	TMap<ECampaignGenerationStep, int32> M_StepAttemptIndices;

	/**
	 * @note Used in: EnemyObjectsPlaced micro placement.
	 * @note Why: Tracks how many enemy items are placed within the current macro step.
	 * @note Technical: Recomputed from micro transactions after undo or reset.
	 * @note Notes: Not persisted across completed macro steps.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	int32 M_EnemyMicroPlacedCount = 0;

	/**
	 * @note Used in: MissionsPlaced micro placement.
	 * @note Why: Tracks how many missions are placed within the current macro step.
	 * @note Technical: Recomputed from micro transactions after undo or reset.
	 * @note Notes: Not persisted across completed macro steps.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	int32 M_MissionMicroPlacedCount = 0;

	/**
	 * @note Used in: Backtracking guardrails.
	 * @note Why: Prevents infinite retry loops across all steps.
	 * @note Technical: Incremented every attempt; compared against MaxTotalAttempts.
	 * @note Notes: Hard fail-safe; exceeding it aborts generation.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	int32 M_TotalAttemptCount = 0;

	/**
	 * @note Used in: ConnectionsCreated.
	 * @note Why: Centralizes tuning for connection creation phases.
	 * @note Technical: Read by ExecuteCreateConnections and helper functions when building the graph.
	 * @note Notes: Changing these requires regenerating connections to take effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation",
		meta = (AllowPrivateAccess = "true"))
	FConnectionGenerationRules ConnectionGenerationRules;

	/**
	 * @note Used in: AnchorPointsGenerated.
	 * @note Why: Controls how anchor points are generated inside the spline boundary.
	 * @note Technical: Read by ExecuteGenerateAnchorPoints for deterministic placement.
	 * @note Notes: Changes take effect on the next anchor generation run.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation",
		meta = (AllowPrivateAccess = "true"))
	FAnchorPointGenerationSettings M_AnchorPointGenerationSettings;

	/**
	 * @note Used in: ConnectionsCreated.
	 * @note Why: Defines which connection actor class is spawned so visuals/behavior can be customized.
	 * @note Technical: Spawned per segment; must be a valid AConnection subclass.
	 * @note Notes: If null or invalid, connection generation will fail.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation",
		meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AConnection> M_ConnectionClass;

	/**
	 * @note Used in: Debugging and backtracking.
	 * @note Why: Tracks the live connection actors generated in the current run.
	 * @note Technical: Populated by ExecuteCreateConnections; used for later debug draws.
	 * @note Notes: Cleared when generation is erased or rolled back.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation",
		meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AConnection>> M_GeneratedConnections;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "Base Z offset for debug text drawn at anchors. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Default Debug Height Offset"))
	float M_DefaultDebugHeightOffset = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "Additional Z offset per stacked message while previous debug text is still displaying. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Added Height If Still Displaying"))
	float M_AddedHeightIfStillDisplaying = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "Display time for rejected placement debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Time Rejected Location"))
	float M_DisplayTimeRejectedLocation = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "Display time for accepted placement debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Display Time Accepted Location"))
	float M_DisplayTimeAcceptedLocation = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "Maximum total rejection debug draws per placement attempt. Use <= 0 for unlimited. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Max Rejection Draws Per Attempt"))
	int32 M_MaxRejectionDrawsPerAttempt = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
		        ToolTip = "Maximum rejection debug draws per reason per placement attempt. Use <= 0 for unlimited. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols.",
		        DisplayName = "Max Rejection Draws Per Reason"))
	int32 M_MaxRejectionDrawsPerReason = 25;



	/**
	 * @note Used in: DebugDrawAllConnections.
	 * @note Why: Keeps debug lines visible long enough for inspection without pausing the editor.
	 * @note Technical: Passed into DrawDebugLine as Duration for every segment.
	 * @note Notes: Does not affect generation; purely debug visualization.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2))
	float M_DebugConnectionDrawDurationSeconds = WorldCampaignDebugDefaults::ConnectionDrawDurationSeconds;

	/**
	 * @note Used in: DebugDrawAllConnections.
	 * @note Why: Makes debug lines readable over dense map art or UI overlays.
	 * @note Technical: Passed into DrawDebugLine as line thickness.
	 * @note Notes: This is visual-only; does not affect collision or selection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2))
	float M_DebugConnectionLineThickness = WorldCampaignDebugDefaults::ConnectionLineThickness;

	/**
	 * @note Used in: DebugDrawAllConnections.
	 * @note Why: Offsets debug lines above the map so they are not Z-fighting with geometry.
	 * @note Technical: Added to the Z of anchor and junction locations before drawing.
	 * @note Notes: Visual-only; does not affect any placement logic.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2))
	float M_DebugConnectionDrawHeightOffset = WorldCampaignDebugDefaults::ConnectionDrawHeightOffset;

	/**
	 * @note Used in: PlayerHQPlaced.
	 * @note Why: Defines player HQ eligibility and safety constraints for the start of the campaign.
	 * @note Technical: Read by ExecutePlaceHQ and BuildHQAnchorCandidates.
	 * @note Notes: Hard filters; if too strict, HQ placement will fail and trigger backtracking.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Player HQ",
		meta = (AllowPrivateAccess = "true"))
	FPlayerHQPlacementRules M_PlayerHQPlacementRules;

	/**
	 * @note Used in: EnemyHQPlaced.
	 * @note Why: Defines where the enemy HQ may appear relative to the player HQ and graph topology.
	 * @note Technical: Read by ExecutePlaceEnemyHQ when building candidate lists and selecting by attempt.
	 * @note Notes: Hard filters; overly strict rules trigger backtracking.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Enemy HQ",
		meta = (AllowPrivateAccess = "true"))
	FEnemyHQPlacementRules M_EnemyHQPlacementRules;

	/**
	 * @note Used in: EnemyWallPlaced.
	 * @note Why: Defines the anchor candidates and preference for placing the enemy wall.
	 * @note Technical: Read by ExecutePlaceEnemyWall when selecting the wall anchor.
	 * @note Notes: Only candidates in this list are considered; empty list triggers failure.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Enemy Wall",
		meta = (AllowPrivateAccess = "true"))
	FEnemyWallPlacementRules M_EnemyWallPlacementRules;

	/**
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Controls spacing and variant selection for enemy item placement.
	 * @note Technical: Read by ExecutePlaceEnemyObjects and per-item placement logic.
	 * @note Notes: Mix of hard filters (spacing) and soft variant selection (ordering).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Enemy Items",
		meta = (AllowPrivateAccess = "true"))
	FEnemyPlacementRules M_EnemyPlacementRules;

	/**
	 * @note Used in: NeutralObjectsPlaced.
	 * @note Why: Controls spacing and selection of neutral items relative to the player HQ and other neutrals.
	 * @note Technical: Read by ExecutePlaceNeutralObjects for hop-distance filtering and preference biasing.
	 * @note Notes: Min/Max values are hard bounds; Preference is a soft bias within those bounds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Neutral Items",
		meta = (AllowPrivateAccess = "true"))
	FNeutralItemPlacementRules M_NeutralItemPlacementRules;

	/**
	 * @note Used in: MissionsPlaced.
	 * @note Why: Defines mission placement tiers, spacing, and adjacency requirements.
	 * @note Technical: Read by ExecutePlaceMissions and per-mission selection logic.
	 * @note Notes: Mix of hard constraints (filters) and preferences (ordering/bias).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Missions",
		meta = (AllowPrivateAccess = "true"))
	FMissionPlacement M_MissionPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Missions",
		meta = (AllowPrivateAccess = "true"))
	TArray<EMapMission> M_ExcludedMissionsFromMapPlacement;

	/**
	 * @note Used in: EnemyObjectsPlaced and NeutralObjectsPlaced setup.
	 * @note Why: Scales base counts by difficulty to tune content density.
	 * @note Technical: Read before placement to compute required item counts and per-type quotas.
	 * @note Notes: Determinism depends on Seed and difficulty settings staying fixed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Counts & Difficulty",
		meta = (AllowPrivateAccess = "true"))
	FWorldCampaignCountDifficultyTuning M_CountAndDifficultyTuning;

	/**
	 * @note Used in: Backtracking when a step fails.
	 * @note Why: Lets designers decide whether to relax rules, retry, or abort per step.
	 * @note Technical: Read by HandleStepFailure and GetFailurePolicyForStep.
	 * @note Notes: Affects determinism because different policies change which steps retry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy",
		meta = (AllowPrivateAccess = "true"))
	FWorldCampaignPlacementFailurePolicy M_PlacementFailurePolicy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldCampaignDebugger> M_WorldCampaignDebugger;

	// Tracks when undo operations are attributed to backtracking failures for reporting.
	bool bM_Report_UndoContextActive = false;
	ECampaignGenerationStep M_Report_CurrentFailureStep = ECampaignGenerationStep::NotStarted;
};
