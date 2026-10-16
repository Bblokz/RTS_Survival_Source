// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ICommands;

namespace FEnemyOrderQueueHelpers
{
	// Grenade and aim abilities run as queued commands; an order that resets the queue would cancel them mid-throw.
	bool GetIsExecutingQueuedTargetAbility(ICommands* Unit);

	/**
	 * @brief Lets a new enemy AI order wait for a running grenade or aim ability instead of cancelling it.
	 * Drops what was queued behind the ability (such as the attack re-queued by the ability AI) so the new order
	 * runs right after the ability.
	 * @param Unit Unit about to receive the order.
	 * @param bInOutResetQueue Whether the order would clear the queue; set to false when it has to wait.
	 */
	void PrepareOrderBehindRunningAbility(ICommands* Unit, bool& bInOutResetQueue);
}
