// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
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
#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignPlacementFailurePolicy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/WorldCampaignAsyncPlacement.h"
#include "GeneratorWorldCampaign.generated.h"

class AWorldPlayerController;
class AAnchorPoint;
class AConnection;
class AWorldSplineBoundary;
class UWorldCampaignDebugger;
class UWorldDataComponent;
class UWorldCountryOccupationRegulator;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Anchor Point Generation",
		meta = (ClampMin = "1", ClampMax = "32",
			ToolTip = "Number of randomized jitter samples attempted per grid cell when trying to place anchors."))
	int32 M_JitterAttemptsPerCell = 4;

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

struct FEmptyAnchorDistance
{
	TObjectPtr<AAnchorPoint> AnchorPoint = nullptr;
	FGuid AnchorKey;
	float DistanceSquared = 0.f;
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
	 * @note Used in: Undo for any sGtep.
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

/**
 * @brief Categorizes timeout fail-safe items without relying on enum-specific containers.
 */
enum class EFailSafeItemKind : uint8
{
	Enemy = 0,
	Neutral = 1,
	Mission = 2
};

/**
 * @brief Plain-data item descriptor used by timeout fail-safe placement.
 * @note Keeps sorting and spacing generic across enemy, neutral, and mission fallbacks.
 */
struct FFailSafeItem
{
	EFailSafeItemKind Kind = EFailSafeItemKind::Enemy;
	uint8 RawEnumValue = 0;
	float MinDistance = 0.f;
	EMapEnemyItem EnemyType = EMapEnemyItem::None;
	EMapNeutralObjectType NeutralType = EMapNeutralObjectType::None;
	EMapMission MissionType = EMapMission::None;
};

/**
 * @brief Tracks timeout fail-safe placement totals for notification and diagnostics.
 */
struct FFailSafePlacementTotals
{
	int32 EnemyPlaced = 0;
	int32 NeutralPlaced = 0;
	int32 MissionPlaced = 0;
	int32 Discarded = 0;
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

using FWorldCampaignPlayerHQPlacementRulesSnapshot =
WorldCampaignAsyncPlacement::FPlayerHQPlacementRulesSnapshot;
using FWorldCampaignEnemyHQPlacementRulesSnapshot =
WorldCampaignAsyncPlacement::FEnemyHQPlacementRulesSnapshot;
using FWorldCampaignEnemyWallPlacementRulesSnapshot =
WorldCampaignAsyncPlacement::FEnemyWallPlacementRulesSnapshot;
using FWorldCampaignPerMissionRulesSnapshot =
WorldCampaignAsyncPlacement::FPerMissionRulesSnapshot;
using FWorldCampaignMissionPlacementSnapshot =
WorldCampaignAsyncPlacement::FMissionPlacementSnapshot;
using FWorldCampaignAnchorSnapshot = WorldCampaignAsyncPlacement::FAnchorSnapshot;
using FWorldCampaignConnectionSnapshot = WorldCampaignAsyncPlacement::FConnectionSnapshot;
using FWorldCampaignPlacementSnapshot = WorldCampaignAsyncPlacement::FPlacementSnapshot;
using FWorldCampaignPlacementDebugEvent = WorldCampaignAsyncPlacement::FPlacementDebugEvent;
using FWorldCampaignPlacementResult = WorldCampaignAsyncPlacement::FPlacementResult;
using FWorldCampaignAsyncPlacementProgress = WorldCampaignAsyncPlacement::FPlacementProgress;

// Captures generator state that the parity validator temporarily mutates while comparing solvers.
struct FWorldCampaignPlacementParitySavedState
{
	FWorldCampaignPlacementState PlacementState;
	FWorldCampaignDerivedData DerivedData;
	TArray<FCampaignGenerationStepTransaction> StepTransactions;
	TMap<ECampaignGenerationStep, int32> StepAttemptIndices;
	int32 TotalAttemptCount = 0;
	int32 EnemyMicroPlacedCount = 0;
	int32 MissionMicroPlacedCount = 0;
	ECampaignGenerationStep GenerationStep = ECampaignGenerationStep::NotStarted;
	ECampaignGenerationStep BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;
	bool bDeferWorldObjectPromotionDuringBacktracking = false;
	bool bReportUndoContextActive = false;
	ECampaignGenerationStep ReportCurrentFailureStep = ECampaignGenerationStep::NotStarted;
};

DECLARE_MULTICAST_DELEGATE(FWorldCampaignGenerationFinishedDelegate);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeWorldGenerator(
		AWorldPlayerController* WorldPlayerController,
		FCampaignGenerationSettings CampaignGenerationSettings, FRTSGameDifficulty DifficultySettings);

	/**
	 * @brief Starts campaign generation after InitializeWorldGenerator has supplied runtime settings.
	 */
	void StartWorldGeneration();

	/**
	 * @brief Validates parity without world promotion so seed-range checks can run unattended.
	 * @param FirstSeed First deterministic seed to validate.
	 * @param SeedCount Number of sequential seeds to validate.
	 * @param OutFailureReason First mismatch or setup failure if validation fails.
	 * @return true when every generated setup produced identical sync and async placement data.
	 */
	bool ValidateAsyncPlacementParityForSeedRange(int32 FirstSeed, int32 SeedCount, FString& OutFailureReason);

	/**
	 * @brief Generates one campaign, prunes it, and verifies only reachable gameplay anchors remain.
	 * @param Seed Seed used for the validation generation.
	 * @param OutFailureReason First pruning invariant failure if validation fails.
	 * @return true when pruning leaves no empty anchors and all kept anchors are reachable from Player HQ.
	 */
	bool GenerateAndValidatePrunedWorldForSeed(int32 Seed, FString& OutFailureReason);

	/**
	 * @brief Validates the currently pruned world graph without regenerating it.
	 * @param OutFailureReason First pruning invariant failure if validation fails.
	 * @return true when cached anchors and connections match the pruned gameplay-only graph invariants.
	 */
	bool ValidateCurrentPrunedWorld(FString& OutFailureReason) const;

	float GetDebugRangeOffset() const { return WorldCampaignDebugDefaults::ConnectionDrawHeightOffset; }
	float GetDebugDisplaySeconds() const { return WorldCampaignDebugDefaults::ConnectionDrawDurationSeconds; }
	float GetDebugLineThickness() const { return WorldCampaignDebugDefaults::ConnectionLineThickness; }
	const FWorldCampaignPlacementState& GetPlacementState() const { return M_PlacementState; }

	/**
	 * @brief Exposes read-only world data access for runtime systems that operate after generation.
	 * @return World data component used by generated map objects and division strength lookup.
	 */
	const UWorldDataComponent* GetWorldDataComponent() const;
	void LoadWorldDataIntoObjects();

	/**
	 * @brief Initializes base fortification strength and fortification modifier reasons on enemy and mission objects.
	 * @param GameDifficulty Current campaign difficulty used to apply WorldData multipliers.
	 * @note Called after world data/object promotion so the component report starts with stable fortification data.
	 */
	void InitMapObjectsBaseFortificationStrength(ERTSGameDifficulty GameDifficulty);

	void InitializeCountryOccupationRegulator();

	/**
	 * @brief Recalculates strategic support strength on enemy and mission objects for the current turn.
	 * @param GameDifficulty Current campaign difficulty used to resolve strategic support definitions.
	 * @note This resets only the strategic support category before applying support areas.
	 */
	void AdjustDifficultyPercentagesForStrategicSupport(ERTSGameDifficulty GameDifficulty);

	/**
	 * @brief Legacy no-op hook; runtime field divisions own their strength refresh outside the generator.
	 * @param GameDifficulty Unused because UWorldDivisionManager resolves current division influence.
	 * @note Call AWorldPlayerController::RefreshWorldDivisionInfluence when reports need rebuilding.
	 */
	void AdjustDifficultyPercentagesForFieldDivisions(ERTSGameDifficulty GameDifficulty);

	FWorldCampaignState BuildWorldCampaignStateFromCurrentGeneration() const;
	void RestoreWorldStateFromSave(const FWorldCampaignState& WorldCampaignState);

	/**
	 * @brief Removes unused empty anchors after generation while preserving gameplay-object reachability from HQ.
	 */
	void PruneUnusedAnchorsAndRepairConnectivity();

	/**
	 * @brief Moves the enemy HQ behind missions that would otherwise require attacking it first.
	 */
	void EnsureMissionObjectsAreReachableBeforeEnemyHQ();

	/**
	 * @brief Broadcasts after campaign generation has reached Finished and world actors are realized.
	 * @note BeginPlay systems that need generated anchors/items should wait for this signal.
	 */
	FWorldCampaignGenerationFinishedDelegate& OnGenerationFinished();

	/**
	 * @brief Reports whether generation has fully completed for the current campaign state.
	 * @return true only when the generator has reached the Finished generation step.
	 */
	bool GetIsGenerationFinished() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, show connection degree details where relevant. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Debug Anchor Degree"))
	bool bM_DebugAnchorDegree = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, include player HQ hop distances in debug strings where applicable. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Debug Player HQ Hops"))
	bool bM_DebugPlayerHQHops = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, include enemy HQ hop distances in enemy placement debug strings. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Debug Enemy HQ Hops"))
	bool bM_DebugEnemyHQHops = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, show variant selection info for enemy placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Display Variation Enemy Object Placement"))
	bool bM_DisplayVariationEnemyObjectPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, show hop distance to nearest same enemy type after placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Display Hops From Same Enemy Items"))
	bool bM_DisplayHopsFromSameEnemyItems = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, show hop distance to other neutral items. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Display Hops From Other Neutral Items"))
	bool bM_DisplayHopsFromOtherNeutralItems = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, display mission placement failure reasons. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Debug Failed Mission Placement"))
	bool bM_DebugFailedMissionPlacement = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, display mission candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Debug Mission Candidate Rejections"))
	bool bM_DebugMissionCandidateRejections = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, display enemy candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Debug Enemy Candidate Rejections"))
	bool bM_DebugEnemyCandidateRejections = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, display neutral candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Debug Neutral Candidate Rejections"))
	bool bM_DebugNeutralCandidateRejections = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, include hop distance from HQ in mission placement debug. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Display Hops From HQ For Missions"))
	bool bM_DisplayHopsFromHQForMissions = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, include mission spacing hop distances when spacing rules are active. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Debug Mission Spacing Hops"))
	bool bM_DebugMissionSpacingHops = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, show min/max connection requirements for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Display Min Max Connections For Mission Placement"))
	bool bM_DisplayMinMaxConnectionsForMissionPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, show adjacency requirement summaries for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Display Mission Adjacency Requirements"))
	bool bM_DisplayMissionAdjacencyRequirements = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"When enabled, show required neutral item type for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
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

	UFUNCTION(CallInEditor, Exec, Category = "01 - World Campaign|Debugging",
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
	void DebugDrawMissionPathsToPlayerHQ() const;

	UFUNCTION(CallInEditor, Category = "01 - World Campaign|Debugging",
		meta = (DisplayPriority = 5))
	void DebugDrawSplineBoundaryArea();

	UFUNCTION(CallInEditor, Category = "01 - World Campaign|Debugging",
		meta = (DisplayPriority = 6))
	void ShowPlacementReport();

	UFUNCTION(CallInEditor, Category = "01 - World Campaign|Debugging",
		meta = (DisplayPriority = 7))
	void ValidateAsyncPlacementParityForCurrentSetup();

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
	bool GetIsValidWorldDataComponent() const;
	bool GetIsValidWorldCountryOccupationRegulator() const;

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

	/**
	 * @brief Validates settings and collects boundary data before generated-anchor spawning begins.
	 * @param OutWorld World used for boundary lookup and anchor spawning.
	 * @param OutAnchorClass Spawn class with the native fallback applied.
	 * @param OutBoundaryPolygon Sampled boundary polygon used to constrain generated anchors.
	 * @return true when anchor generation has all game-thread prerequisites.
	 */
	bool TryPrepareAnchorPointGeneration(UWorld*& OutWorld, TSubclassOf<AAnchorPoint>& OutAnchorClass,
	                                     TArray<FVector2D>& OutBoundaryPolygon) const;

	/**
	 * @brief Collects existing valid anchors in deterministic key order for distance checks.
	 * @param OutSortedExistingAnchors Existing anchors sorted by anchor key.
	 */
	void GatherSortedExistingAnchors(TArray<TObjectPtr<AAnchorPoint>>& OutSortedExistingAnchors) const;

	/**
	 * @brief Builds the per-attempt stream used only by generated-anchor placement.
	 * @param AttemptIndex Retry index for the generated-anchor step.
	 * @return Random stream seeded from the campaign seed and anchor-generation offset.
	 */
	FRandomStream CreateAnchorPointRandomStream(int32 AttemptIndex) const;

	/**
	 * @brief Reports generated-anchor totals after a successful spawn pass.
	 * @param NewAnchorCount Number of anchors spawned in this pass.
	 * @param PreferredTargetCount Preferred target selected for this attempt.
	 * @param RequiredCount Minimum configured anchor count.
	 */
	void DisplayAnchorGenerationResult(int32 NewAnchorCount, int32 PreferredTargetCount, int32 RequiredCount) const;

	bool ExecuteCreateConnections(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceHQ(FCampaignGenerationStepTransaction& OutTransaction);
	/**
	 * @brief Filters player HQ candidates by nearby reachable-anchor count.
	 * @param Candidates Degree-filtered candidate anchors.
	 * @param OutNeighborhoodCandidates Candidates that satisfy the player HQ neighborhood rule.
	 */
	void BuildPlayerHQNeighborhoodCandidates(const TArray<TObjectPtr<AAnchorPoint>>& Candidates,
	                                         TArray<TObjectPtr<AAnchorPoint>>& OutNeighborhoodCandidates) const;
	/**
	 * @brief Selects a deterministic fallback player HQ anchor when strict rules fail.
	 * @param CandidateSource Source anchors to search without strict degree rules.
	 * @param AttemptIndex Current player HQ attempt index.
	 * @return Selected fallback anchor, or nullptr if no valid fallback exists.
	 */
	AAnchorPoint* SelectForcedPlayerHQAnchor(const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource,
	                                         int32 AttemptIndex) const;
	/**
	 * @brief Stores the selected player HQ and promotes the world object when not virtual.
	 * @param AnchorPoint Selected player HQ anchor.
	 * @param OutTransaction Transaction receiving the spawned HQ actor when spawned.
	 * @return true when the placement state was finalized.
	 */
	bool TryFinalizePlayerHQPlacement(AAnchorPoint* AnchorPoint,
	                                  FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyHQ(FCampaignGenerationStepTransaction& OutTransaction);
	/**
	 * @brief Applies configured enemy HQ degree preference while preserving deterministic tie order.
	 * @param InOutCandidates Candidate anchors to sort in place.
	 */
	void SortEnemyHQCandidatesByPreference(TArray<TObjectPtr<AAnchorPoint>>& InOutCandidates) const;
	/**
	 * @brief Selects a deterministic fallback enemy HQ anchor when strict rules fail.
	 * @param CandidateSource Source anchors to search without strict degree rules.
	 * @param AttemptIndex Current enemy HQ attempt index.
	 * @return Selected fallback anchor, or nullptr if no valid fallback exists.
	 */
	AAnchorPoint* SelectForcedEnemyHQAnchor(const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource,
	                                        int32 AttemptIndex) const;
	/**
	 * @brief Stores the selected enemy HQ and promotes the world object when not virtual.
	 * @param AnchorPoint Selected enemy HQ anchor.
	 * @param OutTransaction Transaction receiving the spawned HQ actor when spawned.
	 * @return true when the placement state was finalized.
	 */
	bool TryFinalizeEnemyHQPlacement(AAnchorPoint* AnchorPoint,
	                                 FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyWall(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyObjects(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceNeutralObjects(FCampaignGenerationStepTransaction& OutTransaction);
	/**
	 * @brief Checks neutral placement prerequisites before virtual selection starts.
	 * @return true when player HQ, anchors, and hop caches are available.
	 */
	bool ValidateNeutralObjectPlacementPrerequisites() const;
	/**
	 * @brief Selects neutral placements into working state without spawning actors.
	 * @param WorkingPlacementState Mutable placement state used for virtual selection.
	 * @param WorkingDerivedData Mutable derived data used for virtual counts and spacing.
	 * @param OutPromotions Neutral promotions to realize on the game thread after selection.
	 * @return true when every required neutral item was selected.
	 */
	bool BuildNeutralPlacementPromotions(
		FWorldCampaignPlacementState& WorkingPlacementState,
		FWorldCampaignDerivedData& WorkingDerivedData,
		TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>& OutPromotions);
	/**
	 * @brief Spawns selected neutral world objects after virtual placement has succeeded.
	 * @param Promotions Selected neutral anchor/type pairs.
	 * @param OutTransaction Transaction receiving spawned actors for rollback.
	 */
	void ApplyNeutralPromotionsToWorld(
		const TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>& Promotions,
		FCampaignGenerationStepTransaction& OutTransaction);
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
	bool GetShouldDeferWorldObjectPromotion() const;
	bool RealizeVirtualPlacementState();
	/**
	 * @brief Realizes the deferred player HQ placement from its anchor key.
	 * @param AnchorLookup Lookup from anchor keys to live anchors.
	 * @return true when no player HQ is pending or the promotion succeeded.
	 */
	bool RealizeVirtualPlayerHQ(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup);
	/**
	 * @brief Realizes deferred enemy HQ and enemy item placements.
	 * @param AnchorLookup Lookup from anchor keys to live anchors.
	 * @return true when every enemy promotion succeeds.
	 */
	bool RealizeVirtualEnemyPlacements(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup);
	/**
	 * @brief Realizes deferred neutral item placements.
	 * @param AnchorLookup Lookup from anchor keys to live anchors.
	 * @return true when every neutral promotion succeeds.
	 */
	bool RealizeVirtualNeutralPlacements(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup);
	/**
	 * @brief Realizes deferred mission placements.
	 * @param AnchorLookup Lookup from anchor keys to live anchors.
	 * @return true when every mission promotion succeeds.
	 */
	bool RealizeVirtualMissionPlacements(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup);
	/**
	 * @brief Runs anchor and connection setup that must stay on the game thread before solving.
	 * @return true when the immutable placement snapshot can be built next.
	 */
	bool ExecuteGameThreadSetupStepsBeforeAsyncPlacement();
	/**
	 * @brief Captures generated anchors, connections, rules, and retry state for the worker solver.
	 * @param OutSnapshot Plain-data snapshot filled from current game-thread state.
	 * @return true when every required live actor reference was converted to immutable data.
	 */
	bool BuildPlacementSnapshot(FWorldCampaignPlacementSnapshot& OutSnapshot) const;
	/**
	 * @brief Copies seed, retry, rule, and failure-policy values into the snapshot.
	 * @param OutSnapshot Snapshot receiving scalar generation state and rule structs.
	 */
	void CopyPlacementSnapshotSettings(FWorldCampaignPlacementSnapshot& OutSnapshot) const;
	/**
	 * @brief Copies mission tier and per-mission rule maps into worker-safe snapshot entries.
	 * @param OutSnapshot Snapshot receiving mission placement rules.
	 */
	void BuildMissionRuleSnapshots(FWorldCampaignPlacementSnapshot& OutSnapshot) const;
	/**
	 * @brief Copies key-only candidate arrays before actor pointers are converted to indices.
	 * @param OutSnapshot Snapshot receiving diagnostic candidate key arrays.
	 */
	void AppendPlacementSnapshotCandidateKeys(FWorldCampaignPlacementSnapshot& OutSnapshot) const;
	/**
	 * @brief Copies candidate actor arrays into snapshot indices after anchor records are built.
	 * @param OutSnapshot Snapshot receiving index-based candidate arrays.
	 * @param AnchorIndexByActor Lookup produced while building anchor snapshots.
	 */
	void AppendPlacementSnapshotCandidateIndices(
		FWorldCampaignPlacementSnapshot& OutSnapshot,
		const TMap<const AAnchorPoint*, int32>& AnchorIndexByActor) const;
	/**
	 * @brief Copies one mission rule entry into thread-safe data while preserving override ordering.
	 * @param MissionRules Live generation rule entry configured on the actor.
	 * @param OutMissionRules Snapshot entry used by the worker solver.
	 */
	void BuildMissionRuleSnapshot(const FPerMissionRules& MissionRules,
	                              FWorldCampaignPerMissionRulesSnapshot& OutMissionRules) const;
	/**
	 * @brief Records candidate anchor keys for diagnostics and result mapping.
	 * @param AnchorCandidates Live candidate array from generator settings.
	 * @param OutAnchorCandidateKeys Candidate keys appended in the original array order.
	 */
	void AppendAnchorCandidateKeys(const TArray<TObjectPtr<AAnchorPoint>>& AnchorCandidates,
	                               TArray<FGuid>& OutAnchorCandidateKeys) const;
	/**
	 * @brief Records candidate anchor indices so duplicate keys still preserve actor identity.
	 * @param AnchorCandidates Live candidate array from generator settings.
	 * @param AnchorIndexByActor Snapshot index lookup keyed by live actor pointer on the game thread.
	 * @param OutAnchorCandidateIndices Candidate indices appended in the original array order.
	 */
	void AppendAnchorCandidateIndices(const TArray<TObjectPtr<AAnchorPoint>>& AnchorCandidates,
	                                  const TMap<const AAnchorPoint*, int32>& AnchorIndexByActor,
	                                  TArray<int32>& OutAnchorCandidateIndices) const;
	/**
	 * @brief Converts cached anchor actors into immutable graph nodes for worker traversal.
	 * @param OutSnapshot Snapshot receiving anchor records and cached-order indices.
	 * @param OutAnchorIndexByKey Lookup for key-based membership checks while building the snapshot.
	 * @param OutAnchorIndexByActor Lookup for pointer-array settings that must become stable indices.
	 */
	void BuildAnchorSnapshots(FWorldCampaignPlacementSnapshot& OutSnapshot,
	                          TMap<FGuid, int32>& OutAnchorIndexByKey,
	                          TMap<const AAnchorPoint*, int32>& OutAnchorIndexByActor) const;
	/**
	 * @brief Adds one snapshot entry for each valid cached anchor actor.
	 * @param OutSnapshot Snapshot receiving anchor records and cached-order indices.
	 * @param OutAnchorIndexByKey Lookup filled for key-based membership checks.
	 * @param OutAnchorIndexByActor Lookup filled for converting actor arrays to indices.
	 * @param OutSnapshotAnchorsByIndex Live anchors kept only long enough to copy neighbors.
	 */
	void BuildAnchorSnapshotRecords(FWorldCampaignPlacementSnapshot& OutSnapshot,
	                                TMap<FGuid, int32>& OutAnchorIndexByKey,
	                                TMap<const AAnchorPoint*, int32>& OutAnchorIndexByActor,
	                                TArray<TObjectPtr<AAnchorPoint>>& OutSnapshotAnchorsByIndex) const;
	/**
	 * @brief Copies neighbor keys and indices after every anchor has a stable snapshot index.
	 * @param OutSnapshot Snapshot whose anchors receive neighbor data.
	 * @param SnapshotAnchorsByIndex Live anchors ordered to match snapshot indices.
	 * @param AnchorIndexByActor Lookup used to translate neighbor actor pointers to indices.
	 */
	void BuildAnchorSnapshotNeighbors(FWorldCampaignPlacementSnapshot& OutSnapshot,
	                                  const TArray<TObjectPtr<AAnchorPoint>>& SnapshotAnchorsByIndex,
	                                  const TMap<const AAnchorPoint*, int32>& AnchorIndexByActor) const;
	/**
	 * @brief Converts connection actors into immutable connection records for result parity.
	 * @param OutSnapshot Snapshot receiving connection records.
	 */
	void BuildConnectionSnapshots(FWorldCampaignPlacementSnapshot& OutSnapshot) const;
	/**
	 * @brief Marks candidate membership on anchor snapshots after all rule arrays are indexed.
	 * @param OutSnapshot Snapshot whose anchor entries will receive candidate flags.
	 * @param AnchorIndexByKey Lookup used for key-only candidate arrays.
	 */
	void MarkAnchorSnapshotCandidateMembership(FWorldCampaignPlacementSnapshot& OutSnapshot,
	                                           const TMap<FGuid, int32>& AnchorIndexByKey) const;
	/**
	 * @brief Starts the worker solve from an immutable snapshot and schedules completion polling.
	 */
	void StartAsyncPlacementBacktracking();
	/**
	 * @brief Invalidates any in-flight worker result and clears game-thread polling state.
	 */
	void CancelAsyncPlacementBacktracking();
	/**
	 * @brief Checks whether the worker future is ready without blocking the game thread.
	 */
	void PollAsyncPlacementBacktracking();
	/**
	 * @brief Applies, retries, or rejects the worker result after returning to the game thread.
	 * @param Result Pure placement data produced by the worker.
	 * @param GenerationRequestId Request id used to discard stale async completions.
	 */
	void HandleAsyncPlacementCompleted(FWorldCampaignPlacementResult Result, int32 GenerationRequestId);
	/**
	 * @brief Rewinds game-thread setup when the snapshot solver exhausts fixed setup options.
	 * @param Result Worker result describing how many setup transactions must be undone.
	 * @return true when setup was rewound and rebuilt so async solving can restart.
	 */
	bool ApplyAsyncPlacementGameThreadBacktrack(const FWorldCampaignPlacementResult& Result);
	/**
	 * @brief Realizes pure placement keys into world objects after the worker has succeeded.
	 * @param Result Worker result containing anchor keys and item assignments.
	 * @return true when every placement could be mapped back to a valid anchor.
	 */
	bool ApplyPlacementResultToWorld(const FWorldCampaignPlacementResult& Result);
	/**
	 * @brief Emits watchdog progress for long-running worker solves without blocking them.
	 */
	void LogAsyncPlacementProgressIfNeeded();
	/**
	 * @brief Replays worker diagnostics on the game thread where normal reporting is safe.
	 * @param Result Worker result containing deferred debug events.
	 */
	void ReportPlacementDebugEvents(const FWorldCampaignPlacementResult& Result) const;
	/**
	 * @brief Compares current setup between synchronous placement and the snapshot solver.
	 * @param OutFailureReason First mismatch or setup problem when parity fails.
	 * @return true when both paths produce identical pure placement data.
	 */
	bool ValidateAsyncPlacementParityForCurrentSetup_Internal(FString& OutFailureReason);
	/**
	 * @brief Saves mutable placement state so parity tests can run without changing the map.
	 * @return Snapshot of generator fields that parity validation temporarily mutates.
	 */
	FWorldCampaignPlacementParitySavedState CapturePlacementParityState() const;
	/**
	 * @brief Restores fields captured before a parity comparison run.
	 * @param SavedState Generator state captured before virtual placement.
	 */
	void RestorePlacementParityState(const FWorldCampaignPlacementParitySavedState& SavedState);
	/**
	 * @brief Clears placement maps while keeping setup-derived graph caches for parity runs.
	 * @param SetupDerivedData Derived graph data captured after anchor and connection setup.
	 */
	void ResetPlacementStateForParityRun(const FWorldCampaignDerivedData& SetupDerivedData);
	/**
	 * @brief Runs synchronous placement virtually to provide a parity baseline.
	 * @param OutFailureReason Reason when the synchronous baseline cannot finish.
	 * @return true when synchronous virtual placement reaches Finished.
	 */
	bool ExecuteSynchronousPlacementBacktrackingForParity(FString& OutFailureReason);
	/**
	 * @brief Runs the old placement path virtually so the snapshot solver can be compared.
	 * @param OutResult Pure placement result built from the synchronous virtual run.
	 * @param OutFailureReason Reason when the fixed setup cannot be compared.
	 * @return true when the synchronous path reaches a comparable finished placement.
	 */
	bool RunSynchronousPlacementBacktrackingForParity(FWorldCampaignPlacementResult& OutResult,
	                                                  FString& OutFailureReason);
	bool ExecuteSynchronousPlacementStepForParity(ECampaignGenerationStep StepToExecute);
	FWorldCampaignPlacementResult BuildPlacementResultFromCurrentState() const;
	/**
	 * @brief Checks final placement maps before comparing derived caches.
	 * @param SynchronousResult Result from the existing game-thread placement path.
	 * @param AsyncResult Result from the snapshot solver.
	 * @param OutFailureReason First concrete mismatch if comparison fails.
	 * @return true when placement anchors and item maps match exactly.
	 */
	bool ComparePlacementParityResults(const FWorldCampaignPlacementResult& SynchronousResult,
	                                   const FWorldCampaignPlacementResult& AsyncResult,
	                                   FString& OutFailureReason) const;
	/**
	 * @brief Checks derived caches that later generation/debug logic depends on.
	 * @param SynchronousDerivedData Caches produced by the existing game-thread path.
	 * @param AsyncDerivedData Caches produced by the snapshot solver.
	 * @param OutFailureReason First concrete mismatch if comparison fails.
	 * @return true when all compared derived maps match.
	 */
	bool ComparePlacementParityDerivedData(const FWorldCampaignDerivedData& SynchronousDerivedData,
	                                       const FWorldCampaignDerivedData& AsyncDerivedData,
	                                       FString& OutFailureReason) const;
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

	void AddSavedAnchorsAndMapItems(FWorldCampaignState& WorldCampaignState) const;
	void AddSavedConnections(FWorldCampaignState& WorldCampaignState) const;
	void RestoreSavedAnchors(const FWorldCampaignState& WorldCampaignState,
	                         TMap<FGuid, TObjectPtr<AAnchorPoint>>& OutAnchorsByKey,
	                         TArray<TObjectPtr<AAnchorPoint>>& OutRestoredAnchors);
	void RestoreSavedConnections(const FWorldCampaignState& WorldCampaignState,
	                             const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorsByKey);
	void RestoreSavedMapItems(const FWorldCampaignState& WorldCampaignState,
	                          const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorsByKey);
	void ApplyRestoredPlacementState(const FWorldCampaignState& WorldCampaignState,
	                                 const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorsByKey,
	                                 const TArray<TObjectPtr<AAnchorPoint>>& RestoredAnchors);

	bool ExecuteAllStepsWithBacktracking();
	/**
	 * @brief Resets reporting and enables virtual placement for the synchronous backtracking path.
	 */
	void BeginPlacementBacktrackingRun();
	/**
	 * @brief Builds the fixed generation-step order used by synchronous backtracking.
	 * @return Ordered step states from anchors through missions.
	 */
	TArray<ECampaignGenerationStep> BuildBacktrackingStepOrder() const;
	/**
	 * @brief Maps a generation step to its game-thread execution function.
	 * @param StepToExecute Step state to execute.
	 * @param OutStepFunction Function pointer used by ExecuteStepWithTransaction.
	 * @return true when the step is supported by this path.
	 */
	bool TryGetBacktrackingStepFunction(
		ECampaignGenerationStep StepToExecute,
		bool (AGeneratorWorldCampaign::*&OutStepFunction)(FCampaignGenerationStepTransaction&)) const;
	/**
	 * @brief Prints the placement report when compile-time report debugging is enabled.
	 */
	void ReportPlacementBacktrackingIfEnabled() const;
	bool CanExecuteStep(ECampaignGenerationStep CompletedStep) const;
	ECampaignGenerationStep GetPrerequisiteStep(ECampaignGenerationStep CompletedStep) const;
	ECampaignGenerationStep GetNextStep(ECampaignGenerationStep CurrentStep) const;
	int32 GetStepAttemptIndex(ECampaignGenerationStep CompletedStep) const;
	void IncrementStepAttempt(ECampaignGenerationStep CompletedStep);
	void ResetStepAttemptsFrom(ECampaignGenerationStep CompletedStep);
	EPlacementFailurePolicy GetFailurePolicyForStep(ECampaignGenerationStep FailedStep) const;
	/**
	 * @brief Calculates the requested micro undo depth before clamping to available history.
	 * @param FailedStep Step that failed and may support micro backtracking.
	 * @param FailedStepAttemptIndex Attempt index for the failed step after increment.
	 * @return Requested undo depth, or 0 when the failed step has no micro transaction support.
	 */
	int32 GetDesiredMicroUndoDepthForFailure(ECampaignGenerationStep FailedStep, int32 FailedStepAttemptIndex) const;
	/**
	 * @brief Calculates how far micro backtracking should escalate to escape deterministic thrashing.
	 * @param FailedStep Step that failed and triggered micro backtracking.
	 * @param FailedStepAttemptIndex Attempt index for the failed step after increment.
	 * @param TrailingMicroTransactions Available trailing micro transactions for this step.
	 * @return Number of micro transactions to undo, clamped to available history.
	 */
	int32 GetMicroUndoDepthForFailure(ECampaignGenerationStep FailedStep, int32 FailedStepAttemptIndex,
	                                  int32 TrailingMicroTransactions) const;
	/**
	 * @brief Handles failed steps by undoing transactions and advancing the retry index.
	 * @param FailedStep Step that failed to execute.
	 * @param InOutStepIndex Step index to resume from after backtracking.
	 * @param StepOrder Ordered list of completed step states.
	 * @return true if backtracking can continue.
	 */
	bool HandleStepFailure(ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
	                       const TArray<ECampaignGenerationStep>& StepOrder);
	/**
	 * @brief Handles max-attempt failures before normal backtracking policy is considered.
	 * @param FailedStep Step whose attempt budget was exceeded.
	 * @param bStepAttemptsExceeded Whether the per-step budget failed instead of the total budget.
	 * @param StepOrder Ordered list of completed step states.
	 * @param InOutStepIndex Step index to resume from when fail-safe succeeds.
	 * @return true when generation can continue or finish through the timeout fail-safe.
	 */
	bool HandleAttemptLimitFailure(ECampaignGenerationStep FailedStep, bool bStepAttemptsExceeded,
	                               const TArray<ECampaignGenerationStep>& StepOrder, int32& InOutStepIndex);
	/**
	 * @brief Undoes trailing micro transactions before escalating to macro backtracking.
	 * @param FailedStep Step that failed and may have micro transaction history.
	 * @param AttemptIndex Attempt index after the failed step was incremented.
	 * @param StepOrder Ordered list of completed step states.
	 * @param InOutStepIndex Step index to resume from when micro retry is enough.
	 * @return true when micro undo handled the failure and the same step can retry.
	 */
	bool TryHandleMicroBacktracking(ECampaignGenerationStep FailedStep, int32 AttemptIndex,
	                                const TArray<ECampaignGenerationStep>& StepOrder, int32& InOutStepIndex);
	/**
	 * @brief Retries a failed step while rule relaxation attempts are still available.
	 * @param FailedStep Step that failed under relaxable policy.
	 * @param FailurePolicy Policy selected for the failed step.
	 * @param AttemptIndex Attempt index after the failed step was incremented.
	 * @param StepOrder Ordered list of completed step states.
	 * @param InOutStepIndex Step index to resume from when relaxation applies.
	 * @return true when the failed step should retry without undoing macro transactions.
	 */
	bool TryHandleRuleRelaxationRetry(ECampaignGenerationStep FailedStep, EPlacementFailurePolicy FailurePolicy,
	                                  int32 AttemptIndex, const TArray<ECampaignGenerationStep>& StepOrder,
	                                  int32& InOutStepIndex) const;
	/**
	 * @brief Performs macro transaction undo and advances deterministic retry attempts.
	 * @param FailedStep Step that exhausted micro/relaxation options.
	 * @param FailurePolicy Policy selected for the failed step.
	 * @param AttemptIndex Attempt index after the failed step was incremented.
	 * @param StepOrder Ordered list of completed step states.
	 * @param InOutStepIndex Step index to resume from after macro undo.
	 * @return true after backtracking state has been updated.
	 */
	bool HandleMacroBacktracking(ECampaignGenerationStep FailedStep, EPlacementFailurePolicy FailurePolicy,
	                             int32 AttemptIndex, const TArray<ECampaignGenerationStep>& StepOrder,
	                             int32& InOutStepIndex);
	/**
	 * @brief Sets step index to a step's position, or zero when the step is absent.
	 * @param Step Step to locate in the ordered generation list.
	 * @param StepOrder Ordered list of completed step states.
	 * @param OutStepIndex Index to update.
	 */
	void SetStepIndexOrZero(ECampaignGenerationStep Step, const TArray<ECampaignGenerationStep>& StepOrder,
	                        int32& OutStepIndex) const;
	/**
	 * @brief Marks undo reporting context for debugger output while transactions are rolled back.
	 * @param FailedStep Step whose failure caused the undo.
	 */
	void BeginUndoReportContext(ECampaignGenerationStep FailedStep);
	/**
	 * @brief Clears undo reporting context after rollback work is done.
	 */
	void EndUndoReportContext();
	/**
	 * @brief Advances an earlier retry step when downstream placement forced macro undo.
	 * @param FailedStep Step that originally failed.
	 * @param RetryStep Earlier step that will be retried next.
	 */
	void IncrementBacktrackedRetryStepAttempt(ECampaignGenerationStep FailedStep, ECampaignGenerationStep RetryStep);
	/**
	 * @brief Applies a deterministic last-resort placement pass when attempts exceed limits.
	 * @param FailedStep Step that exceeded attempt limits and triggered the fail-safe.
	 * @return true when the fail-safe ran; false when required data is missing.
	 */
	bool TryApplyTimeoutFailSafePlacement(ECampaignGenerationStep FailedStep);
	/**
	 * @brief Builds the missing item list for timeout fail-safe placement.
	 * @param OutItemsToPlace Sorted missing items with fail-safe distance metadata.
	 */
	void BuildTimeoutFailSafeItemsToPlace(TArray<FFailSafeItem>& OutItemsToPlace) const;
	/**
	 * @brief Captures current state before the timeout fail-safe mutates placement data.
	 * @param FailedStep Step whose attempt budget triggered the fail-safe.
	 * @return Transaction containing pre-fail-safe state for rollback.
	 */
	FCampaignGenerationStepTransaction BuildTimeoutFailSafeTransaction(ECampaignGenerationStep FailedStep) const;
	/**
	 * @brief Assigns timeout fail-safe items and runs optional enemy decluster swaps.
	 * @param FailedStep Step whose attempt budget triggered the fail-safe.
	 * @param bHasValidPlayerHQAnchor Whether distance filtering can use player HQ.
	 * @param EmptyAnchors Empty anchors available for fail-safe placement.
	 * @param ItemsToPlace Missing items that should be assigned.
	 * @param InOutFailSafeTransaction Transaction receiving spawned actors and previous state.
	 * @param OutTotals Placement totals for user notification.
	 */
	void ApplyTimeoutFailSafeAssignments(ECampaignGenerationStep FailedStep, bool bHasValidPlayerHQAnchor,
	                                     TArray<FEmptyAnchorDistance>& EmptyAnchors,
	                                     const TArray<FFailSafeItem>& ItemsToPlace,
	                                     FCampaignGenerationStepTransaction& InOutFailSafeTransaction,
	                                     FFailSafePlacementTotals& OutTotals);
	/**
	 * @brief Adds rollback transaction and reports the timeout fail-safe summary.
	 * @param FailSafeTransaction Transaction produced by fail-safe placement.
	 * @param Totals Placement totals to display.
	 */
	void FinalizeTimeoutFailSafePlacement(const FCampaignGenerationStepTransaction& FailSafeTransaction,
	                                      const FFailSafePlacementTotals& Totals);
	/**
	 * @brief Collects empty anchors for timeout fail-safe placement and reports missing prerequisites.
	 * @param OutEmptyAnchors Sorted empty anchors with distance metadata for placement.
	 * @param bOutHasValidPlayerHQAnchor Whether the Player HQ anchor is valid for distance filtering.
	 * @return true when anchor data is available; false when anchors are missing entirely.
	 */
	bool TryBuildTimeoutFailSafeAnchors(TArray<FEmptyAnchorDistance>& OutEmptyAnchors,
	                                    bool& bOutHasValidPlayerHQAnchor) const;
	float GetFailSafeMinDistanceForMission(EMapMission MissionType) const;
	float GetFailSafeMinDistanceForEnemy(EMapEnemyItem EnemyType) const;
	float GetFailSafeMinDistanceForNeutral(EMapNeutralObjectType NeutralType) const;
	void UndoLastTransaction();
	void UndoConnections(const FCampaignGenerationStepTransaction& Transaction);
	int32 CountTrailingMicroTransactionsForStep(ECampaignGenerationStep Step) const;
	bool IsStepEarlierThan(ECampaignGenerationStep StepToCheck, ECampaignGenerationStep ReferenceStep) const;
	int32 CountMicroTransactionsForStepSinceLastInvalidation(ECampaignGenerationStep MacroStep) const;
	void UpdateMicroPlacementProgressFromTransactions();

	/**
	 * @brief Destroys actors recorded by generation transactions before transaction history is cleared.
	 */
	void DestroyActorsFromStepTransactions();

	/**
	 * @brief Removes generated anchors that may no longer be represented by transaction history.
	 */
	void DestroyGeneratedAnchorActors();

	/**
	 * @brief Resets generation progress counters and cached placement data for a fresh run.
	 */
	void ResetGenerationRuntimeState();

	/**
	 * @brief Clears promoted objects and connection references from all surviving anchor actors.
	 */
	void ClearAllAnchorWorldState();

	/**
	 * @brief Clears optional debugger state after gameplay-facing generation state has been reset.
	 */
	void ResetCampaignDebuggerState();

	void ClearPlacementState();
	void ClearDerivedData();
	bool GenerateWorldWithAsyncPlacementForPruningValidation(FString& OutFailureReason);
	TSet<FGuid> BuildGameplayAnchorKeysForPruning() const;
	void RemovePrunedPlacementStateEntries();
	void RefreshCampaignGraphAfterPruning();
	void RefreshConnectionTransactionAfterPruning();
	bool BuildReachableAnchorKeysWithoutEnemyHQ(TSet<FGuid>& OutReachableAnchorKeys) const;

	/**
	 * @brief Finds the first deterministic mission anchor that becomes unreachable when the HQ is treated as blocked.
	 * @param OutMissionAnchorKey Anchor key for the mission that should swap with the enemy HQ.
	 * @param OutMissionType Mission type currently placed on that anchor.
	 * @return true when a blocked mission exists.
	 */
	bool TryFindMissionObjectReachableOnlyThroughEnemyHQ(
		FGuid& OutMissionAnchorKey,
		EMapMission& OutMissionType) const;

	/**
	 * @brief Swaps the enemy HQ placement with the specified mission placement.
	 * @param MissionAnchorKey Current mission anchor that should become the enemy HQ.
	 * @param MissionType Mission type to move onto the old enemy HQ anchor.
	 * @return true when placement state and promoted actors were both updated.
	 */
	bool TrySwapEnemyHQWithMissionObject(const FGuid& MissionAnchorKey, EMapMission MissionType);

	bool GetPrunedCachedAnchorsHaveOnlyGameplay(FString& OutFailureReason) const;
	bool GetPrunedConnectionsAreValid(FString& OutFailureReason) const;
	TSet<FGuid> BuildPrunedCachedAnchorKeys() const;
	TSet<const AConnection*> BuildPrunedCachedConnectionSet() const;
	bool GetPrunedWorldActorsMatchCachedGraph(FString& OutFailureReason) const;
	bool GetPrunedAnchorActorsMatchCachedGraph(const TSet<FGuid>& CachedAnchorKeys,
	                                           FString& OutFailureReason) const;
	bool GetPrunedConnectionActorsMatchCachedGraph(const TSet<const AConnection*>& CachedConnections,
	                                               FString& OutFailureReason) const;
	bool GetPrunedAnchorsReachableFromPlayerHQ(FString& OutFailureReason) const;
	void CacheAnchorConnectionDegrees();
	void BuildChokepointScoresCache(const AAnchorPoint* OptionalHQAnchor);

	/**
	 * @brief Seeds the chokepoint cache so every valid anchor has a deterministic score entry.
	 * @param CachedAnchors Anchors available for the current generated graph.
	 */
	void InitializeChokepointScores(const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors);

	/**
	 * @brief Adds degree-based fallback scores before any HQ-specific path context exists.
	 * @param CachedAnchors Anchors available for the current generated graph.
	 */
	void AddDegreeChokepointScores(const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors);

	/**
	 * @brief Samples deterministic anchor pairs to estimate chokepoints before HQ placement.
	 * @param CachedAnchors Anchors available for sampling.
	 * @param AnchorLookup Lookup used by the path builder to traverse the generated graph.
	 */
	void AddSampledChokepointPathScores(const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors,
	                                    const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup);

	/**
	 * @brief Scores anchors by paths from the selected HQ once HQ context is known.
	 * @param HQAnchor Anchor used as the path source.
	 * @param CachedAnchors Anchors available as path targets.
	 * @param AnchorLookup Lookup used by the path builder to traverse the generated graph.
	 */
	void AddHQChokepointPathScores(const AAnchorPoint* HQAnchor,
	                               const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors,
	                               const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup);
	void DebugNotifyAnchorPicked(AAnchorPoint* AnchorPoint, const FString& Label, const FColor& Color) const;

	/**
	 * @brief Prepares anchors for a deterministic connection pass so retries start cleanly.
	 * @param OutAnchorPoints Output list of sorted, valid anchors.
	 * @return true when there are enough valid anchors to continue.
	 */
	bool PrepareAnchorsForConnectionGeneration(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints);
	void NormalizeAnchorKeysForConnectionGeneration(TArray<TObjectPtr<AAnchorPoint>>& InOutAnchorPoints) const;

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
	 * @brief Resolves the configured connection class with a native fallback.
	 * @return Spawnable connection actor class.
	 */
	TSubclassOf<AConnection> GetConnectionClassToSpawn() const;

	/**
	 * @brief Spawns the connection actor at the midpoint between two anchors.
	 * @param AnchorPoint First endpoint anchor.
	 * @param CandidateAnchor Second endpoint anchor.
	 * @return Spawned connection actor, or nullptr if the world or spawn failed.
	 */
	AConnection* SpawnConnectionBetweenAnchors(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor);

	/**
	 * @brief Initializes and records a newly spawned connection in the generated graph.
	 * @param NewConnection Connection actor that was just spawned.
	 * @param AnchorPoint First endpoint anchor.
	 * @param CandidateAnchor Second endpoint anchor.
	 * @param ExistingSegments Segment cache updated for later intersection checks.
	 * @param DebugColor Debug draw color if enabled at compile time.
	 * @return true when the connection was valid and fully registered.
	 */
	bool FinalizeCreatedConnection(AConnection* NewConnection, AAnchorPoint* AnchorPoint,
	                               AAnchorPoint* CandidateAnchor, TArray<FConnectionSegment>& ExistingSegments,
	                               const FColor& DebugColor);

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
	 * @note Used in: ExecuteAllStepsWithBacktracking.
	 * @note Why: Keeps backtracking after anchor generation fully virtual by deferring expensive map-object actors.
	 * @note Technical: Manual editor step buttons leave this false so intermediate steps can still visualize actors.
	 * @note Notes: Final realization promotes actors once the full layout reaches Finished.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation",
		meta = (AllowPrivateAccess = "true"))
	bool bM_DeferWorldObjectPromotionDuringBacktracking = false;

	ECampaignGenerationStep M_BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;

	int32 M_AsyncPlacementGenerationRequestId = 0;
	int32 M_AsyncPlacementFutureRequestId = 0;
	TFuture<FWorldCampaignPlacementResult> M_AsyncPlacementFuture;
	FTimerHandle M_AsyncPlacementPollTimerHandle;
	TSharedPtr<FWorldCampaignAsyncPlacementProgress, ESPMode::ThreadSafe> M_AsyncPlacementProgress;
	double M_AsyncPlacementStartTimeSeconds = 0.0;
	double M_AsyncPlacementLastProgressLogTimeSeconds = 0.0;
	FWorldCampaignGenerationFinishedDelegate M_OnGenerationFinished;

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
			ToolTip =
			"Base Z offset for debug text drawn at anchors. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Default Debug Height Offset"))
	float M_DefaultDebugHeightOffset = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"Additional Z offset per stacked message while previous debug text is still displaying. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Added Height If Still Displaying"))
	float M_AddedHeightIfStillDisplaying = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"Display time for rejected placement debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Display Time Rejected Location"))
	float M_DisplayTimeRejectedLocation = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"Display time for accepted placement debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Display Time Accepted Location"))
	float M_DisplayTimeAcceptedLocation = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"Maximum total rejection debug draws per placement attempt. Use <= 0 for unlimited. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
			DisplayName = "Max Rejection Draws Per Attempt"))
	int32 M_MaxRejectionDrawsPerAttempt = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 - World Campaign|Debugging",
		meta = (AllowPrivateAccess = "true", DisplayPriority = 2,
			ToolTip =
			"Maximum rejection debug draws per reason per placement attempt. Use <= 0 for unlimited. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."
			,
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldDataComponent> M_WorldDataComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldCountryOccupationRegulator> M_WorldCountryOccupationRegulator = nullptr;

	// Tracks when undo operations are attributed to backtracking failures for reporting.
	bool bM_Report_UndoContextActive = false;
	ECampaignGenerationStep M_Report_CurrentFailureStep = ECampaignGenerationStep::NotStarted;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;
};
