// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyOrderQueueHelpers.h"

#include "RTS_Survival/Interfaces/Commands.h"

bool FEnemyOrderQueueHelpers::GetIsExecutingQueuedTargetAbility(ICommands* Unit)
{
	if (not Unit)
	{
		return false;
	}
	const EAbilityID ActiveCommand = Unit->GetActiveCommandID();
	return ActiveCommand == EAbilityID::IdThrowGrenade || ActiveCommand == EAbilityID::IdAimAbility;
}

void FEnemyOrderQueueHelpers::PrepareOrderBehindRunningAbility(ICommands* Unit, bool& bInOutResetQueue)
{
	if (not bInOutResetQueue || not GetIsExecutingQueuedTargetAbility(Unit))
	{
		return;
	}
	Unit->ClearQueuedCommandsAfterActive();
	bInOutResetQueue = false;
}
