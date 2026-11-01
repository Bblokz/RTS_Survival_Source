// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

class USurvivalSovietBaseComponent;
class ASurvivalModeDirector;

#if RTS_WITH_MAP_TESTS

/**
 * @brief Headless look-at tool for the Soviet bases; never compiled into a player build.
 * -SovietBasePreview=<spec file> generates the listed bases one batch at a time, captures every one top-down and
 * oblique, writes the plan to JSON, counts spawned against planned and tests paths for all four nav agents.
 * -SovietBaseSurvey=<class list file> spawns one Blueprint at a time to log its bounds and picture its facing.
 * @note -SovietBasePreviewDir=<dir> receives the pictures and JSON; the process exits when the list is done.
 */
class FSurvivalSovietBasePreview
{
public:
	/** @brief Starts the tool when one of its switches is on the command line; otherwise does nothing. */
	static void StartIfRequested(ASurvivalModeDirector* Director, USurvivalSovietBaseComponent* SovietBases);
};

#endif
