// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

class USurvivalEventSubsystem;

#if RTS_WITH_MAP_TESTS

/**
 * @brief Headless proof runs for the convoy and the event scheduler; never in a player build.
 * Runs inside a -SurvivalAutoStart match and drives the scheduler's own StartConvoyEvent path:
 * -ConvoyTest=T0 calibration (each truck, escort vehicle and squad on a flat straight, free and restricted),
 * -ConvoyTest=T1 one convoy on a planned route, logged at 4 Hz with frame strips (-ConvoyShots),
 * -ConvoyTest=Sweep scheduler decisions and routes for -ConvoySweepSeeds=N seeds × rounds 1-20 (no spawning),
 * -ConvoyTest=Watch logs the scheduler in a normal match (no forcing).
 * @note -ConvoyTestDir=<dir> receives units.csv, events.csv, cameras.json, census_after.txt and the frames;
 * -ConvoyDefinition=N, -ConvoyTier=N, -ConvoyEntry=/-ConvoyExit=<edge actor name> force the event.
 */
class FSurvivalConvoyTest
{
public:
	/** @brief Starts a run when -ConvoyTest= is on the command line; otherwise does nothing. */
	static void StartIfRequested(USurvivalEventSubsystem* Subsystem);
};

#endif
