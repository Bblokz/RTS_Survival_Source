// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

class USurvivalPointsOfInterestSubsystem;

#if RTS_WITH_MAP_TESTS

/**
 * @brief Headless look-at and proof tool for the survival points of interest and wildlife; never in a player build.
 * Runs inside a real -SurvivalAutoStart match (it unpauses the world like the player's start button):
 * -SurvivalPOIPreview: manifest, census, held-camera views of every type and animal group, perf on/off, then exit.
 * -SurvivalPOIWatch: manifest and census only, then exit. -SurvivalPOISweep=N: plans seeds 1..N (no spawning) to a CSV.
 * @note -POISeed=N fixes the match seed, -SurvivalPOIDir=<dir> receives everything, -SurvivalPOICaptureAt=<s> (90).
 */
class FSurvivalPOIPreview
{
public:
	/** @brief Starts the tool when one of its switches is on the command line; otherwise does nothing. */
	static void StartIfRequested(USurvivalPointsOfInterestSubsystem* Subsystem);
};

#endif
