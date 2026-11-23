// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "RTSShortsRangeBehaviour.generated.h"

/**
 * @brief Shorts only: a permanent weapon behaviour that multiplies every weapon's range (Segment.Survival's 300 %).
 * @note The multiplier is read from the class default when the behaviour is added; SetRangeMultiplier changes it for
 * the render process (no asset, nothing saved).
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API URTSShortsRangeBehaviour : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	URTSShortsRangeBehaviour();

	/** @param Multiplier Range factor, e.g. 3 for 300 %; applies to behaviours added after the call. */
	static void SetRangeMultiplier(const float Multiplier);
};
