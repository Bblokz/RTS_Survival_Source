#pragma once

#include "CoreMinimal.h"


#include "RTSEngagementStance.generated.h"

UENUM()
enum class ERTSEngagementStance : uint8
{
	Stance_None,
	// Will not move out of attack range to find targets.
	Stance_HoldPosition,
	// Will move within AggroRange > AttackRange if not dug in or in cover.
	Stance_Aggressive
};
