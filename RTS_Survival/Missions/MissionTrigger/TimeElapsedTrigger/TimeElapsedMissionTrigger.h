// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"

#include "TimeElapsedMissionTrigger.generated.h"

/**
 * @brief Starts a mission once the game has run for Seconds (world game time, so pauses do not count), e.g. a bonus
 * objective whose plate appears a few minutes in. Set it as the MissionTrigger of a mission in the manager's Missions
 * array.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UTimeElapsedMissionTrigger : public UMissionTrigger
{
	GENERATED_BODY()

public:
	virtual EMissionTriggerType GetTriggerType() const override { return EMissionTriggerType::TimeElapsed; }

	// Game seconds since the map started.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger", meta = (ClampMin = "0.0", Units = "s"))
	float Seconds = 60.f;

protected:
	virtual bool CheckTrigger_Implementation() const override;
};
