#pragma once

#include "CoreMinimal.h"

class URTSCoverFinderWorldSubsystem;

/**
 * @brief Counts the cover a map's first full scan publishes, per object class and per cover type, and compares
 * the counts with a baseline file kept next to the tests. Run it before and after changing the scanner to see
 * that no cover was lost.
 * Started with -CoverFinderCountCover. -CoverFinderWriteCountBaseline lowers the stored baseline to the counts
 * of this run wherever they are lower, so writing it a few times in a row gives the floor of a map whose objects
 * are placed with some randomness; add -CoverFinderResetCountBaseline to start it afresh from this run alone.
 * -CoverFinderCountTolerance=0.8 changes how much lower than the baseline a total may be.
 */
struct FCoverCountTestScenario
{
	void Start(bool bWriteBaseline);
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	bool bM_IsWaitingForCoverScan = false;
	bool bM_WriteBaseline = false;

	/** @return Published point count per "source/type" key, plus one "total/type" key per cover type. */
	TMap<FString, int32> GatherCounts(const URTSCoverFinderWorldSubsystem& CoverSubsystem) const;
	FString GetBaselineFilePath(const FString& MapName) const;
	bool TryLoadBaseline(const FString& FilePath, TMap<FString, int32>& OutBaseline) const;
	/**
	 * @brief Stores the counts, keeping the lower value of every key the existing baseline already has.
	 * @param FilePath Baseline file of the map.
	 * @param Counts Counts of this run.
	 */
	void WriteBaseline(const FString& FilePath, const TMap<FString, int32>& Counts) const;

	/**
	 * @brief Logs every count next to its baseline and decides the result.
	 * @param Counts Counts of this run.
	 * @param Baseline Counts stored earlier.
	 * @return Number of keys whose count fell below what the tolerance allows.
	 */
	int32 CompareWithBaseline(const TMap<FString, int32>& Counts, const TMap<FString, int32>& Baseline) const;

	// Logs per cover type how many points a soldier of the map can and cannot walk to.
	void LogReachability(const URTSCoverFinderWorldSubsystem& CoverSubsystem) const;
};
