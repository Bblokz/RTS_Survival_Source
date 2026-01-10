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
	bool GetIsFlankableHeavyTank(const FAsyncDetailedUnitState&);
	bool GetIsLightTank(const FAsyncDetailedUnitState&);
	bool GetIsMediumTank(const FAsyncDetailedUnitState&);
	bool GetIsHeavyTank(const FAsyncDetailedUnitState&);
	bool GetIsNomadicHQ(const FAsyncDetailedUnitState&);
	bool GetIsNomadicResourceBuilding(const FAsyncDetailedUnitState&);
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
