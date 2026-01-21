#pragma once

#include "CoreMinimal.h"

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

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

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
USTRUCT()
struct FFindClosestFlankableEnemyHeavy
{
	GENERATED_BODY()

	FFindClosestFlankableEnemyHeavy();

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	FVector StartSearchLocation;

	UPROPERTY()
	int32 MaxHeavyTanksToFlank;

	UPROPERTY()
	int32 MaxSuggestedFlankPositionsPerTank;

	UPROPERTY()
	float DeltaYawFromLeftRight;

	UPROPERTY()
	float MinDistanceToTank;

	UPROPERTY()
	float MaxDistanceToTank;

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

	UPROPERTY()
	int32 RequestID;

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
USTRUCT()
struct FGetPlayerUnitCountsAndBase
{
	GENERATED_BODY()

	FGetPlayerUnitCountsAndBase();

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

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	int32 PlayerLightTanks;

	UPROPERTY()
	int32 PlayerMediumTanks;

	UPROPERTY()
	int32 PlayerHeavyTanks;

	UPROPERTY()
	int32 PlayerSquads;

	UPROPERTY()
	int32 PlayerNomadicVehicles;

	UPROPERTY()
	FVector PlayerHQLocation;

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
USTRUCT()
struct FFindAlliedTanksToRetreat
{
	GENERATED_BODY()

	FFindAlliedTanksToRetreat();

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	int32 MaxTanksToFind;

	UPROPERTY()
	float MaxDistanceFromOtherGroupMembers;

	UPROPERTY()
	int32 MaxIdleHazmatsToConsider;

	UPROPERTY()
	float MaxHealthRatioConsiderUnitToRetreat;

	UPROPERTY()
	float RetreatFormationDistance;

	UPROPERTY()
	int32 MaxFormationWidth;
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
 */
USTRUCT()
struct FDamagedTanksRetreatGroup
{
	GENERATED_BODY()

	FDamagedTanksRetreatGroup();

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> DamagedTanks;

	UPROPERTY()
	TArray<FWeakActorLocations> HazmatsWithFormationLocations;
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

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	FDamagedTanksRetreatGroup Group1;

	UPROPERTY()
	FDamagedTanksRetreatGroup Group2;

	UPROPERTY()
	FDamagedTanksRetreatGroup Group3;
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

	UPROPERTY()
	TArray<FFindClosestFlankableEnemyHeavy> FindClosestFlankableEnemyHeavyRequests;

	UPROPERTY()
	TArray<FGetPlayerUnitCountsAndBase> GetPlayerUnitCountsAndBaseRequests;

	UPROPERTY()
	TArray<FFindAlliedTanksToRetreat> FindAlliedTanksToRetreatRequests;
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

	UPROPERTY()
	TArray<FResultClosestFlankableEnemyHeavy> FindClosestFlankableEnemyHeavyResults;

	UPROPERTY()
	TArray<FResultPlayerUnitCounts> PlayerUnitCountsResults;

	UPROPERTY()
	TArray<FResultAlliedTanksToRetreat> AlliedTanksToRetreatResults;
};
