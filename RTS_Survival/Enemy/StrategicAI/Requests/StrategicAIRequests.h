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
 * @brief Requests flank positions around combat-active player heavy tanks.
 * Used by the async thread to drive heavy-tank flanking queries off detailed unit state.
 *
 * On the async thread, each request is dequeued from the batch and passed to
 * FStrategicAIHelpers::BuildClosestFlankableEnemyHeavyResult with the latest
 * M_DetailedUnitStates snapshot. The helper filters for player-owned units currently in
 * combat, checks the tank subtype to ensure it is a heavy tank, and then builds flank
 * arc positions for every matching target using the request's yaw and distance settings.
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
	 * @note Suggested start value: -1 (`INDEX_NONE`) so uninitialized requests are easy to detect during debugging.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequestID;

	/**
	 * @brief Limits how long strategic decisions may reuse flank data after the game thread receives it.
	 *
	 * @note Used by the heavy-tank flank think step to remove stale result payloads from the blackboard.
	 * @note Suggested start value: 30.0f so flank orders refresh regularly without thrashing async work.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ResultMaxAgeSeconds;

	/**
	 * @brief Constrains per-target suggestion volume to keep downstream decision cost predictable.
	 *
	 * @note Filled on async processing as helper input when building flank arcs for each accepted heavy tank.
	 * @note Not filled when no heavy tank survives filtering and no arc generation runs.
	 * @note Suggested start value: 4 to provide meaningful variety (left/right plus alternates) without flooding selection logic.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSuggestedFlankPositionsPerTank;

	/**
	 * @brief Controls lateral spread so generated flank points reflect intended maneuver aggression.
	 *
	 * @note Filled on async processing from request parameters right before flank position synthesis.
	 * @note Not filled when request never reaches helper execution because batch processing aborts earlier.
	 * @note Suggested start value: 35.0f degrees for clearly lateral movement while avoiding extreme detours.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DeltaYawFromLeftRight;

	/**
	 * @brief Prevents unsafe or noisy close-range points from being proposed as flank destinations.
	 *
	 * @note Filled on async processing as lower distance constraint during flank point generation.
	 * @note Not filled when no flank point generation is attempted for this request.
	 * @note Suggested start value: 600.0f (6 m in UE units) to reduce collision/overlap risk near the tank hull.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinDistanceToTank;

	/**
	 * @brief Prevents overextended maneuver suggestions that would break formation responsiveness.
	 *
	 * @note Filled on async processing as upper distance constraint during flank point generation.
	 * @note Not filled when request is filtered out before helper invocation.
	 * @note Suggested start value: 1600.0f (16 m in UE units) to keep routes practical while still enabling wide flanks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxDistanceToTank;

	/**
	 * @brief Lets callers tune geometric spacing so flank suggestions match squad footprint needs.
	 *
	 * @note Filled on async processing before helper expands candidate flank offsets around each tank.
	 * @note Not filled when no valid target tank exists for this request.
	 * @note Suggested start value: 1.0f to preserve baseline spacing; adjust upward/downward per unit size and maneuver width.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlankingPositionsSpreadScaler;
};

/**
 * @brief Returns per-heavy-tank flank positions computed for a flankable enemy search.
 * Used to feed the game thread a list of flank locations tied to specific target actors.
 *
 * On the async thread, the result is built by iterating qualifying player heavy tanks in combat,
 * creating an FWeakActorLocations entry per tank, and filling it with the tank's UnitActorPtr
 * and flank positions derived from StrategicAIHelperUtilities::BuildFlankPositions. The
 * result array is reserved for the player heavy tanks currently in combat and then moved
 * into the result batch before the callback is posted to the game thread.
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
	 * @brief Lets the strategic blackboard remove stale flank suggestions independently of async completion order.
	 *
	 * @note Filled on the game thread when ProcessClosestFlankableEnemyHeavyResults accepts the result payload.
	 * @note Not filled for results that are discarded before they reach the strategic blackboard.
	 */
	UPROPERTY()
	float ResultTimestampSeconds;

	/**
	 * @brief Returns target-scoped position bundles so the game thread can issue movement with minimal interpretation.
	 *
	 * @note Filled on async processing after each selected heavy tank receives generated flank locations.
	 * @note Not filled when heavy-tank candidate set resolves to empty.
	 */
	UPROPERTY()
	TArray<FWeakActorLocations> FlankLocationsAroundHeavyTank;
};

/**
 * @brief Requests player unit counts and base locations for strategic decisions.
 * Used by the async thread to summarize player forces and key structures.
 *
 * On the async thread, this request is passed to FStrategicAIHelpers::BuildPlayerUnitCountsResult
 * with the current M_DetailedUnitStates array. The helper iterates all player-owned units,
 * increments counts based on tank subtype (armored car/light/medium/heavy), increments squad counts,
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
 * player-owned units. Tank subtypes increment armored car/light/medium/heavy counts, squads increment
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
	 * @brief Captures armored car presence for early vehicle pressure without rescanning all units later.
	 *
	 * @note Filled on async processing while iterating player-owned units and classifying tank subtype.
	 * @note Not filled when player unit scan is skipped due to missing state snapshot.
	 */
	UPROPERTY()
	int32 PlayerArmoredCars;

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


/**
 * @brief Tunes how enemy-base defense arc candidates are generated from detected base footprints.
 * Used by designers to keep async defense points useful without requiring game-thread placement logic.
 */
USTRUCT(BlueprintType)
struct FEnemyBaseDefenseArcCandidateParams
{
	GENERATED_BODY()

	FEnemyBaseDefenseArcCandidateParams();

	/**
	 * @brief Limits how many player approach locations can create arcs for each base.
	 *
	 * @note Higher values defend more simultaneous attack directions but can quickly spend the candidate budget.
	 * @note Set to 0 to ignore player locations and skip defense arc creation while still returning base locations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxThreatLocationsPerBase;

	/**
	 * @brief Merges nearby player locations so one attack group does not create duplicate defensive arcs.
	 *
	 * @note Measured in XY world units before candidate arcs are generated for a base.
	 * @note Use larger values when player bulk locations are noisy or updated frequently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ThreatLocationMergeDistanceXY;

	/**
	 * @brief Merges player locations that attack from almost the same direction relative to a base.
	 *
	 * @note Measured as angle difference around the base center, not distance between player groups.
	 * @note Larger values reduce duplicate front-line arcs when several player groups approach from one side.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ThreatDirectionMergeAngleDegrees;

	/**
	 * @brief Limits how many front-side buildings can anchor arcs for one player approach direction.
	 *
	 * @note Higher values defend wider bases better but produce more candidates before the result cap is applied.
	 * @note Anchors are chosen from core and satellite buildings already accepted into the detected base cluster.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxAnchorBuildingsPerThreatDirection;

	/**
	 * @brief Biases anchor selection toward core buildings because losing them is strategically worse.
	 *
	 * @note Values above SatelliteBuildingAnchorWeight make arcs prefer core structures on the threatened side.
	 * @note Keep this near 1 when satellite defenses should be just as likely as core defenses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CoreBuildingAnchorWeight;

	/**
	 * @brief Biases anchor selection for satellite buildings that extend the defendable base footprint.
	 *
	 * @note Values above CoreBuildingAnchorWeight make arcs prefer outlying satellite structures.
	 * @note Keep this positive so satellite buildings can still create forward defense positions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SatelliteBuildingAnchorWeight;

	/**
	 * @brief Keeps generated positions from sitting too close to their defended building anchor.
	 *
	 * @note Measured in XY world units from the anchor building that created the arc.
	 * @note Use a value above the largest defender/building collision buffer to avoid overlap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinOffsetFromDefendedBuildingXY;

	/**
	 * @brief Sets the intended front-line distance from the defended building toward the player approach.
	 *
	 * @note Arc rows are created around this distance before row offsets and small random jitter are applied.
	 * @note This is the main control for whether defenders stand tight to the base or screen ahead of it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PreferredOffsetFromDefendedBuildingXY;

	/**
	 * @brief Prevents generated positions from drifting too far away from the building they defend.
	 *
	 * @note Candidates beyond this XY distance from their anchor are discarded after random jitter is applied.
	 * @note Keep this higher than PreferredOffsetFromDefendedBuildingXY plus expected row offsets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxOffsetFromDefendedBuildingXY;

	/**
	 * @brief Keeps generated positions clear of every core or satellite building in the defended base.
	 *
	 * @note This catches cases where an arc around one anchor would overlap another nearby building.
	 * @note Measured in XY world units against the accepted cluster footprint only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinOffsetFromAnyBaseBuildingXY;

	/**
	 * @brief Filters final defense positions that would be too close to existing CPU-owned units.
	 *
	 * @note Applied after all arc candidates are accumulated so defenders avoid stacking on allied units or structures.
	 * @note Set to 0 to disable this final allied-unit spacing filter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinDistanceDefPositionFromAlliedUnits;

	/**
	 * @brief Controls how many candidate unit slots are produced across each individual arc row.
	 *
	 * @note Odd values place one candidate on the center approach direction and usually look best.
	 * @note Values below 1 disable arc point creation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PointsPerArc;

	/**
	 * @brief Sets the total horizontal fan width for each defense arc.
	 *
	 * @note Wider arcs cover broad attacks, while narrow arcs create compact guard groups.
	 * @note Measured in degrees around the direction from the defended building to the player location.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArcAngleDegrees;

	/**
	 * @brief Creates layered defensive rows for each selected building and player approach direction.
	 *
	 * @note More rows improve visual depth and tactical fallback options but increase candidate count quickly.
	 * @note Values below 1 disable arc row creation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ArcRowsPerAnchor;

	/**
	 * @brief Separates layered arc rows so defenders do not stand on the same front line.
	 *
	 * @note Added to PreferredOffsetFromDefendedBuildingXY for each additional row.
	 * @note Keep this near expected unit spacing so rows are distinct but still support each other.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArcRowOffsetXY;

	/**
	 * @brief Widens later arc rows so outer defenders naturally cover a broader front.
	 *
	 * @note A value of 1 keeps all rows the same width; values above 1 widen each row progressively.
	 * @note Small increases usually look best because the arc should not swing wildly between rows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArcRowAngleScale;

	/**
	 * @brief Removes generated candidates that are too close to already accepted defense positions.
	 *
	 * @note Applied while candidates are added and again helps when multiple anchors produce overlapping arcs.
	 * @note Use a value near the intended defender footprint or formation spacing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinPointSpacingXY;

	/**
	 * @brief Adds mild location variation so repeated requests do not create identical formations.
	 *
	 * @note The random offset is clamped in XY and then validated against building and unit spacing rules.
	 * @note Keep this much lower than MinPointSpacingXY to avoid unstable-looking formations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PositionJitterXY;

	/**
	 * @brief Adds mild row-distance variation without changing the overall defensive shape.
	 *
	 * @note Expressed as a fraction of the row distance, so 0.08 means up to eight percent variation.
	 * @note Keep this small to avoid defenders jumping too far between async updates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArcDistanceJitterRatio;

	/**
	 * @brief Offsets side-unit facing from the center threat direction for a more natural arc.
	 *
	 * @note Center candidates face the player most directly while edge candidates rotate up to this amount.
	 * @note Measured in degrees and combined with the small random yaw jitter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxYawOffsetFromThreatDegrees;

	/**
	 * @brief Slightly increases yaw variation on outer rows so layered arcs do not look cloned.
	 *
	 * @note A value of 1 keeps row yaw variation identical; values above 1 widen outer row facings.
	 * @note Keep this subtle to preserve the main rule that defenders face the player approach.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OuterArcYawScale;

	/**
	 * @brief Adds small random facing variation after the formation yaw is calculated.
	 *
	 * @note This prevents repeated requests from returning identical rotations for the same geometry.
	 * @note Keep this low so defenders still clearly face the relevant player location.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float YawJitterDegrees;

	/**
	 * @brief Merges final candidates that land nearly on top of each other.
	 *
	 * @note Useful when neighboring anchors or multiple player directions create overlapping arc positions.
	 * @note This is separate from MinPointSpacingXY so designers can tune visual spacing and duplicate cleanup independently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DuplicateCandidateMergeDistanceXY;

	/**
	 * @brief Caps candidate count per accepted base before results are returned to the game thread.
	 *
	 * @note The best early-generated candidates are kept, so lower values favor closer threats and stronger anchors.
	 * @note Set to 0 to allow all candidates for each base, bounded only by the total result cap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxDefensePointCandidatesPerBase;

	/**
	 * @brief Caps total defense candidates across all returned bases in this async result.
	 *
	 * @note Prevents large base counts or many threat directions from producing expensive result payloads.
	 * @note Set to 0 to allow all generated candidates after validation and duplicate filtering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxDefensePointCandidatesTotal;
};

/**
 * @brief Carries one async-generated defensive placement candidate and its intended facing.
 * Used by strategic consumers to place defenders without recalculating threat-facing rotation on the game thread.
 */
USTRUCT(BlueprintType)
struct FDefensePositions
{
	GENERATED_BODY()

	FDefensePositions();

	/**
	 * @brief Marks where a defender can stand while protecting one detected enemy base.
	 *
	 * @note Filled only when player locations were supplied and arc generation produced valid candidates.
	 * @note Not filled for base-only requests or candidates filtered by spacing rules.
	 */
	UPROPERTY()
	FVector Location;

	/**
	 * @brief Stores the world yaw that faces the defended arc toward the relevant player approach.
	 *
	 * @note Filled alongside Location so consumers can orient units consistently with the generated arc.
	 * @note Includes small designer-controlled variation to avoid identical repeated formations.
	 */
	UPROPERTY()
	float YawRotation;
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
	 * @brief Supplies player attack positions that should shape optional base-defense arc candidates.
	 *
	 * @note Base locations are still found when this is empty; only DefensePointCandidates are skipped.
	 * @note Filled by callers from current player unit, bulk, or pressure locations before queuing the async request.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FVector> PlayerUnitLocationsToDefendAgainst;

	/**
	 * @brief Controls optional arc candidate generation around detected base buildings.
	 *
	 * @note Used only when PlayerUnitLocationsToDefendAgainst contains at least one usable location.
	 * @note Base cluster detection still uses the core and satellite settings below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnemyBaseDefenseArcCandidateParams DefenseArcCandidateParams;

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

	/**
	 * @brief Provides optional unit placement candidates for defending detected bases against supplied player positions.
	 *
	 * @note Filled after base clustering only when PlayerUnitLocationsToDefendAgainst produced valid arc candidates.
	 * @note Not filled when the request asks only for base locations or all candidates fail spacing filters.
	 */
	UPROPERTY()
	TArray<FDefensePositions> DefensePointCandidates;
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
	UPROPERTY()
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
 * @brief Describes one player force-concentration query for mobile combat units.
 * Used by async strategic AI to find average locations of meaningful squad and tank bulks.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FFindPlayerUnitBulkLocations
{
	GENERATED_BODY()

	FFindPlayerUnitBulkLocations();

	/** Identifier copied into results so callers can match async responses safely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequestID;

	/** Maximum planar distance where mobile combat units can belong to the same bulk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClusterRadiusXY;

	/** Minimum raw squad/tank count required before a bulk is strategically meaningful. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinUnitsPerBulk;

	/** Minimum weighted score required so low-value groups do not become macro targets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinWeightedBulkScore;

	/** Maximum accepted average spread from center; zero disables the spread gate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxAverageBulkRadiusXY;

	/** Caps returned force concentrations so downstream strategic choices stay bounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxBulksToReturn;

	/** Score contribution for each player squad in a bulk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SquadBulkScore;

	/** Score contribution for each player light tank in a bulk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LightTankBulkScore;

	/** Score contribution for each player medium tank in a bulk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MediumTankBulkScore;

	/** Score contribution for each player heavy tank in a bulk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeavyTankBulkScore;

	/** Score contribution for each player armored car in a bulk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArmoredCarBulkScore;
};

/**
 * @brief Holds one accepted player force concentration for strategic decisions.
 * Used by game-thread AI to target or avoid mobile groups without recomputing clustering.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FPlayerUnitBulkLocation
{
	GENERATED_BODY()

	/** Weighted average world location of the accepted squad/tank bulk. */
	UPROPERTY(BlueprintReadOnly)
	FVector BulkLocation = FVector::ZeroVector;

	/** Final weighted score used to sort stronger bulks before weaker ones. */
	UPROPERTY(BlueprintReadOnly)
	float Score = 0.f;

	/** Sum of accepted unit weights so consumers can distinguish size from raw counts. */
	UPROPERTY(BlueprintReadOnly)
	float WeightedUnitCount = 0.f;

	/** Average planar distance from units to the returned bulk center. */
	UPROPERTY(BlueprintReadOnly)
	float AverageUnitDistanceFromCenter = 0.f;

	/** Farthest planar distance from any contributing unit to the returned bulk center. */
	UPROPERTY(BlueprintReadOnly)
	float MaxUnitDistanceFromCenter = 0.f;

	/** Number of squads contributing to this bulk. */
	UPROPERTY(BlueprintReadOnly)
	int32 SquadCount = 0;

	/** Number of light tanks contributing to this bulk. */
	UPROPERTY(BlueprintReadOnly)
	int32 LightTankCount = 0;

	/** Number of medium tanks contributing to this bulk. */
	UPROPERTY(BlueprintReadOnly)
	int32 MediumTankCount = 0;

	/** Number of heavy tanks contributing to this bulk. */
	UPROPERTY(BlueprintReadOnly)
	int32 HeavyTankCount = 0;

	/** Number of armored cars contributing to this bulk. */
	UPROPERTY(BlueprintReadOnly)
	int32 ArmoredCarCount = 0;

	/** Player units that contributed to this mobile combat bulk. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> UnitsInBulk;
};

/**
 * @brief Returns discovered player squad/tank force concentrations in score order.
 * Used to feed strategic AI compact macro targets around the player's mobile army.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FResultPlayerUnitBulkLocations
{
	GENERATED_BODY()

	FResultPlayerUnitBulkLocations();

	/** Identifier copied from request so result routing remains deterministic. */
	UPROPERTY(BlueprintReadOnly)
	int32 RequestID;

	/** Accepted player mobile-combat bulks sorted from strongest to weakest. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FPlayerUnitBulkLocation> PlayerUnitBulks;
};

/**
 * @brief Aggregates multiple strategic AI request types into a single async batch.
 * Used to enqueue all pending strategic AI queries for one async processing pass.
 *
 * On the async thread, FGetAsyncTarget::ProcessStrategicAIRequests dequeues this batch and
 * processes each request array independently: it reserves result array capacity to match the
 * request counts, iterates each request, and calls the corresponding helper function using
 * the current M_DetailedUnitStates snapshot, including player mobile-combat bulk clustering.
 * The resulting arrays are assembled into a
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

	/** Batches player-force clustering so macro targets share one consistent mobile-unit snapshot. */
	UPROPERTY()
	TArray<FFindPlayerUnitBulkLocations> FindPlayerUnitBulkLocationsRequests;
};

/**
 * @brief Aggregates the async-computed results for all strategic AI request types.
 * Used to deliver a single payload of computed strategic data back to the game thread.
 *
 * On the async thread, each result array is filled in the same order as its matching request
 * array: flank results are built from distance-sorted heavy tanks and flank arcs, unit counts
 * are built from a full player unit scan, retreat groups are built from damaged tank
 * grouping plus hazmat formation assignments, and player bulk locations are built from
 * clustered mobile combat units. The batch is moved into the callback lambda
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

	/** Returns player mobile-combat bulk locations in request order for deterministic consumption. */
	UPROPERTY()
	TArray<FResultPlayerUnitBulkLocations> PlayerUnitBulkLocationsResults;
};
