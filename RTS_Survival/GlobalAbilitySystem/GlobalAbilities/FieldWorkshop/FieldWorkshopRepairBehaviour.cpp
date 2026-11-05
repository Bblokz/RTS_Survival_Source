// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldWorkshopRepairBehaviour.h"

namespace FieldWorkshopRepairConstants
{
	// A Tier III depot: a medium tank at half health is whole again in about 25 s.
	constexpr float RepairPerSecond = 25.f;
	// Longer than the aura's 1 s interval so the repair never gaps, short enough to stop soon after leaving.
	constexpr float LifetimeSeconds = 1.5f;
}

UFieldWorkshopRepairBehaviour::UFieldWorkshopRepairBehaviour()
{
	using namespace FieldWorkshopRepairConstants;
	HealAmountPerSecond = RepairPerSecond;
	BehaviourLifeTime = EBehaviourLifeTime::Timed;
	M_LifeTimeDuration = LifetimeSeconds;
	BehaviourStackRule = EBehaviourStackRule::Refresh;
	M_MaxStackCount = 1;
	bM_UsesTick = true;
	BehaviourIcon = EBehaviourIcon::Repairing;
	M_BuffType = EBuffDebuffType::Buff;
	M_TitleText = TEXT("<Text_Title>Field Workshop Repairs</>");
	M_DisplayText = FString::Printf(TEXT("Repaired for <Text_Exp>%d HP/s</> by a nearby field workshop."),
	                                FMath::RoundToInt(RepairPerSecond));
}
