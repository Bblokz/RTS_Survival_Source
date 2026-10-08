#pragma once

#include "CoreMinimal.h"

class ASquadUnit;
class URTSCoverFinderWorldSubsystem;
class UWorld;

/** What a soldier is doing about cover at one moment of a fight. */
enum class ECoverObservedState : uint8
{
	CoverProtected,
	CoverExposed,
	WalkingToCover,
	OpenStationary,
	OpenMoving,
	Count
};

/** Per-soldier memory between two ticks of the observer. */
struct FCoverObservedUnit
{
	float LastHealth = 0.0f;
	// World time at which the soldier first came under fire while standing in the open; negative when it is not.
	float UnderFireInOpenSinceSeconds = -1.0f;
	int32 OwningPlayer = 0;
	ECoverObservedState LastState = ECoverObservedState::OpenStationary;
	bool bWasUnderFire = false;
};

/** Everything measured for one player over the observed fight. */
struct FCoverObservedSideTotals
{
	// Soldier-seconds spent in each state while at least one enemy weapon had the soldier as its target.
	float UnderFireSeconds[static_cast<int32>(ECoverObservedState::Count)] = {};
	float NotUnderFireSeconds[static_cast<int32>(ECoverObservedState::Count)] = {};
	float DamageTaken[static_cast<int32>(ECoverObservedState::Count)] = {};
	int32 Deaths[static_cast<int32>(ECoverObservedState::Count)] = {};

	// Of the under-fire seconds in cover: how many of the attackers had the cover between them and the soldier.
	float ShieldedFromAllSeconds = 0.0f;
	float ShieldedFromSomeSeconds = 0.0f;
	float ShieldedFromNoneSeconds = 0.0f;

	// Attacker-seconds per state in which something solid stood on the straight line from the attacker's weapon
	// to the soldier, and in which nothing did. This is what actually stops bullets, whatever the cover's angle.
	float LineOfFireBlockedSeconds[static_cast<int32>(ECoverObservedState::Count)] = {};
	float LineOfFireOpenSeconds[static_cast<int32>(ECoverObservedState::Count)] = {};

	// Of the under-fire seconds standing in the open: whether free cover facing the attacker was in reach.
	float OpenWithUsableCoverNearbySeconds = 0.0f;
	float OpenWithoutUsableCoverSeconds = 0.0f;

	// From first being shot at in the open to sitting in cover.
	float SecondsToCoverSum = 0.0f;
	int32 SecondsToCoverSamples = 0;
	int32 CoverEnteredCount = 0;
	int32 CoverLeftCount = 0;
};

/**
 * @brief Watches a fight on the cover test map and measures how well cover shields the soldiers: how long they
 * are under fire in each state, how much damage each state costs, whether their cover faces their attackers,
 * and how long they take to reach cover. Used by the TestCover scenario with -CoverFinderObserveCombat; it only
 * reads the game and logs RTS_COVER_OBSERVE lines.
 */
struct FCoverCombatObserver
{
	void Reset();
	void Tick(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem, float DeltaTime, float ElapsedSeconds);
	void LogSummary() const;

private:
	TMap<TWeakObjectPtr<ASquadUnit>, FCoverObservedUnit> M_Units;
	TMap<int32, FCoverObservedSideTotals> M_SideTotals;
	float M_NextSampleLogSeconds = 0.0f;

	/**
	 * @brief Lists, for every soldier that is some enemy weapon's target in range, where those enemies stand.
	 * @param World World to read the soldiers from.
	 * @param OutAttackerLocations Attacker locations per targeted soldier.
	 */
	void GatherAttackers(UWorld& World, TMap<const ASquadUnit*, TArray<FVector>>& OutAttackerLocations) const;
	ECoverObservedState ClassifyUnit(const ASquadUnit& SquadUnit) const;

	/**
	 * @brief Adds one soldier's last DeltaTime to the totals of its side.
	 * @param SquadUnit Living soldier.
	 * @param AttackerLocations Enemies shooting at it; nullptr when nobody is.
	 * @param CoverSubsystem Cover service, for the cover around a soldier standing in the open.
	 * @param DeltaTime Seconds since the previous tick.
	 * @param ElapsedSeconds Seconds since the fight began, for the transition log.
	 */
	void ObserveUnit(
		ASquadUnit& SquadUnit,
		const TArray<FVector>* AttackerLocations,
		const URTSCoverFinderWorldSubsystem& CoverSubsystem,
		float DeltaTime,
		float ElapsedSeconds);
	void AccumulateShielding(
		const ASquadUnit& SquadUnit,
		const TArray<FVector>& AttackerLocations,
		const URTSCoverFinderWorldSubsystem& CoverSubsystem,
		float DeltaTime,
		FCoverObservedSideTotals& InOutTotals) const;
	/**
	 * @brief Traces the attackers' lines of fire at one soldier on the channel their weapons use.
	 * @param SquadUnit Soldier being shot at.
	 * @param State What the soldier is doing; a soldier crouched in cover is a lower target.
	 * @param AttackerLocations Enemies shooting at it.
	 * @param DeltaTime Seconds since the previous tick.
	 * @param InOutTotals Totals of the soldier's side.
	 */
	void AccumulateLinesOfFire(
		const ASquadUnit& SquadUnit,
		ECoverObservedState State,
		const TArray<FVector>& AttackerLocations,
		float DeltaTime,
		FCoverObservedSideTotals& InOutTotals) const;
	bool GetHasUsableCoverNearby(
		const ASquadUnit& SquadUnit,
		const FVector& AttackerLocation,
		const URTSCoverFinderWorldSubsystem& CoverSubsystem) const;
	void RecordDeaths();
	void LogSample(float ElapsedSeconds) const;
};
