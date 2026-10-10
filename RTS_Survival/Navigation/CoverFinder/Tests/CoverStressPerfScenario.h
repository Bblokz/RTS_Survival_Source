#pragma once

#include "CoreMinimal.h"

class ASquadUnit;
class URTSCoverFinderWorldSubsystem;
class UWorld;

/** One timed part of the stress system: every sample is the average of a batch of calls on one soldier. */
struct FCoverStressPerfMetric
{
	const TCHAR* Name = TEXT("");
	double TotalNanoseconds = 0.0;
	double FastestSampleNanoseconds = TNumericLimits<double>::Max();
	int64 CallCount = 0;
	// Objects that existed after the calls and not before: what the garbage collector has to clean up later.
	int64 CreatedObjectCount = 0;
};

/**
 * @brief Measures what infantry stress costs, on the soldiers that hold cover on the map: the extra work per
 * hit a soldier takes, starting, ticking and ending a stress, restarting the spray animation, and the search
 * for cover further away. Everything is measured in one frame, on real soldiers, and logged as RTS_STRESS_PERF.
 * Started with -CoverFinderMeasureStress on a map where squads take cover by themselves.
 */
struct FCoverStressPerfScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	bool bM_IsWaitingForCoverScan = false;
	bool bM_IsWaitingForSoldiers = false;
	float M_MeasureDeadlineWorldSeconds = 0.0f;

	TArray<ASquadUnit*> FindSoldiersHoldingCover(UWorld& World) const;
	void Measure(UWorld& World, URTSCoverFinderWorldSubsystem& CoverSubsystem, const TArray<ASquadUnit*>& Soldiers);

	// The work a hit adds on top of taking the damage, in a soldier that is nowhere near stressed.
	void MeasureDamagePath(const TArray<ASquadUnit*>& Soldiers) const;
	void MeasureStressLifetime(
		URTSCoverFinderWorldSubsystem& CoverSubsystem,
		const TArray<ASquadUnit*>& Soldiers) const;
	void Report(const FCoverStressPerfMetric& Metric) const;
};
