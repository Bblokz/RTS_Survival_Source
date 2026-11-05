// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldWorkshopRepairBehaviour.h"

namespace FieldWorkshopRepairConstants
{
	// A Tier III depot: a medium tank at half health is whole again in about 25 s.
	constexpr float RepairPerSecond = 25.f;
	// UBehaviourComp ticks every 2 s (heal and expiry come in 2 s steps); the aura refreshes every 1 s, so the
	// lifetime must outlast a component tick plus an aura interval. Leaving the radius or the workshop's end removes
	// the behaviour at once (the aura's leave/EndPlay), so this is only the backstop.
	constexpr float LifetimeSeconds = 4.f;
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
