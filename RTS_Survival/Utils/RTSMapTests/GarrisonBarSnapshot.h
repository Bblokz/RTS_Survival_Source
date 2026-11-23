// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
class UWorld;

/**
 * Map tests only (-RTSGarrisonBarSnapshot=<dir> with -RTSMapSmokeTest): renders the health bar of the first unit with a
 * garrison bar (transports) and of the first with a modules bar (tanks) to PNG at their widget component's draw size,
 * each with two module-icon-sized images in its ModuleBox, so the icon placement can be reviewed without a damaged
 * vehicle on screen.
 */
namespace RTSGarrisonBarSnapshot
{
	/**
	 * @brief Writes <dir>/<unit class>_<bar class>.png for the first garrison bar and the first modules bar found.
	 * @param World The running map.
	 * @param OutputDirectory Folder for the PNGs.
	 * @return Number of PNGs written.
	 */
	int32 WriteSnapshots(UWorld* World, const FString& OutputDirectory);

	/**
	 * @brief Map tests (-RTSLogLandscapeComponents): logs every landscape component with its material instance,
	 * location and painted layers, to find the components whose cooked material failed (named only by instance).
	 * @param World The running map.
	 * @return Number of components logged.
	 */
	int32 LogLandscapeComponents(UWorld* World);
}
#endif
