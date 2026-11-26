// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"
#include "MissionClassCompletedMissionTrigger.generated.h"

class AMissionManager;

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

	// Also activate when the watched mission FAILED (e.g. a stealth stage that fails when the alarm is raised).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Trigger|Mission Class Completed")
	bool bAlsoTriggerWhenFailed = false;

	// Optional: the trigger also activates when any of these classes completed (an OR with MissionClassToWatch), e.g.
	// an objective that a bonus stage reveals early but that must still appear when a later stage begins.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Trigger|Mission Class Completed")
	TArray<TSubclassOf<UMissionBase>> AlsoWatchMissionClasses;

	// Below this difficulty the trigger never fires, so the mission (and its plate) never appears, e.g. a bonus that
	// only exists on Hard and up. The default lets every difficulty through.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Trigger|Mission Class Completed")
	ERTSGameDifficulty MinimumDifficulty = ERTSGameDifficulty::NewToRTS;

	virtual bool CheckTrigger_Implementation() const override;

private:
	bool GetHasClassEnded(const AMissionManager& MissionManager, const TSubclassOf<UMissionBase>& MissionClass) const;
};

