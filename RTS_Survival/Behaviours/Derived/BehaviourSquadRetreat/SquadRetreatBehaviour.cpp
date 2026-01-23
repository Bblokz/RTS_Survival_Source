// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadRetreatBehaviour.h"

namespace SquadRetreatBehaviourConstants
{
	constexpr float RetreatMovementMultiplier = 1.5f;
}

USquadRetreatBehaviour::USquadRetreatBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::Permanent;
	BehaviourMovementMultipliers.MaxWalkSpeedMultiplier = SquadRetreatBehaviourConstants::RetreatMovementMultiplier;
	BehaviourMovementMultipliers.MaxAccelerationMultiplier = SquadRetreatBehaviourConstants::RetreatMovementMultiplier;
}
