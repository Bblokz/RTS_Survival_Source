#pragma once

#include "CoreMinimal.h"

struct FAsyncDetailedUnitState;
struct FFindAlliedTanksToRetreat;
struct FFindClosestFlankableEnemyHeavy;
struct FGetPlayerUnitCountsAndBase;
struct FResultAlliedTanksToRetreat;
struct FResultClosestFlankableEnemyHeavy;
struct FResultPlayerUnitCounts;
struct FFindEnemyBaseClusters;
struct FResultEnemyBaseClusters;
struct FFindLocationsUnderPlayerAttack;
struct FResultLocationsUnderPlayerAttack;
struct FFindPlayerUnitBulkLocations;
struct FResultPlayerUnitBulkLocations;
struct FFindConstructionLocations;
struct FResultConstructionLocations;
struct FFindMineLocations;
struct FResultMineLocations;
struct FEnemyBasePointCoreBuildings;
struct FEnemyBasePointSatelliteBuildings;
struct FStrategicAIRequester;

/**
 * Builds the strategic AI results on the async thread.
 * Every Build function that reads the unit snapshot exists twice: with the requester (the AI player the batch is
 * answered for, with its hostile and friendly masks copied by value) and without, which answers for the first AI
 * player fighting only the human, as when there was one AI player. In the requester versions "player" units are
 * the units of players hostile to the requester, and "enemy" or "allied" units the requester's own.
 */
namespace FStrategicAIHelpers
{
	/**
	 * @brief Keeps flanking logic focused on heavy armor so AI avoids mis-prioritizing light targets.
	 * @param UnitState Snapshot data evaluated for heavy tank flanking eligibility.
	 * @return True when the snapshot represents a heavy tank that can be flanked.
	 */
	bool GetIsFlankableHeavyTank(const FAsyncDetailedUnitState&);

	/**
	 * @brief Filters unit counts to the light armor bucket so strategy layers can balance compositions.
	 * @param UnitState Snapshot data evaluated for light tank classification.
	 * @return True when the snapshot represents a light tank.
	 */
	bool GetIsLightTank(const FAsyncDetailedUnitState&);

	/**
	 * @brief Segments medium armor so aggregate reports stay actionable for match pacing.
	 * @param UnitState Snapshot data evaluated for medium tank classification.
	 * @return True when the snapshot represents a medium tank.
	 */
	bool GetIsMediumTank(const FAsyncDetailedUnitState&);

	/**
	 * @brief Identifies heavy armor for targeting and retreat logic that depends on durability thresholds.
	 * @param UnitState Snapshot data evaluated for heavy tank classification.
	 * @return True when the snapshot represents a heavy tank.
	 */
	bool GetIsHeavyTank(const FAsyncDetailedUnitState&);

	/**
	 * @brief Distinguishes the allied HQ so strategy can track base integrity separately.
	 * @param UnitState Snapshot data evaluated for HQ identification.
	 * @return True when the snapshot represents the nomadic HQ.
	 */
	bool GetIsNomadicHQ(const FAsyncDetailedUnitState&);

	/**
	 * @brief Tracks resource buildings so base reports reflect economic pressure.
	 * @param UnitState Snapshot data evaluated for resource building identification.
	 * @return True when the snapshot represents a nomadic resource building.
	 */
	bool GetIsNomadicResourceBuilding(const FAsyncDetailedUnitState&);

	/**
	 * @brief Flags hazmat engineers so retreat group logic can allocate escort formations.
	 * @param UnitState Snapshot data evaluated for hazmat engineer identification.
	 * @return True when the snapshot represents a hazmat engineer.
	 */
	bool GetIsHazmatEngineer(const FAsyncDetailedUnitState&);

	/**
	 * @brief Builds flank locations for combat-active player heavy tanks in the async strategic AI batch.
	 * @param Request Flanking parameters to shape the output positions.
	 * @param DetailedUnitStates Unit snapshot data to search for heavy tanks.
	 * @return Result payload that maps each combat-active player heavy tank to suggested flank positions.
	 */
	FResultClosestFlankableEnemyHeavy BuildClosestFlankableEnemyHeavyResult(
		const FFindClosestFlankableEnemyHeavy& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates);

	/**
	 * @brief Builds flank locations for the combat-active heavy tanks of the players the requester fights.
	 * @param Request Flanking parameters to shape the output positions.
	 * @param DetailedUnitStates Unit snapshot data to search for heavy tanks.
	 * @param Requester AI player the result is for, with its hostile mask.
	 * @return Result payload that maps each combat-active hostile heavy tank to suggested flank positions.
	 */
	FResultClosestFlankableEnemyHeavy BuildClosestFlankableEnemyHeavyResult(
		const FFindClosestFlankableEnemyHeavy& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates,
		const FStrategicAIRequester& Requester);

	/**
	 * @brief Summarizes player unit counts and base locations for strategic AI decisions.
	 * @param Request Request identifier used to match results to callers.
	 * @param DetailedUnitStates Unit snapshot data to aggregate.
	 * @return Result payload containing counts and base locations.
	 */
	FResultPlayerUnitCounts BuildPlayerUnitCountsResult(
		const FGetPlayerUnitCountsAndBase& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates);

	/**
	 * @brief Counts the units of every player the requester fights; the one HQ location is that of the
	 * lowest-numbered hostile player, so the human's when the human is hostile.
	 * @param Request Request identifier used to match results to callers.
	 * @param DetailedUnitStates Unit snapshot data to aggregate.
	 * @param Requester AI player the result is for, with its hostile mask.
	 * @return Result payload containing counts and base locations.
	 */
	FResultPlayerUnitCounts BuildPlayerUnitCountsResult(
		const FGetPlayerUnitCountsAndBase& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates,
		const FStrategicAIRequester& Requester);

	/**
	 * @brief Finds allied retreat groups and candidate hazmat formation locations.
	 * @param Request Parameters for grouping damaged tanks and selecting hazmat engineers.
	 * @param DetailedUnitStates Unit snapshot data to evaluate.
	 * @return Result payload with up to three retreat groups.
	 */
	FResultAlliedTanksToRetreat BuildAlliedTanksToRetreatResult(
		const FFindAlliedTanksToRetreat& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates);

	/**
	 * @brief Groups the requester's own damaged tanks and idle hazmats; never an ally's, because the requesting
	 * controller orders every actor in the result.
	 * @param Request Parameters for grouping damaged tanks and selecting hazmat engineers.
	 * @param DetailedUnitStates Unit snapshot data to evaluate.
	 * @param Requester AI player whose units are grouped.
	 * @return Result payload with up to three retreat groups.
	 */
	FResultAlliedTanksToRetreat BuildAlliedTanksToRetreatResult(
		const FFindAlliedTanksToRetreat& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates,
		const FStrategicAIRequester& Requester);

	FResultEnemyBaseClusters BuildEnemyBaseClustersResult(
		const FFindEnemyBaseClusters& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates);

	/**
	 * @brief Finds the bases of the requester (its own buildings only) and defense positions around them, which
	 * stay clear of the requester's and its allies' units.
	 * @param Request Building types, clustering thresholds and the locations to defend against.
	 * @param DetailedUnitStates Unit snapshot data to cluster.
	 * @param Requester AI player whose bases are found, with its friendly mask.
	 * @return Base points, satellite points and optional defense position candidates.
	 */
	FResultEnemyBaseClusters BuildEnemyBaseClustersResult(
		const FFindEnemyBaseClusters& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates,
		const FStrategicAIRequester& Requester);

	FResultLocationsUnderPlayerAttack BuildLocationsUnderPlayerAttackResult(
		const FFindLocationsUnderPlayerAttack& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates);

	/**
	 * @brief Flags the locations under pressure from units of the players the requester fights.
	 * @param Request Locations to evaluate and the threat weights.
	 * @param DetailedUnitStates Unit snapshot data to weigh.
	 * @param Requester AI player the result is for, with its hostile mask.
	 * @return The locations that passed the under-attack criteria, with their attackers.
	 */
	FResultLocationsUnderPlayerAttack BuildLocationsUnderPlayerAttackResult(
		const FFindLocationsUnderPlayerAttack& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates,
		const FStrategicAIRequester& Requester);

	/**
	 * @brief Finds player mobile-combat concentrations so strategy can pick macro pressure points.
	 * @param Request Clustering parameters and unit weights used to accept meaningful bulks.
	 * @param DetailedUnitStates Unit snapshot data to cluster.
	 * @return Result payload containing accepted squad/tank bulk locations.
	 */
	FResultPlayerUnitBulkLocations BuildPlayerUnitBulkLocationsResult(
		const FFindPlayerUnitBulkLocations& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates);

	/**
	 * @brief Finds the mobile-combat concentrations of the players the requester fights.
	 * @param Request Clustering parameters and unit weights used to accept meaningful bulks.
	 * @param DetailedUnitStates Unit snapshot data to cluster.
	 * @param Requester AI player the result is for, with its hostile mask.
	 * @return Result payload containing accepted squad/tank bulk locations.
	 */
	FResultPlayerUnitBulkLocations BuildPlayerUnitBulkLocationsResult(
		const FFindPlayerUnitBulkLocations& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates,
		const FStrategicAIRequester& Requester);
	/**
	 * @brief Builds cleaned construction locations while keeping new field builds outside known enemy bases.
	 * @param Request Defense anchors, player bulk locations, and arc cleanup settings.
	 * @param CachedEnemyBaseCoreBuildings Latest accepted core buildings used for wide base-exclusion filtering.
	 * @param CachedEnemyBaseSatelliteBuildings Latest accepted satellite buildings used for tighter base-exclusion filtering.
	 * @return Result payload containing only de-duplicated and base-filtered construction locations.
	 */
	FResultConstructionLocations BuildConstructionLocationsResult(
		const FFindConstructionLocations& Request,
		const TArray<FEnemyBasePointCoreBuildings>& CachedEnemyBaseCoreBuildings,
		const TArray<FEnemyBasePointSatelliteBuildings>& CachedEnemyBaseSatelliteBuildings);

	/**
	 * @brief Builds tactical mine candidates without reading game-thread objects on the async worker.
	 * @param Request Base, player, defense, and sampled-road snapshot plus compact mine placement settings.
	 * @return Road-bound and outer-defense-arc mine candidates filtered outside every enemy base.
	 */
	FResultMineLocations BuildMineLocationsResult(const FFindMineLocations& Request);

}
