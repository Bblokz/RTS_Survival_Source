#pragma once

#include "CoreMinimal.h"

struct FAsyncDetailedUnitState;
struct FFindAlliedTanksToRetreat;
struct FFindClosestFlankableEnemyHeavy;
struct FGetPlayerUnitCountsAndBase;
struct FResultAlliedTanksToRetreat;
struct FResultClosestFlankableEnemyHeavy;
struct FResultPlayerUnitCounts;

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
	 * @brief Builds flankable heavy tank targets for the async strategic AI batch.
	 * @param Request Flanking parameters to shape the output positions.
	 * @param DetailedUnitStates Unit snapshot data to search for heavy tanks.
	 * @return Result payload that maps each chosen tank to suggested flank positions.
	 */
	FResultClosestFlankableEnemyHeavy BuildClosestFlankableEnemyHeavyResult(
		const FFindClosestFlankableEnemyHeavy& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates);

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
	 * @brief Finds allied retreat groups and candidate hazmat formation locations.
	 * @param Request Parameters for grouping damaged tanks and selecting hazmat engineers.
	 * @param DetailedUnitStates Unit snapshot data to evaluate.
	 * @return Result payload with up to three retreat groups.
	 */
	FResultAlliedTanksToRetreat BuildAlliedTanksToRetreatResult(
		const FFindAlliedTanksToRetreat& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates);
}
