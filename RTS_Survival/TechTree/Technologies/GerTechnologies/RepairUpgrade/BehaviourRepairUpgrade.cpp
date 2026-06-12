// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "BehaviourRepairUpgrade.h"

UBehaviourRepairUpgrade::UBehaviourRepairUpgrade()
{
	BehaviourLifeTime = EBehaviourLifeTime::Permanent;
	BehaviourIcon = EBehaviourIcon::TechRepairUpgrade;
	M_TitleText = "Repair Upgrade";
	M_DisplayText = "Repair speed increased by 33%.";
}
