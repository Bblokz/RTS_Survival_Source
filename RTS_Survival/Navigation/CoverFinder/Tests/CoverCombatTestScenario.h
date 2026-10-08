#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverCombatScoring.h"

class ASquadUnit;
class URTSCoverFinderWorldSubsystem;
class UWorld;

/** What the combat cover map test counted while it moved an enemy around one soldier. */
struct FCoverCombatTestTotals
{
	int32 DirectionsTested = 0;
	int32 DirectionsWithCover = 0;
	int32 ChosenPointsFacingEnemy = 0;
	int32 RepositionsWhenFlanked = 0;
	int32 FailedChecks = 0;
};

/**
 * @brief Checks combat cover selection against the real cover of a map, without waiting for a firefight to
 * happen to produce a flank. Places an enemy on every side of one soldier and asks the cover subsystem where
 * that soldier should be. Started with the -CoverFinderValidateCombatCover command-line switch.
 */
struct FCoverCombatTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	bool bM_IsWaitingForCoverScan = false;

	void RunChecks(UWorld& World, URTSCoverFinderWorldSubsystem& CoverSubsystem) const;

	/** @return False when the map has no living player soldier and enemy soldier to test with. */
	bool FindTestUnits(UWorld& World, ASquadUnit*& OutSoldier, ASquadUnit*& OutEnemy) const;

	/**
	 * @brief Finds the free cover point with the most other free cover around it, so every enemy bearing has
	 * several points to choose between.
	 * @param Soldier Soldier whose own reservations do not count as taken.
	 * @param OutLocation Location of that point.
	 * @return False when no free cover is published.
	 */
	bool FindDensestFreeCoverLocation(
		const URTSCoverFinderWorldSubsystem& CoverSubsystem,
		const ASquadUnit& Soldier,
		FVector& OutLocation) const;

	/**
	 * @brief Confirms the subsystem did not pass over a better usable point.
	 * @return True when every free point that outscores the chosen one has no firing lane to the enemy.
	 */
	bool GetIsBestUsablePoint(
		URTSCoverFinderWorldSubsystem& CoverSubsystem,
		const ASquadUnit& Soldier,
		const ASquadUnit& Enemy,
		const FRTSCombatCoverThreats& Threats,
		const FRTSCoverPoint& ChosenPoint) const;

	/**
	 * @brief Tests one enemy bearing: the chosen point must face the enemy, must be kept while the enemy stays,
	 * and may only be given up for a clearly better point once the enemy stands on the opposite side.
	 * @param Soldier Soldier that asks for cover.
	 * @param Enemy Enemy that is moved to the tested bearing.
	 * @param EnemyDirection Horizontal direction from the soldier to the enemy.
	 * @param InOutTotals Counters and failed checks.
	 */
	void CheckEnemyDirection(
		URTSCoverFinderWorldSubsystem& CoverSubsystem,
		ASquadUnit& Soldier,
		ASquadUnit& Enemy,
		const FVector& EnemyDirection,
		FCoverCombatTestTotals& InOutTotals) const;

	// Moves the enemy and returns the single-threat set that points at its new location.
	FRTSCombatCoverThreats PlaceEnemy(const ASquadUnit& Soldier, ASquadUnit& Enemy, const FVector& Direction) const;
	void Check(bool bCondition, const TCHAR* Description, FCoverCombatTestTotals& InOutTotals) const;
};
