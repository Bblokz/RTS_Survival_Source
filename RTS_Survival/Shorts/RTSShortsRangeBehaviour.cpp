// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSShortsRangeBehaviour.h"

URTSShortsRangeBehaviour::URTSShortsRangeBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::Permanent;
	// One instance per unit: a second add must not stack a second multiplier on top.
	BehaviourStackRule = EBehaviourStackRule::None;
	M_MaxStackCount = 1;
	BehaviourWeaponMultipliers.RangeMlt = 3.f;
}

void URTSShortsRangeBehaviour::SetRangeMultiplier(const float Multiplier)
{
	URTSShortsRangeBehaviour* Defaults = GetMutableDefault<URTSShortsRangeBehaviour>();
	Defaults->BehaviourWeaponMultipliers.RangeMlt = FMath::Max(Multiplier, 0.01f);
}
