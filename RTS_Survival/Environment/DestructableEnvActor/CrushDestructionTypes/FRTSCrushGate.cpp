// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FRTSCrushGate.h"

#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"

bool FRTSCrushGate::GetPassesCrushGate(const AActor* OtherActor, const ECrushDestructionType CrushDestructionType)
{
	const IRTSNavAgentInterface* NavAgent = Cast<IRTSNavAgentInterface>(OtherActor);
	if (not NavAgent)
	{
		return false;
	}
	const ERTSNavAgents AgentType = NavAgent->GetRTSNavAgentType();
	switch (CrushDestructionType)
	{
	case ECrushDestructionType::HeavyTank:
		return AgentType == ERTSNavAgents::HeavyTank;

	case ECrushDestructionType::MediumOrHeavyTank:
		return AgentType == ERTSNavAgents::MediumTank || AgentType == ERTSNavAgents::HeavyTank;

	case ECrushDestructionType::AnyTank:
	default:
		return AgentType != ERTSNavAgents::Character;
	}
}
