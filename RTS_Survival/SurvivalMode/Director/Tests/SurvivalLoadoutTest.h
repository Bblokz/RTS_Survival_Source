// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

class ASurvivalModeDirector;

#if RTS_WITH_MAP_TESTS

/**
 * @brief Headless proof run for the locked start army and the reward picker; never in a player build.
 * Started by the director with -SurvivalLoadoutTest: drives the real army selection widget (tries to remove,
 * move and clear the locked HQ, harvesters and scavengers, then confirms), confirms a start location, logs where
 * every player unit landed, then opens test intermissions and clicks a reward card (twice) through the card
 * button, logging whether the picker is on screen and where the keyboard focus went.
 * @note -SurvivalLoadoutTestDir=<dir> receives the screenshots; -SurvivalStartIndex=N picks the start location;
 * -SurvivalGrace=N shortens the grace period.
 */
class FSurvivalLoadoutTest
{
public:
	/** @brief Starts the run when -SurvivalLoadoutTest is on the command line; otherwise does nothing. */
	static void StartIfRequested(ASurvivalModeDirector* Director);
};

#endif
