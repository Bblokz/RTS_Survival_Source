// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"
#include "MissionClassCompletedMissionTrigger.generated.h"

/**
 * @brief Allows a mission to wait until another mission class was completed by the mission manager.
 * Designers assign the exact mission class to watch so mission branches can react to class-specific completion.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UMissionClassCompletedMissionTrigger : public UMissionTrigger
{
	GENERATED_BODY()

public:
	virtual EMissionTriggerType GetTriggerType() const override
	{
		return EMissionTriggerType::MissionClassCompleted;
	}

	// The trigger activates when a mission with this exact class has completed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Trigger|Mission Class Completed")
	TSubclassOf<UMissionBase> MissionClassToWatch;

	virtual bool CheckTrigger_Implementation() const override;
};

