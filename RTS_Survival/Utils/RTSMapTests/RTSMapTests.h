#pragma once

#include "Misc/Build.h"

// The map test and measuring scenarios (started from the command line, for example -CoverFinderValidateStress
// or -SquadPerfMeasure) order units around, hurt them and time them. None of that may reach a player: with this
// at zero the scenarios, the members that host them and the hooks they use are not compiled at all, which is
// the case in every shipping build.
#ifndef RTS_WITH_MAP_TESTS
#define RTS_WITH_MAP_TESTS (!UE_BUILD_SHIPPING)
#endif
