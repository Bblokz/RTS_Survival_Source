// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ECrushDestructionType.h"

/**
 * @brief The nav-agent rule that decides whether an overlapping actor may crush something.
 * Shared by the destructible spline pieces and the crushable crop fields so both crush exactly alike.
 */
struct RTS_SURVIVAL_API FRTSCrushGate
{
	/**
	 * @param OtherActor The overlapping actor; only actors implementing IRTSNavAgentInterface can crush.
	 * @param CrushDestructionType Which vehicle classes are heavy enough.
	 * @return True when OtherActor's nav agent type satisfies the crush rule.
	 */
	static bool GetPassesCrushGate(const AActor* OtherActor, const ECrushDestructionType CrushDestructionType);
};
