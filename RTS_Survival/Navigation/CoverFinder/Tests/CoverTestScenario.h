#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverCombatObserver.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverPoseProbe.h"

class ASquadController;
class ASquadUnit;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class ECoverTestScenarioPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	ObservingIdleCover,
	ObservingCombatCover
};

/** One snapshot of how every live squad unit currently relates to the cover system. */
struct FCoverTestUnitCounts
{
	TSet<int32> OwningPlayers;
	TMap<int32, int32> AliveUnitsPerPlayer;
	TMap<int32, int32> OccupyingUnitsPerPlayer;
	TMap<int32, int32> EngagingFromCoverPerPlayer;
	TMap<int32, int32> UnitsWithTargetInRangePerPlayer;
	TMap<int32, int32> HealthPerPlayer;
	int32 AssignedUnitCount = 0;
	int32 MovingUnitCount = 0;
	int32 OccupyingUnitCount = 0;
	int32 CrouchOccupantCount = 0;
	int32 StandingLeftOccupantCount = 0;
	int32 StandingRightOccupantCount = 0;
	int32 ExposedUnitCount = 0;
	int32 ExposedLeftUnitCount = 0;
	int32 ExposedRightUnitCount = 0;
	// Largest gap between a resting unit and the location its cover animation is supposed to have left it at.
	float MaximumSettledCapsuleError = 0.0f;
	int32 DuplicateReservationCount = 0;
	int32 OccupantsAwayFromPointCount = 0;
	// Cover must only coexist with an idle or attack command on both the unit and its squad.
	int32 CommandInterferenceCount = 0;
	int32 EngagingFromCoverCount = 0;
	int32 AttackCommandOccupantCount = 0;
};

/** One squad of either team that is ordered to attack once the two teams are in contact. */
struct FCoverTestAttackingSquad
{
	TWeakObjectPtr<ASquadController> SquadController;
	float NextAttackOrderSeconds = 0.0f;
	int32 AttackOrdersIssued = 0;
};

/** Highest values seen while the two teams fight, because a single end-of-test snapshot can miss short states. */
struct FCoverTestCombatTotals
{
	TMap<int32, int32> AliveUnitsPerPlayerAtStart;
	int64 FiringLaneTraceCount = 0;
	int64 FiringLaneRejectionCount = 0;
	int32 ApproachOrdersIssued = 0;
	int32 AttackOrdersIssued = 0;
	int32 PeakOccupyingUnitCount = 0;
	int32 PeakEngagingFromCoverCount = 0;
	int32 PeakAttackCommandOccupantCount = 0;
	int32 PeakExposedUnitCount = 0;
	int32 PeakExposedLeftUnitCount = 0;
	int32 PeakExposedRightUnitCount = 0;
	int32 PeakCrouchOccupantCount = 0;
	int32 MisalignedOccupantSamples = 0;
	float MaximumSettledCapsuleError = 0.0f;
	int32 DuplicateReservationSamples = 0;
	int32 CommandInterferenceSamples = 0;
};

/**
 * @brief Drives the TestCover map through an idle phase and an ordered engagement, then logs PASS or FAIL.
 * Owned and ticked by the cover subsystem; started with RTS.CoverFinder.ValidateTestCover or
 * the -CoverFinderValidateTestCover command-line switch.
 */
struct FCoverTestScenario
{
	/**
	 * @brief Arms the scenario; observation only begins once the first full cover scan has been published.
	 * @param IdleObservationSeconds Game time units get to walk into cover before the idle assertions run.
	 * @param bCaptureScreenshot Requests the top-down debug capture after the final result.
	 */
	void Start(float IdleObservationSeconds, bool bCaptureScreenshot);

	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem, float DeltaTime);

	// -CoverFinderApproachAnims=PathA+PathB gives every soldier of the world these approach moves, once, so
	// slides and rolls can be tried in any unattended run without assigning them on the animation Blueprint.
	static void GiveSoldiersCommandLineApproachMoves(UWorld& World);

private:
	FCoverTestCombatTotals M_CombatTotals;
	TArray<FCoverTestAttackingSquad> M_AttackingSquads;
	// Last seen location per unit, to catch a capsule that jumps further in one frame than any cover step allows.
	TMap<TWeakObjectPtr<ASquadUnit>, FVector> M_LastUnitLocations;
	int32 M_UnitLocationJumpCount = 0;
	int32 M_UnitMeshBlowUpCount = 0;
	ECoverTestScenarioPhase M_Phase = ECoverTestScenarioPhase::Inactive;
	float M_PhaseElapsedSeconds = 0.0f;
	float M_IdleObservationSeconds = 0.0f;
	float M_CombatObservationSeconds = 0.0f;
	float M_NextCombatSampleLogSeconds = 0.0f;
	bool bM_CaptureScreenshot = false;
	// -CoverFinderPlayerApproaches swaps the roles so the player squads walk into contact instead of the enemy.
	bool bM_PlayerSquadsApproach = false;
	bool bM_IdlePhasePassed = false;

	// -CoverFinderEnemyAdvance: the approaching side keeps pushing forward in bounds instead of holding at range.
	bool bM_ApproachingSquadsAdvance = false;
	float M_NextAdvanceOrderSeconds = 0.0f;

	// -CoverFinderObserveCombat: measures how well cover shields both sides during the fight.
	bool bM_ObserveCombat = false;
	FCoverCombatObserver M_CombatObserver;

	// -CoverFinderCloseUps: logs bone heights and saves close-up pictures of soldiers in cover.
	FCoverPoseProbe M_PoseProbe;


	// Unrendered meshes stop advancing montages, which would hide every cover clip from an unattended run.
	void KeepUnitAnimationTicking(UWorld& World) const;

	// Flags units that teleport or whose mesh bounds explode, which is what a one-frame visual glitch looks like.
	void WatchUnitsForVisualGlitches(UWorld& World);
	void WatchUnitForVisualGlitches(ASquadUnit& SquadUnit);

	// The map boots paused behind the start-game widget, which would freeze every unit in an unattended run.
	void ResumeWorldIfPaused(UWorld& World) const;
	void TickWaitingForCoverScan(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void FinishIdlePhase(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void TickCombatPhase(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void FinishCombatPhase(UWorld& World, URTSCoverFinderWorldSubsystem& CoverSubsystem);

	/**
	 * @brief Walks one team to the protected-against side of occupied cover so peeking gets exercised.
	 * @return Number of squads that accepted the move order.
	 */
	int32 OrderSquadsToApproach(UWorld& World);

	/**
	 * @brief Prefers a spot in front of a standing-cover occupant; falls back to the nearest opposing unit.
	 * @param SquadController Squad that will be sent.
	 * @param ApproachingSquadIndex Spreads the squads sideways so they do not stack on one location.
	 * @param OutApproachLocation Destination just outside contact range.
	 * @return False when no opposing unit exists to approach.
	 */
	bool FindApproachLocation(
		UWorld& World,
		const ASquadController& SquadController,
		int32 ApproachingSquadIndex,
		FVector& OutApproachLocation) const;

	/**
	 * @brief Moves every squad of the approaching side one bound closer to its nearest enemy and lets it attack
	 * again on arrival, so the defenders face an enemy that keeps changing position.
	 */
	void OrderApproachingSquadsToAdvance(UWorld& World);

	/** Attack orders against unseen targets complete at once, so idle squads in contact are ordered again. */
	void OrderIdleAttackingSquadsToAttack(UWorld& World);
	bool TryOrderSquadToAttack(UWorld& World, ASquadController& SquadController) const;

	FCoverTestUnitCounts GatherUnitCounts(UWorld& World) const;
	void AccumulateUnitCounts(ASquadUnit& SquadUnit, TSet<int64>& InOutReservedPointIds, FCoverTestUnitCounts& InOutCounts) const;
	void LogUnitStates(UWorld& World) const;
	void EnterPhase(ECoverTestScenarioPhase NewPhase);
};
