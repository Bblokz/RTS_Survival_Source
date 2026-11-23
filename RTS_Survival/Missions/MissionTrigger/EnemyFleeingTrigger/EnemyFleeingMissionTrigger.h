// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"

#include "EnemyFleeingMissionTrigger.generated.h"

/**
 * @brief Starts a mission once the first enemy unit flees to its rally point (UEnemyFallbackSubsystem), e.g. the
 * "Stop the runners" plate. Set it as the MissionTrigger of a mission in the manager's Missions array.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UEnemyFleeingMissionTrigger : public UMissionTrigger
{
	GENERATED_BODY()

public:
	virtual EMissionTriggerType GetTriggerType() const override { return EMissionTriggerType::EnemyFleeing; }

protected:
	virtual bool CheckTrigger_Implementation() const override;
};
