#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/DirectControl_Retreat/RetreatGroupState.h"

#include "StrategicAIRequests.generated.h"

/**
 * @brief Holds an actor pointer alongside computed world locations for tactical movement.
 * Used by async strategic AI results to provide the engine thread with targetable positions.
 *
 * On the async thread, this struct is populated in two places: flank results attach the
 * heavy tank's UnitActorPtr and locations from BuildFlankPositions using the request
 * arc settings, and retreat results attach the hazmat UnitActorPtr with formation positions
 * built from the hazmat's location/rotation and the damaged tank group size. The async thread
 * never mutates these entries after creation; it moves them into the result batches before
 * dispatching the game thread callback.
 */
USTRUCT()
struct FWeakActorLocations
{
	GENERATED_BODY()

	FWeakActorLocations();

	/**
	 * @brief Keeps the source actor tied to generated positions so consumers can issue follow-up orders.
	 *
	 * @note Filled on async processing when a valid unit candidate is accepted for a flank or retreat output entry.
	 * @note Not filled when candidate filtering removes the unit before result materialization.
	 */
	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	/**
	 * @brief Carries precomputed world-space options to avoid recomputing tactical geometry on game thread.
	 *
	 * @note Filled on async processing immediately after Actor is chosen and helper functions generate position sets.
	 * @note Not filled when no valid flank/formation points are produced for that actor.
	 */
	UPROPERTY()
	TArray<FVector> Locations;
};

/**
 * @brief Requests flank positions around the closest flankable enemy heavy tanks.
 * Used by the async thread to drive heavy-tank flanking queries off detailed unit state.
 *
 * On the async thread, each request is dequeued from the batch and passed to
 * FStrategicAIHelpers::BuildClosestFlankableEnemyHeavyResult with the latest
 * M_DetailedUnitStates snapshot. The helper filters for player-owned units, checks the
 * tank subtype to ensure it is a heavy tank, computes distance to StartSearchLocation,
 * sorts candidates by distance, and then builds flank arc positions for up to
 * MaxHeavyTanksToFlank units using the request's yaw and distance settings.
 */
USTRUCT(Blueprintable)
struct FFindClosestFlankableEnemyHeavy
{
	GENERATED_BODY()

	FFindClosestFlankableEnemyHeavy();

	/**
	 * @brief Preserves request/result pairing so async outputs can be matched without relying on array index assumptions.
	 *
	 * @note Filled on async processing by copying the inbound request identifier into each produced result payload.
	 * @note Not filled only for malformed requests that never enter the processing loop.
	 */
	UPROPERTY()
	int32 RequestID;

	/**
	 * @brief Defines the tactical origin used to prioritize which enemies are worth flanking first.
	 *
	 * @note Filled on async processing from the queued request before distance sorting begins.
	 * @note Not filled when no flank request was enqueued for the current batch slot.
	 */
	UPROPERTY()
	FVector StartSearchLocation;

	/**
	 * @brief Caps expensive flank generation so one request cannot monopolize async compute time.
	 *
	 * @note Filled on async processing from request tuning values before candidate truncation occurs.
	 * @note Not filled when request construction is skipped by higher-level AI logic.
	 */
	UPROPERTY()
	int32 MaxHeavyTanksToFlank;

	/**
	 * @brief Constrains per-target suggestion volume to keep downstream decision cost predictable.
	 *
	 * @note Filled on async processing as helper input when building flank arcs for each accepted heavy tank.
	 * @note Not filled when no heavy tank survives filtering and no arc generation runs.
	 */
	UPROPERTY()
	int32 MaxSuggestedFlankPositionsPerTank;

	/**
	 * @brief Controls lateral spread so generated flank points reflect intended maneuver aggression.
	 *
	 * @note Filled on async processing from request parameters right before flank position synthesis.
	 * @note Not filled when request never reaches helper execution because batch processing aborts earlier.
	 */
	UPROPERTY()
	float DeltaYawFromLeftRight;

	/**
	 * @brief Prevents unsafe or noisy close-range points from being proposed as flank destinations.
	 *
	 * @note Filled on async processing as lower distance constraint during flank point generation.
	 * @note Not filled when no flank point generation is attempted for this request.
	 */
	UPROPERTY()
	float MinDistanceToTank;

	/**
	 * @brief Prevents overextended maneuver suggestions that would break formation responsiveness.
	 *
	 * @note Filled on async processing as upper distance constraint during flank point generation.
	 * @note Not filled when request is filtered out before helper invocation.
	 */
	UPROPERTY()
	float MaxDistanceToTank;

	/**
	 * @brief Lets callers tune geometric spacing so flank suggestions match squad footprint needs.
	 *
	 * @note Filled on async processing before helper expands candidate flank offsets around each tank.
	 * @note Not filled when no valid target tank exists for this request.
	 */
	UPROPERTY()
	float FlankingPositionsSpreadScaler;
};

/**
 * @brief Returns per-heavy-tank flank positions computed for a flankable enemy search.
 * Used to feed the game thread a list of flank locations tied to specific target actors.
 *
 * On the async thread, the result is built by iterating the closest qualifying heavy tanks,
 * creating an FWeakActorLocations entry per tank, and filling it with the tank's UnitActorPtr
 * and flank positions derived from StrategicAIHelperUtilities::BuildFlankPositions. The
 * result array is reserved based on MaxHeavyTanksToFlank, populated in sorted-distance order,
 * and then moved into the result batch before the callback is posted to the game thread.
 */
USTRUCT()
struct FResultClosestFlankableEnemyHeavy
{
	GENERATED_BODY()

	FResultClosestFlankableEnemyHeavy();

	/**
	 * @brief Preserves request/result pairing so async outputs can be matched without relying on array index assumptions.
	 *
	 * @note Filled on async processing by copying the inbound request identifier into each produced result payload.
	 * @note Not filled only for malformed requests that never enter the processing loop.
	 */
	UPROPERTY()
	int32 RequestID;

	/**
	 * @brief Returns target-scoped position bundles so the game thread can issue movement with minimal interpretation.
	 *
	 * @note Filled on async processing after each selected heavy tank receives generated flank locations.
	 * @note Not filled when heavy-tank candidate set resolves to empty.
	 */
	UPROPERTY()
	TArray<FWeakActorLocations> Locations;
};

/**
 * @brief Requests player unit counts and base locations for strategic decisions.
 * Used by the async thread to summarize player forces and key structures.
 *
 * On the async thread, this request is passed to FStrategicAIHelpers::BuildPlayerUnitCountsResult
 * with the current M_DetailedUnitStates array. The helper iterates all player-owned units,
 * increments counts based on tank subtype (light/medium/heavy), increments squad counts,
 * identifies the nomadic HQ to store its location, and collects nomadic resource building
 * locations; all results are accumulated into a single struct and returned.
 */
USTRUCT(Blueprintable)
struct FGetPlayerUnitCountsAndBase
{
	GENERATED_BODY()

	FGetPlayerUnitCountsAndBase();

	/**
	 * @brief Preserves request/result pairing so async outputs can be matched without relying on array index assumptions.
	 *
	 * @note Filled on async processing by copying the inbound request identifier into each produced result payload.
	 * @note Not filled only for malformed requests that never enter the processing loop.
	 */
	UPROPERTY()
	int32 RequestID;
};

/**
 * @brief Returns summarized counts and locations for player units and structures.
 * Used as the async thread's condensed view of player force composition and base layout.
 *
 * On the async thread, values are populated by a full scan of M_DetailedUnitStates for
 * player-owned units. Tank subtypes increment light/medium/heavy counts, squads increment
 * PlayerSquads, nomadic HQ sets PlayerHQLocation, nomadic resource buildings append to
 * PlayerResourceBuildings, and remaining nomadic units increment PlayerNomadicVehicles.
 */
USTRUCT()
struct FResultPlayerUnitCounts
{
	GENERATED_BODY()

	FResultPlayerUnitCounts();

	/**
	 * @brief Preserves request/result pairing so async outputs can be matched without relying on array index assumptions.
	 *
	 * @note Filled on async processing by copying the inbound request identifier into each produced result payload.
	 * @note Not filled only for malformed requests that never enter the processing loop.
	 */
	UPROPERTY()
	int32 RequestID;

	/**
	 * @brief Captures light armor presence to drive strategic weighting without rescanning all units later.
	 *
	 * @note Filled on async processing while iterating player-owned units and classifying tank subtype.
	 * @note Not filled when player unit scan is skipped due to missing state snapshot.
	 */
	UPROPERTY()
	int32 PlayerLightTanks;

	/**
	 * @brief Captures medium armor presence for midline pressure estimation in later planners.
	 *
	 * @note Filled on async processing while iterating player-owned units and classifying tank subtype.
	 * @note Not filled when player unit scan cannot run for the batch.
	 */
	UPROPERTY()
	int32 PlayerMediumTanks;

	/**
	 * @brief Captures heavy armor presence to influence threat and durability heuristics downstream.
	 *
	 * @note Filled on async processing while iterating player-owned units and classifying tank subtype.
	 * @note Not filled when no valid unit-state snapshot is available.
	 */
	UPROPERTY()
	int32 PlayerHeavyTanks;

	/**
	 * @brief Tracks infantry-style force count to support mixed-composition strategic responses.
	 *
	 * @note Filled on async processing when player squad unit entries are encountered in the state scan.
	 * @note Not filled when request was not submitted in the current batch.
	 */
	UPROPERTY()
	int32 PlayerSquads;

	/**
	 * @brief Represents auxiliary nomadic vehicle pressure without forcing multiple category passes.
	 *
	 * @note Filled on async processing for nomadic units that are neither HQ nor resource buildings.
	 * @note Not filled when no qualifying nomadic units are found during scan.
	 */
	UPROPERTY()
	int32 PlayerNomadicVehicles;

	/**
	 * @brief Provides a stable anchor point for macro-level pathing and threat evaluation decisions.
	 *
	 * @note Filled on async processing once the player nomadic HQ entry is identified in unit states.
	 * @note Not filled when HQ is absent from snapshot or ownership/type checks fail.
	 */
	UPROPERTY()
	FVector PlayerHQLocation;

	/**
	 * @brief Supplies strategic expansion anchors so pressure analysis can target economic footprint.
	 *
	 * @note Filled on async processing by appending locations of player-owned nomadic resource buildings.
	 * @note Not filled when no matching buildings are present in scanned state.
	 */
	UPROPERTY()
	TArray<FVector> PlayerResourceBuildings;
};

/**
 * @brief Requests grouping of damaged allied tanks and nearby hazmats for retreat.
 * Used by the async thread to plan retreat formations and support pairings.
 *
 * On the async thread, this request is passed to FStrategicAIHelpers::BuildAlliedTanksToRetreatResult
 * with M_DetailedUnitStates. The helper filters enemy-owned tanks under
 * MaxHealthRatioConsiderUnitToRetreat, sorts by lowest health, groups up to MaxTanksToFind
 * into at most three proximity-based groups using MaxDistanceFromOtherGroupMembers, then
 * finds idle, out-of-combat hazmat engineers and assigns each group the closest candidates
 * up to MaxIdleHazmatsToConsider with formation positions based on RetreatFormationDistance
 * and MaxFormationWidth.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FFindAlliedTanksToRetreat
{
	GENERATED_BODY()

	FFindAlliedTanksToRetreat();

	/**
	 * @brief Preserves request/result pairing so async outputs can be matched without relying on array index assumptions.
	 *
	 * @note Filled on async processing by copying the inbound request identifier into each produced result payload.
	 * @note Not filled only for malformed requests that never enter the processing loop.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequestID;

	/**
	 * @brief Bounds retreat grouping complexity so grouping remains fast under heavy combat load.
	 *
	 * @note Filled on async processing from request constraints before damaged-tank sorting and truncation.
	 * @note Not filled when retreat request is absent from batch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxTanksToFind;

	/**
	 * @brief Defines cohesion tolerance so retreat groups remain supportable by escorts.
	 *
	 * @note Filled on async processing before proximity-based grouping comparisons are executed.
	 * @note Not filled when no damaged tanks qualify for grouping.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxDistanceFromOtherGroupMembers;

	/**
	 * @brief Limits escort search breadth to keep healer assignment work deterministic per group.
	 *
	 * @note Filled on async processing before idle hazmat candidate filtering and distance sorting.
	 * @note Not filled when retreat request is skipped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxIdleHazmatsToConsider;

	/**
	 * @brief Encodes retreat eligibility so only sufficiently damaged units enter regrouping logic.
	 *
	 * @note Filled on async processing as the health threshold during damaged-tank filtering.
	 * @note Not filled when unit health data is unavailable for request execution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HealthRatioThresholdConsiderUnitToRetreat;

	/**
	 * @brief Tunes escort standoff distance so healers form practical support geometry around retreaters.
	 *
	 * @note Filled on async processing before hazmat formation positions are generated.
	 * @note Not filled when no hazmats are assigned to any group.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RetreatFormationDistance;

	/**
	 * @brief Caps formation breadth to prevent overly wide escort layouts that break control coherence.
	 *
	 * @note Filled on async processing as helper input while building retreat formation position sets.
	 * @note Not filled when retreat formation generation is bypassed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxFormationWidth;

	// Units already assigned to active retreat groups and therefore excluded from new grouping.
	/**
	 * @brief Prevents duplicate assignment so units already committed to active retreats are not regrouped.
	 *
	 * @note Filled on async processing from caller-provided exclusion list before damaged tank collection.
	 * @note Not filled when caller provides no exclusions for this batch entry.
	 */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ExcludedRetreatUnitActors;
};

/**
 * @brief Holds one retreat group of damaged tanks and hazmat escort locations.
 * Used to convey the async-computed groupings and escort formations to the game thread.
 *
 * On the async thread, group membership is built by clustering damaged tank UnitActorPtr
 * entries based on proximity checks against MaxDistanceFromOtherGroupMembers. After grouping,
 * hazmat engineers are sorted by distance to the group's average location and each selected
 * hazmat receives formation positions computed from its rotation and the group's size;
 * these FWeakActorLocations entries are appended to HazmatsWithFormationLocations.
 *
 * if no hazmats found then no locations;
 * the locations are not projected.
 */
USTRUCT()
struct FDamagedTanksRetreatGroup
{
	GENERATED_BODY()

	FDamagedTanksRetreatGroup();

	/**
	 * @brief Persists grouped retreat members so follow-up commands target a consistent damaged cohort.
	 *
	 * @note Filled on async processing during proximity clustering of eligible damaged allied tanks.
	 * @note Not filled when no damaged tanks satisfy retreat criteria.
	 */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> DamagedTanks;

	/**
	 * @brief Associates support units with ready-to-use formation points to speed game-thread order issuance.
	 *
	 * @note Filled on async processing after idle hazmats are matched and formation offsets are generated.
	 * @note Not filled when no suitable hazmat escorts are discovered.
	 */
	UPROPERTY()
	TArray<FWeakActorLocations> HazmatsWithFormationLocations;

	/**
	 * @brief Carries retreat lifecycle state so consumers can advance behavior without external bookkeeping.
	 *
	 * @note Filled on async processing with default not-initialized state, then updated later by gameplay systems.
	 * @note Not filled only if struct construction itself is bypassed due to request failure.
	 */
	UPROPERTY()
	EEnemyRetreatGroupState M_CurrentRetreatGroupState = EEnemyRetreatGroupState::NotInitialized;
};

/**
 * @brief Returns up to three retreat groups for damaged allied tanks and escorts.
 * Used to deliver the async thread's retreat grouping and formation positions.
 *
 * On the async thread, FStrategicAIHelpers::BuildAlliedTanksToRetreatResult builds a list of
 * group builders, assigns hazmat formations, then moves the first three groups into Group1-3
 * if present. The groups are ordered by the processing order of the lowest-health tanks and
 * their proximity grouping, and then moved into the result batch for the game thread callback.
 */
USTRUCT()
struct FResultAlliedTanksToRetreat
{
	GENERATED_BODY()

	FResultAlliedTanksToRetreat();

	/**
	 * @brief Preserves request/result pairing so async outputs can be matched without relying on array index assumptions.
	 *
	 * @note Filled on async processing by copying the inbound request identifier into each produced result payload.
	 * @note Not filled only for malformed requests that never enter the processing loop.
	 */
	UPROPERTY()
	int32 RequestID;

	/**
	 * @brief Reserves deterministic primary slot so top-priority retreat cluster can be consumed predictably.
	 *
	 * @note Filled on async processing with first built retreat group when at least one group exists.
	 * @note Not filled when zero groups are produced for the request.
	 */
	UPROPERTY()
	FDamagedTanksRetreatGroup Group1;

	/**
	 * @brief Reserves deterministic secondary slot to avoid dynamic container handling in blueprint consumers.
	 *
	 * @note Filled on async processing with second built retreat group when at least two groups exist.
	 * @note Not filled when fewer than two groups are produced.
	 */
	UPROPERTY()
	FDamagedTanksRetreatGroup Group2;

	/**
	 * @brief Reserves deterministic tertiary slot for final retreat group without extra allocation churn.
	 *
	 * @note Filled on async processing with third built retreat group when at least three groups exist.
	 * @note Not filled when fewer than three groups are produced.
	 */
	UPROPERTY()
	FDamagedTanksRetreatGroup Group3;
};

USTRUCT(Blueprintable)
struct FFindEnemyBaseClusters
{
	GENERATED_BODY()

	FFindEnemyBaseClusters();

	/**
	 * @brief Preserves request/result pairing so async outputs can be matched without relying on array index assumptions.
	 *
	 * @note Filled on async processing by copying the inbound request identifier into each produced result payload.
	 * @note Not filled only for malformed requests that never enter the processing loop.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequestID;

	/**
	 * @brief Defines anchor structure classes so base clustering emphasizes strategic core infrastructure.
	 *
	 * @note Filled on async processing from request filters before scanning enemy building states.
	 * @note Not filled when caller omits core type filters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EBuildingExpansionType> CoreBuildingTypes;

	/**
	 * @brief Adds supporting structure classes so clusters capture full base footprint context.
	 *
	 * @note A satellite point is only allowed to attach to an already existing base cluster.
	 * @note Sattelites cannot create clusters.
	 * @note Filled on async processing from request filters alongside core type matching rules.
	 * @note Not filled when caller omits satellite filters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EBuildingExpansionType> SatelliteBuildingTypes;

	/**
	 * @brief Sets planar grouping radius so nearby core buildings collapse into one base candidate. Uses with the MinCoreNeighbors
	 * where we look in this radius for neighbors; if this radius is low we may not find enough neighbors to facilitate a cluster.
	 *
	 * @note Filled on async processing before cluster-neighbor searches are executed.
	 * @note Not filled when cluster request is not queued.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CoreClusterDistanceXY;

	/**
	 * @brief Applies density gate so sparse structures do not become false-positive base centers.
	* Higher MinCoreNeighbors = fewer bases found
	* Lower MinCoreNeighbors = more bases found
	 *
	 * @note Filled on async processing as minimum-neighbor threshold during core cluster validation.
	 * @note Not filled when no core candidates are found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinCoreNeighbors;

	/**
	 * @brief Requires enough total structures so returned bases reflect meaningful strategic investment.
	 *
	 * @note Filled on async processing before final cluster acceptance scoring.
	 * @note Not filled when preliminary clustering yields no candidates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinTotalBuildingsPerBase;

	/**
	 * @brief Prevents oversized outputs and keeps downstream planning bounded to top opportunities.
	 *
	 * @note Filled on async processing before score-sorted cluster truncation.
	 * @note Not filled when no base clusters pass filters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxBasesToReturn;

	/**
	 * @brief Filters weak clusters so consumers only react to bases with sufficient strategic signal.
	 *
	 * @note Filled on async processing as score cutoff during final base selection.
	 * @note Not filled when scoring stage is never reached.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinBaseScoreToReturn;
};

USTRUCT()
struct FResultEnemyBaseClusters
{
	GENERATED_BODY()

	FResultEnemyBaseClusters();

	/**
	 * @brief Preserves request/result pairing so async outputs can be matched without relying on array index assumptions.
	 *
	 * @note Filled on async processing by copying the inbound request identifier into each produced result payload.
	 * @note Not filled only for malformed requests that never enter the processing loop.
	 */
	UPROPERTY()
	int32 RequestID;

	/**
	 * @brief Provides compact base centroids so strategic systems can reason about enemy macro presence quickly.
	 *
	 * @note Filled on async processing after accepted base clusters are scored and converted to representative points.
	 * @note Not filled when no clusters exceed required thresholds.
	 */
	UPROPERTY()
	TArray<FVector> BasePoints;
};

/**
 * @brief Describes one location threat analysis query for player attack pressure.
 * Used by async strategic AI to determine which defended points are under meaningful player attack risk.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FFindLocationsUnderPlayerAttack
{
	GENERATED_BODY()

	FFindLocationsUnderPlayerAttack();

	/** Identifier copied into results so callers can match async responses safely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequestID;

	/** Candidate defended locations to evaluate against player-unit pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> LocationsToEvaluate;

	/** Maximum distance where units still contribute pressure to a location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxInfluenceRadius;

	/** Shared distance falloff exponent; higher values make far units lose pressure faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistanceExponent;

	/** Minimum final threat score needed before a location is considered under attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinimumThreatScoreToFlagLocation;

	/** Minimum weighted attacker count required to avoid lone-unit false positives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinimumEffectiveAttackerCount;

	/** Amplifies multi-unit groups so larger formations can threaten from farther away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GroupAmplifierPerExtraEffectiveAttacker;

	/** Score contribution for each nearby player squad. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SquadThreatScore;

	/** Score contribution for each nearby player light tank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LightTankThreatScore;

	/** Score contribution for each nearby player medium tank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MediumTankThreatScore;

	/** Score contribution for each nearby player heavy tank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeavyTankThreatScore;

	/** Score contribution for each nearby player armored car. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArmoredCarThreatScore;
};

/**
 * @brief Holds one analyzed location that passed the under-attack criteria.
 * Used by game-thread strategic logic to select responses without recomputing threat geometry.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FPlayerAttackLocationEvaluation
{
	GENERATED_BODY()

	/** Defended location from the original request that was flagged under attack. */
	UPROPERTY(BlueprintReadOnly)
	FVector LocationUnderAttack = FVector::ZeroVector;

	/** Weighted average distance of units considered attackers for this location. */
	UPROPERTY(BlueprintReadOnly)
	float AverageAttackerDistance = 0.f;

	/** Weighted average world location of units considered attackers for this location. */
	UPROPERTY(BlueprintReadOnly)
	FVector AverageAttackerLocation = FVector::ZeroVector;

	/** Player units that contributed to this location being flagged under attack. */
	UPROPERTY(BlueprintReadOnly)
	TArray<TWeakObjectPtr<AActor>> AttackingUnits;
};

/**
 * @brief Returns all locations from a request that are currently under player attack pressure.
 * Used to deliver compact, directly actionable attack-risk data back to strategic AI callers.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FResultLocationsUnderPlayerAttack
{
	GENERATED_BODY()

	FResultLocationsUnderPlayerAttack();

	/** Identifier copied from request so result routing remains deterministic. */
	UPROPERTY(BlueprintReadOnly)
	int32 RequestID;

	/** Locations flagged under attack with aggregate attacker metrics. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FPlayerAttackLocationEvaluation> LocationsUnderAttack;
};

/**
 * @brief Aggregates multiple strategic AI request types into a single async batch.
 * Used to enqueue all pending strategic AI queries for one async processing pass.
 *
 * On the async thread, FGetAsyncTarget::ProcessStrategicAIRequests dequeues this batch and
 * processes each request array independently: it reserves result array capacity to match the
 * request counts, iterates each request, and calls the corresponding helper function using
 * the current M_DetailedUnitStates snapshot. The resulting arrays are assembled into a
 * FStrategicAIResultBatch before invoking the callback on the game thread.
 */
USTRUCT()
struct FStrategicAIRequestBatch
{
	GENERATED_BODY()

	FStrategicAIRequestBatch();

	/**
	 * @brief Batches homogeneous flank queries to amortize async scheduling and callback overhead.
	 *
	 * @note Filled on async processing from queue contents before per-request helper execution loop begins.
	 * @note Not filled when no flank requests were enqueued this frame.
	 */
	UPROPERTY()
	TArray<FFindClosestFlankableEnemyHeavy> FindClosestFlankableEnemyHeavyRequests;

	/**
	 * @brief Batches economy/composition snapshots so strategic decisions share a consistent state sample.
	 *
	 * @note Filled on async processing from queue contents before count-building helper loop begins.
	 * @note Not filled when no count/base requests were queued.
	 */
	UPROPERTY()
	TArray<FGetPlayerUnitCountsAndBase> GetPlayerUnitCountsAndBaseRequests;

	/**
	 * @brief Batches retreat-planning work so damaged-unit triage is processed in one async pass.
	 *
	 * @note Filled on async processing from queue contents before retreat helper loop starts.
	 * @note Not filled when retreat planners submit no work.
	 */
	UPROPERTY()
	TArray<FFindAlliedTanksToRetreat> FindAlliedTanksToRetreatRequests;

	/**
	 * @brief Batches base-discovery queries to reuse the same async state snapshot efficiently.
	 *
	 * @note Filled on async processing from queue contents before base-cluster helper loop starts.
	 * @note Not filled when no base-cluster requests are pending.
	 */
	UPROPERTY()
	TArray<FFindEnemyBaseClusters> FindEnemyBaseClustersRequests;

	/** Batches attack-risk location evaluations so all points share one consistent unit snapshot. */
	UPROPERTY()
	TArray<FFindLocationsUnderPlayerAttack> FindLocationsUnderPlayerAttackRequests;
};

/**
 * @brief Aggregates the async-computed results for all strategic AI request types.
 * Used to deliver a single payload of computed strategic data back to the game thread.
 *
 * On the async thread, each result array is filled in the same order as its matching request
 * array: flank results are built from distance-sorted heavy tanks and flank arcs, unit counts
 * are built from a full player unit scan, and retreat groups are built from damaged tank
 * grouping plus hazmat formation assignments. The batch is moved into the callback lambda
 * that is dispatched to ENamedThreads::GameThread.
 */
USTRUCT()
struct FStrategicAIResultBatch
{
	GENERATED_BODY()

	FStrategicAIResultBatch();

	/**
	 * @brief Returns all flank outputs together to preserve request-order semantics through callback handoff.
	 *
	 * @note Filled on async processing by appending one result per flank request in iteration order.
	 * @note Not filled when flank request array is empty.
	 */
	UPROPERTY()
	TArray<FResultClosestFlankableEnemyHeavy> FindClosestFlankableEnemyHeavyResults;

	/**
	 * @brief Returns composition summaries together so planners can consume synchronized strategic counts.
	 *
	 * @note Filled on async processing by appending one summary per count request in iteration order.
	 * @note Not filled when no count requests were processed.
	 */
	UPROPERTY()
	TArray<FResultPlayerUnitCounts> PlayerUnitCountsResults;

	/**
	 * @brief Returns retreat grouping outputs as one block to simplify game-thread command orchestration.
	 *
	 * @note Filled on async processing by appending one retreat result per retreat request.
	 * @note Not filled when retreat request array is empty.
	 */
	UPROPERTY()
	TArray<FResultAlliedTanksToRetreat> AlliedTanksToRetreatResults;

	/**
	 * @brief Returns discovered base points in batch form to keep macro-target updates coherent.
	 *
	 * @note Filled on async processing by appending one cluster result per base-cluster request.
	 * @note Not filled when no base-cluster requests were processed.
	 */
	UPROPERTY()
	TArray<FResultEnemyBaseClusters> EnemyBaseClustersResults;

	/** Returns under-attack location evaluations in request order for deterministic consumption. */
	UPROPERTY()
	TArray<FResultLocationsUnderPlayerAttack> LocationsUnderPlayerAttackResults;
};
