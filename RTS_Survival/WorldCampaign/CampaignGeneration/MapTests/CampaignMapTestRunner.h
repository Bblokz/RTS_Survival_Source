// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

// -----------------------------------------------------------------------------------------------------
// Campaign map determinism test harness.
//
// This entire header (and its matching .cpp) only contains code when RTS_WITH_CAMPAIGN_MAP_TESTS is
// defined, which is done exclusively by RTS_SurvivalShippingCampaignMapTests.Target.cs. In every other
// target the macro is undefined and this file collapses to nothing, so the regular game/shipping build
// is unaffected. There are deliberately no UObjects here so Unreal Header Tool never generates code for
// this module on account of the harness.
// -----------------------------------------------------------------------------------------------------

#if defined(RTS_WITH_CAMPAIGN_MAP_TESTS) && RTS_WITH_CAMPAIGN_MAP_TESTS

#include "CoreMinimal.h"

class UWorld;

namespace CampaignMapTest
{
	/** Number of sequential seeds a full unattended sweep validates when no count is supplied. */
	inline constexpr int32 DefaultSeedCount = 100;

	/** First seed a full unattended sweep starts from when no start seed is supplied. */
	inline constexpr int32 DefaultFirstSeed = 0;

	/**
	 * @brief Runs the backtracking generator once per seed and writes a per-seed determinism log plus a summary.
	 *
	 * For each seed in [FirstSeed, FirstSeed + SeedCount) the existing generator entry point
	 * AGeneratorWorldCampaign::GenerateAndValidatePrunedWorldForSeed is invoked, then the resulting
	 * placement state is dumped to Saved/CampaignMapTests/<RunStamp>/Seed_XXXXXX.log in a canonical,
	 * sorted form so a designer can diff two runs of the same seed to confirm determinism. A Summary.txt
	 * aggregates every seed with a stable fingerprint and the list of any seeds that failed to generate.
	 *
	 * @param World World that already contains the placed campaign generator (the loaded campaign map).
	 * @param FirstSeed First deterministic seed to generate.
	 * @param SeedCount Number of sequential seeds to generate (must be > 0).
	 * @param bQuitWhenDone When true, requests application exit after the sweep so headless runs terminate.
	 */
	void RunCampaignMapTestSweep(UWorld* World, int32 FirstSeed, int32 SeedCount, bool bQuitWhenDone);
}

#endif // defined(RTS_WITH_CAMPAIGN_MAP_TESTS) && RTS_WITH_CAMPAIGN_MAP_TESTS
