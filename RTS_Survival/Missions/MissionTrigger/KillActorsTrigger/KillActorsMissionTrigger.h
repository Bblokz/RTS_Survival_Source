
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"
#include "KillActorsMissionTrigger.generated.h"

UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UKillActorsMissionTrigger : public UMissionTrigger
{
    GENERATED_BODY()

public:
    virtual EMissionTriggerType GetTriggerType() const override { return EMissionTriggerType::KillActors; }

    // When this trigger is active, these actors must be “killed” (i.e. become invalid) to satisfy the trigger.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Trigger|Kill Actors")
    TArray<AActor*> ActorsToKill;

    virtual bool CheckTrigger_Implementation() const override;
};
